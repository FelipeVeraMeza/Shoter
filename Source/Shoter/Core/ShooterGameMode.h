#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterTypes.h"
#include "ShooterGameMode.generated.h"

class AShooterPlayerController;
class AAIController;
class AWeaponBase;

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

	/** Probabilidad de que un bot lleve el fusil de tirador en vez del de asalto. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bots", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MarksmanBotChance = 0.25f;

	/**
	 * Armas principales que ofrece la pantalla de equipamiento. La primera
	 * es la de fábrica. Vacío: no se puede elegir.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipamiento")
	TArray<TSubclassOf<AWeaponBase>> PrimaryWeaponOptions;

	/** Segundos con la tabla final en pantalla antes de empezar otra partida. Cero: no se reinicia. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reglas", meta = (ClampMin = "0.0"))
	float PostMatchRestartDelay = 20.f;

	UFUNCTION(BlueprintCallable, Category = "Bots")
	void SpawnBot();

	UFUNCTION(BlueprintPure, Category = "Bots")
	int32 GetBotCount() const { return BotControllers.Num(); }

	/** Solo servidor. Registra la eliminación y programa la reaparición. */
	void OnPlayerKilled(AShooterPlayerController* Victim, AController* Killer);

	/** Solo servidor. Lo mismo para un bot: cuenta la baja y lo hace reaparecer. */
	void OnBotKilled(AController* Victim, AController* Killer);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle MatchTimerHandle;

	/** Momento en que arrancó la partida, para medir su duración. */
	float MatchStartWorldTime = 0.f;

	FTimerHandle BotFillTimerHandle;
	FTimerHandle RestartTimerHandle;

	/** Controladores de los bots: sobreviven a cada muerte, el pawn no. */
	UPROPERTY()
	TArray<TObjectPtr<AAIController>> BotControllers;

	/** Índice para dar a cada bot un indicativo distinto. */
	int32 BotNameIndex = 0;

	/** Bajas, marcador y registro: común a jugadores y bots. */
	void RegisterKill(AController* Victim, AController* Killer);

	void RespawnBot(AAIController* BotController);

	/** Crea el pawn de un bot en un punto de aparición y lo posee. */
	bool SpawnPawnForBot(AAIController* BotController);

	int32 CountHumanPlayers() const;
	void RestartMatch();

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
