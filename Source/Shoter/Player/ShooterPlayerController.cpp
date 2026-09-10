#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameMode.h"
#include "Net/UnrealNetwork.h"

void AShooterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShooterPlayerController, RespawnWorldTime, COND_OwnerOnly);
}

void AShooterPlayerController::Client_NotifyHitConfirmed_Implementation(bool bHeadshot)
{
	LastHitConfirmedTime = GetWorld()->GetTimeSeconds();
	bLastHitWasHeadshot = bHeadshot;
}

void AShooterPlayerController::Client_NotifyDamageTaken_Implementation(const FVector_NetQuantize& DamageSourceLocation)
{
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();
	LastDamageSourceLocation = DamageSourceLocation;
}

float AShooterPlayerController::GetTimeSinceHitConfirmed() const
{
	return GetWorld()->GetTimeSeconds() - LastHitConfirmedTime;
}

float AShooterPlayerController::GetTimeSinceDamageTaken() const
{
	return GetWorld()->GetTimeSeconds() - LastDamageTakenTime;
}

float AShooterPlayerController::GetRespawnCountdown() const
{
	return FMath::Max(RespawnWorldTime - GetWorld()->GetTimeSeconds(), 0.f);
}

void AShooterPlayerController::OnCharacterDied(AController* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AShooterGameMode* GameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>())
	{
		GameMode->OnPlayerKilled(this, Killer);
	}
}
