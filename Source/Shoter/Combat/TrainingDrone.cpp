#include "Combat/TrainingDrone.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Prefijada con el nombre de la clase: el unity build de Unreal fusiona
	// varios .cpp en una unidad de traducción, y un nombre genérico en un
	// namespace anónimo colisiona con el de otro archivo del mismo módulo.
	/** Duración del destello al recibir un impacto, en segundos. */
	constexpr float DroneHitFlashDuration = 0.1f;
}

ATrainingDrone::ATrainingDrone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// Esfera del motor por defecto: el dron funciona antes de tener su modelo.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereAsset.Object);
	}

	MeshComponent->SetRelativeScale3D(FVector(0.55f));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetMobility(EComponentMobility::Movable);
}

void ATrainingDrone::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	// Desfase derivado de la posición: cada dron flota a su ritmo sin
	// necesidad de configurarlo uno a uno.
	PhaseOffset = FMath::Fmod(FMath::Abs(HomeLocation.X + HomeLocation.Y) * 0.01f, TWO_PI);

	if (HasAuthority())
	{
		Health = MaxHealth;
	}

	if (MeshComponent)
	{
		DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	}

	UpdateVisuals();
}

void ATrainingDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATrainingDrone, Health);
}

void ATrainingDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HitFlashRemaining > 0.f)
	{
		HitFlashRemaining = FMath::Max(HitFlashRemaining - DeltaSeconds, 0.f);
		UpdateVisuals();
	}

	if (IsDestroyed())
	{
		return;
	}

	// El movimiento lo calcula cada máquina con el tiempo del mundo: queda
	// sincronizado sin gastar ancho de banda replicando la posición.
	const float Time = GetWorld()->GetTimeSeconds() + PhaseOffset;

	FVector Offset = FVector::ZeroVector;
	if (HoverAmplitude > 0.f)
	{
		Offset.Z = FMath::Sin(Time * HoverSpeed) * HoverAmplitude;
	}
	if (StrafeAmplitude > 0.f)
	{
		Offset.Y = FMath::Sin(Time * StrafeSpeed) * StrafeAmplitude;
	}

	if (!Offset.IsNearlyZero())
	{
		SetActorLocation(HomeLocation + Offset);
	}

	if (!FMath::IsNearlyZero(SpinSpeed))
	{
		AddActorLocalRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f));
	}
}

float ATrainingDrone::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Solo el servidor aplica daño; el cliente recibe Health replicado.
	if (!HasAuthority() || ActualDamage <= 0.f || IsDestroyed())
	{
		return ActualDamage;
	}

	Health = FMath::Max(Health - ActualDamage, 0.f);
	UpdateVisuals();

	Multicast_PlayHitEffects(GetActorLocation());

	if (Health <= 0.f)
	{
		HandleDestruction(EventInstigator);
	}

	return ActualDamage;
}

void ATrainingDrone::Multicast_PlayHitEffects_Implementation(const FVector_NetQuantize& ImpactLocation)
{
	HitFlashRemaining = DroneHitFlashDuration;
	UpdateVisuals();

	// Chispas en el punto de impacto (RF2).
	if (HitSparks)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitSparks, ImpactLocation);
	}

	// Sonido espacializado: se oye desde donde ocurre (RNF3).
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, ImpactLocation);
	}
}

void ATrainingDrone::HandleDestruction(AController* InstigatedBy)
{
	Multicast_PlayDestruction();
	Multicast_SetActive(false);

	OnDroneDestroyed.Broadcast(this);
}

void ATrainingDrone::Multicast_PlayDestruction_Implementation()
{
	const FVector Location = GetActorLocation();

	if (DestructionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DestructionEffect, Location);
	}

	if (DestructionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, Location);
	}
}

void ATrainingDrone::Multicast_SetActive_Implementation(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	SetActorTickEnabled(bActive);
}

void ATrainingDrone::ResetDrone()
{
	if (!HasAuthority())
	{
		return;
	}

	Health = MaxHealth;
	HitFlashRemaining = 0.f;

	SetActorLocation(HomeLocation);
	Multicast_SetActive(true);
	UpdateVisuals();
}

void ATrainingDrone::OnRep_Health()
{
	UpdateVisuals();
}

void ATrainingDrone::UpdateVisuals()
{
	if (!DynamicMaterial)
	{
		return;
	}

	FLinearColor Color = FMath::Lerp(DamagedColor, HealthyColor, GetHealthPercent());

	// Destello blanco tras el impacto: confirma el acierto al instante.
	if (HitFlashRemaining > 0.f)
	{
		const float Alpha = HitFlashRemaining / DroneHitFlashDuration;
		Color = FMath::Lerp(Color, FLinearColor::White, Alpha);
	}

	// El material del motor expone el color base con estos nombres según la versión.
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);

	// Los materiales con emisión brillan al recibir daño.
	DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"),
		HitFlashRemaining > 0.f ? 8.f : 1.f);
}
