#include "Weapons/InventoryComponent.h"
#include "Weapons/WeaponBase.h"
#include "Player/ShooterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UInventoryComponent::UInventoryComponent()
{
	// El balanceo del arma necesita actualizarse cada frame (RF5).
	PrimaryComponentTick.bCanEverTick = true;
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

	for (const TSubclassOf<AWeaponBase>& WeaponClass : DefaultWeapons)
	{
		if (!WeaponClass)
		{
			continue;
		}

		AWeaponBase* Weapon = GetWorld()->SpawnActor<AWeaponBase>(WeaponClass, Character->GetActorTransform(), SpawnParams);
		if (Weapon)
		{
			Weapon->SetOwningCharacter(Character);
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

void UInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateWeaponBob(DeltaTime);
}

void UInventoryComponent::UpdateWeaponBob(float DeltaTime)
{
	AShooterCharacter* Character = GetShooterOwner();

	// Solo tiene sentido para quien ve el arma en primera persona.
	if (!Character || !Character->IsLocallyControlled() || !CurrentWeapon || BobAmount <= 0.f)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const float Speed = Character->GetVelocity().Size2D();
	const bool bOnGround = !Movement->IsFalling();

	FVector TargetOffset = FVector::ZeroVector;

	if (Speed > 10.f && bOnGround)
	{
		// La frecuencia sigue a la velocidad real: correr acelera el ciclo.
		const float SpeedRatio = Speed / FMath::Max(Character->WalkSpeed, 1.f);
		const float Frequency = BobFrequency * FMath::Clamp(SpeedRatio, 0.5f, SprintBobMultiplier);

		BobPhase += DeltaTime * Frequency;

		// Figura de ocho: horizontal al doble de frecuencia que vertical, que
		// es como se mueve el cuerpo al caminar.
		const float Intensity = BobAmount * FMath::Clamp(SpeedRatio, 0.f, SprintBobMultiplier);
		TargetOffset.Y = FMath::Sin(BobPhase) * Intensity;
		TargetOffset.Z = FMath::Sin(BobPhase * 2.f) * Intensity * 0.5f;
	}
	else
	{
		// Parado: la fase se reinicia para que el siguiente paso empiece limpio.
		BobPhase = 0.f;
	}

	// Apuntar reduce el balanceo: el arma se estabiliza contra el hombro.
	if (Character->IsAiming())
	{
		TargetOffset *= 0.25f;
	}

	CurrentBobOffset = FMath::VInterpTo(CurrentBobOffset, TargetOffset, DeltaTime, BobRecoverySpeed);

	CurrentWeapon->SetActorRelativeLocation(FirstPersonOffset + CurrentBobOffset);
}

void UInventoryComponent::AttachWeaponToOwner(AWeaponBase* Weapon)
{
	AShooterCharacter* Character = GetShooterOwner();
	if (!Weapon || !Character)
	{
		return;
	}

	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, true);

	// Sin malla de personaje todavía: el arma se ancla a la cámara para que quede
	// visible en primera persona. Al llegar el personaje con esqueleto, esto pasa
	// a un socket de la mano.
	if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
	{
		if (CharacterMesh->DoesSocketExist(TEXT("WeaponSocket")))
		{
			Weapon->AttachToComponent(CharacterMesh, Rules, TEXT("WeaponSocket"));
			return;
		}
	}

	if (UCameraComponent* Camera = Character->GetFirstPersonCamera())
	{
		Weapon->AttachToComponent(Camera, Rules);
		Weapon->SetActorRelativeLocation(FirstPersonOffset);
		Weapon->SetActorRelativeRotation(FirstPersonRotation);
		Weapon->SetActorScale3D(FVector(FirstPersonScale));
	}
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
