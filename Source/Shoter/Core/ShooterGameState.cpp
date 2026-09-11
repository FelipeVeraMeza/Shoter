#include "Core/ShooterGameState.h"
#include "Net/UnrealNetwork.h"

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameState, MatchPhase);
	DOREPLIFETIME(AShooterGameState, MatchEndWorldTime);
	DOREPLIFETIME(AShooterGameState, ScoreAlpha);
	DOREPLIFETIME(AShooterGameState, ScoreBravo);
	DOREPLIFETIME(AShooterGameState, KillFeed);
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

void AShooterGameState::AddKillEvent(const FString& KillerName, const FString& VictimName,
	const FString& WeaponName, bool bHeadshot)
{
	if (!HasAuthority())
	{
		return;
	}

	FShooterKillEvent Event;
	Event.KillerName = KillerName;
	Event.VictimName = VictimName;
	Event.WeaponName = WeaponName;
	Event.bHeadshot = bHeadshot;
	Event.WorldTime = GetWorld()->GetTimeSeconds();

	// La más reciente va primero: el HUD dibuja de arriba abajo.
	KillFeed.Insert(Event, 0);

	if (KillFeed.Num() > MaxKillFeedEntries)
	{
		KillFeed.SetNum(MaxKillFeedEntries);
	}
}
