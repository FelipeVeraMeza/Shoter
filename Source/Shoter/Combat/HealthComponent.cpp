#include "Combat/HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;

	if (GetOwnerRole() == ROLE_Authority)
	{
		BandageCount = MaxBandages;

		if (AActor* Owner = GetOwner())
		{
			// El daño puntual llega antes que el genérico: deja anotada la
			// zona para que HandleTakeAnyDamage la use.
			Owner->OnTakePointDamage.AddDynamic(this, &UHealthComponent::HandleTakePointDamage);
			Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
		}
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, bIsDead);
	DOREPLIFETIME(UHealthComponent, bLegInjured);
	DOREPLIFETIME_CONDITION(UHealthComponent, BleedingWounds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHealthComponent, bIsBandaging, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHealthComponent, BandageCount, COND_OwnerOnly);
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwnerRole() != ROLE_Authority || bIsDead)
	{
		return;
	}

	// La hemorragia no se detiene sola: cada herida abierta resta salud
	// hasta que el jugador se vende o muere.
	if (BleedingWounds > 0 && !bInvulnerable)
	{
		ApplyHealthLoss(BleedingWounds * BleedRatePerWound * DeltaTime, LastWoundInstigator.Get());
		if (bIsDead)
		{
			return;
		}
	}

	if (RegenRate <= 0.f || BleedingWounds > 0)
	{
		return;
	}

	if (Health >= MaxHealth)
	{
		return;
	}

	const float TimeSinceDamage = GetWorld()->GetTimeSeconds() - LastDamageTime;
	if (TimeSinceDamage < RegenDelay)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Min(Health + RegenRate * DeltaTime, MaxHealth);
	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);
}

void UHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	// Se consume aquí, antes de cualquier descarte: si quedara anotada, el
	// siguiente daño sin zona (una caída) heredaría la del impacto ignorado.
	const EShooterHitZone Zone = PendingHitZone;
	PendingHitZone = EShooterHitZone::None;

	// La invulnerabilidad descarta el daño antes de tocar la salud (RF3).
	if (Damage <= 0.f || bIsDead || bInvulnerable)
	{
		return;
	}

	LastDamageTime = GetWorld()->GetTimeSeconds();

	// Las balas abren heridas; las caídas y explosiones, no. La zona solo
	// existe si el daño llegó como impacto puntual.
	if (Zone != EShooterHitZone::None && Damage >= BleedDamageThreshold)
	{
		BleedingWounds = FMath::Min(BleedingWounds + 1, MaxBleedingWounds);
		LastWoundInstigator = InstigatedBy;
	}

	if (Zone == EShooterHitZone::Leg)
	{
		bLegInjured = true;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);

	OnHealthChanged.Broadcast(Health, Health - OldHealth, InstigatedBy);

	if (Health <= 0.f)
	{
		bIsDead = true;
		OnOwnerDied.Broadcast(GetOwner(), InstigatedBy);
	}
}

void UHealthComponent::HandleTakePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
	FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
	FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	PendingHitZone = NameToHitZone(BoneName);

	// Sin zona reconocible (por ejemplo, metralla) se trata como torso.
	if (PendingHitZone == EShooterHitZone::None)
	{
		PendingHitZone = EShooterHitZone::Torso;
	}
}

void UHealthComponent::ApplyHealthLoss(float Amount, AController* Instigator)
{
	if (Amount <= 0.f || bIsDead)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);

	// Sin instigador en el aviso: el indicador de daño del HUD no debe
	// señalar a nadie por una herida que ya estaba abierta.
	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);

	if (Health <= 0.f)
	{
		bIsDead = true;
		BleedingWounds = 0;
		OnOwnerDied.Broadcast(GetOwner(), Instigator);
	}
}

void UHealthComponent::ApplyLegInjury()
{
	if (GetOwnerRole() == ROLE_Authority && !bIsDead && !bInvulnerable)
	{
		bLegInjured = true;
	}
}

void UHealthComponent::StartBandage()
{
	if (bIsDead || bIsBandaging || BandageCount <= 0)
	{
		return;
	}

	if (GetOwnerRole() == ROLE_Authority)
	{
		Server_StartBandage_Implementation();
	}
	else
	{
		Server_StartBandage();
	}
}

void UHealthComponent::Server_StartBandage_Implementation()
{
	if (bIsDead || bIsBandaging || BandageCount <= 0)
	{
		return;
	}

	// Vendarse sin heridas ni daño gastaría una venda para nada.
	if (BleedingWounds == 0 && !bLegInjured && Health >= MaxHealth)
	{
		return;
	}

	bIsBandaging = true;
	GetWorld()->GetTimerManager().SetTimer(BandageTimerHandle, this, &UHealthComponent::FinishBandage,
		BandageDuration, false);
}

void UHealthComponent::FinishBandage()
{
	bIsBandaging = false;

	if (bIsDead)
	{
		return;
	}

	--BandageCount;
	BleedingWounds = 0;
	bLegInjured = false;
	LastWoundInstigator.Reset();

	const float OldHealth = Health;
	Health = FMath::Min(Health + BandageHealAmount, MaxHealth);
	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);
}

FName UHealthComponent::HitZoneToName(EShooterHitZone Zone)
{
	switch (Zone)
	{
	case EShooterHitZone::Head:		return TEXT("zone_head");
	case EShooterHitZone::Torso:	return TEXT("zone_torso");
	case EShooterHitZone::Stomach:	return TEXT("zone_stomach");
	case EShooterHitZone::Arm:		return TEXT("zone_arm");
	case EShooterHitZone::Leg:		return TEXT("zone_leg");
	default:						return NAME_None;
	}
}

EShooterHitZone UHealthComponent::NameToHitZone(FName Name)
{
	static const TMap<FName, EShooterHitZone> Zones = {
		{ TEXT("zone_head"), EShooterHitZone::Head },
		{ TEXT("zone_torso"), EShooterHitZone::Torso },
		{ TEXT("zone_stomach"), EShooterHitZone::Stomach },
		{ TEXT("zone_arm"), EShooterHitZone::Arm },
		{ TEXT("zone_leg"), EShooterHitZone::Leg },
	};

	const EShooterHitZone* Found = Zones.Find(Name);
	return Found ? *Found : EShooterHitZone::None;
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);
}

void UHealthComponent::SetInvulnerable(bool bNewInvulnerable)
{
	bInvulnerable = bNewInvulnerable;
}

void UHealthComponent::ResetHealth()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	const float OldHealth = Health;

	Health = MaxHealth;
	bIsDead = false;
	LastDamageTime = -1000.f;
	BleedingWounds = 0;
	bLegInjured = false;
	bIsBandaging = false;
	BandageCount = MaxBandages;
	LastWoundInstigator.Reset();
	GetWorld()->GetTimerManager().ClearTimer(BandageTimerHandle);

	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);
}
