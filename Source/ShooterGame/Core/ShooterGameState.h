#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterTypes.h"
#include "ShooterGameState.generated.h"

UCLASS()
class SHOOTERGAME_API AShooterGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Partida")
	EMatchPhase GetMatchPhase() const { return MatchPhase; }

	UFUNCTION(BlueprintPure, Category = "Partida")
	float GetTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Partida")
	int32 GetTeamScore(EShooterTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Partida")
	EShooterTeam GetWinningTeam() const;

	/** Solo servidor. */
	void SetMatchPhase(EMatchPhase NewPhase) { MatchPhase = NewPhase; }
	void SetMatchEndWorldTime(float WorldTime) { MatchEndWorldTime = WorldTime; }
	void AddTeamScore(EShooterTeam Team, int32 Amount);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	EMatchPhase MatchPhase = EMatchPhase::WaitingToStart;

	UPROPERTY(Replicated)
	float MatchEndWorldTime = 0.f;

	UPROPERTY(Replicated)
	int32 ScoreAlpha = 0;

	UPROPERTY(Replicated)
	int32 ScoreBravo = 0;
};
