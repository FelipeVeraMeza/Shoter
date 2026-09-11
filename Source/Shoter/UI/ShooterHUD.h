#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

class AShooterCharacter;
class AShooterPlayerController;

/**
 * HUD dibujado con Canvas: no depende de ningún asset, así que funciona desde la
 * primera compilación. Se reemplazará por UMG/CommonUI en la Fase 2.
 */
UCLASS()
class SHOTER_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FLinearColor CrosshairColor = FLinearColor(0.1f, 1.f, 0.35f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float HitMarkerDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float DamageIndicatorDuration = 1.2f;

	/** Segundos que una línea del registro permanece visible. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float KillFeedDuration = 6.f;

private:
	void DrawCrosshair(AShooterCharacter* Character);
	void DrawHitMarker(AShooterPlayerController* Controller);
	void DrawHealth(AShooterCharacter* Character);
	void DrawAmmo(AShooterCharacter* Character);
	void DrawMatchInfo();

	/** Estado de las zonas de Dominación: quién controla cada una (RF-8.3). */
	void DrawControlZones();

	/** Oleada actual y enemigos restantes (RF-8.4). */
	void DrawWaveInfo();
	void DrawDamageIndicator(AShooterCharacter* Character, AShooterPlayerController* Controller);
	void DrawRespawnCountdown(AShooterPlayerController* Controller);

	/** Pantalla de resultados al terminar la partida (RF-8.6). */
	void DrawScoreboard();

	/** Tabla de posiciones durante la partida, con Tab (RF-7.8). */
	void DrawLiveScoreboard();

	/** Registro de eliminaciones, arriba a la derecha (RF-7.7). */
	void DrawKillFeed();

	/** Cabecera de columnas, compartida por ambas tablas. */
	void DrawColumns(float X, float Y, float Width);

	void DrawPlayerRows(const TArray<const class AShooterPlayerState*>& Players,
		float X, float Y, float Width, float RowHeight);

	/** Convierte un ángulo de dispersión en separación de la mira, en píxeles. */
	float SpreadDegreesToPixels(float SpreadDegrees) const;
};
