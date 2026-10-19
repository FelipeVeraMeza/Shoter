#include "Effects/ShooterTracer.h"
#include "Effects/ShooterEffects.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

AShooterTracer::AShooterTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
	InitialLifeSpan = 2.f;

	// Raíz aparte: el actor mira en la dirección de vuelo y el cilindro se
	// gira dentro de él.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Streak = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Streak"));
	Streak->SetupAttachment(RootComponent);
	Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Streak->SetCastShadow(false);
	Streak->SetGenerateOverlapEvents(false);
	Streak->SetMobility(EComponentMobility::Movable);
	Streak->SetHiddenInGame(true);
}

void AShooterTracer::Launch(const FVector& Start, const FVector& Direction, float BulletSpeed, const AActor* IgnoredActor)
{
	StartLocation = Start;
	TravelDirection = Direction;
	Speed = FMath::Clamp(BulletSpeed * 0.55f, 15000.f, 60000.f);

	// El trazo termina donde termine la bala visible: lo primero que hay en
	// la línea, o 300 m.
	const FVector End = Start + Direction * 30000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShooterTracer), false, IgnoredActor);
	Params.AddIgnoredActor(this);
	TotalDistance = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
		? Hit.Distance
		: 30000.f;

	if (UStaticMesh* Cylinder = ShooterEffects::GetBasicShape(TEXT("Cylinder")))
	{
		Streak->SetStaticMesh(Cylinder);
	}
	if (UMaterialInterface* Material = ShooterEffects::GetTracerMaterial())
	{
		Streak->SetMaterial(0, Material);
	}

	HeadDistance = 0.f;
	SetActorRotation(Direction.Rotation());
}

void AShooterTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	HeadDistance += Speed * DeltaSeconds;

	const float Head = FMath::Min(HeadDistance, TotalDistance);
	const float Tail = FMath::Clamp(HeadDistance - StreakLength, 0.f, TotalDistance);

	if (Tail >= TotalDistance)
	{
		Destroy();
		return;
	}

	const float Length = FMath::Max(Head - Tail, 1.f);
	const FVector Center = StartLocation + TravelDirection * ((Head + Tail) * 0.5f);

	// Grosor según la distancia a la cámara: a 100 m un trazo de 1,5 cm
	// ocuparía menos de un píxel y la trazadora no se vería.
	float Thickness = 1.4f;
	FVector Viewer;
	if (ShooterEffects::GetViewerLocation(GetWorld(), Viewer))
	{
		Thickness += FVector::Dist(Viewer, Center) * 0.0009f;
	}

	// El cilindro del motor mide 100 cm y su eje es Z: se gira para que el
	// eje apunte hacia delante y se escala a lo largo.
	SetActorLocation(Center);
	Streak->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Streak->SetRelativeScale3D(FVector(Thickness / 100.f, Thickness / 100.f, Length / 100.f));
	Streak->SetHiddenInGame(false);
}
