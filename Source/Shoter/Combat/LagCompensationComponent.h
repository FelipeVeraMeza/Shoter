#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShooterTypes.h"
#include "LagCompensationComponent.generated.h"

class AShooterCharacter;

/**
 * Compensación de latencia (RNF-2.4).
 *
 * Cada personaje graba en el servidor un historial de su posición. Cuando llega un
 * disparo, el servidor rebobina a los demás personajes al instante en que el
 * atacante realmente los vio y resuelve el impacto contra esas posiciones pasadas.
 *
 * Sin esto, un jugador con 100 ms de latencia debe apuntar por delante del enemigo.
 */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class SHOTER_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULagCompensationComponent();

	/** Segundos de historial conservados. Más allá de esto no se puede rebobinar. */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación", meta = (ClampMin = "0.1"))
	float MaxRecordTime = 1.f;

	/** Altura relativa de la cápsula a partir de la cual un impacto cuenta como headshot. */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadshotHeightFraction = 0.82f;

	/** Estado del personaje en un instante pasado, interpolado entre frames grabados. */
	bool GetFrameAtTime(float WorldTime, FShooterFramePackage& OutFrame) const;

	/**
	 * Busca el personaje impactado por un rayo, rebobinando a todos al instante indicado.
	 * Solo tiene sentido llamarla en el servidor.
	 */
	static bool FindBestCharacterHit(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		float RewindTime,
		const AActor* IgnoredActor,
		AShooterCharacter*& OutHitCharacter,
		FVector& OutImpactPoint,
		bool& bOutHeadshot);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Historial ordenado de más antiguo a más reciente. */
	TArray<FShooterFramePackage> History;

	void SaveFrame();

	/** Intersección rayo-cápsula. Devuelve el punto más cercano del rayo al eje de la cápsula. */
	static bool RayIntersectsCapsule(
		const FVector& RayStart,
		const FVector& RayEnd,
		const FShooterFramePackage& Frame,
		FVector& OutImpactPoint,
		float& OutDistanceAlongRay,
		float& OutHeightFraction);
};
