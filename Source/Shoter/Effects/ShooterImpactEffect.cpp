#include "Effects/ShooterImpactEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr int32 ChipCount = 4;
}

AShooterImpactEffect::AShooterImpactEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
	InitialLifeSpan = 3.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Puff = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Puff"));
	Puff->SetupAttachment(RootComponent);
	Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puff->SetCastShadow(false);
	Puff->SetGenerateOverlapEvents(false);

	for (int32 Index = 0; Index < ChipCount; ++Index)
	{
		UStaticMeshComponent* Chip = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("Chip%d"), Index));
		Chip->SetupAttachment(RootComponent);
		Chip->SetCastShadow(false);
		Chip->SetGenerateOverlapEvents(false);
		Chip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chip->SetHiddenInGame(true);
		Chips.Add(Chip);
	}
}

FLinearColor AShooterImpactEffect::GetDustColor(EShooterImpactKind InKind)
{
	switch (InKind)
	{
	case EShooterImpactKind::Brick:  return FLinearColor(0.42f, 0.22f, 0.15f, 1.f);
	case EShooterImpactKind::Wood:   return FLinearColor(0.38f, 0.28f, 0.17f, 1.f);
	case EShooterImpactKind::Metal:  return FLinearColor(0.25f, 0.24f, 0.23f, 1.f);
	case EShooterImpactKind::Dirt:   return FLinearColor(0.36f, 0.3f, 0.21f, 1.f);
	case EShooterImpactKind::Flesh:  return FLinearColor(0.28f, 0.015f, 0.01f, 1.f);
	default:                         return FLinearColor(0.5f, 0.48f, 0.45f, 1.f);
	}
}

void AShooterImpactEffect::Init(EShooterImpactKind InKind, const FVector& Normal, float InScale, bool bSpawnChips)
{
	Kind = InKind;
	SurfaceNormal = Normal.GetSafeNormal();
	EffectScale = FMath::Max(InScale, 0.1f);
	Age = 0.f;

	const bool bFlesh = Kind == EShooterImpactKind::Flesh;
	PuffLifetime = (bFlesh ? 0.5f : 1.4f) * FMath::Sqrt(EffectScale);
	PuffMaxSize = (bFlesh ? 35.f : 70.f) * EffectScale;

	if (UStaticMesh* Sphere = ShooterEffects::GetBasicShape(TEXT("Sphere")))
	{
		Puff->SetStaticMesh(Sphere);
	}

	if (UMaterialInterface* Dust = ShooterEffects::GetDustMaterial())
	{
		PuffMaterial = UMaterialInstanceDynamic::Create(Dust, this);
		PuffMaterial->SetVectorParameterValue(TEXT("Color"), GetDustColor(Kind));
		PuffMaterial->SetScalarParameterValue(TEXT("Opacidad"), 0.f);
		Puff->SetMaterial(0, PuffMaterial);
	}
	else
	{
		// Sin el material translúcido una esfera opaca taparía la vista.
		Puff->SetHiddenInGame(true);
	}

	Puff->SetWorldScale3D(FVector(0.02f));

	// Marca en la pared: el agujero se queda tras el polvo.
	if (!bFlesh)
	{
		if (UMaterialInterface* DecalMaterial = ShooterEffects::GetImpactDecalMaterial())
		{
			const float Size = (Kind == EShooterImpactKind::Wood ? 4.f : 5.5f) * EffectScale;
			FRotator DecalRotation = (-SurfaceNormal).Rotation();
			DecalRotation.Roll = FMath::FRandRange(0.f, 360.f);

			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, DecalMaterial,
				FVector(6.f, Size, Size), GetActorLocation(), DecalRotation, 0.f))
			{
				Decal->SetFadeOut(25.f, 5.f, true);
				Decal->SetFadeScreenSize(0.002f);
			}
		}
	}

	if (!bSpawnChips || bFlesh)
	{
		return;
	}

	UStaticMesh* Cube = ShooterEffects::GetBasicShape(TEXT("Cube"));
	const FLinearColor ChipColor = GetDustColor(Kind);

	for (UStaticMeshComponent* Chip : Chips)
	{
		if (!Chip || !Cube)
		{
			continue;
		}

		Chip->SetStaticMesh(Cube);
		if (PuffMaterial)
		{
			// Mismo color que el polvo, pero opaco: el material admite ambas cosas.
			UMaterialInstanceDynamic* ChipMaterial = UMaterialInstanceDynamic::Create(PuffMaterial->Parent, this);
			ChipMaterial->SetVectorParameterValue(TEXT("Color"), ChipColor * 0.7f);
			ChipMaterial->SetScalarParameterValue(TEXT("Opacidad"), 1.f);
			Chip->SetMaterial(0, ChipMaterial);
		}

		const float ChipSize = FMath::FRandRange(0.6f, 1.6f) * EffectScale;
		Chip->SetWorldScale3D(FVector(ChipSize / 100.f));
		Chip->SetWorldLocation(GetActorLocation() + SurfaceNormal * 2.f);
		Chip->SetHiddenInGame(false);

		Chip->SetCollisionProfileName(TEXT("PhysicsActor"));
		Chip->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Chip->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Chip->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Chip->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		Chip->SetSimulatePhysics(true);
		Chip->SetMassOverrideInKg(NAME_None, 0.01f, true);

		// Salen en un cono alrededor de la normal, como al desconcharse.
		const FVector Direction = FMath::VRandCone(SurfaceNormal, FMath::DegreesToRadians(55.f));
		Chip->SetPhysicsLinearVelocity(Direction * FMath::FRandRange(250.f, 650.f) * FMath::Sqrt(EffectScale));
		Chip->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 1200.f);
	}
}

void AShooterImpactEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	if (!PuffMaterial)
	{
		return;
	}

	const float T = FMath::Clamp(Age / FMath::Max(PuffLifetime, 0.05f), 0.f, 1.f);

	// Crece rápido al principio y frena, como el polvo al expandirse; la
	// opacidad sube en un instante y se apaga despacio.
	const float Growth = 1.f - FMath::Square(1.f - T);
	const float Size = FMath::Lerp(PuffMaxSize * 0.15f, PuffMaxSize, Growth);
	const float Opacity = (T < 0.08f ? T / 0.08f : 1.f - (T - 0.08f) / 0.92f) * 0.55f;

	Puff->SetWorldScale3D(FVector(Size / 100.f));

	// La nube se separa de la superficie y sube un poco.
	Puff->SetWorldLocation(GetActorLocation() + SurfaceNormal * Size * 0.35f + FVector(0.f, 0.f, Size * 0.2f * T));
	PuffMaterial->SetScalarParameterValue(TEXT("Opacidad"), FMath::Max(Opacity, 0.f));

	if (T >= 1.f)
	{
		Puff->SetHiddenInGame(true);
	}
}
