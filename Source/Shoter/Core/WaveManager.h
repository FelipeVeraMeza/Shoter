#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveManager.generated.h"

class AAIController;
class AShooterCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCleared, int32, WaveNumber);

/**
 * Gestor del modo cooperativo Oleadas (RF-8.4).
 *
 * Reutiliza el AShooterBotController existente en vez de una IA propia: el
 * enemigo de oleadas y el bot de relleno se comportan igual, y mantener dos
 * implementaciones divergiría con cada ajuste.
 *
 * Solo corre en el servidor; los clientes reciben el número de oleada y los
 * enemigos restantes por replicación (RNF-3.1).
 */
UCLASS()
class SHOTER_API AWaveManager : public AActor
{
	GENERATED_BODY()

public:
	AWaveManager();

	/** Personaje que encarnan los enemigos. Sin esto no se genera ninguno. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas")
	TSubclassOf<AShooterCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas")
	TSubclassOf<AAIController> EnemyControllerClass;

	/** Enemigos de la primera oleada. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas", meta = (ClampMin = "1"))
	int32 BaseEnemyCount = 4;

	/**
	 * Enemigos que añade cada oleada sobre la anterior.
	 *
	 * Crecimiento lineal y no exponencial: con multiplicador, la oleada 10 ya
	 * sería injugable y la 3 trivial. Sumar mantiene la curva legible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas", meta = (ClampMin = "0"))
	int32 EnemiesPerWave = 2;

	/** Tope de enemigos vivos a la vez, por rendimiento (RNF-1.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxConcurrentEnemies = 12;

	/** Descanso entre oleadas, para recargar y reposicionarse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas", meta = (ClampMin = "0.0"))
	float TimeBetweenWaves = 8.f;

	/** Separación entre apariciones dentro de una misma oleada. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas", meta = (ClampMin = "0.0"))
	float SpawnInterval = 1.2f;

	/** Distancia mínima a un jugador al aparecer: que no surjan encima. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oleadas", meta = (ClampMin = "0.0"))
	float MinimumSpawnDistance = 1500.f;

	UPROPERTY(BlueprintAssignable, Category = "Oleadas")
	FOnWaveStarted OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Oleadas")
	FOnWaveCleared OnWaveCleared;

	UFUNCTION(BlueprintPure, Category = "Oleadas")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Oleadas")
	int32 GetEnemiesRemaining() const { return EnemiesRemaining; }

	UFUNCTION(BlueprintPure, Category = "Oleadas")
	bool IsWaveInProgress() const { return bWaveInProgress; }

	/** Segundos hasta la próxima oleada; cero si ya está en curso. */
	UFUNCTION(BlueprintPure, Category = "Oleadas")
	float GetTimeUntilNextWave() const;

	/** Solo servidor. Arranca la secuencia de oleadas. */
	UFUNCTION(BlueprintCallable, Category = "Oleadas")
	void StartWaves();

	UFUNCTION(BlueprintCallable, Category = "Oleadas")
	void StopWaves();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 CurrentWave = 0;

	UPROPERTY(Replicated)
	int32 EnemiesRemaining = 0;

	UPROPERTY(Replicated)
	bool bWaveInProgress = false;

	/** Instante en que empieza la próxima oleada, para la cuenta atrás. */
	UPROPERTY(Replicated)
	float NextWaveWorldTime = 0.f;

	/** Enemigos aún por generar de la oleada actual. */
	int32 PendingSpawns = 0;

	UPROPERTY()
	TArray<TObjectPtr<APawn>> LiveEnemies;

	FTimerHandle WaveTimerHandle;
	FTimerHandle SpawnTimerHandle;

	void BeginNextWave();

	/** Genera un enemigo si cabe dentro del tope de concurrentes. */
	void SpawnEnemy();

	/** Punto de aparición alejado de los jugadores vivos. */
	bool FindSpawnTransform(FTransform& OutTransform) const;

	/** Descuenta el enemigo caído y cierra la oleada si era el último. */
	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor, AController* Killer);

	void CheckWaveCleared();
};
