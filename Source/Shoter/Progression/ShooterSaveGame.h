#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ShooterSaveGame.generated.h"

/**
 * Progreso y ajustes del jugador, guardados en disco.
 *
 * El almacenamiento definitivo vive en el servidor (RNF-3.4); esta clase cubre
 * la persistencia local (RF-9.5, RF-11.9) y sirve de caché mientras el backend
 * no exista.
 */
UCLASS()
class SHOTER_API UShooterSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Nombre visible en partida (RF-1.3). */
	UPROPERTY(BlueprintReadWrite, Category = "Perfil")
	FString PlayerName = TEXT("Jugador");

	UPROPERTY(BlueprintReadWrite, Category = "Progresión")
	int32 Level = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Progresión")
	int32 ExperiencePoints = 0;

	/** Estadísticas acumuladas entre partidas (RF-9.6). */
	UPROPERTY(BlueprintReadWrite, Category = "Estadísticas")
	int32 TotalKills = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Estadísticas")
	int32 TotalDeaths = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Estadísticas")
	int32 MatchesPlayed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Estadísticas")
	int32 ShotsFired = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Estadísticas")
	int32 ShotsHit = 0;

	/** Armas desbloqueadas por nivel (RF-9.3). */
	UPROPERTY(BlueprintReadWrite, Category = "Progresión")
	TArray<FName> UnlockedWeapons;

	// --- Ajustes (RF-11) ---

	UPROPERTY(BlueprintReadWrite, Category = "Ajustes", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float MouseSensitivity = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Ajustes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Ajustes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EffectsVolume = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Ajustes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicVolume = 0.7f;

	UPROPERTY(BlueprintReadWrite, Category = "Ajustes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VoiceVolume = 1.f;

	/** Color de la mira, personalizable (RF-11.8). */
	UPROPERTY(BlueprintReadWrite, Category = "Ajustes")
	FLinearColor CrosshairColor = FLinearColor(0.1f, 1.f, 0.35f, 1.f);

	/** Modo de daltonismo: 0 ninguno, 1 protanopia, 2 deuteranopia, 3 tritanopia (RF-11.7). */
	UPROPERTY(BlueprintReadWrite, Category = "Ajustes", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ColorBlindMode = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Ajustes")
	bool bInvertY = false;

	/**
	 * HUD realista: sin mira, sin minimapa y sin contador de balas exacto.
	 * Activo por defecto porque es la experiencia que define el juego; el
	 * HUD completo queda para quien lo prefiera o esté aprendiendo.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Ajustes")
	bool bRealisticHUD = true;

	/**
	 * Cultura elegida por el jugador ("es", "en"). Vacío significa "la del
	 * sistema": así un perfil nuevo respeta el idioma de Steam o de Windows
	 * en vez de imponer español (RNF-6.3).
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Ajustes")
	FString Language;

	/**
	 * Disparos efectuados con cada arma, indexados por su nombre de asset
	 * (RF-9.4).
	 *
	 * El desbloqueo de accesorios se mide por arma y no globalmente, que es lo
	 * que pide el requerimiento: los accesorios del rifle se ganan usando el
	 * rifle.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Progresión")
	TMap<FName, int32> WeaponShotCounts;

	/** Accesorios ya desbloqueados, por nombre de asset. */
	UPROPERTY(BlueprintReadWrite, Category = "Progresión")
	TSet<FName> UnlockedAttachments;
};
