#include "Combat/HealthComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

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
		if (AActor* Owner = GetOwner())
		{
			Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
		}
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, bIsDead);
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwnerRole() != ROLE_Authority || bIsDead || RegenRate <= 0.f)
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
	if (Damage <= 0.f || bIsDead)
	{
		return;
	}

	LastDamageTime = GetWorld()->GetTimeSeconds();

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);

	OnHealthChanged.Broadcast(Health, Health - OldHealth, InstigatedBy);

	if (Health <= 0.f)
	{
		bIsDead = true;
		OnOwnerDied.Broadcast(GetOwner(), InstigatedBy);
	}
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth, nullptr);
}
