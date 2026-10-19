#include "Weapons/ShooterGrenade.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Effects/ShooterEffects.h"

AShooterGrenade::AShooterGrenade()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(8.f);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Esfera del motor: la granada se ve sin necesidad de arte propio.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereAsset.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.16f));
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 0.f;
	ProjectileMovement->MaxSpeed = 4000.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.35f;
	ProjectileMovement->Friction = 0.4f;
	ProjectileMovement->ProjectileGravityScale = 1.f;

	// Sin límite de vida propio: la detona el temporizador de la espoleta.
	InitialLifeSpan = 0.f;
}

void AShooterGrenade::BeginPlay()
{
	Super::BeginPlay();

	// Solo el servidor cuenta la espoleta; el resultado se replica.
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FuseTimerHandle, this,
			&AShooterGrenade::Explode, FuseTime, false);
	}
}

void AShooterGrenade::Launch(const FVector& Direction)
{
	if (!ProjectileMovement)
	{
		return;
	}

	ProjectileMovement->Velocity = Direction.GetSafeNormal() * ThrowSpeed;
}

void AShooterGrenade::Explode()
{
	if (!HasAuthority())
	{
		return;
	}

	const FVector Location = GetActorLocation();

	// Daño radial con caída: máximo en InnerRadius, cero en OuterRadius.
	// El lanzador también se ve afectado, como en cualquier shooter.
	TArray<AActor*> IgnoredActors;
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		MaxDamage,
		MaxDamage * 0.15f,
		Location,
		InnerRadius,
		OuterRadius,
		1.f,
		UDamageType::StaticClass(),
		IgnoredActors,
		this,
		GetInstigatorController());

	Multicast_PlayExplosion(Location);

	Destroy();
}

void AShooterGrenade::Multicast_PlayExplosion_Implementation(const FVector_NetQuantize& Location)
{
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ExplosionEffect, Location);
	}
	else
	{
		// Sin sistema de partículas: nube de tierra grande, esquirlas y un
		// destello que ilumina el entorno un instante.
		ShooterEffects::SpawnImpact(GetWorld(), Location, FVector::UpVector, EShooterImpactKind::Dirt, 5.f);
		ShooterEffects::SpawnLightFlash(GetWorld(), Location + FVector(0.f, 0.f, 60.f),
			FLinearColor(1.f, 0.7f, 0.4f), 6000.f, 1600.f, 0.18f);
	}

	// Sin sonido propio, el disparo de la plantilla muy grave hace de detonación.
	USoundBase* Sound = ExplosionSound ? ExplosionSound.Get() : ShooterEffects::GetDefaultGunshot();
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, ExplosionSound ? 1.f : 2.5f,
			ExplosionSound ? 1.f : 0.42f, 0.f, ShooterEffects::GetGunshotAttenuation());
	}
}
