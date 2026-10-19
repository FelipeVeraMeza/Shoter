#include "Weapons/InventoryComponent.h"
#include "Weapons/WeaponBase.h"
#include "Player/ShooterCharacter.h"
#include "Core/ShooterPlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

UInventoryComponent::UInventoryComponent()
{
	// La colocación en primera persona la lleva UShooterFirstPersonComponent:
	// el inventario no necesita actualizarse cada fotograma.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInventoryComponent, Weapons);
	DOREPLIFETIME(UInventoryComponent, CurrentWeapon);
}

AShooterCharacter* UInventoryComponent::GetShooterOwner() const
{
	return Cast<AShooterCharacter>(GetOwner());
}

TArray<TSubclassOf<AWeaponBase>> UInventoryComponent::GetLoadout() const
{
	TArray<TSubclassOf<AWeaponBase>> Loadout = DefaultWeapons;

	// La principal elegida en la pantalla de equipamiento sustituye a la de
	// fábrica. El servidor ya validó la elección al recibirla.
	const AShooterCharacter* Character = GetShooterOwner();
	const AShooterPlayerState* ShooterState = Character ? Character->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (ShooterState && ShooterState->GetPreferredPrimary())
	{
		if (Loadout.Num() > 0)
		{
			Loadout[0] = ShooterState->GetPreferredPrimary();
		}
		else
		{
			Loadout.Add(ShooterState->GetPreferredPrimary());
		}
	}

	return Loadout;
}

void UInventoryComponent::SpawnDefaultWeapons()
{
	AShooterCharacter* Character = GetShooterOwner();
	if (!Character || Character->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	DestroyAllWeapons();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const TSubclassOf<AWeaponBase>& WeaponClass : GetLoadout())
	{
		if (!WeaponClass)
		{
			continue;
		}

		AWeaponBase* Weapon = GetWorld()->SpawnActor<AWeaponBase>(WeaponClass, Character->GetActorTransform(), SpawnParams);
		if (Weapon)
		{
			Weapon->SetOwningCharacter(Character);
			Weapon->SetActorHiddenInGame(true);
			AttachWeaponToOwner(Weapon);
			Weapons.Add(Weapon);
		}
	}

	if (Weapons.Num() > 0)
	{
		EquipWeaponAtIndex(0);
	}
}

void UInventoryComponent::DestroyAllWeapons()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	for (AWeaponBase* Weapon : Weapons)
	{
		if (Weapon)
		{
			Weapon->Destroy();
		}
	}

	Weapons.Empty();
	CurrentWeapon = nullptr;
	CurrentIndex = 0;
}

void UInventoryComponent::EquipWeaponAtIndex(int32 Index)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return;
	}

	if (GetOwnerRole() != ROLE_Authority)
	{
		Server_EquipWeaponAtIndex(Index);
		return;
	}

	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
		CurrentWeapon->SetActorHiddenInGame(true);
	}

	CurrentIndex = Index;
	CurrentWeapon = Weapons[Index];

	if (CurrentWeapon)
	{
		CurrentWeapon->SetActorHiddenInGame(false);
		AttachWeaponToOwner(CurrentWeapon);
	}

	OnWeaponChanged.Broadcast(CurrentWeapon);
}

void UInventoryComponent::Server_EquipWeaponAtIndex_Implementation(int32 Index)
{
	EquipWeaponAtIndex(Index);
}

void UInventoryComponent::SelectNextWeapon()
{
	if (Weapons.Num() < 2)
	{
		return;
	}

	EquipWeaponAtIndex((CurrentIndex + 1) % Weapons.Num());
}

void UInventoryComponent::AttachWeaponToOwner(AWeaponBase* Weapon)
{
	AShooterCharacter* Character = GetShooterOwner();
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Weapon || !Body)
	{
		return;
	}

	// El arma del mundo va en la mano del cuerpo, que es lo que ven los
	// demás. La de primera persona la coloca UShooterFirstPersonComponent en
	// los brazos del jugador local.
	const FName Socket = Body->DoesSocketExist(HandSocket) ? HandSocket : FName(TEXT("hand_r"));
	Weapon->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	Weapon->SetActorRelativeScale3D(FVector::OneVector);
}

void UInventoryComponent::OnRep_CurrentWeapon()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->SetActorHiddenInGame(false);
		AttachWeaponToOwner(CurrentWeapon);
	}

	OnWeaponChanged.Broadcast(CurrentWeapon);
}
