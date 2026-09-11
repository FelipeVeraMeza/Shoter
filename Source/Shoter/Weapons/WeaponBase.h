#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "WeaponBase.generated.h"

class UWeaponDataAsset;
class USkeletalMeshComponent;
class AShooterCharacter;

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

private:
	UPROPERTY()
	TObjectPtr<AShooterCharacter> OwningCharacter;

	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	UPROPERTY(Replicated)
	int32 ReserveAmmo = 0;

	UPROPERTY(Replicated)
	bool bIsReloading = false;

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
	void SpawnImpactEffect(const FVector& Location) const;

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
	void Multicast_PlayImpactEffect(const FVector_NetQuantize& ImpactLocation);

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	void FinishReload();
	void ContinueBurst();

	float GetRewindTimeForOwner() const;
};
