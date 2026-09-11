#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/LagCompensationComponent.h"
#include "Weapons/InventoryComponent.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/ShooterGrenade.h"
#include "Combat/TrainingRangeManager.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Settings/ShooterSettingsSubsystem.h"
#include "UI/ShooterMenuSubsystem.h"
#include "Engine/GameInstance.h"
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
	DOREPLIFETIME_CONDITION(AShooterCharacter, bIsSliding, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AShooterCharacter, GrenadeCount, COND_OwnerOnly);
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
		GrenadeCount = MaxGrenades;
	}
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateSlide(DeltaSeconds);

	if (FirstPersonCamera && IsLocallyControlled())
	{
		const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;
		const float NewFOV = FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaSeconds, 15.f);
		FirstPersonCamera->SetFieldOfView(NewFOV);
	}
}

void AShooterCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	AddInputMappingContexts();
}

void AShooterCharacter::AddInputMappingContexts()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	for (const TObjectPtr<UInputMappingContext>& Context : DefaultMappingContexts)
	{
		if (Context)
		{
			Subsystem->AddMappingContext(Context, 0);
		}
	}
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AShooterCharacter::Move);
	}

	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AShooterCharacter::Look);
	}

	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	if (FireAction)
	{
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::StartFire);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::StopFire);
	}

	if (AimAction)
	{
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &AShooterCharacter::StartAiming);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &AShooterCharacter::StopAiming);
	}

	if (SprintAction)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AShooterCharacter::StartSprinting);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AShooterCharacter::StopSprinting);
	}

	if (ReloadAction)
	{
		Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AShooterCharacter::Reload);
	}

	if (SwitchWeaponAction)
	{
		Input->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &AShooterCharacter::SwitchWeapon);
	}

	if (CrouchAction)
	{
		Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AShooterCharacter::ToggleCrouch);
	}

	if (PauseAction)
	{
		Input->BindAction(PauseAction, ETriggerEvent::Started, this, &AShooterCharacter::TogglePauseMenu);
	}

	if (GrenadeAction)
	{
		Input->BindAction(GrenadeAction, ETriggerEvent::Started, this, &AShooterCharacter::ThrowGrenade);
	}

	if (ResetRangeAction)
	{
		Input->BindAction(ResetRangeAction, ETriggerEvent::Started, this, &AShooterCharacter::RequestRangeReset);
	}
}

void AShooterCharacter::RequestRangeReset()
{
	// El gestor puede no existir fuera de la zona de entrenamiento.
	for (TActorIterator<ATrainingRangeManager> It(GetWorld()); It; ++It)
	{
		if (ATrainingRangeManager* Manager = *It)
		{
			Manager->ResetRange();
			return;
		}
	}
}

void AShooterCharacter::ThrowGrenade()
{
	const float Now = GetWorld()->GetTimeSeconds();

	if (IsDead() || GrenadeCount <= 0 || (Now - LastGrenadeTime) < GrenadeCooldown)
	{
		return;
	}

	const UCameraComponent* Camera = GetFirstPersonCamera();
	if (!Camera || !GrenadeClass)
	{
		return;
	}

	LastGrenadeTime = Now;

	// Sale desde delante de la cámara, algo elevada: lanzar en arco es más
	// útil que lanzar recto.
	const FVector Origin = Camera->GetComponentLocation() + Camera->GetForwardVector() * 60.f;
	const FVector Direction = (Camera->GetForwardVector() + FVector(0.f, 0.f, 0.25f)).GetSafeNormal();

	Server_ThrowGrenade(Origin, Direction);
}

void AShooterCharacter::Server_ThrowGrenade_Implementation(
	const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction)
{
	if (!GrenadeRateLimiter.AllowCall(GetWorld()->GetTimeSeconds()))
	{
		return;
	}

	if (GrenadeCount <= 0 || !GrenadeClass || IsDead())
	{
		return;
	}

	// El servidor valida que el lanzamiento parta de donde el jugador está.
	if (FVector::DistSquared(Origin, GetActorLocation()) > FMath::Square(350.f))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AShooterGrenade* Grenade = GetWorld()->SpawnActor<AShooterGrenade>(
		GrenadeClass, Origin, Direction.Rotation(), SpawnParams);

	if (Grenade)
	{
		Grenade->Launch(Direction);
		--GrenadeCount;
	}
}

void AShooterCharacter::TogglePauseMenu()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterMenuSubsystem* Menu = GameInstance->GetSubsystem<UShooterMenuSubsystem>())
		{
			Menu->TogglePause();
		}
	}
}

void AShooterCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || IsDead())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AShooterCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	// Sensibilidad e inversión salen de los ajustes del jugador (RF-11.1).
	float Sensitivity = 1.f;
	float PitchSign = 1.f;

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UShooterSettingsSubsystem* Settings =
			GameInstance->GetSubsystem<UShooterSettingsSubsystem>())
		{
			Sensitivity = Settings->GetMouseSensitivity();
			PitchSign = Settings->GetInvertY() ? -1.f : 1.f;
		}
	}

	// AddControllerPitchInput mira hacia abajo con valores positivos, pero el
	// ratón entrega Y positivo al desplazarse hacia arriba: hay que negarlo
	// para que el control no salga invertido por defecto.
	AddControllerYawInput(Axis.X * Sensitivity);
	AddControllerPitchInput(-Axis.Y * Sensitivity * PitchSign);
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
	if (!StateRateLimiter.AllowCall(GetWorld()->GetTimeSeconds()))
	{
		return;
	}

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
	if (!StateRateLimiter.AllowCall(GetWorld()->GetTimeSeconds()))
	{
		return;
	}

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

	// Agacharse en plena carrera desliza en vez de frenar en seco (RF-4.6).
	if (bIsSprinting && !bIsSliding && !GetCharacterMovement()->IsFalling())
	{
		StartSlide();
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

void AShooterCharacter::StartSlide()
{
	const float Now = GetWorld()->GetTimeSeconds();

	// El enfriamiento evita encadenar deslizamientos como forma de correr.
	if (bIsSliding || (Now - LastSlideEndTime) < SlideCooldown)
	{
		return;
	}

	bIsSliding = true;
	SlideTimeRemaining = SlideDuration;

	Crouch();

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement)
	{
		// El impulso va en la dirección de avance, no la de la mirada: mirar
		// hacia atrás a mitad de deslizamiento no debe frenarlo.
		const FVector SlideDirection = GetVelocity().GetSafeNormal2D().IsNearlyZero()
			? GetActorForwardVector()
			: GetVelocity().GetSafeNormal2D();

		Movement->MaxWalkSpeedCrouched = SlideSpeed;
		Movement->Velocity = SlideDirection * SlideSpeed;
	}

	if (!HasAuthority())
	{
		Server_SetSliding(true);
	}
}

void AShooterCharacter::StopSlide()
{
	if (!bIsSliding)
	{
		return;
	}

	bIsSliding = false;
	SlideTimeRemaining = 0.f;
	LastSlideEndTime = GetWorld()->GetTimeSeconds();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	}

	UnCrouch();

	if (!HasAuthority())
	{
		Server_SetSliding(false);
	}
}

void AShooterCharacter::UpdateSlide(float DeltaSeconds)
{
	if (!bIsSliding)
	{
		return;
	}

	SlideTimeRemaining -= DeltaSeconds;

	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// Termina al agotarse el tiempo, al saltar o al frenarse por rozamiento.
	const bool bTooSlow = Movement && Movement->Velocity.Size2D() < CrouchSpeed;
	if (SlideTimeRemaining <= 0.f || bTooSlow || (Movement && Movement->IsFalling()))
	{
		StopSlide();
		return;
	}

	// Deceleración progresiva: el deslizamiento pierde fuerza hacia el final.
	if (Movement)
	{
		const float Alpha = FMath::Clamp(SlideTimeRemaining / FMath::Max(SlideDuration, 0.01f), 0.f, 1.f);
		const float TargetSpeed = FMath::Lerp(CrouchSpeed, SlideSpeed, Alpha);
		Movement->Velocity = Movement->Velocity.GetSafeNormal2D() * TargetSpeed;
	}
}

void AShooterCharacter::Server_SetSliding_Implementation(bool bNewSliding)
{
	if (!StateRateLimiter.AllowCall(GetWorld()->GetTimeSeconds()))
	{
		return;
	}

	bIsSliding = bNewSliding;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeedCrouched = bNewSliding ? SlideSpeed : CrouchSpeed;
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
