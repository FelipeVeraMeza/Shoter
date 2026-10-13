#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "Weapons/WeaponAttachmentDataAsset.h"
#include "Core/RPCRateLimiter.h"
#include "WeaponBase.generated.h"

class UWeaponDataAsset;
class USkeletalMeshComponent;
class AShooterCharacter;
class UPhysicalMaterial;

/**
 * Lo necesario para calcular el daño de un disparo, copiado del arma.
 *
 * Una bala simulada puede seguir en el aire cuando el arma ya no existe (el
 * tirador murió): por eso no guarda el arma, sino su modelo de daño.
 */
struct SHOTER_API FShooterDamageModel
{
	TWeakObjectPtr<const UWeaponDataAsset> Data;
	float DamageMultiplier = 1.f;
	float RangeMultiplier = 1.f;
	float Range = 15000.f;
	float VelocityDecayDistance = 90000.f;

	/** Daño a esa distancia y en esa zona, con caída y accesorios aplicados. */
	float GetDamage(float Distance, EShooterHitZone Zone) const;
};

/**
 * Arma como actor replicado. El cliente predice los efectos cosméticos y el HUD,
 * pero el servidor es el único que decide impactos, daño y munición real (RNF-3.2).
 */
UCLASS()
class SHOTER_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arma")
	TObjectPtr<UWeaponDataAsset> WeaponData;

	/**
	 * Accesorios equipados (RF-5.10).
	 *
	 * Replicado porque el servidor aplica los modificadores al calcular daño y
	 * dispersión: si solo viviera en el cliente, este podría anunciar cualquier
	 * combinación (RNF-3.2).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Attachments, EditDefaultsOnly, BlueprintReadOnly, Category = "Arma")
	FWeaponAttachmentSet Attachments;

	/** Solo servidor. Cambia los accesorios y reajusta el cargador. */
	void SetAttachments(const FWeaponAttachmentSet& NewAttachments);

	// --- Valores del arma con los accesorios ya aplicados (RF-5.10) ---
	//
	// El resto del código pregunta por estos en vez de leer el DataAsset
	// directamente, así añadir un modificador no obliga a tocar cada uso.

	UFUNCTION(BlueprintPure, Category = "Arma")
	int32 GetEffectiveMagazineSize() const;

	UFUNCTION(BlueprintPure, Category = "Arma")
	float GetEffectiveReloadTime() const;

	UFUNCTION(BlueprintPure, Category = "Arma")
	float GetEffectiveADSTime() const;

	/** Daño a esa distancia, con la caída y los accesorios aplicados. */
	UFUNCTION(BlueprintPure, Category = "Arma")
	float GetEffectiveDamageAtDistance(float Distance) const;

	/** Copia del modelo de daño actual, para las balas en vuelo. */
	FShooterDamageModel MakeDamageSnapshot() const;

	/** Solo servidor. Reproduce un impacto en todos los clientes. */
	void BroadcastImpact(const FVector& Location, const FVector& Normal, const UPhysicalMaterial* SurfaceMaterial);

	// --- Selector de fuego ---

	UFUNCTION(BlueprintPure, Category = "Arma")
	EWeaponFireMode GetCurrentFireMode() const { return CurrentFireMode; }

	/** Pasa a la siguiente posición del selector que admita el arma. */
	void CycleFireMode();

	// --- Cargadores ---

	/** Cargadores de reserva, con las balas que le quedan a cada uno. */
	UFUNCTION(BlueprintPure, Category = "Arma")
	const TArray<int32>& GetSpareMagazines() const { return SpareMagazines; }

	/** Segundos desde que el jugador comprobó el cargador (para el HUD realista). */
	float GetTimeSinceAmmoCheck() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arma")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Petición de disparo desde el jugador. Válida en cliente y servidor. */
	void TryStartFire();
	void StopFire();

	/** Petición de recarga. */
	void TryReload();

	UFUNCTION(BlueprintPure, Category = "Arma")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Arma")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "Arma")
	bool IsReloading() const { return bIsReloading; }

	/** Dispersión actual en grados, usada por el HUD para abrir o cerrar la mira. */
	UFUNCTION(BlueprintPure, Category = "Arma")
	float GetCurrentSpreadDegrees() const;

	void SetOwningCharacter(AShooterCharacter* InCharacter);

	/** Cargador lleno y reserva al máximo, sin esperar la recarga (RF6). */
	UFUNCTION(BlueprintCallable, Category = "Arma")
	void ResetAmmo();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Un cargador ampliado debe notarse sin esperar a la siguiente recarga. */
	UFUNCTION()
	void OnRep_Attachments();

private:
	UPROPERTY()
	TObjectPtr<AShooterCharacter> OwningCharacter;

	/** Balas en el arma: cargador más la de la recámara. */
	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	/** Suma de los cargadores de reserva, para el HUD y los bots. */
	UPROPERTY(Replicated)
	int32 ReserveAmmo = 0;

	/**
	 * Cargadores de reserva. Recargar no rellena el cargador de un fondo
	 * común: se cambia por el más lleno, y el que se quita vuelve a la
	 * bolsa con lo que le quedaba. Recargar tras dos disparos tiene un coste.
	 */
	UPROPERTY(Replicated)
	TArray<int32> SpareMagazines;

	UPROPERTY(Replicated)
	EWeaponFireMode CurrentFireMode = EWeaponFireMode::Automatic;

	float LastAmmoCheckTime = -1000.f;

	/** Rellena los cargadores de reserva según MaxReserveAmmo. */
	void RefillMagazines();

	void UpdateReserveAmmo();

	/** Índice del cargador de reserva más lleno, o INDEX_NONE. */
	int32 FindFullestMagazine() const;

	UFUNCTION(Server, Reliable)
	void Server_SetFireMode(EWeaponFireMode NewMode);

	UPROPERTY(Replicated)
	bool bIsReloading = false;

	/**
	 * Frena a un cliente que invoque Server_Fire en bucle (RNF-3.5).
	 *
	 * El margen sobre la cadencia real es amplio a propósito: con pérdida de
	 * paquetes las RPC fiables llegan agrupadas, y un límite ajustado
	 * penalizaría a un jugador con mala conexión en vez de a un tramposo.
	 */
	FRPCRateLimiter FireRateLimiter{ 30, 1.f };

	bool bWantsToFire = false;
	float LastFireTime = -1000.f;
	float AccumulatedSpread = 0.f;
	int32 ShotsInCurrentBurst = 0;
	int32 ConsecutiveShots = 0;
	FTimerHandle ReloadTimerHandle;
	FTimerHandle BurstTimerHandle;

	bool CanFire() const;
	void FireOnce();
	void ApplyRecoilToOwner();
	void PlayLocalFireEffects();
	void SpawnImpactEffect(const FVector& Location, const FVector& Normal, const UPhysicalMaterial* SurfaceMaterial) const;

	/** Dirección corregida por la puesta a cero de las miras. */
	FVector ApplyZeroing(const FVector& Direction) const;

	/** Dirección del disparo tras aplicar la dispersión actual. */
	FVector CalculateShotDirection(const FVector& AimDirection) const;

	/**
	 * Resuelve el disparo en el servidor. No recibe la marca de tiempo del cliente
	 * a propósito: un cliente modificado podría falsearla para rebobinar a su gusto.
	 * El servidor deduce el instante a rebobinar desde la latencia medida.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Fire(const FVector_NetQuantize& TraceStart, const FVector_NetQuantizeNormal& ShotDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireEffects();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactEffect(const FVector_NetQuantize& ImpactLocation,
		const FVector_NetQuantizeNormal& ImpactNormal, const UPhysicalMaterial* SurfaceMaterial);

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	void FinishReload();
	void ContinueBurst();

	float GetRewindTimeForOwner() const;
};
