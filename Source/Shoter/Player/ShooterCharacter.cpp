#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Combat/HealthComponent.h"
#include "Combat/LagCompensationComponent.h"
#include "Weapons/InventoryComponent.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
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
#include "Kismet/GameplayStatics.h"

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
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;

	// Inercia de un cuerpo con 20 kg de equipo: arranca y frena en unos
	// tres pasos, no en el instante. El salto es el de una persona cargada,
	// y en el aire apenas se puede corregir la trayectoria.
	Movement->MaxAcceleration = 1400.f;
	Movement->BrakingDecelerationWalking = 1300.f;
	Movement->GroundFriction = 7.f;
	Movement->JumpZVelocity = 360.f;
	Movement->AirControl = 0.08f;

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
	DOREPLIFETIME_CONDITION(AShooterCharacter, LeanInput, COND_SkipOwner);
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

	if (FirstPersonCamera)
	{
		DefaultCameraLocation = FirstPersonCamera->GetRelativeLocation();
	}

	Stamina = MaxStamina;
	BreathRemaining = MaxBreathHold;

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
	UpdateStamina(DeltaSeconds);
	UpdateLean(DeltaSeconds);

	// Barato y evita perseguir cada causa por separado: apuntar, correr y
	// las heridas de pierna cambian la velocidad desde sitios distintos.
	UpdateMaxWalkSpeed();

	if (FirstPersonCamera && IsLocallyControlled())
	{
		// Cada arma tiene su aumento: unas miras metálicas no acercan como
		// un visor.
		const AWeaponBase* Weapon = GetCurrentWeapon();
		const float WeaponAimFOV = (Weapon && Weapon->WeaponData && Weapon->WeaponData->AimFOV > 0.f)
			? Weapon->WeaponData->AimFOV
			: AimFOV;

		const float TargetFOV = bIsAiming ? WeaponAimFOV : DefaultFOV;
		const float NewFOV = FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaSeconds, 12.f);
		FirstPersonCamera->SetFieldOfView(NewFOV);

		UpdateViewModifiers(DeltaSeconds);
		UpdateCameraEffects();
	}
}

bool AShooterCharacter::IsWeaponReady() const
{
	if (bIsSprinting || (HealthComponent && HealthComponent->IsBandaging()))
	{
		return false;
	}

	return (GetWorld()->GetTimeSeconds() - LastSprintEndTime) >= SprintOutTime;
}

FVector AShooterCharacter::GetShotOrigin() const
{
	// La cámara ya incluye el desplazamiento de la inclinación.
	return FirstPersonCamera ? FirstPersonCamera->GetComponentLocation() : GetPawnViewLocation();
}

FVector AShooterCharacter::GetLeanWorldOffset() const
{
	return GetActorRightVector() * (CurrentLean * LeanDistance);
}

void AShooterCharacter::AddRecoil(float PitchDegrees, float YawDegrees, float RecoveryFraction)
{
	const FRotator Kick(PitchDegrees, YawDegrees, 0.f);

	PendingRecoil += Kick;
	RecoilToRecover += Kick * FMath::Clamp(RecoveryFraction, 0.f, 1.f);
	LastRecoilTime = GetWorld()->GetTimeSeconds();
}

void AShooterCharacter::AddViewRotation(const FRotator& Delta)
{
	if (!Controller || Delta.IsNearlyZero(1.e-4f))
	{
		return;
	}

	// Se escribe la rotación directamente en vez de usar AddControllerPitchInput:
	// esa vía multiplica por las escalas de entrada heredadas del motor, y el
	// retroceso de los DataAssets debe medirse en grados reales.
	FRotator Rotation = Controller->GetControlRotation() + Delta;
	Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch, -89.f, 89.f);
	Controller->SetControlRotation(Rotation);
}

void AShooterCharacter::UpdateStamina(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bMoving = GetVelocity().Size2D() > 10.f;

	if (bIsSprinting && bMoving)
	{
		Stamina = FMath::Max(Stamina - SprintStaminaCost * DeltaSeconds, 0.f);
		LastStaminaUseTime = Now;

		if (Stamina <= 0.f)
		{
			StopSprinting();
		}
	}
	else if (Now - LastStaminaUseTime >= StaminaRegenDelay)
	{
		// Agachado se recupera antes: es la postura de descanso en combate.
		const float CrouchBonus = bIsCrouched ? 1.4f : 1.f;
		Stamina = FMath::Min(Stamina + StaminaRegenRate * CrouchBonus * DeltaSeconds, MaxStamina);
	}

	// Respiración: se gasta conteniéndola y se recupera respirando.
	if (bHoldingBreath)
	{
		BreathRemaining = FMath::Max(BreathRemaining - DeltaSeconds, 0.f);
		if (BreathRemaining <= 0.f)
		{
			bHoldingBreath = false;
			bBreathExhausted = true;
		}
	}
	else
	{
		BreathRemaining = FMath::Min(BreathRemaining + DeltaSeconds * 0.6f, MaxBreathHold);
		if (bBreathExhausted && BreathRemaining >= MaxBreathHold * 0.5f)
		{
			bBreathExhausted = false;
		}
	}
}

void AShooterCharacter::UpdateLean(float DeltaSeconds)
{
	CurrentLean = FMath::FInterpTo(CurrentLean, static_cast<float>(LeanInput), DeltaSeconds, LeanSpeed);

	if (!FirstPersonCamera || !IsLocallyControlled())
	{
		return;
	}

	LeanClearance = 1.f;

	if (FMath::Abs(CurrentLean) > 0.01f)
	{
		// La cabeza no atraviesa paredes: se recorta la inclinación hasta lo
		// que cabe. Sin esto, asomarse pegado a un muro dejaría ver detrás.
		const FVector Base = GetCapsuleComponent()->GetComponentTransform().TransformPosition(DefaultCameraLocation);
		const FVector Target = Base + GetActorRightVector() * (CurrentLean * LeanDistance);

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShooterLean), false, this);
		if (GetWorld()->SweepSingleByChannel(Hit, Base, Target, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(12.f), Params))
		{
			LeanClearance = Hit.Time;
		}
	}

	FirstPersonCamera->SetRelativeLocation(DefaultCameraLocation
		+ FVector(0.f, CurrentLean * LeanDistance * LeanClearance, 0.f));
}

void AShooterCharacter::UpdateViewModifiers(float DeltaSeconds)
{
	if (!Controller || !Controller->IsLocalPlayerController() || IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	FRotator Delta = FRotator::ZeroRotator;

	// Retroceso: el golpe se reparte en unos pocos frames...
	const FRotator Kick = PendingRecoil * FMath::Min(1.f, DeltaSeconds * RecoilKickSpeed);
	PendingRecoil -= Kick;
	Delta += Kick;

	// ...y al dejar de disparar el tirador devuelve el arma a su sitio.
	if (Now - LastRecoilTime > 0.12f && !RecoilToRecover.IsNearlyZero(1.e-3f))
	{
		const FRotator Back = RecoilToRecover * FMath::Min(1.f, DeltaSeconds * RecoilRecoverySpeed);
		RecoilToRecover -= Back;
		Delta -= Back;
	}

	// Balanceo de la mira: pulso y respiración. Se aplica a la vista y no
	// solo al arma, así que la bala va a donde de verdad apunta el jugador.
	const AWeaponBase* Weapon = GetCurrentWeapon();
	const float WeaponSway = (Weapon && Weapon->WeaponData) ? Weapon->WeaponData->SwayMultiplier : 1.f;
	const float StaminaPercent = GetStaminaPercent();

	float TargetAmplitude = AimSwayAmplitude * WeaponSway * (bIsAiming ? 1.f : 0.4f);
	TargetAmplitude *= 1.f + (1.f - StaminaPercent) * 1.5f;
	TargetAmplitude *= 1.f + FMath::Clamp(GetVelocity().Size2D() / FMath::Max(WalkSpeed, 1.f), 0.f, 1.f) * 0.8f;

	if (bIsCrouched)
	{
		TargetAmplitude *= 0.6f;
	}

	if (const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(Controller))
	{
		TargetAmplitude *= 1.f + ShooterPC->GetSuppressionLevel() * 2.f;
	}

	if (bHoldingBreath)
	{
		TargetAmplitude *= 0.15f;
	}
	else if (bBreathExhausted)
	{
		TargetAmplitude *= 2.f;
	}

	CurrentSwayAmplitude = FMath::FInterpTo(CurrentSwayAmplitude, TargetAmplitude, DeltaSeconds, 4.f);

	// Cansado, el pulso se acelera.
	SwayTime += DeltaSeconds * AimSwayFrequency * (1.f + (1.f - StaminaPercent));

	const FRotator NewSway(
		FMath::PerlinNoise1D(SwayTime) * CurrentSwayAmplitude,
		FMath::PerlinNoise1D(SwayTime + 37.13f) * CurrentSwayAmplitude * 1.3f,
		0.f);

	Delta += NewSway - AppliedSway;
	AppliedSway = NewSway;

	// Inclinación: la vista gira con la cabeza. El balanceo y el retroceso
	// son relativos; el giro lateral, absoluto.
	FRotator Rotation = Controller->GetControlRotation() + Delta;
	Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch, -89.f, 89.f);
	Rotation.Roll = CurrentLean * LeanClearance * LeanRollDegrees;
	Controller->SetControlRotation(Rotation);
}

void AShooterCharacter::UpdateCameraEffects()
{
	if (!FirstPersonCamera)
	{
		return;
	}

	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(Controller);
	const float Suppression = ShooterPC ? ShooterPC->GetSuppressionLevel() : 0.f;

	// Por debajo de la mitad de la salud la visión se apaga poco a poco: el
	// HUD realista no muestra la barra, así que la pantalla tiene que avisar.
	const float HealthPercent = HealthComponent ? HealthComponent->GetHealthPercent() : 1.f;
	const float Injury = FMath::Clamp((0.5f - HealthPercent) / 0.5f, 0.f, 1.f);

	FPostProcessSettings& Settings = FirstPersonCamera->PostProcessSettings;
	FirstPersonCamera->PostProcessBlendWeight = 1.f;

	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.35f + Suppression * 0.8f + Injury * 0.6f;

	Settings.bOverride_SceneFringeIntensity = true;
	Settings.SceneFringeIntensity = Suppression * 3.f;

	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.f - Injury * 0.75f);
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

	if (LeanLeftAction)
	{
		Input->BindAction(LeanLeftAction, ETriggerEvent::Started, this, &AShooterCharacter::StartLeanLeft);
		Input->BindAction(LeanLeftAction, ETriggerEvent::Completed, this, &AShooterCharacter::StopLeanLeft);
	}

	if (LeanRightAction)
	{
		Input->BindAction(LeanRightAction, ETriggerEvent::Started, this, &AShooterCharacter::StartLeanRight);
		Input->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &AShooterCharacter::StopLeanRight);
	}

	if (FireModeAction)
	{
		Input->BindAction(FireModeAction, ETriggerEvent::Started, this, &AShooterCharacter::CycleFireMode);
	}

	if (BandageAction)
	{
		Input->BindAction(BandageAction, ETriggerEvent::Started, this, &AShooterCharacter::StartBandage);
	}
}

void AShooterCharacter::StartLeanLeft()
{
	bLeanLeftHeld = true;
	UpdateLeanInput();
}

void AShooterCharacter::StopLeanLeft()
{
	bLeanLeftHeld = false;
	UpdateLeanInput();
}

void AShooterCharacter::StartLeanRight()
{
	bLeanRightHeld = true;
	UpdateLeanInput();
}

void AShooterCharacter::StopLeanRight()
{
	bLeanRightHeld = false;
	UpdateLeanInput();
}

void AShooterCharacter::UpdateLeanInput()
{
	// Pulsar ambas a la vez se anula; corriendo no se puede asomar.
	int8 NewLean = static_cast<int8>((bLeanRightHeld ? 1 : 0) - (bLeanLeftHeld ? 1 : 0));
	if (bIsSprinting || IsDead())
	{
		NewLean = 0;
	}

	if (NewLean == LeanInput)
	{
		return;
	}

	LeanInput = NewLean;
	if (!HasAuthority())
	{
		Server_SetLean(NewLean);
	}
}

void AShooterCharacter::Server_SetLean_Implementation(int8 NewLean)
{
	if (!StateRateLimiter.AllowCall(GetWorld()->GetTimeSeconds()))
	{
		return;
	}

	LeanInput = static_cast<int8>(FMath::Clamp<int32>(NewLean, -1, 1));
}

void AShooterCharacter::CycleFireMode()
{
	if (IsDead())
	{
		return;
	}

	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->CycleFireMode();
	}
}

void AShooterCharacter::StartBandage()
{
	if (IsDead() || !HealthComponent)
	{
		return;
	}

	// Vendarse ocupa las dos manos.
	StopFire();
	StopAiming();
	StopSprinting();

	HealthComponent->StartBandage();
}

void AShooterCharacter::Jump()
{
	// Saltar cansado no es posible: evita el salto como forma de esquivar.
	if (IsDead() || Stamina < JumpStaminaCost || bIsCrouched)
	{
		return;
	}

	if (!GetCharacterMovement() || GetCharacterMovement()->IsFalling())
	{
		return;
	}

	Stamina -= JumpStaminaCost;
	LastStaminaUseTime = GetWorld()->GetTimeSeconds();

	Super::Jump();
}

void AShooterCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (!HasAuthority() || FallDamageLethalSpeed <= FallDamageMinSpeed)
	{
		return;
	}

	const float FallSpeed = -GetVelocity().Z;
	if (FallSpeed <= FallDamageMinSpeed)
	{
		return;
	}

	// Lineal entre el umbral (10 de daño) y la velocidad mortal: una caída
	// de dos pisos duele, una de seis mata.
	const float Alpha = (FallSpeed - FallDamageMinSpeed) / (FallDamageLethalSpeed - FallDamageMinSpeed);
	const float Damage = FMath::Lerp(10.f, HealthComponent ? HealthComponent->MaxHealth : 100.f, FMath::Clamp(Alpha, 0.f, 1.f));

	UGameplayStatics::ApplyDamage(this, Damage, nullptr, this, UDamageType::StaticClass());

	if (HealthComponent && Alpha > 0.3f)
	{
		HealthComponent->ApplyLegInjury();
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
	if (IsDead() || bIsSprinting || (HealthComponent && HealthComponent->IsBandaging()))
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
	bHoldingBreath = false;
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

	// Con la mira puesta, la tecla de correr contiene la respiración: es el
	// mismo dedo y nadie corre apuntando.
	if (bIsAiming)
	{
		if (!bBreathExhausted && BreathRemaining > 0.f)
		{
			bHoldingBreath = true;
		}
		return;
	}

	if (Stamina < MinStaminaToSprint || (HealthComponent && HealthComponent->IsBandaging()))
	{
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}

	bIsSprinting = true;
	LeanInput = 0;
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
	bHoldingBreath = false;

	if (bIsSprinting)
	{
		LastSprintEndTime = GetWorld()->GetTimeSeconds();
	}

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

	// El servidor lleva su propia cuenta de resistencia: un cliente
	// modificado no puede correr sin límite.
	if (bNewSprinting && Stamina < MinStaminaToSprint)
	{
		return;
	}

	if (bIsSprinting && !bNewSprinting)
	{
		LastSprintEndTime = GetWorld()->GetTimeSeconds();
	}

	bIsSprinting = bNewSprinting;
	if (bNewSprinting)
	{
		bIsAiming = false;
		LeanInput = 0;
	}
	UpdateMaxWalkSpeed();
}

void AShooterCharacter::ToggleCrouch()
{
	if (IsDead())
	{
		return;
	}

	// Agacharse en plena carrera desliza en vez de frenar en seco (RF-4.6),
	// solo si el modo lo permite.
	if (bAllowSlide && bIsSprinting && !bIsSliding && !GetCharacterMovement()->IsFalling())
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
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// Solo se corre hacia delante: de lado o de espaldas el cuerpo no da
	// para sprintar.
	const FVector Velocity2D = GetVelocity().GetSafeNormal2D();
	const bool bMovingForward = Velocity2D.IsNearlyZero()
		|| FVector::DotProduct(Velocity2D, GetActorForwardVector()) > 0.5f;

	float Speed = (bIsSprinting && bMovingForward) ? SprintSpeed : WalkSpeed;
	if (bIsAiming)
	{
		Speed *= AimSpeedMultiplier;
	}

	const float InjuryMultiplier = HealthComponent ? HealthComponent->GetMovementMultiplier() : 1.f;
	Movement->MaxWalkSpeed = Speed * InjuryMultiplier;

	if (!bIsSliding)
	{
		Movement->MaxWalkSpeedCrouched = CrouchSpeed * InjuryMultiplier * (bIsAiming ? 0.8f : 1.f);
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
