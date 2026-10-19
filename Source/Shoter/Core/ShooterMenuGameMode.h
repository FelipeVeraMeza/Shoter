#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterMenuGameMode.generated.h"

/**
 * Modo del nivel de menú (Lvl_Menu): sin personaje ni HUD de combate.
 *
 * Al entrar el jugador, la vista pasa a la cámara del decorado (la primera
 * CameraActor del nivel) y se abre el menú principal.
 */
UCLASS()
class SHOTER_API AShooterMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShooterMenuGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;

private:
	void ShowMainMenu(TWeakObjectPtr<APlayerController> Controller);
};
