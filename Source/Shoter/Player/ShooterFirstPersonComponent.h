#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShooterFirstPersonComponent.generated.h"

class AShooterCharacter;
class AWeaponBase;
class UWeaponDataAsset;
class UCameraComponent;
class USkeletalMeshComponent;
class UAnimSequenceBase;
class USkeletalMesh;

/**
 * Vista en primera persona: brazos que sostienen el arma.
 *
 * El arma se coloca respecto a la cámara (a la cadera o con la mira en el eje
 * de la vista) y los brazos se ajustan después para que la mano derecha la
 * empuñe. Así apuntar siempre alinea la mira, con cualquier malla de arma, y
 * la pose de los brazos sale de las animaciones del maniquí.
 *
 * Encima se suman los movimientos que dan peso al arma: balanceo al andar,
 * retraso al girar, golpe de retroceso, bajada al correr, recarga y cambio.
 *
 * Solo trabaja para el jugador local; en los demás personajes no hace nada.
 */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class SHOTER_API UShooterFirstPersonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShooterFirstPersonComponent();

	/** Campo de visión con el que se dibujan brazos y arma, a la cadera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona", meta = (ClampMin = "30.0", ClampMax = "110.0"))
	float ViewModelFOV = 74.f;

	/** Campo de visión de brazos y arma con la mira puesta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona", meta = (ClampMin = "20.0", ClampMax = "110.0"))
	float AimViewModelFOV = 52.f;

	/**
	 * Escala de primera persona de la cámara: acerca brazos y arma al ojo
	 * para que no atraviesen las paredes al pegarse a ellas.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ViewModelScale = 0.6f;

	/** Socket de la mano derecha del maniquí donde se ancla el arma. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona")
	FName HandSocket = TEXT("HandGrip_R");

	/** Balanceo al andar, en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento", meta = (ClampMin = "0.0"))
	float BobAmount = 0.7f;

	/** Pasos por segundo a velocidad de marcha. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento", meta = (ClampMin = "0.1"))
	float BobFrequency = 1.9f;

	/** Grados que el arma se retrasa por cada grado por segundo de giro de la vista. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento", meta = (ClampMin = "0.0"))
	float LookLag = 0.012f;

	/** Retraso máximo del arma al girar, en grados. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento", meta = (ClampMin = "0.0"))
	float MaxLookLag = 3.5f;

	/** Posición del arma a la carrera: baja y cruzada sobre el pecho. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento")
	FVector SprintOffset = FVector(-5.f, -5.f, -7.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento")
	FRotator SprintRotation = FRotator(-18.f, 30.f, 14.f);

	/** Segundos que tarda en subir el arma tras cambiarla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primera persona|Movimiento", meta = (ClampMin = "0.05"))
	float EquipTime = 0.45f;

	void SetupView(USkeletalMeshComponent* InArms, UCameraComponent* InCamera);

	/** Golpe visual del disparo. Lo llama el arma al disparar. */
	void NotifyShotFired(const UWeaponDataAsset* Data);

	/** 0 a la cadera, 1 con la mira en el ojo. */
	UFUNCTION(BlueprintPure, Category = "Primera persona")
	float GetAimAlpha() const { return AimAlpha; }

	/** Con visor de aumentos y la mira puesta se ve a través del visor: el HUD lo dibuja. */
	UFUNCTION(BlueprintPure, Category = "Primera persona")
	bool IsScopeViewActive() const { return bScopeView; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Arms;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<AWeaponBase> EquippedWeapon;

	UPROPERTY()
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequenceBase> ReloadAnim;

	/** Malla de los brazos con la que se preparó la animación. */
	UPROPERTY()
	TObjectPtr<USkeletalMesh> PreparedArmsMesh;

	bool bArmsUsable = false;
	bool bWasReloading = false;
	bool bPlayingReload = false;
	bool bScopeView = false;

	float AimAlpha = 0.f;
	float SprintAlpha = 0.f;
	float ReloadAlpha = 0.f;
	float LowerAlpha = 1.f;
	float EquipElapsed = 0.f;
	float ReloadElapsed = 0.f;
	float ReloadDuration = 0.f;

	float BobPhase = 0.f;
	FVector BobOffset = FVector::ZeroVector;

	FRotator LastControlRotation = FRotator::ZeroRotator;
	FRotator LagRotation = FRotator::ZeroRotator;

	float KickBack = 0.f;
	float KickPitch = 0.f;
	float KickRoll = 0.f;

	AShooterCharacter* GetCharacter() const;
	bool IsLocalViewer() const;

	void HandleWeaponChanged(AWeaponBase* NewWeapon);
	void UpdateAnimation(AWeaponBase* Weapon, float DeltaTime);
	void UpdateMotion(AShooterCharacter* Character, AWeaponBase* Weapon, float DeltaTime);

	/** Transform del arma en el espacio de la cámara. */
	FTransform ComputeWeaponTransform(const AWeaponBase* Weapon) const;

	/** Coloca brazos y arma; si los brazos no sirven, ancla el arma a la cámara. */
	void ApplyWeaponTransform(AWeaponBase* Weapon, const FTransform& WeaponInCamera);

	void SetViewModelHidden(AWeaponBase* Weapon, bool bHidden);
};
