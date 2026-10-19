#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterLightFlash.generated.h"

class UPointLightComponent;

/** Luz puntual que se apaga en pocos milisegundos: fogonazos lejanos y explosiones. */
UCLASS(NotBlueprintable)
class SHOTER_API AShooterLightFlash : public AActor
{
	GENERATED_BODY()

public:
	AShooterLightFlash();

	void Init(const FLinearColor& Color, float Intensity, float Radius, float Duration);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	float PeakIntensity = 0.f;
	float FlashDuration = 0.05f;
	float Age = 0.f;
};
