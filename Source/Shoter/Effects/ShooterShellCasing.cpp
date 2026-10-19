#include "Effects/ShooterShellCasing.h"
#include "Effects/ShooterEffects.h"
#include "Components/StaticMeshComponent.h"

int32 AShooterShellCasing::LiveCount = 0;

AShooterShellCasing::AShooterShellCasing()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	InitialLifeSpan = 6.f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCastShadow(false);
	Mesh->SetGenerateOverlapEvents(false);

	// Choca con el escenario pero no con personajes ni cámaras: un casquillo
	// no debe empujar a nadie ni tapar la vista.
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Mesh->BodyInstance.bUseCCD = true;
}

void AShooterShellCasing::BeginPlay()
{
	Super::BeginPlay();
	++LiveCount;
}

void AShooterShellCasing::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	--LiveCount;
	Super::EndPlay(EndPlayReason);
}

void AShooterShellCasing::Eject(const FVector& Velocity, float Length)
{
	if (UStaticMesh* Cylinder = ShooterEffects::GetBasicShape(TEXT("Cylinder")))
	{
		Mesh->SetStaticMesh(Cylinder);
	}
	if (UMaterialInterface* Brass = ShooterEffects::GetBrassMaterial())
	{
		Mesh->SetMaterial(0, Brass);
	}

	// Diámetro proporcional a la longitud: un 5,56 mide 4,5 x 0,95 cm.
	const float Diameter = Length * 0.22f;
	Mesh->SetWorldScale3D(FVector(Diameter / 100.f, Diameter / 100.f, Length / 100.f));

	Mesh->SetSimulatePhysics(true);
	Mesh->SetMassOverrideInKg(NAME_None, 0.012f, true);
	Mesh->SetLinearDamping(0.2f);
	Mesh->SetAngularDamping(0.6f);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector(
		FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-1800.f, 1800.f)));
}
