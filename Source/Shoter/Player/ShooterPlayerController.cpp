#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameMode.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Engine/GameInstance.h"
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

	// Precisión acumulada (RF-9.6). El disparo se cuenta al efectuarlo, en
	// RegisterShotFired; aquí solo se marca el acierto.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterProgressionSubsystem* Progression =
			GameInstance->GetSubsystem<UShooterProgressionSubsystem>())
		{
			Progression->RecordShot(true);
		}
	}
}

void AShooterPlayerController::RegisterShotFired()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterProgressionSubsystem* Progression =
			GameInstance->GetSubsystem<UShooterProgressionSubsystem>())
		{
			// Un disparo sin acierto; el impacto lo confirma el servidor aparte.
			Progression->RecordShot(false);
		}
	}
}

void AShooterPlayerController::Client_NotifyDamageTaken_Implementation(const FVector_NetQuantize& DamageSourceLocation)
{
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();
	LastDamageSourceLocation = DamageSourceLocation;
}

void AShooterPlayerController::Client_NotifyKilledBy_Implementation(
	const FString& KillerName, const FString& WeaponName)
{
	LastKillerName = KillerName;
	LastKillerWeapon = WeaponName;
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
