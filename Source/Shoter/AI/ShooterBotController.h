#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShooterBotController.generated.h"

class AShooterCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * Bot de combate (RF-3.6, RF-8.4).
 *
 * Patrulla, persigue al enemigo que ve y dispara a cubierto de su alcance
 * efectivo. Implementado con una máquina de estados simple en vez de un árbol
 * de comportamiento: son cuatro estados y así el balance queda en propiedades
 * editables, sin assets intermedios.
 */
UCLASS()
class SHOTER_API AShooterBotController : public AAIController
{
	GENERATED_BODY()

public:
	AShooterBotController();

	/** Distancia a la que el bot deja de acercarse y dispara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "100.0"))
	float PreferredCombatDistance = 1200.f;

	/** Alcance de visión, en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "100.0"))
	float SightRange = 4500.f;

	/** Ángulo de visión en grados, a cada lado. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float SightAngle = 75.f;

	/**
	 * Grados por segundo que el bot gira para apuntar. Valores bajos dan un
	 * rival más benévolo: es la palanca principal de dificultad.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "10.0"))
	float AimSpeed = 180.f;

	/** Error de puntería en grados. Cero convierte al bot en infalible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "0.0"))
	float AimErrorDegrees = 4.f;

	/** Segundos que sigue persiguiendo tras perder de vista al objetivo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "0.0"))
	float MemoryDuration = 5.f;

	/** Radio de patrulla alrededor del punto de aparición. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "100.0"))
	float PatrolRadius = 2500.f;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<AShooterCharacter> CurrentTarget;

	/** Última posición conocida del objetivo, para seguir buscándolo. */
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	float TimeSinceTargetSeen = 0.f;
	FVector PatrolOrigin = FVector::ZeroVector;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Enemigo vivo más cercano dentro del alcance de visión. */
	AShooterCharacter* FindNearestEnemy() const;

	void UpdateCombat(float DeltaSeconds);
	void UpdatePatrol();

	/** Gira hacia el objetivo a AimSpeed y devuelve si ya está encarado. */
	bool AimAtTarget(float DeltaSeconds);

	bool IsSameTeam(const AShooterCharacter* Other) const;
};
