#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShooterCharacter.generated.h"

class UCameraComponent;
class UHealthComponent;
class UInventoryComponent;
class ULagCompensationComponent;
class AWeaponBase;
class UInputAction;
class UInputMappingContext;
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

	UFUNCTION()
	void HandleDeath(AActor* Victim, AController* Killer);

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta, AController* InstigatedBy);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeath();
};
