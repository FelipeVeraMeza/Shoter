#include "UI/ShooterMenuWidget.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Progression/ShooterSaveGame.h"
#include "Settings/ShooterSettingsSubsystem.h"
#include "Engine/GameInstance.h"

UShooterMenuSubsystem* UShooterMenuWidget::GetMenu() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UShooterMenuSubsystem>() : nullptr;
}

UShooterProgressionSubsystem* UShooterMenuWidget::GetProgression() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UShooterProgressionSubsystem>() : nullptr;
}

UShooterSettingsSubsystem* UShooterMenuWidget::GetSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UShooterSettingsSubsystem>() : nullptr;
}

void UShooterMenuWidget::OpenScreen(EShooterMenuScreen Screen)
{
	if (UShooterMenuSubsystem* Menu = GetMenu())
	{
		Menu->OpenScreen(Screen);
	}
}

void UShooterMenuWidget::GoBack()
{
	if (UShooterMenuSubsystem* Menu = GetMenu())
	{
		Menu->GoBack();
	}
}

void UShooterMenuWidget::CloseMenu()
{
	if (UShooterMenuSubsystem* Menu = GetMenu())
	{
		Menu->CloseMenu();
	}
}

void UShooterMenuWidget::QuitGame()
{
	if (UShooterMenuSubsystem* Menu = GetMenu())
	{
		Menu->QuitGame();
	}
}

void UShooterMenuWidget::LeaveMatch()
{
	if (UShooterMenuSubsystem* Menu = GetMenu())
	{
		Menu->LeaveMatch();
	}
}

void UShooterMenuWidget::StartMatch(FName LevelName)
{
	if (UShooterMenuSubsystem* Menu = GetMenu())
	{
		Menu->StartMatch(LevelName);
	}
}

void UShooterMenuWidget::GetProgressSummary(int32& OutLevel, int32& OutCurrentXP, int32& OutRequiredXP) const
{
	OutLevel = 1;
	OutCurrentXP = 0;
	OutRequiredXP = 1;

	if (const UShooterProgressionSubsystem* Progression = GetProgression())
	{
		OutLevel = Progression->GetLevel();
		Progression->GetLevelProgress(OutCurrentXP, OutRequiredXP);
	}
}

FText UShooterMenuWidget::GetStatsSummary() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	if (!Progression)
	{
		return FText::GetEmpty();
	}

	const UShooterSaveGame* Save = Progression->GetSaveData();
	const int32 Matches = Save ? Save->MatchesPlayed : 0;

	return FText::FromString(FString::Printf(
		TEXT("K/D %.2f  ·  Precisión %d %%  ·  %d partidas"),
		Progression->GetKillDeathRatio(),
		FMath::RoundToInt(Progression->GetAccuracy() * 100.f),
		Matches));
}

FText UShooterMenuWidget::GetPlayerName() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	return Progression ? FText::FromString(Progression->GetPlayerName()) : FText::GetEmpty();
}

void UShooterMenuWidget::SetPlayerName(const FText& NewName)
{
	if (UShooterProgressionSubsystem* Progression = GetProgression())
	{
		Progression->SetPlayerName(NewName.ToString());
	}
}
