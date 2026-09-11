#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class AWeaponBase;
class AShooterCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, AWeaponBase*, NewWeapon);

/** Armas que porta el personaje. El servidor las crea y replica. */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class SHOTER_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventario")
	FOnWeaponChanged OnWeaponChanged;

	/** Armas con las que aparece el jugador: principal y secundaria. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario")
	TArray<TSubclassOf<AWeaponBase>> DefaultWeapons;

	/**
	 * Colocación del arma respecto a la cámara, mientras el personaje no tenga
	 * un socket de mano. Adelante, a la derecha y algo por debajo del centro.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Vista")
	FVector FirstPersonOffset = FVector(40.f, 16.f, -18.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Vista")
	FRotator FirstPersonRotation = FRotator(0.f, 0.f, 0.f);

	/** Las mallas de arma suelen ser de tamaño real: en vista de cañón estorban. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Vista",
		meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float FirstPersonScale = 0.12f;

	/**
	 * Balanceo del arma al caminar (RF5). Da sensación de peso: sin él el
	 * arma parece pegada a la pantalla.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Balanceo",
		meta = (ClampMin = "0.0"))
	float BobAmount = 1.6f;

	/** Ciclos de balanceo por segundo a velocidad de marcha. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Balanceo",
		meta = (ClampMin = "0.1"))
	float BobFrequency = 7.f;

	/** Cuánto se amplifica el balanceo al correr. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Balanceo",
		meta = (ClampMin = "1.0"))
	float SprintBobMultiplier = 1.8f;

	/** Rapidez con que el arma vuelve al centro al detenerse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario|Balanceo",
		meta = (ClampMin = "0.1"))
	float BobRecoverySpeed = 8.f;

	UFUNCTION(BlueprintPure, Category = "Inventario")
	AWeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

	/** Solo servidor. Crea el equipamiento inicial y equipa la primera arma. */
	void SpawnDefaultWeapons();

	/** Solo servidor. Destruye todas las armas (al morir). */
	void DestroyAllWeapons();

	void EquipWeaponAtIndex(int32 Index);
	void SelectNextWeapon();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Replicated)
	TArray<TObjectPtr<AWeaponBase>> Weapons;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AWeaponBase> CurrentWeapon;

	int32 CurrentIndex = 0;

	UFUNCTION()
	void OnRep_CurrentWeapon();

	UFUNCTION(Server, Reliable)
	void Server_EquipWeaponAtIndex(int32 Index);

	void AttachWeaponToOwner(AWeaponBase* Weapon);
	AShooterCharacter* GetShooterOwner() const;

	/** Balanceo del arma según la velocidad del portador (RF5). */
	void UpdateWeaponBob(float DeltaTime);

	/** Fase del ciclo de balanceo, en radianes. */
	float BobPhase = 0.f;

	/** Desplazamiento actual, interpolado para que no dé saltos. */
	FVector CurrentBobOffset = FVector::ZeroVector;
};
