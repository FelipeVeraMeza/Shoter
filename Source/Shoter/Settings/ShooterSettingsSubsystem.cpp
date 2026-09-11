#include "Settings/ShooterSettingsSubsystem.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Progression/ShooterSaveGame.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Kismet/GameplayStatics.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

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

	ApplyAllVolumes();

	// El idioma debe aplicarse al arrancar: el perfil ya está cargado por la
	// dependencia de arriba, así que aquí ya se sabe cuál eligió el jugador.
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;
	if (Save && !Save->Language.IsEmpty())
	{
		FInternationalization::Get().SetCurrentCulture(Save->Language);
	}
}

void UShooterSettingsSubsystem::ApplyVolumeToClass(
	const TSoftObjectPtr<USoundClass>& SoundClass, float Volume)
{
	// Sin mezcla ni clase asignadas el valor solo se guarda: es lo esperado
	// mientras el proyecto no tenga assets de audio.
	USoundMix* Mix = MainSoundMix.LoadSynchronous();
	USoundClass* Class = SoundClass.LoadSynchronous();

	if (!Mix || !Class)
	{
		return;
	}

	UGameplayStatics::SetSoundMixClassOverride(
		GetGameInstance(), Mix, Class, Volume, 1.f, 0.1f, true);

	UGameplayStatics::PushSoundMixModifier(GetGameInstance(), Mix);
}

void UShooterSettingsSubsystem::ApplyAllVolumes()
{
	ApplyVolumeToClass(MasterSoundClass, GetMasterVolume());
	ApplyVolumeToClass(EffectsSoundClass, GetEffectsVolume());
	ApplyVolumeToClass(MusicSoundClass, GetMusicVolume());
	ApplyVolumeToClass(VoiceSoundClass, GetVoiceVolume());
}

UShooterProgressionSubsystem* UShooterSettingsSubsystem::GetProgression() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UShooterProgressionSubsystem>() : nullptr;
}

FString UShooterSettingsSubsystem::GetLanguage() const
{
	const UShooterProgressionSubsystem* Progression = GetProgression();
	const UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr;

	// Un perfil sin idioma elegido hereda el del sistema, que es lo que el
	// jugador espera la primera vez que abre el juego (RNF-6.3).
	if (!Save || Save->Language.IsEmpty())
	{
		return FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
	}

	return Save->Language;
}

void UShooterSettingsSubsystem::SetLanguage(const FString& CultureCode)
{
	if (CultureCode.IsEmpty() || !GetAvailableLanguages().Contains(CultureCode))
	{
		return;
	}

	// SetCurrentCulture cambia formato de números y fechas además del idioma;
	// es lo correcto para un cambio elegido por el jugador.
	FInternationalization::Get().SetCurrentCulture(CultureCode);

	UShooterProgressionSubsystem* Progression = GetProgression();
	if (UShooterSaveGame* Save = Progression ? Progression->GetSaveData() : nullptr)
	{
		Save->Language = CultureCode;
		SaveProfile();
	}
}

bool UShooterSettingsSubsystem::RemapKey(APlayerController* PlayerController,
	FName MappingName, const FKey& NewKey)
{
	if (!PlayerController || MappingName.IsNone() || !NewKey.IsValid())
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;

	UEnhancedInputUserSettings* UserSettings = InputSubsystem
		? InputSubsystem->GetUserSettings() : nullptr;

	if (!UserSettings)
	{
		return false;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.NewKey = NewKey;

	// Sin slot explícito la reasignación sustituye la primera tecla de la
	// acción, que es lo que espera quien pulsa "cambiar" en el menú.
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.bCreateMatchingSlotIfNeeded = true;

	FGameplayTagContainer FailureReason;
	UserSettings->MapPlayerKey(Args, FailureReason);

	if (!FailureReason.IsEmpty())
	{
		return false;
	}

	// ApplySettings surte efecto en la sesión; SaveSettings lo persiste
	// (RF-11.9). Sin la segunda llamada el cambio se pierde al cerrar.
	UserSettings->ApplySettings();
	UserSettings->SaveSettings();

	return true;
}

void UShooterSettingsSubsystem::ResetKeyBindings(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;

	UEnhancedInputUserSettings* UserSettings = InputSubsystem
		? InputSubsystem->GetUserSettings() : nullptr;

	if (!UserSettings)
	{
		return;
	}

	if (UEnhancedPlayerMappableKeyProfile* Profile = UserSettings->GetCurrentKeyProfile())
	{
		Profile->ResetToDefault();
	}

	UserSettings->ApplySettings();
	UserSettings->SaveSettings();
}

TArray<FString> UShooterSettingsSubsystem::GetAvailableLanguages() const
{
	// Fijas en v1.0 (RNF-6.2). Cuando haya más culturas conviene leerlas de
	// las que el paquete traiga compiladas en vez de mantener esta lista.
	return { TEXT("es"), TEXT("en") };
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
	ApplyVolumeToClass(MasterSoundClass, Save->MasterVolume);
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
	ApplyVolumeToClass(EffectsSoundClass, Save->EffectsVolume);
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
	ApplyVolumeToClass(MusicSoundClass, Save->MusicVolume);
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
	ApplyVolumeToClass(VoiceSoundClass, Save->VoiceVolume);
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

	ApplyAllVolumes();
	SaveProfile();
}
