#include "Core/ShooterGameState.h"
#include "Net/UnrealNetwork.h"

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameState, MatchPhase);
	DOREPLIFETIME(AShooterGameState, MatchEndWorldTime);
	DOREPLIFETIME(AShooterGameState, ScoreAlpha);
	DOREPLIFETIME(AShooterGameState, ScoreBravo);
}

float AShooterGameState::GetTimeRemaining() const
{
	if (MatchPhase != EMatchPhase::InProgress)
	{
		return 0.f;
	}

	return FMath::Max(MatchEndWorldTime - GetWorld()->GetTimeSeconds(), 0.f);
}

int32 AShooterGameState::GetTeamScore(EShooterTeam Team) const
{
	switch (Team)
	{
	case EShooterTeam::Alpha: return ScoreAlpha;
	case EShooterTeam::Bravo: return ScoreBravo;
	default: return 0;
	}
}

EShooterTeam AShooterGameState::GetWinningTeam() const
{
	if (ScoreAlpha == ScoreBravo)
	{
		return EShooterTeam::None;
	}

	return ScoreAlpha > ScoreBravo ? EShooterTeam::Alpha : EShooterTeam::Bravo;
}

void AShooterGameState::AddTeamScore(EShooterTeam Team, int32 Amount)
{
	switch (Team)
	{
	case EShooterTeam::Alpha: ScoreAlpha += Amount; break;
	case EShooterTeam::Bravo: ScoreBravo += Amount; break;
	default: break;
	}
}
