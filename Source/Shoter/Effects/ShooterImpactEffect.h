#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Effects/ShooterEffects.h"
#include "ShooterImpactEffect.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Impacto de bala: nube de polvo, esquirlas y marca en la superficie.
 *
 * Sustituye a un sistema de partículas. La nube es una esfera translúcida que
 * crece y se desvanece; las esquirlas, cubos diminutos con física. El color
 * sale de la superficie: polvo gris en hormigón, rojizo en ladrillo, astillas
 * en madera, sangre en un cuerpo.
 */
UCLASS(NotBlueprintable)
class SHOTER_API AShooterImpactEffect : public AActor
{
	GENERATED_BODY()

public:
	AShooterImpactEffect();

	void Init(EShooterImpactKind InKind, const FVector& Normal, float InScale, bool bSpawnChips);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Puff;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Chips;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PuffMaterial;

	EShooterImpactKind Kind = EShooterImpactKind::Concrete;
	FVector SurfaceNormal = FVector::UpVector;
	float EffectScale = 1.f;
	float Age = 0.f;

	/** Duración de la nube de polvo. */
	float PuffLifetime = 1.4f;
	float PuffMaxSize = 60.f;

	static FLinearColor GetDustColor(EShooterImpactKind Kind);
};
