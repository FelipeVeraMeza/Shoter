#include "UI/ShooterMenuSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

void UShooterMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UShooterMenuSubsystem::Deinitialize()
{
	RemoveActiveWidget();

	Super::Deinitialize();
}

APlayerController* UShooterMenuSubsystem::GetLocalController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

void UShooterMenuSubsystem::OpenScreen(EShooterMenuScreen Screen)
{
	if (Screen == EShooterMenuScreen::None)
	{
		CloseMenu();
		return;
	}

	// La pantalla actual pasa a la pila para que GoBack pueda volver a ella.
	if (CurrentScreen != EShooterMenuScreen::None && CurrentScreen != Screen)
	{
		NavigationStack.Push(CurrentScreen);
	}

	CurrentScreen = Screen;
	ShowWidgetForScreen(Screen);
	UpdateInputMode();

	OnScreenChanged.Broadcast(Screen);
}

void UShooterMenuSubsystem::GoBack()
{
	if (NavigationStack.Num() == 0)
	{
		CloseMenu();
		return;
	}

	CurrentScreen = NavigationStack.Pop();
	ShowWidgetForScreen(CurrentScreen);
	UpdateInputMode();

	OnScreenChanged.Broadcast(CurrentScreen);
}

void UShooterMenuSubsystem::CloseMenu()
{
	RemoveActiveWidget();

	CurrentScreen = EShooterMenuScreen::None;
	NavigationStack.Reset();

	UpdateInputMode();

	OnScreenChanged.Broadcast(EShooterMenuScreen::None);
}

void UShooterMenuSubsystem::TogglePause()
{
	if (CurrentScreen == EShooterMenuScreen::None)
	{
		OpenScreen(EShooterMenuScreen::Pause);
		return;
	}

	// Desde cualquier pantalla abierta en partida, Escape cierra el menú.
	CloseMenu();
}

void UShooterMenuSubsystem::ShowWidgetForScreen(EShooterMenuScreen Screen)
{
	RemoveActiveWidget();

	const TSoftClassPtr<UUserWidget>* WidgetClassPtr = ScreenWidgets.Find(Screen);
	if (!WidgetClassPtr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Menu] Sin widget asignado para la pantalla %d. "
				 "Configúralo en Project Settings -> Shoter Menu."),
			static_cast<int32>(Screen));
		return;
	}

	// Carga síncrona: los menús son pequeños y el jugador espera respuesta
	// inmediata al pulsar.
	UClass* WidgetClass = WidgetClassPtr->LoadSynchronous();
	if (!WidgetClass)
	{
		return;
	}

	APlayerController* Controller = GetLocalController();
	if (!Controller)
	{
		return;
	}

	ActiveWidget = CreateWidget<UUserWidget>(Controller, WidgetClass);
	if (ActiveWidget)
	{
		ActiveWidget->AddToViewport(10);
	}
}

void UShooterMenuSubsystem::RemoveActiveWidget()
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
}

void UShooterMenuSubsystem::UpdateInputMode()
{
	APlayerController* Controller = GetLocalController();
	if (!Controller)
	{
		return;
	}

	const bool bMenuOpen = IsMenuOpen();

	if (bMenuOpen)
	{
		// El juego sigue corriendo detrás del menú de pausa: un FPS multijugador
		// no puede detenerse porque un jugador abra opciones.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);

		Controller->SetInputMode(InputMode);
		Controller->SetShowMouseCursor(true);
		return;
	}

	Controller->SetInputMode(FInputModeGameOnly());
	Controller->SetShowMouseCursor(false);
}

void UShooterMenuSubsystem::QuitGame()
{
	APlayerController* Controller = GetLocalController();

	// Quit cierra el proceso de forma ordenada: guarda y libera recursos,
	// a diferencia de matar la ventana (RF-2.4).
	UKismetSystemLibrary::QuitGame(GetGameInstance(), Controller,
		EQuitPreference::Quit, false);
}

void UShooterMenuSubsystem::LeaveMatch()
{
	CloseMenu();

	if (APlayerController* Controller = GetLocalController())
	{
		Controller->ClientTravel(TEXT("/Game/Maps/Lvl_Menu"), ETravelType::TRAVEL_Absolute);
	}
}

void UShooterMenuSubsystem::StartMatch(FName LevelName)
{
	CloseMenu();

	UGameplayStatics::OpenLevel(GetGameInstance(), LevelName);
}
