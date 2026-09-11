#include "Settings/ShooterSettingsSubsystem.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Progression/ShooterSaveGame.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameInstance.h"

void UShooterSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// La progresión guarda el perfil donde viven los ajustes de juego.
	Collection.InitializeDependency<UShooterProgressionSubsystem>();

	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->LoadSettings();
		Settings->ApplySettings(false);
	}
}

UShooterProgressionSubsystem* UShooterSettingsSubsystem::GetProgression() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UShooterProgressionSubsystem>() : nullptr;
}

void UShooterSettingsSubsystem::SaveProfile()
{
	if (UShooterProgressionSubsystem* Progression = GetProgression())
	{
		Progression->Save();
	}
}

// --- Sensibilidad ---

float UShooterSettingsSubsystem::GetMouseSensitivity() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->MouseSensitivity : 1.f;
}

void UShooterSettingsSubsystem::SetMouseSensitivity(float Value)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->MouseSensitivity = FMath::Clamp(Value, 0.05f, 5.f);
	SaveProfile();
}

bool UShooterSettingsSubsystem::GetInvertY() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->bInvertY : false;
}

void UShooterSettingsSubsystem::SetInvertY(bool bInvert)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->bInvertY = bInvert;
	SaveProfile();
}

// --- Vídeo ---

int32 UShooterSettingsSubsystem::GetQualityPreset() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings ? Settings->GetOverallScalabilityLevel() : 2;
}

void UShooterSettingsSubsystem::SetQualityPreset(int32 Preset)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Preset, 0, 4));
		Settings->ApplySettings(false);
	}
}

bool UShooterSettingsSubsystem::GetVSyncEnabled() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings ? Settings->IsVSyncEnabled() : false;
}

void UShooterSettingsSubsystem::SetVSyncEnabled(bool bEnabled)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetVSyncEnabled(bEnabled);
		Settings->ApplySettings(false);
	}
}

float UShooterSettingsSubsystem::GetFrameRateLimit() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings ? Settings->GetFrameRateLimit() : 0.f;
}

void UShooterSettingsSubsystem::SetFrameRateLimit(float Limit)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		// Cero significa sin límite.
		Settings->SetFrameRateLimit(FMath::Max(Limit, 0.f));
		Settings->ApplySettings(false);
	}
}

EShooterWindowMode UShooterSettingsSubsystem::GetWindowMode() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return EShooterWindowMode::Fullscreen;
	}

	switch (Settings->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen:			return EShooterWindowMode::Fullscreen;
	case EWindowMode::WindowedFullscreen:	return EShooterWindowMode::WindowedFullscreen;
	default:								return EShooterWindowMode::Windowed;
	}
}

void UShooterSettingsSubsystem::SetWindowMode(EShooterWindowMode Mode)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}

	EWindowMode::Type EngineMode = EWindowMode::Fullscreen;
	switch (Mode)
	{
	case EShooterWindowMode::WindowedFullscreen:	EngineMode = EWindowMode::WindowedFullscreen; break;
	case EShooterWindowMode::Windowed:				EngineMode = EWindowMode::Windowed; break;
	default:										break;
	}

	Settings->SetFullscreenMode(EngineMode);
	Settings->ApplySettings(false);
}

FIntPoint UShooterSettingsSubsystem::GetResolution() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return Settings ? Settings->GetScreenResolution() : FIntPoint(1920, 1080);
}

void UShooterSettingsSubsystem::SetResolution(FIntPoint Resolution)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetScreenResolution(Resolution);
		Settings->ApplySettings(false);
	}
}

TArray<FIntPoint> UShooterSettingsSubsystem::GetSupportedResolutions() const
{
	TArray<FIntPoint> Resolutions;
	FScreenResolutionArray Found;

	if (RHIGetAvailableResolutions(Found, true))
	{
		for (const FScreenResolutionRHI& Entry : Found)
		{
			const FIntPoint Resolution(Entry.Width, Entry.Height);

			// El mismo tamaño aparece repetido por cada frecuencia de refresco.
			if (Resolution.X >= 1280 && !Resolutions.Contains(Resolution))
			{
				Resolutions.Add(Resolution);
			}
		}
	}

	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X * A.Y < B.X * B.Y;
	});

	return Resolutions;
}

// --- Audio ---

float UShooterSettingsSubsystem::GetMasterVolume() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->MasterVolume : 1.f;
}

void UShooterSettingsSubsystem::SetMasterVolume(float Value)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->MasterVolume = FMath::Clamp(Value, 0.f, 1.f);
	SaveProfile();
}

float UShooterSettingsSubsystem::GetEffectsVolume() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->EffectsVolume : 1.f;
}

void UShooterSettingsSubsystem::SetEffectsVolume(float Value)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->EffectsVolume = FMath::Clamp(Value, 0.f, 1.f);
	SaveProfile();
}

float UShooterSettingsSubsystem::GetMusicVolume() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->MusicVolume : 0.7f;
}

void UShooterSettingsSubsystem::SetMusicVolume(float Value)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->MusicVolume = FMath::Clamp(Value, 0.f, 1.f);
	SaveProfile();
}

float UShooterSettingsSubsystem::GetVoiceVolume() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->VoiceVolume : 1.f;
}

void UShooterSettingsSubsystem::SetVoiceVolume(float Value)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->VoiceVolume = FMath::Clamp(Value, 0.f, 1.f);
	SaveProfile();
}

// --- Accesibilidad ---

int32 UShooterSettingsSubsystem::GetColorBlindMode() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->ColorBlindMode : 0;
}

void UShooterSettingsSubsystem::SetColorBlindMode(int32 Mode)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->ColorBlindMode = FMath::Clamp(Mode, 0, 3);
	SaveProfile();
}

FLinearColor UShooterSettingsSubsystem::GetCrosshairColor() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	return Save ? Save->CrosshairColor : FLinearColor(0.1f, 1.f, 0.35f, 1.f);
}

void UShooterSettingsSubsystem::SetCrosshairColor(FLinearColor Color)
{
	UShooterProgressionSubsystem* Progression = GetProgression();
	UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (!Save)
	{
		return;
	}

	Save->CrosshairColor = Color;
	SaveProfile();
}

void UShooterSettingsSubsystem::ApplyAndSave()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}

	SaveProfile();
}
