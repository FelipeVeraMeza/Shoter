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
class SHOTER_API UWeaponDataAsset : public UPrimaryDataAsset
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

	/**
	 * Posiciones del selector de fuego, en el orden en que se recorren.
	 *
	 * Vacío equivale a solo FireMode. Un fusil real tiene seguro, tiro a tiro
	 * y automático; el seguro se omite porque en partida solo estorbaría.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad")
	TArray<EWeaponFireMode> AvailableFireModes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0"))
	float HeadshotMultiplier = 1.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0"))
	float RoundsPerMinute = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1"))
	int32 MagazineSize = 24;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 180;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float ReloadTime = 2.1f;

	/**
	 * Recarga con el arma vacía: hay que montar el cerrojo además de cambiar
	 * el cargador. Cero usa ReloadTime * 1.3.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0"))
	float EmptyReloadTime = 0.f;

	/**
	 * Cerrojo cerrado: una recarga táctica conserva la bala de la recámara,
	 * así que el arma queda con cargador + 1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
	bool bClosedBolt = true;

	// --- Zonas de impacto ---
	//
	// El torso es la referencia (x1). Los multiplicadores viven en el arma y
	// no en el personaje porque un calibre pesado mutila donde uno ligero no.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance|Zonas", meta = (ClampMin = "0.0"))
	float StomachMultiplier = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance|Zonas", meta = (ClampMin = "0.0"))
	float LimbMultiplier = 0.6f;

	// --- Balística (bala simulada, no trazado instantáneo) ---

	/**
	 * Si está activo, la bala viaja con velocidad finita, cae por gravedad y
	 * puede atravesar materiales. Si no, el disparo es instantáneo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balística")
	bool bUseBallistics = true;

	/** Velocidad en boca, en cm/s. 5,56 mm ≈ 92 000; 9 mm ≈ 37 000; 7,62x51 ≈ 83 000. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balística", meta = (ClampMin = "1000.0"))
	float MuzzleVelocity = 88000.f;

	/**
	 * Distancia en la que la bala pierde ~63 % de su velocidad (decaimiento
	 * exponencial). Modelo simple del rozamiento: más alto = bala más tensa.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balística", meta = (ClampMin = "1000.0"))
	float VelocityDecayDistance = 90000.f;

	/**
	 * Distancia de puesta a cero de las miras, en cm. A esa distancia la bala
	 * cruza la línea de mira: más cerca impacta algo alto, más lejos, bajo.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balística", meta = (ClampMin = "0.0"))
	float ZeroingDistance = 10000.f;

	/**
	 * Capacidad de atravesar materiales, en cm de "material de resistencia 1"
	 * (madera maciza). Una pared de 20 cm de hormigón (resistencia 4) cuesta 80.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balística", meta = (ClampMin = "0.0"))
	float PenetrationPower = 30.f;

	// --- Manejo ---

	/** Campo de visión al apuntar. Cero usa el del personaje. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Manejo", meta = (ClampMin = "0.0", ClampMax = "120.0"))
	float AimFOV = 0.f;

	/**
	 * Cuánto se mueve la mira al apuntar (respiración y pulso). Un arma
	 * pesada y larga oscila más que una pistola.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Manejo", meta = (ClampMin = "0.0"))
	float SwayMultiplier = 1.f;

	/**
	 * Fracción del retroceso que el tirador recupera solo al dejar de
	 * disparar. Por debajo de 1: una ráfaga larga sube la mira de verdad.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retroceso", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RecoilRecoveryFraction = 0.7f;

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

	/** Multiplicador de daño de una zona del cuerpo. */
	UFUNCTION(BlueprintPure, Category = "Balance")
	float GetZoneMultiplier(EShooterHitZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "Balance")
	float GetEmptyReloadTime() const;

	/** Modos del selector, con FireMode como único si no hay lista. */
	TArray<EWeaponFireMode> GetFireModes() const;
};
