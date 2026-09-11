#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingRangeManager.generated.h"

class ATrainingDrone;
class AShooterCharacter;

/**
 * Gestor de la zona de entrenamiento (RF6).
 *
 * Repone drones, munición y salud de una vez. Colocar uno en el nivel basta:
 * encuentra los drones por sí mismo.
 */
UCLASS()
class SHOTER_API ATrainingRangeManager : public AActor
{
	GENERATED_BODY()

public:
	ATrainingRangeManager();

	/** Repone los drones destruidos pasado este tiempo. Cero desactiva. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrenamiento", meta = (ClampMin = "0.0"))
	float AutoRespawnDelay = 4.f;

	/** El jugador no recibe daño mientras entrena (RF3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entrenamiento")
	bool bPlayerInvulnerable = true;

	/** Reinicio completo: drones, munición y salud (RF6). */
	UFUNCTION(BlueprintCallable, Category = "Entrenamiento")
	void ResetRange();

	/** Solo los drones, sin tocar al jugador. */
	UFUNCTION(BlueprintCallable, Category = "Entrenamiento")
	void ResetDrones();

	/** Munición al máximo y salud llena. */
	UFUNCTION(BlueprintCallable, Category = "Entrenamiento")
	void ResetPlayer();

	UFUNCTION(BlueprintCallable, Category = "Entrenamiento")
	void SetPlayerInvulnerable(bool bInvulnerable);

	UFUNCTION(BlueprintPure, Category = "Entrenamiento")
	int32 GetActiveDroneCount() const;

	UFUNCTION(BlueprintPure, Category = "Entrenamiento")
	int32 GetTotalDroneCount() const { return Drones.Num(); }

protected:
	virtual void BeginPlay() override;

private:
	/** Drones del nivel, recogidos al empezar. */
	UPROPERTY()
	TArray<TObjectPtr<ATrainingDrone>> Drones;

	UFUNCTION()
	void HandleDroneDestroyed(ATrainingDrone* Drone);

	/** Repone un dron concreto tras AutoRespawnDelay. */
	void RespawnDrone(ATrainingDrone* Drone);

	/** Personaje del jugador local, o nullptr si aún no existe. */
	AShooterCharacter* GetLocalCharacter() const;
};
