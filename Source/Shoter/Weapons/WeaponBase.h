#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "Weapons/WeaponAttachmentDataAsset.h"
#include "Core/RPCRateLimiter.h"
#include "WeaponBase.generated.h"

class UWeaponDataAsset;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class AShooterCharacter;
class UPhysicalMaterial;

/**
 * Geometría del arma deducida de su malla, para colocarla en primera persona.
 *
 * Las mallas de la plantilla no traen un socket de mira ni un eje documentado:
 * el eje largo de la caja es el cañón (orientado hacia el socket Muzzle) y el
 * borde superior, la línea de mira. Todo en el espacio de la malla.
 */
struct SHOTER_API FWeaponViewFrame
{
	bool bValid = false;

	/** Ejes de la malla: adelante (cañón), arriba y derecha. */
	FVector Forward = FVector::ForwardVector;
	FVector Up = FVector::UpVector;
	FVector Right = FVector::RightVector;

	/** Rotación que lleva la malla al marco de la vista (X adelante, Z arriba). */
	FQuat MeshToView = FQuat::Identity;

	/** Punto por el que se apunta: centro de la ventana del visor o línea de miras. */
	FVector SightLocation = FVector::ZeroVector;

	FVector MuzzleLocation = FVector::ZeroVector;

	/** Ventana de expulsión de casquillos, a la derecha del cajón. */
	FVector EjectLocation = FVector::ZeroVector;

	/** Borde superior del arma sobre el que se monta el visor. */
	FVector MountLocation = FVector::ZeroVector;
};

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

	/** Solo servidor. Reproduce un impacto en todos los clientes. bFlesh: la bala dio en un cuerpo. */
	void BroadcastImpact(const FVector& Location, const FVector& Normal, const UPhysicalMaterial* SurfaceMaterial,
		bool bFlesh = false);

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

	/** Malla que ven los demás, en la mano del cuerpo. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arma")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/**
	 * Malla que ve el propio jugador, en la mano de sus brazos de primera
	 * persona. Separada de la otra porque cada una cuelga de un esqueleto
	 * distinto y se dibuja con el campo de visión de primera persona.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arma")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Geometría para colocar el arma ante la cámara. */
	const FWeaponViewFrame& GetViewFrame() const { return ViewFrame; }

	/** Duración de la recarga que empieza ahora (táctica o en vacío). */
	float GetCurrentReloadDuration() const;

	/** Oculta la malla de primera persona y su visor (al mirar por un visor de aumentos). */
	void SetFirstPersonHidden(bool bShouldHide);

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

	// --- Visor holográfico (solo primera persona) ---
	//
	// Se construye con cajas: base, dos montantes y la capucha forman la
	// ventana, y un plano con la retícula hace de cristal. Un cubo macizo
	// taparía la vista al apuntar.

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SightParts;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SightGlass;

	// --- Fogonazo ---

	/** Planos cruzados del fogonazo: los que ve el tirador y los que ven los demás. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FirstPersonFlash;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ThirdPersonFlash;

	/** La luz del fogonazo ilumina el entorno para todos. */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> FlashLight;

	FWeaponViewFrame ViewFrame;

	/** Piezas del visor en uso: las demás quedan ocultas. */
	int32 ActiveSightParts = 0;
	bool bSightGlassActive = false;
	bool bFirstPersonHidden = false;

	/** Segundos que le quedan visible al fogonazo. */
	float FlashTimeRemaining = 0.f;

	/** Disparos desde la última trazadora, contados en cada máquina. */
	int32 ShotsSinceTracer = 0;

	/** Deduce ViewFrame de la malla y coloca visor y fogonazo. */
	void BuildViewFrame();
	void LayoutSight();
	void LayoutFlash(const TArray<TObjectPtr<UStaticMeshComponent>>& Planes, USkeletalMeshComponent* Parent);
	void ShowMuzzleFlash();
	void UpdateMuzzleFlash(float DeltaSeconds);

	/** Trazadora y casquillo de un disparo en la dirección indicada. */
	void SpawnShotCosmetics(const FVector& Direction);

	/** El tirador local ve su arma de primera persona; los demás, la del cuerpo. */
	bool IsViewedInFirstPerson() const;
	USkeletalMeshComponent* GetVisibleMesh() const;

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
	void PlayLocalFireEffects(const FVector& Direction);
	void SpawnImpactEffect(const FVector& Location, const FVector& Normal, const UPhysicalMaterial* SurfaceMaterial,
		bool bFlesh) const;

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

	/** Direction: hacia dónde salió la bala, para que los demás vean la trazadora. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireEffects(const FVector_NetQuantizeNormal& Direction);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactEffect(const FVector_NetQuantize& ImpactLocation,
		const FVector_NetQuantizeNormal& ImpactNormal, const UPhysicalMaterial* SurfaceMaterial, bool bFlesh);

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	void FinishReload();
	void ContinueBurst();

	float GetRewindTimeForOwner() const;
};
