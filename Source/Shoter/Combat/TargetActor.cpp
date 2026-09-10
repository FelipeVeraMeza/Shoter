#include "Combat/TargetActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATargetActor::ATargetActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// Cubo del motor: el blanco se ve sin necesidad de arte propio.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMeshAsset.Object);
	}

	MeshComponent->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetMobility(EComponentMobility::Movable);
}

void ATargetActor::BeginPlay()
{
	Super::BeginPlay();

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

	if (Health <= 0.f)
	{
		HandleDestroyed();
	}

	return ActualDamage;
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

	const FLinearColor Color = FMath::Lerp(DamagedColor, HealthyColor, GetHealthPercent());

	// El material del motor expone el color base con estos nombres según la versión.
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
}
