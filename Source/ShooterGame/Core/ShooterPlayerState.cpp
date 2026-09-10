#include "Core/ShooterPlayerState.h"
#include "Net/UnrealNetwork.h"

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerState, Kills);
	DOREPLIFETIME(AShooterPlayerState, Deaths);
	DOREPLIFETIME(AShooterPlayerState, Team);
}

void AShooterPlayerState::AddKill()
{
	++Kills;
	SetScore(GetScore() + 1.f);
}

void AShooterPlayerState::AddDeath()
{
	++Deaths;
}
