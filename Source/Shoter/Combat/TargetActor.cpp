#include "Combat/TargetActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** Duración del destello blanco al recibir un impacto, en segundos. */
	constexpr float HitFlashDuration = 0.12f;
}

ATargetActor::ATargetActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// Esfera del motor: blanco circular sin necesidad de arte propio.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMeshAsset.Object);
	}

	MeshComponent->SetRelativeScale3D(FVector(0.7f));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetMobility(EComponentMobility::Movable);
}

void ATargetActor::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

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

void ATargetActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATargetActor, Health);
}

void ATargetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HitFlashRemaining > 0.f)
	{
		HitFlashRemaining = FMath::Max(HitFlashRemaining - DeltaSeconds, 0.f);
		UpdateVisuals();
	}

	// El vaivén lo calcula cada máquina con el tiempo del mundo: así queda
	// sincronizado sin gastar ancho de banda replicando la posición.
	if (SwingAmplitude > 0.f)
	{
		const float Phase = GetWorld()->GetTimeSeconds() * SwingSpeed + HomeLocation.X * 0.01f;
		SetActorLocation(HomeLocation + FVector(0.f, FMath::Sin(Phase) * SwingAmplitude, 0.f));
	}
}

float ATargetActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Solo el servidor aplica daño; el cliente recibe Health replicado.
	if (!HasAuthority() || ActualDamage <= 0.f || Health <= 0.f)
	{
		return ActualDamage;
	}

	Health = FMath::Max(Health - ActualDamage, 0.f);
	UpdateVisuals();
	Multicast_PlayHitFeedback();

	if (Health <= 0.f)
	{
		HandleDestroyed();
	}

	return ActualDamage;
}

void ATargetActor::Multicast_PlayHitFeedback_Implementation()
{
	HitFlashRemaining = HitFlashDuration;
	UpdateVisuals();
}

void ATargetActor::HandleDestroyed()
{
	// Sin reaparición configurada, el blanco desaparece definitivamente.
	if (RespawnDelay <= 0.f)
	{
		Destroy();
		return;
	}

	Multicast_SetVisible(false);

	GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ATargetActor::Respawn, RespawnDelay, false);
}

void ATargetActor::Respawn()
{
	if (!HasAuthority())
	{
		return;
	}

	Health = MaxHealth;
	Multicast_SetVisible(true);
	UpdateVisuals();
}

void ATargetActor::Multicast_SetVisible_Implementation(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	SetActorEnableCollision(bVisible);
}

void ATargetActor::OnRep_Health()
{
	UpdateVisuals();
}

void ATargetActor::UpdateVisuals()
{
	if (!DynamicMaterial)
	{
		return;
	}

	FLinearColor Color = FMath::Lerp(DamagedColor, HealthyColor, GetHealthPercent());

	// Destello blanco justo tras el impacto: confirma el acierto al instante.
	if (HitFlashRemaining > 0.f)
	{
		const float Alpha = HitFlashRemaining / HitFlashDuration;
		Color = FMath::Lerp(Color, FLinearColor::White, Alpha);
	}

	// El material del motor expone el color base con estos nombres según la versión.
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
}
