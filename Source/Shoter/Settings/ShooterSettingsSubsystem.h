#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShooterSettingsSubsystem.generated.h"

class UShooterProgressionSubsystem;
class USoundMix;
class USoundClass;

UENUM(BlueprintType)
enum class EShooterWindowMode : uint8
{
	Fullscreen			UMETA(DisplayName = "Pantalla completa"),
	WindowedFullscreen	UMETA(DisplayName = "Ventana sin bordes"),
	Windowed			UMETA(DisplayName = "Ventana")
};

/**
 * Ajustes del jugador (RF-11).
 *
 * Los de vídeo se delegan en UGameUserSettings, que es quien sabe aplicarlos;
 * los de juego (sensibilidad, volúmenes, mira) viven en el perfil guardado
 * para que persistan entre sesiones (RF-11.9).
 */
UCLASS(Config = Game)
class SHOTER_API UShooterSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- Sensibilidad (RF-11.1) ---

	UFUNCTION(BlueprintPure, Category = "Ajustes|Control")
	float GetMouseSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Control")
	void SetMouseSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Control")
	bool GetInvertY() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Control")
	void SetInvertY(bool bInvert);

	// --- Vídeo (RF-11.3, RF-11.5, RF-11.6) ---

	/** Preset de calidad: 0 baja, 1 media, 2 alta, 3 épica, 4 cinemática. */
	UFUNCTION(BlueprintPure, Category = "Ajustes|Vídeo")
	int32 GetQualityPreset() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Vídeo")
	void SetQualityPreset(int32 Preset);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Vídeo")
	bool GetVSyncEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Vídeo")
	void SetVSyncEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Vídeo")
	float GetFrameRateLimit() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Vídeo")
	void SetFrameRateLimit(float Limit);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Vídeo")
	EShooterWindowMode GetWindowMode() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Vídeo")
	void SetWindowMode(EShooterWindowMode Mode);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Vídeo")
	FIntPoint GetResolution() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Vídeo")
	void SetResolution(FIntPoint Resolution);

	/** Resoluciones admitidas por el monitor, de menor a mayor (RNF-5.3). */
	UFUNCTION(BlueprintPure, Category = "Ajustes|Vídeo")
	TArray<FIntPoint> GetSupportedResolutions() const;

	// --- Audio (RF-11.4) ---

	UFUNCTION(BlueprintPure, Category = "Ajustes|Audio")
	float GetMasterVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Audio")
	void SetMasterVolume(float Value);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Audio")
	float GetEffectsVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Audio")
	void SetEffectsVolume(float Value);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Audio")
	float GetMusicVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Audio")
	void SetMusicVolume(float Value);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Audio")
	float GetVoiceVolume() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Audio")
	void SetVoiceVolume(float Value);

	// --- Accesibilidad (RF-11.7, RF-11.8) ---

	UFUNCTION(BlueprintPure, Category = "Ajustes|Accesibilidad")
	int32 GetColorBlindMode() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Accesibilidad")
	void SetColorBlindMode(int32 Mode);

	UFUNCTION(BlueprintPure, Category = "Ajustes|Accesibilidad")
	FLinearColor GetCrosshairColor() const;

	UFUNCTION(BlueprintCallable, Category = "Ajustes|Accesibilidad")
	void SetCrosshairColor(FLinearColor Color);

	// --- Idioma (RNF-6.2, RNF-6.3) ---

	/** Código de cultura activo ("es", "en"). */
	UFUNCTION(BlueprintPure, Category = "Ajustes|Idioma")
	FString GetLanguage() const;

	/**
	 * Cambia el idioma y lo persiste.
	 *
	 * Afecta a todo texto que pase por FText, así que el HUD se traduce sin
	 * reiniciar. Los valores ya formateados en pantalla se redibujan al
	 * siguiente frame porque el HUD por Canvas se repinta cada uno.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ajustes|Idioma")
	void SetLanguage(const FString& CultureCode);

	/** Culturas disponibles, para poblar el desplegable del menú. */
	UFUNCTION(BlueprintPure, Category = "Ajustes|Idioma")
	TArray<FString> GetAvailableLanguages() const;

	/** Escribe ajustes de vídeo y perfil (RF-11.9). */
	UFUNCTION(BlueprintCallable, Category = "Ajustes")
	void ApplyAndSave();

	/**
	 * Mezcla y clases de sonido a las que enrutar los volúmenes (RNF3).
	 * Sin ellas los valores se guardan pero no afectan al audio; se asignan
	 * desde la configuración del proyecto cuando existan los assets.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundMix> MainSoundMix;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

private:
	UShooterProgressionSubsystem* GetProgression() const;

	/** Envía el volumen guardado a su Sound Class (RNF3). */
	void ApplyVolumeToClass(const TSoftObjectPtr<USoundClass>& SoundClass, float Volume);

	/** Reenvía los cuatro canales; se llama al arrancar y al cambiar uno. */
	void ApplyAllVolumes();

	/** Guarda el perfil sin tocar los ajustes de vídeo. */
	void SaveProfile();
};
