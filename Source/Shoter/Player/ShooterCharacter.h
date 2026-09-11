#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/RPCRateLimiter.h"
#include "ShooterCharacter.generated.h"

class UCameraComponent;
class UHealthComponent;
class UInventoryComponent;
class ULagCompensationComponent;
class AWeaponBase;
class UInputAction;
class UInputMappingContext;
class AShooterGrenade;
struct FInputActionValue;

UCLASS()
class SHOTER_API AShooterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AShooterCharacter();

	UFUNCTION(BlueprintPure, Category = "Personaje")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Personaje")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Personaje")
	UInventoryComponent* GetInventory() const { return Inventory; }

	ULagCompensationComponent* GetLagCompensation() const { return LagCompensation; }

	UFUNCTION(BlueprintPure, Category = "Personaje")
	AWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Personaje")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Personaje")
	bool IsDead() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	float WalkSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	float SprintSpeed = 650.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	float CrouchSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cámara")
	float DefaultFOV = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cámara")
	float AimFOV = 65.f;

	/** Velocidad inicial del deslizamiento (RF-4.6). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento", meta = (ClampMin = "0.0"))
	float SlideSpeed = 950.f;

	/** Duración máxima del deslizamiento, en segundos. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento", meta = (ClampMin = "0.1"))
	float SlideDuration = 0.75f;

	/** Segundos antes de poder volver a deslizarse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento", meta = (ClampMin = "0.0"))
	float SlideCooldown = 1.2f;

	UFUNCTION(BlueprintPure, Category = "Movimiento")
	bool IsSliding() const { return bIsSliding; }

	/**
	 * Contextos y acciones de Enhanced Input. Se asignan en el Blueprint del
	 * personaje: las teclas concretas viven en los assets, no en C++ (RF-11.2).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SwitchWeaponAction;

	/** Abre el menú de pausa (RF-3.4). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GrenadeAction;

	/** Repone drones, munición y salud en la zona de entrenamiento (RF6). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ResetRangeAction;

	/** Granada que lanza el personaje (RF-5.11). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granadas")
	TSubclassOf<AShooterGrenade> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granadas", meta = (ClampMin = "0"))
	int32 MaxGrenades = 2;

	/** Segundos entre lanzamientos, para que no se encadenen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granadas", meta = (ClampMin = "0.0"))
	float GrenadeCooldown = 1.f;

	UFUNCTION(BlueprintPure, Category = "Granadas")
	int32 GetGrenadeCount() const { return GrenadeCount; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PawnClientRestart() override;

	/** Registra los mapping contexts en el subsistema del jugador local. */
	void AddInputMappingContexts();

private:
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<ULagCompensationComponent> LagCompensation;

	UPROPERTY(Replicated)
	bool bIsAiming = false;

	UPROPERTY(Replicated)
	bool bIsSprinting = false;

	UPROPERTY(Replicated)
	bool bIsSliding = false;

	float SlideTimeRemaining = 0.f;
	float LastSlideEndTime = -1000.f;

	UPROPERTY(Replicated)
	int32 GrenadeCount = 0;

	float LastGrenadeTime = -1000.f;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void StartFire();
	void StopFire();
	void Reload();
	void SwitchWeapon();

	void StartAiming();
	void StopAiming();
	void StartSprinting();
	void StopSprinting();
	void ToggleCrouch();
	void TogglePauseMenu();

	/** Agacharse mientras se corre desliza en vez de frenar (RF-4.6). */
	void StartSlide();
	void StopSlide();
	void UpdateSlide(float DeltaSeconds);

	/** Lanza una granada en la dirección de la mira (RF-5.11). */
	void ThrowGrenade();

	/** Pide al gestor de la zona que reinicie el escenario (RF6). */
	void RequestRangeReset();

	UFUNCTION(Server, Reliable)
	void Server_ThrowGrenade(const FVector_NetQuantize& Origin,
		const FVector_NetQuantizeNormal& Direction);

	void UpdateMaxWalkSpeed();

	/**
	 * El estado de apuntar y correr se predice en el cliente para que responda al
	 * instante, y se confirma en el servidor. Un componente de movimiento propio
	 * con FSavedMove eliminaría la corrección residual (pendiente para F2).
	 */
	UFUNCTION(Server, Reliable)
	void Server_SetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void Server_SetSliding(bool bNewSliding);

	/**
	 * Limita las RPC de estado (apuntar, correr, deslizarse) y la granada
	 * (RNF-3.5).
	 *
	 * Comparten limitador porque el ataque que importa es la suma: alternar
	 * entre ellas eludiría un contador por RPC. El de estado es generoso
	 * porque alternar apuntado es legítimo y frecuente; el de granada, estricto,
	 * porque su cadencia real es de una cada pocos segundos.
	 */
	FRPCRateLimiter StateRateLimiter{ 40, 1.f };
	FRPCRateLimiter GrenadeRateLimiter{ 5, 1.f };

	UFUNCTION()
	void HandleDeath(AActor* Victim, AController* Killer);

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta, AController* InstigatedBy);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeath();
};
