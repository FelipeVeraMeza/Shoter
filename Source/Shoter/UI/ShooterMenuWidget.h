#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ShooterMenuSubsystem.h"
#include "ShooterMenuWidget.generated.h"

class UShooterProgressionSubsystem;
class UShooterSettingsSubsystem;

/**
 * Base de las pantallas del menú.
 *
 * Da acceso a los subsistemas y a las acciones de navegación, de modo que el
 * Blueprint del widget solo tenga que enlazar botones a estas funciones: la
 * lógica sigue viviendo en C++ (RNF-9.2).
 */
UCLASS(Abstract)
class SHOTER_API UShooterMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Navegación (RF-2.1) ---

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void OpenScreen(EShooterMenuScreen Screen);

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void GoBack();

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void QuitGame();

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void LeaveMatch();

	/** Arranca la partida en el nivel indicado (RF-2.2). */
	UFUNCTION(BlueprintCallable, Category = "Menú")
	void StartMatch(FName LevelName);

	// --- Datos para mostrar ---

	UFUNCTION(BlueprintPure, Category = "Menú")
	UShooterProgressionSubsystem* GetProgression() const;

	UFUNCTION(BlueprintPure, Category = "Menú")
	UShooterSettingsSubsystem* GetSettings() const;

	UFUNCTION(BlueprintPure, Category = "Menú")
	UShooterMenuSubsystem* GetMenu() const;

	/** Nivel, XP actual y XP del nivel, en una sola llamada (RF-2.3). */
	UFUNCTION(BlueprintPure, Category = "Menú")
	void GetProgressSummary(int32& OutLevel, int32& OutCurrentXP, int32& OutRequiredXP) const;

	/** Texto listo para mostrar: "K/D 1.42 · Precisión 34 % · 27 partidas". */
	UFUNCTION(BlueprintPure, Category = "Menú")
	FText GetStatsSummary() const;

	UFUNCTION(BlueprintPure, Category = "Menú")
	FText GetPlayerName() const;

	UFUNCTION(BlueprintCallable, Category = "Menú")
	void SetPlayerName(const FText& NewName);
};
