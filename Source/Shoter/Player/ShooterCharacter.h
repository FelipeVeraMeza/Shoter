#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/RPCRateLimiter.h"
#include "ShooterTypes.h"
#include "ShooterCharacter.generated.h"

class UCameraComponent;
class USkeletalMesh;
class UShooterFirstPersonComponent;
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

	/** Brazos que solo ve el propio jugador. */
	USkeletalMeshComponent* GetFirstPersonArms() const { return FirstPersonArms; }

	UShooterFirstPersonComponent* GetFirstPersonView() const { return FirstPersonView; }

	/**
	 * Hacia dónde apunta el personaje: la rotación de su controlador.
	 *
	 * No se usa la de la cámara porque solo gira la del jugador que mira a
	 * través de ella; la de un bot se queda horizontal.
	 */
	FRotator GetAimRotation() const;

	/** Para la IA: apuntar o dejar de apuntar con la mira. */
	void SetWantsToAim(bool bWantsToAim);

	/** Para la IA: vendarse si hay heridas y vendas. */
	void RequestBandage();

	UFUNCTION(BlueprintPure, Category = "Personaje")
	AWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Personaje")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Personaje")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Personaje")
	bool IsSprinting() const { return bIsSprinting; }

	/**
	 * El arma está en posición de tiro: ni corriendo, ni recién salido del
	 * sprint, ni vendándose.
	 */
	UFUNCTION(BlueprintPure, Category = "Personaje")
	bool IsWeaponReady() const;

	/** Origen del disparo: la vista, desplazada si el personaje se asoma. */
	FVector GetShotOrigin() const;

	/** Desplazamiento de la cabeza por la inclinación, en espacio de mundo. */
	FVector GetLeanWorldOffset() const;

	/**
	 * Retroceso de un disparo, en grados. Se reparte en unos milisegundos y
	 * RecoveryFraction de él se recupera al dejar de disparar.
	 */
	void AddRecoil(float PitchDegrees, float YawDegrees, float RecoveryFraction);

	// --- Movimiento ---
	//
	// Velocidades de persona equipada, no de atleta: 3,6 m/s andando con el
	// arma en posición y casi 6 m/s a la carrera.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	float WalkSpeed = 360.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	float SprintSpeed = 590.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	float CrouchSpeed = 170.f;

	/** Fracción de la velocidad conservada al apuntar con la mira. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float AimSpeedMultiplier = 0.55f;

	/** Segundos que tarda el arma en volver a la posición de tiro tras correr. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento", meta = (ClampMin = "0.0"))
	float SprintOutTime = 0.25f;

	/**
	 * Agacharse a la carrera desliza. Desactivado por defecto: es un recurso
	 * arcade que no encaja en un combate realista.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movimiento")
	bool bAllowSlide = false;

	// --- Resistencia ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resistencia", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.f;

	/** Consumo por segundo de carrera. Con 100 de máximo, unos 8 s de sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resistencia", meta = (ClampMin = "0.0"))
	float SprintStaminaCost = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resistencia", meta = (ClampMin = "0.0"))
	float JumpStaminaCost = 18.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resistencia", meta = (ClampMin = "0.0"))
	float StaminaRegenRate = 14.f;

	/** Segundos de descanso antes de recuperar resistencia. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resistencia", meta = (ClampMin = "0.0"))
	float StaminaRegenDelay = 1.2f;

	/** Resistencia mínima para empezar a correr: evita el sprint a tirones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resistencia", meta = (ClampMin = "0.0"))
	float MinStaminaToSprint = 15.f;

	UFUNCTION(BlueprintPure, Category = "Resistencia")
	float GetStaminaPercent() const { return Stamina / FMath::Max(MaxStamina, 1.f); }

	// --- Puntería ---

	/**
	 * Amplitud del balanceo de la mira al apuntar, en grados. Crece con el
	 * cansancio y el movimiento; se reduce agachado y conteniendo la respiración.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puntería", meta = (ClampMin = "0.0"))
	float AimSwayAmplitude = 0.35f;

	/** Ciclos de balanceo por segundo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puntería", meta = (ClampMin = "0.01"))
	float AimSwayFrequency = 0.45f;

	/** Segundos que se puede contener la respiración (correr mientras se apunta). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puntería", meta = (ClampMin = "0.0"))
	float MaxBreathHold = 5.f;

	/** Velocidad a la que el retroceso sube el arma, en "fracción por segundo". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puntería", meta = (ClampMin = "1.0"))
	float RecoilKickSpeed = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Puntería", meta = (ClampMin = "0.1"))
	float RecoilRecoverySpeed = 7.f;

	UFUNCTION(BlueprintPure, Category = "Puntería")
	bool IsHoldingBreath() const { return bHoldingBreath; }

	UFUNCTION(BlueprintPure, Category = "Puntería")
	float GetBreathPercent() const { return BreathRemaining / FMath::Max(MaxBreathHold, 0.01f); }

	// --- Inclinación ---

	/** Desplazamiento lateral de la cabeza al asomarse, en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inclinación", meta = (ClampMin = "0.0"))
	float LeanDistance = 38.f;

	/** Giro de la vista al asomarse, en grados. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inclinación", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float LeanRollDegrees = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inclinación", meta = (ClampMin = "0.1"))
	float LeanSpeed = 8.f;

	// --- Caídas ---

	/** Velocidad vertical (cm/s) a partir de la cual una caída hace daño: unos 6 m. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Caídas", meta = (ClampMin = "0.0"))
	float FallDamageMinSpeed = 1100.f;

	/** Velocidad vertical que resulta mortal: unos 20 m. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Caídas", meta = (ClampMin = "0.0"))
	float FallDamageLethalSpeed = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cámara")
	float DefaultFOV = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cámara")
	float AimFOV = 60.f;

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

	/** Asomarse a la izquierda / derecha mientras se mantiene pulsado. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LeanLeftAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LeanRightAction;

	/** Selector de fuego: tiro a tiro, ráfaga, automático. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireModeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BandageAction;

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

	// --- Aspecto por equipo ---
	//
	// Los dos maniquíes de la plantilla distinguen a los equipos a distancia,
	// y el tinte de uniforme (verde oliva frente a arena) a media distancia.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipo")
	TObjectPtr<USkeletalMesh> AlphaTeamMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipo")
	TObjectPtr<USkeletalMesh> BravoTeamMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipo")
	FLinearColor AlphaTeamTint = FLinearColor(0.16f, 0.19f, 0.11f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipo")
	FLinearColor BravoTeamTint = FLinearColor(0.42f, 0.34f, 0.22f);

	/** Parámetro de color del material del maniquí. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipo")
	FName TeamTintParameter = TEXT("Paint Tint");

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PawnClientRestart() override;
	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;

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

	/**
	 * Brazos de primera persona. Es el mismo maniquí que el cuerpo, con la
	 * cabeza y las piernas ocultas, anclado a la cámara.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	UPROPERTY(VisibleAnywhere, Category = "Componentes")
	TObjectPtr<UShooterFirstPersonComponent> FirstPersonView;

	/** Equipo cuyo aspecto ya se aplicó, para no rehacerlo cada fotograma. */
	EShooterTeam AppliedTeam = EShooterTeam::None;
	bool bTeamAppearanceApplied = false;

	/** Malla, tinte y brazos según el equipo del PlayerState. */
	void UpdateTeamAppearance();

	/** Copia al brazo la malla y los materiales del cuerpo. */
	void SyncFirstPersonArms();

	/** El cuerpo cambia de pose según el arma: fusil o pistola. */
	UFUNCTION()
	void HandleWeaponChanged(AWeaponBase* NewWeapon);

	UPROPERTY(Replicated)
	bool bIsAiming = false;

	UPROPERTY(Replicated)
	bool bIsSprinting = false;

	UPROPERTY(Replicated)
	bool bIsSliding = false;

	/** Dirección de la inclinación pedida: -1 izquierda, 0 recto, 1 derecha. */
	UPROPERTY(Replicated)
	int8 LeanInput = 0;

	/** Inclinación actual, interpolada hacia LeanInput en todas las máquinas. */
	float CurrentLean = 0.f;

	/** Fracción de la inclinación que cabe sin meter la cabeza en una pared. */
	float LeanClearance = 1.f;

	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;

	FVector DefaultCameraLocation = FVector(0.f, 0.f, 64.f);

	float Stamina = 100.f;
	float LastStaminaUseTime = -1000.f;
	float LastSprintEndTime = -1000.f;

	bool bHoldingBreath = false;
	float BreathRemaining = 5.f;

	/** Tras agotar el aliento, el pulso se dispara un rato. */
	bool bBreathExhausted = false;

	/** Balanceo aplicado en el frame anterior, para aplicar solo la diferencia. */
	FRotator AppliedSway = FRotator::ZeroRotator;
	float SwayTime = 0.f;

	/** Amplitud efectiva, suavizada: un cambio brusco haría saltar la mira. */
	float CurrentSwayAmplitude = 0.f;

	/** Retroceso aún por aplicar a la vista. */
	FRotator PendingRecoil = FRotator::ZeroRotator;

	/** Retroceso que se devolverá al dejar de disparar. */
	FRotator RecoilToRecover = FRotator::ZeroRotator;
	float LastRecoilTime = -1000.f;

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

	void StartLeanLeft();
	void StopLeanLeft();
	void StartLeanRight();
	void StopLeanRight();
	void UpdateLeanInput();
	void CycleFireMode();
	void StartBandage();

	UFUNCTION(Server, Reliable)
	void Server_SetLean(int8 NewLean);

	void UpdateStamina(float DeltaSeconds);
	void UpdateLean(float DeltaSeconds);

	/** Balanceo, retroceso e inclinación: todo lo que mueve la vista sin el ratón. */
	void UpdateViewModifiers(float DeltaSeconds);

	/** Viñeta y desaturación por supresión y heridas. */
	void UpdateCameraEffects();

	/** Gira la vista sin pasar por las escalas de entrada del controlador. */
	void AddViewRotation(const FRotator& Delta);

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
