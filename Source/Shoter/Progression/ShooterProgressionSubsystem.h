#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShooterProgressionSubsystem.generated.h"

class UShooterSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExperienceGained, int32, Amount, int32, NewTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUp, int32, NewLevel);

/**
 * Progresión y ajustes del jugador (RF-9, RF-11).
 *
 * Vive en el GameInstance, así que sobrevive a los cambios de nivel. Guarda en
 * disco; cuando exista el backend (RNF-3.4) esta clase pasa a ser su caché
 * local sin que cambie el resto del código.
 */
UCLASS()
class SHOTER_API UShooterProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Progresión")
	FOnExperienceGained OnExperienceGained;

	UPROPERTY(BlueprintAssignable, Category = "Progresión")
	FOnLevelUp OnLevelUp;

	/** XP por eliminación, asistencia y por terminar la partida (RF-9.1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 ExperiencePerKill = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 ExperiencePerAssist = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 ExperiencePerMatch = 250;

	/** XP necesaria para pasar del nivel 1 al 2. Cada nivel sube un 15 %. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1"))
	int32 BaseExperiencePerLevel = 1000;

	UFUNCTION(BlueprintPure, Category = "Progresión")
	int32 GetLevel() const;

	UFUNCTION(BlueprintPure, Category = "Progresión")
	int32 GetExperience() const;

	/** XP dentro del nivel actual y la necesaria para el siguiente (RF-2.3). */
	UFUNCTION(BlueprintPure, Category = "Progresión")
	void GetLevelProgress(int32& OutCurrent, int32& OutRequired) const;

	UFUNCTION(BlueprintPure, Category = "Progresión")
	int32 GetExperienceForLevel(int32 Level) const;

	/** Suma XP y sube de nivel si corresponde (RF-9.1, RF-9.2). */
	UFUNCTION(BlueprintCallable, Category = "Progresión")
	void AddExperience(int32 Amount);

	/** Registra el resultado de una partida y guarda (RF-9.6). */
	UFUNCTION(BlueprintCallable, Category = "Progresión")
	void RecordMatchResult(int32 Kills, int32 Deaths, int32 Assists);

	UFUNCTION(BlueprintCallable, Category = "Estadísticas")
	void RecordShot(bool bHit);

	UFUNCTION(BlueprintPure, Category = "Estadísticas")
	float GetAccuracy() const;

	UFUNCTION(BlueprintPure, Category = "Estadísticas")
	float GetKillDeathRatio() const;

	/** Un arma se desbloquea al alcanzar su nivel (RF-9.3). */
	UFUNCTION(BlueprintPure, Category = "Progresión")
	bool IsWeaponUnlocked(FName WeaponId) const;

	UFUNCTION(BlueprintCallable, Category = "Progresión")
	void UnlockWeapon(FName WeaponId);

	UFUNCTION(BlueprintPure, Category = "Perfil")
	FString GetPlayerName() const;

	UFUNCTION(BlueprintCallable, Category = "Perfil")
	void SetPlayerName(const FString& NewName);

	UFUNCTION(BlueprintPure, Category = "Ajustes")
	UShooterSaveGame* GetSaveData() const { return SaveData; }

	/** Escribe en disco. Se llama solo al terminar partida y al cambiar ajustes. */
	UFUNCTION(BlueprintCallable, Category = "Persistencia")
	void Save();

	UFUNCTION(BlueprintCallable, Category = "Persistencia")
	void Load();

private:
	UPROPERTY()
	TObjectPtr<UShooterSaveGame> SaveData;

	static const FString SlotName;

	/** Aplica los desbloqueos que correspondan al nivel actual. */
	void RefreshUnlocks();
};
