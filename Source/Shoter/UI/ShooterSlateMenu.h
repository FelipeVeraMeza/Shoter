#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/ShooterMenuSubsystem.h"

class UShooterMenuSubsystem;
class UShooterSettingsSubsystem;
class APlayerController;

/**
 * Pantallas del menú construidas en Slate (RF-2).
 *
 * No dependen de ningún asset: el juego tiene menú principal, selección de
 * partida, opciones, controles, equipamiento y pausa desde la primera
 * compilación. Si se asigna un Widget Blueprint a una pantalla en la
 * configuración, ese tiene prioridad y esta no se usa.
 *
 * Toda la lógica pasa por los subsistemas (menú, ajustes, progresión): aquí
 * solo se compone lo que se ve.
 */
class SHOTER_API SShooterMenuScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShooterMenuScreen)
		: _Screen(EShooterMenuScreen::Main)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UShooterMenuSubsystem>, Menu)
		SLATE_ARGUMENT(EShooterMenuScreen, Screen)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<UShooterMenuSubsystem> Menu;
	EShooterMenuScreen Screen = EShooterMenuScreen::Main;

	/** Valores de los deslizadores mientras se arrastran; se guardan al soltar. */
	float PendingSensitivity = 1.f;
	float PendingVolume = 1.f;

	/** Modo elegido en la pantalla de partida, tal como lo lee el modo de juego. */
	FString SelectedMode = TEXT("equipos");

	/** Arma marcada al instante, antes de que el servidor confirme la elección. */
	TWeakObjectPtr<UClass> LocalPrimaryChoice;

	// --- Pantallas ---

	TSharedRef<SWidget> BuildMain();
	TSharedRef<SWidget> BuildPlay();
	TSharedRef<SWidget> BuildOptions();
	TSharedRef<SWidget> BuildControls();
	TSharedRef<SWidget> BuildLoadout();
	TSharedRef<SWidget> BuildProgression();
	TSharedRef<SWidget> BuildPause();

	// --- Piezas ---

	/** Columna izquierda con título y contenido, sobre fondo oscuro o translúcido. */
	TSharedRef<SWidget> MakeFrame(const FText& Title, const FText& Subtitle, TSharedRef<SWidget> Content);

	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClicked, bool bPrimary = false);

	/** Botón que se queda marcado si IsSelected devuelve true. */
	TSharedRef<SWidget> MakeToggleButton(const FText& Label, TFunction<bool()> IsSelected, TFunction<void()> OnClicked);

	TSharedRef<SWidget> MakeSectionLabel(const FText& Label);
	TSharedRef<SWidget> MakeCheckRow(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set);
	TSharedRef<SWidget> MakeKeyRow(const FText& Action, const FText& Keys);

	// --- Accesos ---

	UShooterMenuSubsystem* GetMenu() const { return Menu.Get(); }
	UShooterSettingsSubsystem* GetSettings() const;
	APlayerController* GetPlayerController() const;
	bool IsInMatch() const;
};
