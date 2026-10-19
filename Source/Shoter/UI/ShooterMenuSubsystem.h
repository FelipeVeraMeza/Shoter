#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShooterMenuSubsystem.generated.h"

class UUserWidget;
class SWidget;

UENUM(BlueprintType)
enum class EShooterMenuScreen : uint8
{
	None		UMETA(DisplayName = "Ninguna"),
	Main		UMETA(DisplayName = "Menú principal"),
	Play		UMETA(DisplayName = "Jugar"),
	Arsenal		UMETA(DisplayName = "Arsenal"),
	Progression	UMETA(DisplayName = "Progresión"),
	Options		UMETA(DisplayName = "Opciones"),
	Pause		UMETA(DisplayName = "Pausa"),
	Loadout		UMETA(DisplayName = "Equipamiento"),
	Controls	UMETA(DisplayName = "Controles")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuScreenChanged, EShooterMenuScreen, NewScreen);

/**
 * Navegación entre pantallas del menú (RF-2).
 *
 * El C++ decide qué pantalla toca y gestiona la pila de navegación; los
 * widgets solo componen y muestran (RNF-9.2). Las clases de widget se asignan
 * en Project Settings → Shoter Menu.
 *
 * Una pantalla sin widget asignado se dibuja con el menú integrado en Slate
 * (SShooterMenuScreen): el juego tiene menús completos sin depender de que
 * alguien maquete los Widget Blueprints en el editor.
 */
UCLASS(Config = Game)
class SHOTER_API UShooterMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Menú")
	FOnMenuScreenChanged OnScreenChanged;

	/** Widget de cada pantalla. Se asignan desde la configuración del proyecto. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Menú")
	TMap<EShooterMenuScreen, TSoftClassPtr<UUserWidget>> ScreenWidgets;

	UFUNCTION(BlueprintPure, Category = "Menú")
	EShooterMenuScreen GetCurrentScreen() const { return CurrentScreen; }

	UFUNCTION(BlueprintPure, Category = "Menú")
	bool IsMenuOpen() const { return CurrentScreen != EShooterMenuScreen::None; }

	/** Abre una pantalla, apilando la anterior para poder volver. */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void OpenScreen(EShooterMenuScreen Screen);

	/** Vuelve a la pantalla anterior; si no hay, cierra el menú. */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void GoBack();

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void CloseMenu();

	/** Abre o cierra la pausa en partida (RF-3.4). */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void TogglePause();

	/** Sale del juego de forma ordenada (RF-2.4). */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void QuitGame();

	/** Abandona la partida y vuelve al menú principal (RF-3.4). */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void LeaveMatch();

	/** Carga un nivel y arranca la partida. */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void StartMatch(FName LevelName);

	/**
	 * Carga un nivel con opciones de partida en la URL ("modo=dominacion",
	 * "jugadores=8"): las lee el modo de juego al arrancar.
	 */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void StartMatchWithOptions(FName LevelName, const FString& Options);

	/** Jugadores por partida elegidos en el menú; los huecos se rellenan con bots. */
	UPROPERTY(BlueprintReadWrite, Category = "Menú")
	int32 MatchPlayerCount = 8;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveWidget;

	/** Pantalla integrada en Slate, cuando no hay widget asignado. */
	TSharedPtr<SWidget> SlateWidget;

	/** La vista no gira con el ratón mientras hay un menú abierto. */
	bool bLookInputIgnored = false;

	EShooterMenuScreen CurrentScreen = EShooterMenuScreen::None;

	/** Pantallas por las que se ha pasado, para que GoBack tenga sentido. */
	TArray<EShooterMenuScreen> NavigationStack;

	void ShowWidgetForScreen(EShooterMenuScreen Screen);
	void RemoveActiveWidget();

	/** Cursor visible y entrada al menú mientras haya una pantalla abierta. */
	void UpdateInputMode();

	APlayerController* GetLocalController() const;
};
