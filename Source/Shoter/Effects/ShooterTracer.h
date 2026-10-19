#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTracer.generated.h"

class UStaticMeshComponent;

/**
 * Trazadora: un trazo incandescente que recorre la trayectoria de la bala.
 *
 * Es solo cosmética y local: el daño lo resuelve la balística del servidor.
 * Vuela algo más despacio que la bala real para que el ojo la siga; a 900 m/s
 * cruzaría la pantalla en dos fotogramas y parecería un láser.
 */
UCLASS(NotBlueprintable)
class SHOTER_API AShooterTracer : public AActor
{
	GENERATED_BODY()

public:
	AShooterTracer();

	void Launch(const FVector& Start, const FVector& Direction, float BulletSpeed, const AActor* IgnoredActor);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Streak;

	FVector StartLocation = FVector::ZeroVector;
	FVector TravelDirection = FVector::ForwardVector;
	float TotalDistance = 0.f;
	float HeadDistance = 0.f;
	float Speed = 50000.f;

	/** Longitud del trazo visible, en cm. */
	static constexpr float StreakLength = 650.f;
};
