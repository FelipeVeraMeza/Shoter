#include "Core/ShooterMenuGameMode.h"
#include "UI/ShooterMenuSubsystem.h"
#include "Camera/CameraActor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AShooterMenuGameMode::AShooterMenuGameMode()
{
	// En el menú no se juega: el jugador entra como espectador inmóvil y la
	// vista la da la cámara del decorado.
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = true;
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void AShooterMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer || !NewPlayer->IsLocalController())
	{
		return;
	}

	// Un fotograma después: el viewport y la cámara del jugador terminan de
	// crearse tras PostLogin.
	TWeakObjectPtr<APlayerController> WeakController = NewPlayer;
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
		this, &AShooterMenuGameMode::ShowMainMenu, WeakController));
}

void AShooterMenuGameMode::ShowMainMenu(TWeakObjectPtr<APlayerController> Controller)
{
	APlayerController* PlayerController = Controller.Get();
	if (!PlayerController)
	{
		return;
	}

	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		PlayerController->SetViewTarget(*It);
		break;
	}

	if (UShooterMenuSubsystem* Menu = GetGameInstance() ? GetGameInstance()->GetSubsystem<UShooterMenuSubsystem>() : nullptr)
	{
		Menu->OpenScreen(EShooterMenuScreen::Main);
	}
}
