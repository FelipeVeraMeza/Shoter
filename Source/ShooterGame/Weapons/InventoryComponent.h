#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class AWeaponBase;
class AShooterCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, AWeaponBase*, NewWeapon);

/** Armas que porta el personaje. El servidor las crea y replica. */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class SHOOTERGAME_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventario")
	FOnWeaponChanged OnWeaponChanged;

	/** Armas con las que aparece el jugador: principal y secundaria. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventario")
	TArray<TSubclassOf<AWeaponBase>> DefaultWeapons;

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
};
