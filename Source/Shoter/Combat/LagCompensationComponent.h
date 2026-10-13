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

	/**
	 * Radio de la cabeza, en cm.
	 *
	 * La cabeza se prueba como una esfera aparte y no como una franja de la
	 * cápsula: la cápsula es mucho más ancha que un cráneo, y contar como
	 * disparo a la cabeza todo lo que roza su parte alta premiaría la falta
	 * de puntería.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación", meta = (ClampMin = "5.0"))
	float HeadRadius = 13.f;

	/** Por encima de esta altura relativa, un impacto en la cápsula que no tocó la cabeza es aire. */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación|Zonas", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShoulderHeightFraction = 0.84f;

	/** Por debajo de esta altura, el impacto es en el abdomen. */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación|Zonas", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChestHeightFraction = 0.62f;

	/** Por debajo de esta altura, el impacto es en las piernas. */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación|Zonas", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaistHeightFraction = 0.45f;

	/** Distancia al eje, como fracción del radio, a partir de la cual el torso pasa a ser brazo. */
	UPROPERTY(EditDefaultsOnly, Category = "Compensación|Zonas", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmRadiusFraction = 0.62f;

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
		EShooterHitZone& OutZone);

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
		float& OutHeightFraction,
		float& OutAxisDistanceFraction);

	/**
	 * Resuelve el impacto de un rayo contra un personaje rebobinado: primero
	 * la cabeza, después el cuerpo por zonas.
	 */
	bool TraceAgainstFrame(
		const FVector& RayStart,
		const FVector& RayEnd,
		const FShooterFramePackage& Frame,
		FVector& OutImpactPoint,
		float& OutDistanceAlongRay,
		EShooterHitZone& OutZone) const;
};
