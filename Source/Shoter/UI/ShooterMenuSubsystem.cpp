#include "UI/ShooterMenuSubsystem.h"
#include "UI/ShooterSlateMenu.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
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

	APlayerController* Controller = GetLocalController();
	if (!Controller)
	{
		return;
	}

	// Carga síncrona: los menús son pequeños y el jugador espera respuesta
	// inmediata al pulsar.
	const TSoftClassPtr<UUserWidget>* WidgetClassPtr = ScreenWidgets.Find(Screen);
	UClass* WidgetClass = WidgetClassPtr ? WidgetClassPtr->LoadSynchronous() : nullptr;

	if (!WidgetClass)
	{
		// Sin Widget Blueprint asignado: menú integrado.
		UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
		if (!Viewport)
		{
			return;
		}

		TSharedRef<SShooterMenuScreen> Screenwidget = SNew(SShooterMenuScreen)
			.Menu(this)
			.Screen(Screen);
		SlateWidget = Screenwidget;
		Viewport->AddViewportWidgetContent(Screenwidget, 20);

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(Screenwidget);
		}
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

	if (SlateWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(SlateWidget.ToSharedRef());
		}
		SlateWidget.Reset();
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

	if (bMenuOpen != bLookInputIgnored)
	{
		bLookInputIgnored = bMenuOpen;
		if (bMenuOpen)
		{
			Controller->SetIgnoreLookInput(true);
		}
		else
		{
			Controller->ResetIgnoreLookInput();
		}
	}

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

	// Al cerrar el menú el foco vuelve al juego: sin esto la primera tecla
	// iría a un widget que ya no existe.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
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

void UShooterMenuSubsystem::StartMatchWithOptions(FName LevelName, const FString& Options)
{
	CloseMenu();

	UGameplayStatics::OpenLevel(GetGameInstance(), LevelName, true, Options);
}
