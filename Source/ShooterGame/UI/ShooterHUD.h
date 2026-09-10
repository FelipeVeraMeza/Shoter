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
class SHOOTERGAME_API AShooterHUD : public AHUD
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

private:
	void DrawCrosshair(AShooterCharacter* Character);
	void DrawHitMarker(AShooterPlayerController* Controller);
	void DrawHealth(AShooterCharacter* Character);
	void DrawAmmo(AShooterCharacter* Character);
	void DrawMatchInfo();
	void DrawDamageIndicator(AShooterCharacter* Character, AShooterPlayerController* Controller);
	void DrawRespawnCountdown(AShooterPlayerController* Controller);

	/** Convierte un ángulo de dispersión en separación de la mira, en píxeles. */
	float SpreadDegreesToPixels(float SpreadDegrees) const;
};
