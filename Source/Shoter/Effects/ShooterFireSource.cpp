#include "Effects/ShooterFireSource.h"
#include "Effects/ShooterEffects.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AShooterFireSource::AShooterFireSource()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(RootComponent);
	FireLight->SetMobility(EComponentMobility::Movable);
	FireLight->SetIntensityUnits(ELightUnits::Candelas);
	FireLight->SetLightColor(FLinearColor(1.f, 0.48f, 0.16f));
	FireLight->SetCastShadows(true);

	for (int32 Index = 0; Index < FlameCount; ++Index)
	{
		UStaticMeshComponent* Flame = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Flame%d"), Index));
		Flame->SetupAttachment(RootComponent);
		Flame->SetMobility(EComponentMobility::Movable);
		Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flame->SetCastShadow(false);
		Flame->SetGenerateOverlapEvents(false);
		Flames.Add(Flame);
	}

	for (int32 Index = 0; Index < SmokeCount; ++Index)
	{
		UStaticMeshComponent* Puff = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Smoke%d"), Index));
		Puff->SetupAttachment(RootComponent);
		Puff->SetMobility(EComponentMobility::Movable);
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetCastShadow(false);
		Puff->SetGenerateOverlapEvents(false);
		Puff->SetHiddenInGame(true);
		SmokePuffs.Add(Puff);
	}
}

void AShooterFireSource::BeginPlay()
{
	Super::BeginPlay();

	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 60.f * FireScale));
	FireLight->SetAttenuationRadius(700.f * FireScale);
	FireLight->SetSourceRadius(25.f * FireScale);

	// Planos del motor (1 m, normal +Z) puestos de pie y girados entre sí:
	// desde cualquier ángulo se ve al menos uno de frente.
	UStaticMesh* Plane = ShooterEffects::GetBasicShape(TEXT("Plane"));
	UMaterialInterface* FlameMaterial = ShooterEffects::GetFlameMaterial();
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		UStaticMeshComponent* Flame = Flames[Index];
		Flame->SetStaticMesh(Plane);
		if (FlameMaterial)
		{
			Flame->SetMaterial(0, FlameMaterial);
		}
		else
		{
			Flame->SetHiddenInGame(true);
		}
		Flame->SetRelativeRotation(FRotator(0.f, Index * (180.f / FlameCount), 90.f));
	}

	UStaticMesh* Sphere = ShooterEffects::GetBasicShape(TEXT("Sphere"));
	UMaterialInterface* Dust = ShooterEffects::GetDustMaterial();
	const FLinearColor SmokeColor = FMath::Lerp(FLinearColor(0.45f, 0.44f, 0.42f), FLinearColor(0.025f, 0.024f, 0.022f), SmokeDarkness);

	SmokeAges.SetNum(SmokeCount);
	SmokeOffsets.SetNum(SmokeCount);
	SmokeLifetime = 5.f + FireScale * 1.5f;

	for (int32 Index = 0; Index < SmokePuffs.Num(); ++Index)
	{
		// Escalonadas: cada bocanada nace cuando le toca, no todas a la vez.
		SmokeAges[Index] = -SmokeLifetime * Index / SmokeCount;
		SmokeOffsets[Index] = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 30.f * FireScale;

		if (!bEmitSmoke || !Dust || !Sphere)
		{
			continue;
		}

		SmokePuffs[Index]->SetStaticMesh(Sphere);
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Dust, this);
		Material->SetVectorParameterValue(TEXT("Color"), SmokeColor);
		Material->SetScalarParameterValue(TEXT("Opacidad"), 0.f);
		SmokePuffs[Index]->SetMaterial(0, Material);
		SmokeMaterials.Add(Material);
	}

	// Cada fuego parpadea a su ritmo.
	Time = FMath::FRandRange(0.f, 100.f);
}

void AShooterFireSource::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Time += DeltaSeconds;

	// El parpadeo combina dos frecuencias: una lenta (la llama que crece y
	// mengua) y otra rápida (el chisporroteo).
	const float Flicker = 0.72f + 0.2f * FMath::PerlinNoise1D(Time * 2.3f) + 0.12f * FMath::PerlinNoise1D(Time * 11.f);
	FireLight->SetIntensity(LightIntensity * FireScale * FMath::Max(Flicker, 0.3f));
	FireLight->SetRelativeLocation(FVector(
		FMath::PerlinNoise1D(Time * 3.1f) * 12.f,
		FMath::PerlinNoise1D(Time * 2.7f + 9.f) * 12.f,
		60.f * FireScale + FMath::PerlinNoise1D(Time * 4.f) * 10.f) );

	UpdateFlames();

	if (bEmitSmoke && SmokeMaterials.Num() == SmokePuffs.Num())
	{
		UpdateSmoke(DeltaSeconds);
	}
}

void AShooterFireSource::UpdateFlames()
{
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		const float Phase = Time * 7.f + Index * 1.7f;
		const float Height = (1.1f + 0.25f * FMath::PerlinNoise1D(Phase)) * FireScale;
		const float Width = (0.75f + 0.1f * FMath::PerlinNoise1D(Phase + 4.f)) * FireScale;

		// El plano del motor tiene el centro en el origen: se sube media
		// altura para que la llama arranque del suelo del fuego.
		UStaticMeshComponent* Flame = Flames[Index];
		Flame->SetRelativeScale3D(FVector(Width, Height, 1.f));
		Flame->SetRelativeLocation(FVector(0.f, 0.f, Height * 50.f));
	}
}

void AShooterFireSource::UpdateSmoke(float DeltaSeconds)
{
	for (int32 Index = 0; Index < SmokePuffs.Num(); ++Index)
	{
		float& Age = SmokeAges[Index];
		Age += DeltaSeconds;

		if (Age < 0.f)
		{
			continue;
		}

		if (Age > SmokeLifetime)
		{
			Age -= SmokeLifetime;
			SmokeOffsets[Index] = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 30.f * FireScale;
		}

		const float T = Age / SmokeLifetime;

		// Sube deprisa al salir del fuego y se frena y ensancha arriba; el
		// viento lo inclina cada vez más.
		const float Rise = (1.f - FMath::Square(1.f - T)) * 900.f * FireScale + 120.f * FireScale;
		const FVector Drift = Wind * Age * (0.4f + T);
		const float Size = FMath::Lerp(70.f, 420.f, T) * FireScale;
		const float Opacity = (T < 0.12f ? T / 0.12f : 1.f - (T - 0.12f) / 0.88f) * 0.5f;

		UStaticMeshComponent* Puff = SmokePuffs[Index];
		Puff->SetRelativeLocation(SmokeOffsets[Index] + Drift + FVector(0.f, 0.f, Rise));
		Puff->SetRelativeScale3D(FVector(Size / 100.f));
		Puff->SetHiddenInGame(false);
		SmokeMaterials[Index]->SetScalarParameterValue(TEXT("Opacidad"), FMath::Max(Opacity, 0.f));
	}
}
