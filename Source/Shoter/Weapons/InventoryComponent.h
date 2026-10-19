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

	/**
	 * Armas con las que aparece el jugador: principal y secundaria. La
	 * principal puede sustituirla la elegida en la pantalla de equipamiento.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario")
	TArray<TSubclassOf<AWeaponBase>> DefaultWeapons;

	/**
	 * Socket de la mano derecha del cuerpo donde se lleva el arma. El
	 * esqueleto del maniquí lo trae; sin él se usa el hueso de la mano.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario")
	FName HandSocket = TEXT("HandGrip_R");

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

	/** Armas con las que aparece, con la principal elegida por el jugador. */
	TArray<TSubclassOf<AWeaponBase>> GetLoadout() const;
};
