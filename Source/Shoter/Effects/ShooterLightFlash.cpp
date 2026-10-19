#include "Effects/ShooterLightFlash.h"
#include "Components/PointLightComponent.h"

AShooterLightFlash::AShooterLightFlash()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
	InitialLifeSpan = 3.f;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	RootComponent = Light;
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);

	// Un destello de milisegundos no necesita sombras: costaría un mapa de
	// sombras entero para algo que el ojo apenas registra.
	Light->SetCastShadows(false);
}

void AShooterLightFlash::Init(const FLinearColor& Color, float Intensity, float Radius, float Duration)
{
	PeakIntensity = Intensity;
	FlashDuration = FMath::Max(Duration, 0.01f);
	Age = 0.f;

	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(Radius);
	Light->SetIntensity(Intensity);
}

void AShooterLightFlash::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= FlashDuration)
	{
		Destroy();
		return;
	}

	// Caída cuadrática: el destello es casi todo pico.
	const float Remaining = 1.f - Age / FlashDuration;
	Light->SetIntensity(PeakIntensity * Remaining * Remaining);
}
