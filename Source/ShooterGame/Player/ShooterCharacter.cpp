#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/LagCompensationComponent.h"
#include "Weapons/InventoryComponent.h"
#include "Weapons/WeaponBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

AShooterCharacter::AShooterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->JumpZVelocity = 420.f;
	Movement->AirControl = 0.35f;
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

	// La malla no se ve en primera persona pero sí para los demás jugadores.
	GetMesh()->SetOwnerNoSee(true);
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShooterCharacter, bIsAiming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AShooterCharacter, bIsSprinting, COND_SkipOwner);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnOwnerDied.AddDynamic(this, &AShooterCharacter::HandleDeath);
		HealthComponent->OnHealthChanged.AddDynamic(this, &AShooterCharacter::HandleHealthChanged);
	}

	UpdateMaxWalkSpeed();
}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority() && Inventory)
	{
		Inventory->SpawnDefaultWeapons();
	}
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FirstPersonCamera && IsLocallyControlled())
	{
		const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;
		const float NewFOV = FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaSeconds, 15.f);
		FirstPersonCamera->SetFieldOfView(NewFOV);
	}
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AShooterCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AShooterCharacter::StopFire);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AShooterCharacter::StartAiming);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AShooterCharacter::StopAiming);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AShooterCharacter::StartSprinting);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AShooterCharacter::StopSprinting);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AShooterCharacter::Reload);
	PlayerInputComponent->BindAction("SwitchWeapon", IE_Pressed, this, &AShooterCharacter::SwitchWeapon);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AShooterCharacter::ToggleCrouch);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
}

void AShooterCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.f && !IsDead())
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.f && !IsDead())
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
	}
}

AWeaponBase* AShooterCharacter::GetCurrentWeapon() const
{
	return Inventory ? Inventory->GetCurrentWeapon() : nullptr;
}

bool AShooterCharacter::IsDead() const
{
	return HealthComponent && HealthComponent->IsDead();
}

void AShooterCharacter::StartFire()
{
	if (IsDead() || bIsSprinting)
	{
		return;
	}

	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->TryStartFire();
	}
}

void AShooterCharacter::StopFire()
{
	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->StopFire();
	}
}

void AShooterCharacter::Reload()
{
	if (IsDead())
	{
		return;
	}

	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->TryReload();
	}
}

void AShooterCharacter::SwitchWeapon()
{
	if (!IsDead() && Inventory)
	{
		Inventory->SelectNextWeapon();
	}
}

void AShooterCharacter::StartAiming()
{
	if (IsDead() || bIsSprinting)
	{
		return;
	}

	bIsAiming = true;
	if (!HasAuthority())
	{
		Server_SetAiming(true);
	}
}

void AShooterCharacter::StopAiming()
{
	bIsAiming = false;
	if (!HasAuthority())
	{
		Server_SetAiming(false);
	}
}

void AShooterCharacter::Server_SetAiming_Implementation(bool bNewAiming)
{
	bIsAiming = bNewAiming;
}

void AShooterCharacter::StartSprinting()
{
	if (IsDead())
	{
		return;
	}

	bIsSprinting = true;
	bIsAiming = false;
	UpdateMaxWalkSpeed();

	StopFire();

	if (!HasAuthority())
	{
		Server_SetSprinting(true);
	}
}

void AShooterCharacter::StopSprinting()
{
	bIsSprinting = false;
	UpdateMaxWalkSpeed();

	if (!HasAuthority())
	{
		Server_SetSprinting(false);
	}
}

void AShooterCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	if (bNewSprinting)
	{
		bIsAiming = false;
	}
	UpdateMaxWalkSpeed();
}

void AShooterCharacter::ToggleCrouch()
{
	if (IsDead())
	{
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AShooterCharacter::UpdateMaxWalkSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}
}

void AShooterCharacter::HandleDeath(AActor* Victim, AController* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Inventory)
	{
		Inventory->DestroyAllWeapons();
	}

	Multicast_PlayDeath();

	if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetController()))
	{
		ShooterPC->OnCharacterDied(Killer);
	}
}

void AShooterCharacter::HandleHealthChanged(float NewHealth, float Delta, AController* InstigatedBy)
{
	if (!HasAuthority() || Delta >= 0.f || !InstigatedBy)
	{
		return;
	}

	const APawn* InstigatorPawn = InstigatedBy->GetPawn();
	if (!InstigatorPawn)
	{
		return;
	}

	if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetController()))
	{
		ShooterPC->Client_NotifyDamageTaken(InstigatorPawn->GetActorLocation());
	}
}

void AShooterCharacter::Multicast_PlayDeath_Implementation()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CharacterMesh->SetSimulatePhysics(true);
	}
}
