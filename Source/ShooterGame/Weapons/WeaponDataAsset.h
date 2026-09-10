#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShooterTypes.h"
#include "WeaponDataAsset.generated.h"

class USkeletalMesh;
class USoundBase;
class UNiagaraSystem;
class UCurveFloat;

/**
 * Configuración completa de un arma. Ningún valor de balance vive en C++:
 * ajustar un arma es editar este asset, sin recompilar (RNF-9.3).
 */
UCLASS(BlueprintType)
class SHOOTERGAME_API UWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad")
	EWeaponFireMode FireMode = EWeaponFireMode::Automatic;

	/** Disparos por ráfaga. Solo se usa con FireMode == Burst. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad", meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 1.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0"))
	float RoundsPerMinute = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 180;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float ReloadTime = 2.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float ADSTime = 0.25f;

	/** Alcance máximo del trazado, en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float Range = 15000.f;

	/** Distancia sin caída de daño, en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float EffectiveRange = 3000.f;

	/**
	 * Multiplicador de daño según la distancia normalizada entre EffectiveRange y Range.
	 * Sin curva asignada, el daño no decae.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
	TObjectPtr<UCurveFloat> DamageFalloff;

	/** Dispersión en grados disparando desde la cadera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Precisión", meta = (ClampMin = "0.0"))
	float SpreadHip = 3.5f;

	/** Dispersión en grados apuntando con la mira. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Precisión", meta = (ClampMin = "0.0"))
	float SpreadADS = 0.4f;

	/** Dispersión añadida por disparo sostenido, hasta SpreadMax. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Precisión", meta = (ClampMin = "0.0"))
	float SpreadPerShot = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Precisión", meta = (ClampMin = "0.0"))
	float SpreadMax = 8.f;

	/** Grados por segundo que tarda la dispersión en volver a su valor base. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Precisión", meta = (ClampMin = "0.0"))
	float SpreadRecoveryRate = 6.f;

	/** Retroceso vertical en grados por disparo, indexado por número de disparo en la ráfaga. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retroceso")
	TObjectPtr<UCurveFloat> RecoilVertical;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retroceso")
	TObjectPtr<UCurveFloat> RecoilHorizontal;

	/** Valor por defecto si no hay curva de retroceso vertical asignada. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retroceso", meta = (ClampMin = "0.0"))
	float BaseRecoilVertical = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retroceso")
	float BaseRecoilHorizontal = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UNiagaraSystem> MuzzleFlash;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> EmptySound;

	UFUNCTION(BlueprintPure, Category = "Balance")
	float GetSecondsBetweenShots() const;

	/** Daño final tras aplicar la caída por distancia. */
	UFUNCTION(BlueprintPure, Category = "Balance")
	float GetDamageAtDistance(float Distance) const;
};
