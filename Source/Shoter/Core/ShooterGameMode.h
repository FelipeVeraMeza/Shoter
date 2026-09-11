#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterTypes.h"
#include "ShooterGameMode.generated.h"

class AShooterPlayerController;

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

	/** Distancia mínima deseada a un enemigo al reaparecer (RF-6.5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas", meta = (ClampMin = "0.0"))
	float MinimumSafeSpawnDistance = 2500.f;

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

	void StartMatch();
	void FinishMatch();

	/** Otorga la XP de la partida al jugador local (RF-9.1). */
	void AwardEndOfMatchExperience();
	void RespawnPlayer(AShooterPlayerController* Controller);
	EShooterTeam ChooseTeamForNewPlayer() const;
	void CheckWinCondition();
};
