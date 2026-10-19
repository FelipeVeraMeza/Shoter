#include "Player/ShooterFirstPersonComponent.h"
#include "Player/ShooterCharacter.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Combat/HealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	/**
	 * Animaciones del maniquí por empuñadura, para armas cuyo DataAsset no
	 * traiga las suyas. Son las de la plantilla de Unreal.
	 */
	UAnimSequenceBase* LoadFallbackAnim(EWeaponHoldType HoldType, bool bReload)
	{
		const TCHAR* Path = nullptr;
		if (HoldType == EWeaponHoldType::Pistol)
		{
			Path = bReload
				? TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload")
				: TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS");
		}
		else
		{
			Path = bReload
				? TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload")
				: TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS");
		}
		return LoadObject<UAnimSequenceBase>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	float SmoothStep(float Alpha)
	{
		return Alpha * Alpha * (3.f - 2.f * Alpha);
	}
}

UShooterFirstPersonComponent::UShooterFirstPersonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Después de la animación: los brazos se colocan con la pose ya evaluada
	// de este fotograma. Antes, el arma temblaría un fotograma por detrás.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UShooterFirstPersonComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AShooterCharacter* Character = GetCharacter())
	{
		LastControlRotation = Character->GetControlRotation();
	}
}

void UShooterFirstPersonComponent::SetupView(USkeletalMeshComponent* InArms, UCameraComponent* InCamera)
{
	Arms = InArms;
	Camera = InCamera;

	if (Camera)
	{
		Camera->SetEnableFirstPersonFieldOfView(true);
		Camera->SetEnableFirstPersonScale(true);
		Camera->SetFirstPersonFieldOfView(ViewModelFOV);
		Camera->SetFirstPersonScale(ViewModelScale);
	}
}

AShooterCharacter* UShooterFirstPersonComponent::GetCharacter() const
{
	return Cast<AShooterCharacter>(GetOwner());
}

bool UShooterFirstPersonComponent::IsLocalViewer() const
{
	const AShooterCharacter* Character = GetCharacter();
	return Character && Character->IsLocallyControlled() && Character->IsPlayerControlled();
}

void UShooterFirstPersonComponent::NotifyShotFired(const UWeaponDataAsset* Data)
{
	if (!Data)
	{
		return;
	}

	// Con la mira puesta el arma va apoyada en el hombro: retrocede menos.
	const float Support = FMath::Lerp(1.f, 0.55f, AimAlpha);
	KickBack = FMath::Min(KickBack + Data->VisualKickBack * Support, 7.f);
	KickPitch = FMath::Min(KickPitch + Data->VisualKickPitch * Support, 9.f);
	KickRoll += FMath::FRandRange(-1.2f, 1.2f) * Support;
}

void UShooterFirstPersonComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AShooterCharacter* Character = GetCharacter();
	if (!Character || !Arms || !Camera)
	{
		return;
	}

	if (!IsLocalViewer())
	{
		Arms->SetHiddenInGame(true);
		return;
	}

	// Cambiar la malla (el maniquí de cada equipo llega con el PlayerState)
	// reinicia la animación de los brazos: hay que volver a prepararla, o se
	// quedarían en pose de referencia.
	AWeaponBase* Weapon = Character->GetCurrentWeapon();
	if (Weapon != EquippedWeapon || Arms->GetSkeletalMeshAsset() != PreparedArmsMesh)
	{
		HandleWeaponChanged(Weapon);
	}

	if (!Weapon || !Weapon->WeaponData || Character->IsDead() || !Weapon->GetViewFrame().bValid)
	{
		bScopeView = false;
		SetViewModelHidden(Weapon, true);
		return;
	}

	UpdateAnimation(Weapon, DeltaTime);
	UpdateMotion(Character, Weapon, DeltaTime);

	// Con visor de aumentos y la mira puesta se mira a través del visor: el
	// HUD dibuja la imagen del visor y el arma deja de verse.
	bScopeView = Weapon->WeaponData->SightType == EWeaponSightType::Scope && AimAlpha > 0.92f;
	SetViewModelHidden(Weapon, bScopeView);

	Camera->SetFirstPersonFieldOfView(FMath::Lerp(ViewModelFOV, AimViewModelFOV, SmoothStep(AimAlpha)));

	ApplyWeaponTransform(Weapon, ComputeWeaponTransform(Weapon));
}

void UShooterFirstPersonComponent::HandleWeaponChanged(AWeaponBase* NewWeapon)
{
	// Solo un cambio de arma real baja el arma; rehacer la animación por un
	// cambio de malla no debe notarse.
	if (NewWeapon != EquippedWeapon)
	{
		EquipElapsed = 0.f;
		LowerAlpha = 1.f;
	}
	EquippedWeapon = NewWeapon;
	PreparedArmsMesh = Arms->GetSkeletalMeshAsset();
	bWasReloading = false;
	bPlayingReload = false;
	IdleAnim = nullptr;
	ReloadAnim = nullptr;

	if (!NewWeapon || !NewWeapon->WeaponData)
	{
		return;
	}

	const UWeaponDataAsset* Data = NewWeapon->WeaponData;
	IdleAnim = Data->FirstPersonIdleAnim ? Data->FirstPersonIdleAnim.Get() : LoadFallbackAnim(Data->HoldType, false);
	ReloadAnim = Data->FirstPersonReloadAnim ? Data->FirstPersonReloadAnim.Get() : LoadFallbackAnim(Data->HoldType, true);

	// Sin pose de brazos, unos brazos en T sujetando el arma serían peor que
	// el arma sola: en ese caso el arma se ancla a la cámara.
	bArmsUsable = IdleAnim && Arms->GetSkeletalMeshAsset() && Arms->DoesSocketExist(HandSocket);

	USkeletalMeshComponent* FirstPersonMesh = NewWeapon->GetFirstPersonMesh();
	if (bArmsUsable)
	{
		FirstPersonMesh->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
		Arms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Arms->PlayAnimation(IdleAnim, true);
		Arms->SetPlayRate(1.f);
	}
	else
	{
		FirstPersonMesh->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);
	}
	FirstPersonMesh->SetRelativeScale3D(FVector::OneVector);

	// La visibilidad "solo el dueño" se resuelve al crear el proxy de render:
	// si el arma llegó por red antes que su dueño, hay que rehacerlo.
	FirstPersonMesh->MarkRenderStateDirty();
	if (NewWeapon->WeaponMesh)
	{
		NewWeapon->WeaponMesh->MarkRenderStateDirty();
	}
}

void UShooterFirstPersonComponent::UpdateAnimation(AWeaponBase* Weapon, float DeltaTime)
{
	if (!bArmsUsable)
	{
		return;
	}

	const bool bReloading = Weapon->IsReloading();

	// La recarga de los brazos dura lo que dura la del arma: la animación se
	// estira o se acorta.
	if (bReloading && !bWasReloading)
	{
		ReloadDuration = FMath::Max(Weapon->GetCurrentReloadDuration(), 0.3f);
		ReloadElapsed = 0.f;

		if (ReloadAnim)
		{
			Arms->PlayAnimation(ReloadAnim, false);
			Arms->SetPlayRate(FMath::Max(ReloadAnim->GetPlayLength(), 0.1f) / ReloadDuration);
			bPlayingReload = true;
		}
	}

	if (bReloading)
	{
		ReloadElapsed += DeltaTime;
	}

	if (bPlayingReload && (!bReloading || !Arms->IsPlaying()))
	{
		Arms->PlayAnimation(IdleAnim, true);
		Arms->SetPlayRate(1.f);
		bPlayingReload = false;
	}

	bWasReloading = bReloading;
}

void UShooterFirstPersonComponent::UpdateMotion(AShooterCharacter* Character, AWeaponBase* Weapon, float DeltaTime)
{
	const UHealthComponent* Health = Character->GetHealthComponent();
	const bool bBandaging = Health && Health->IsBandaging();

	// --- Apuntar ---
	const float ADSTime = FMath::Max(Weapon->GetEffectiveADSTime(), 0.05f);
	const bool bWantsAim = Character->IsAiming() && Character->IsWeaponReady() && !Weapon->IsReloading() && !bBandaging;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bWantsAim ? 1.f : 0.f, DeltaTime, 1.f / ADSTime);

	// --- Posturas ---
	const float Speed = Character->GetVelocity().Size2D();
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bOnGround = Movement && !Movement->IsFalling();

	SprintAlpha = FMath::FInterpTo(SprintAlpha, (Character->IsSprinting() && Speed > 50.f) ? 1.f : 0.f, DeltaTime, 9.f);
	ReloadAlpha = FMath::FInterpTo(ReloadAlpha, Weapon->IsReloading() ? 1.f : 0.f, DeltaTime, 8.f);

	// Al cambiar de arma o vendarse, el arma baja fuera de la vista.
	EquipElapsed += DeltaTime;
	LowerAlpha = FMath::FInterpConstantTo(LowerAlpha, bBandaging ? 1.f : 0.f, DeltaTime, 1.f / EquipTime);

	// --- Balanceo al andar ---
	FVector TargetBob = FVector::ZeroVector;
	if (Speed > 10.f && bOnGround)
	{
		const float SpeedRatio = Speed / FMath::Max(Character->WalkSpeed, 1.f);
		BobPhase += DeltaTime * BobFrequency * FMath::Clamp(SpeedRatio, 0.5f, 1.8f) * UE_TWO_PI;

		// Figura de ocho: lateral a un paso por ciclo, vertical a dos, que
		// es como sube y baja el cuerpo al caminar.
		const float Intensity = BobAmount * FMath::Clamp(SpeedRatio, 0.f, 1.8f) * (1.f - 0.8f * AimAlpha);
		TargetBob.Y = FMath::Sin(BobPhase) * Intensity;
		TargetBob.Z = -FMath::Abs(FMath::Cos(BobPhase)) * Intensity * 0.8f;
	}

	// Respiración en reposo: el arma nunca está del todo quieta.
	const float Time = GetWorld()->GetTimeSeconds();
	TargetBob.Z += FMath::Sin(Time * 1.6f) * 0.12f * (1.f - 0.7f * AimAlpha);

	BobOffset = FMath::VInterpTo(BobOffset, TargetBob, DeltaTime, 10.f);

	// --- Retraso al girar ---
	const FRotator Control = Character->GetControlRotation();
	const FRotator Delta = (Control - LastControlRotation).GetNormalized();
	LastControlRotation = Control;

	FRotator TargetLag = FRotator::ZeroRotator;
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		const float LagScale = LookLag * (1.f - 0.75f * AimAlpha);
		TargetLag.Pitch = FMath::Clamp(-Delta.Pitch / DeltaTime * LagScale, -MaxLookLag, MaxLookLag);
		TargetLag.Yaw = FMath::Clamp(-Delta.Yaw / DeltaTime * LagScale, -MaxLookLag, MaxLookLag);
	}
	LagRotation = FMath::RInterpTo(LagRotation, TargetLag, DeltaTime, 12.f);

	// --- Retroceso visual: golpe instantáneo y vuelta amortiguada ---
	KickBack = FMath::FInterpTo(KickBack, 0.f, DeltaTime, 14.f);
	KickPitch = FMath::FInterpTo(KickPitch, 0.f, DeltaTime, 11.f);
	KickRoll = FMath::FInterpTo(KickRoll, 0.f, DeltaTime, 9.f);
}

FTransform UShooterFirstPersonComponent::ComputeWeaponTransform(const AWeaponBase* Weapon) const
{
	const UWeaponDataAsset* Data = Weapon->WeaponData;
	const FWeaponViewFrame& Frame = Weapon->GetViewFrame();

	const float Aim = SmoothStep(AimAlpha);

	// La mira va de su posición a la cadera al eje de la vista.
	const FVector Sight = FMath::Lerp(Data->HipSightOffset, FVector(Data->AimEyeDistance, 0.f, 0.f), Aim);

	const FVector Offset = BobOffset
		+ FVector(-KickBack, 0.f, 0.f)
		+ SprintOffset * SprintAlpha
		+ FVector(-3.f, -2.f, -5.f) * ReloadAlpha
		+ FVector(-4.f, 0.f, -24.f) * LowerAlpha;

	// Giros sumados en el espacio de la cámara, alrededor de la mira: con el
	// arma encarada, el golpe levanta la boca sin desalinear la mira.
	const FRotator Extra(
		KickPitch + LagRotation.Pitch + SprintRotation.Pitch * SprintAlpha + 8.f * ReloadAlpha - 40.f * LowerAlpha,
		LagRotation.Yaw + SprintRotation.Yaw * SprintAlpha - 10.f * ReloadAlpha,
		KickRoll + SprintRotation.Roll * SprintAlpha - 25.f * ReloadAlpha + BobOffset.Y * 1.5f);

	const FQuat Rotation = FQuat(Extra) * Frame.MeshToView;
	const FVector Location = Sight + Offset - Rotation.RotateVector(Frame.SightLocation);

	return FTransform(Rotation, Location);
}

void UShooterFirstPersonComponent::ApplyWeaponTransform(AWeaponBase* Weapon, const FTransform& WeaponInCamera)
{
	USkeletalMeshComponent* FirstPersonMesh = Weapon->GetFirstPersonMesh();

	if (!bArmsUsable)
	{
		FirstPersonMesh->SetRelativeTransform(WeaponInCamera);
		return;
	}

	if (FirstPersonMesh->GetAttachParent() != Arms || FirstPersonMesh->GetAttachSocketName() != HandSocket)
	{
		FirstPersonMesh->AttachToComponent(Arms, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
	}

	// El arma cuelga de la mano: arma = mano * brazos. Para dejar el arma en
	// WeaponInCamera, los brazos van a mano⁻¹ * arma.
	const FTransform Hand = Arms->GetSocketTransform(HandSocket, RTS_Component);
	FTransform ArmsInCamera = Hand.Inverse() * WeaponInCamera;
	ArmsInCamera.SetScale3D(FVector::OneVector);
	Arms->SetRelativeTransform(ArmsInCamera);
}

void UShooterFirstPersonComponent::SetViewModelHidden(AWeaponBase* Weapon, bool bHidden)
{
	Arms->SetHiddenInGame(bHidden || !bArmsUsable);

	if (Weapon)
	{
		Weapon->SetFirstPersonHidden(bHidden);
	}
}
