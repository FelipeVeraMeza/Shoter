#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterTypes.h"
#include "ShooterGameMode.generated.h"

class AShooterPlayerController;
class AAIController;

UCLASS()
class SHOTER_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShooterGameMode();

	/**
	 * Partida sin final: ignora MatchDuration y ScoreToWin. Pensado para la zona
	 * de entrenamiento, donde un cronómetro cortaría la práctica a mitad.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas")
	bool bEndlessMatch = false;

	/** Duración de la partida en segundos. Sin efecto si bEndlessMatch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas", meta = (ClampMin = "10.0"))
	float MatchDuration = 480.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas", meta = (ClampMin = "0.0"))
	float RespawnDelay = 4.f;

	/** Eliminaciones necesarias para ganar. Cero desactiva la condición. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas", meta = (ClampMin = "0"))
	int32 ScoreToWin = 50;

	/** Si está activo, los jugadores se reparten en dos equipos. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas")
	bool bTeamBased = true;

	/**
	 * Modo activo. Determina de dónde salen los puntos: de las eliminaciones
	 * en los modos de combate, de las zonas en Dominación (RF-8.3).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas")
	EShooterGameModeType GameModeType = EShooterGameModeType::TeamDeathmatch;

	/** Distancia mínima deseada a un enemigo al reaparecer (RF-6.5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas", meta = (ClampMin = "0.0"))
	float MinimumSafeSpawnDistance = 2500.f;

	/**
	 * Jugadores que debe haber en la partida. Si faltan, se rellenan con bots
	 * (RF-3.6). Cero desactiva el relleno.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots", meta = (ClampMin = "0", ClampMax = "16"))
	int32 DesiredPlayerCount = 0;

	/** Controlador de los bots. Sin esto no se crea ninguno. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots")
	TSubclassOf<AAIController> BotControllerClass;

	/** Segundos tras el inicio antes de rellenar, para dar tiempo a que entren jugadores. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots", meta = (ClampMin = "0.0"))
	float BotFillDelay = 3.f;

	UFUNCTION(BlueprintCallable, Category = "Bots")
	void SpawnBot();

	UFUNCTION(BlueprintPure, Category = "Bots")
	int32 GetBotCount() const { return SpawnedBots.Num(); }

	/** Solo servidor. Registra la eliminación y programa la reaparición. */
	void OnPlayerKilled(AShooterPlayerController* Victim, AController* Killer);

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle MatchTimerHandle;

	/** Momento en que arrancó la partida, para medir su duración. */
	float MatchStartWorldTime = 0.f;

	FTimerHandle BotFillTimerHandle;

	UPROPERTY()
	TArray<TObjectPtr<APawn>> SpawnedBots;

	void StartMatch();
	void FinishMatch();

	/** Otorga la XP de la partida al jugador local (RF-9.1). */
	void AwardEndOfMatchExperience();

	/** Crea los bots que falten para alcanzar DesiredPlayerCount (RF-3.6). */
	void FillWithBots();
	void RespawnPlayer(AShooterPlayerController* Controller);
	EShooterTeam ChooseTeamForNewPlayer() const;
	void CheckWinCondition();

	/**
	 * En Dominación el marcador sube solo, sin que muera nadie, así que la
	 * victoria no puede comprobarse únicamente al registrar una eliminación.
	 */
	FTimerHandle DominationCheckHandle;
};
