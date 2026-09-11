#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Curves/CurveFloat.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Combat/LagCompensationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCastShadow(false);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponData)
	{
		if (WeaponData->WeaponMesh)
		{
			WeaponMesh->SetSkeletalMeshAsset(WeaponData->WeaponMesh);
		}

		if (HasAuthority())
		{
			CurrentAmmo = WeaponData->MagazineSize;
			ReserveAmmo = WeaponData->MaxReserveAmmo;
		}
	}
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, CurrentAmmo);
	DOREPLIFETIME(AWeaponBase, ReserveAmmo);
	DOREPLIFETIME(AWeaponBase, bIsReloading);
}

void AWeaponBase::SetOwningCharacter(AShooterCharacter* InCharacter)
{
	OwningCharacter = InCharacter;
	SetOwner(InCharacter);
}

void AWeaponBase::ResetAmmo()
{
	if (!HasAuthority() || !WeaponData)
	{
		return;
	}

	// Cancela también una recarga en curso: el reinicio debe ser instantáneo.
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);

	CurrentAmmo = WeaponData->MagazineSize;
	ReserveAmmo = WeaponData->MaxReserveAmmo;
	bIsReloading = false;
	ConsecutiveShots = 0;
	AccumulatedSpread = 0.f;
}

void AWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (WeaponData && AccumulatedSpread > 0.f)
	{
		AccumulatedSpread = FMath::Max(AccumulatedSpread - WeaponData->SpreadRecoveryRate * DeltaSeconds, 0.f);
	}

	if (!OwningCharacter || !OwningCharacter->IsLocallyControlled())
	{
		return;
	}

	if (bWantsToFire && WeaponData && WeaponData->FireMode == EWeaponFireMode::Automatic && CanFire())
	{
		FireOnce();
	}
}

bool AWeaponBase::CanFire() const
{
	if (!WeaponData || bIsReloading || !OwningCharacter || OwningCharacter->IsDead())
	{
		return false;
	}

	if (CurrentAmmo <= 0)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	return (Now - LastFireTime) >= WeaponData->GetSecondsBetweenShots();
}

void AWeaponBase::TryStartFire()
{
	if (!WeaponData)
	{
		return;
	}

	bWantsToFire = true;

	if (CurrentAmmo <= 0 && !bIsReloading)
	{
		if (WeaponData->EmptySound && OwningCharacter && OwningCharacter->IsLocallyControlled())
		{
			UGameplayStatics::PlaySoundAtLocation(this, WeaponData->EmptySound, GetActorLocation());
		}
		TryReload();
		return;
	}

	if (!CanFire())
	{
		return;
	}

	if (WeaponData->FireMode == EWeaponFireMode::Burst)
	{
		ShotsInCurrentBurst = 0;
		ContinueBurst();
		return;
	}

	FireOnce();
}

void AWeaponBase::StopFire()
{
	bWantsToFire = false;
	ConsecutiveShots = 0;
}

void AWeaponBase::ContinueBurst()
{
	if (!WeaponData || !CanFire())
	{
		return;
	}

	FireOnce();
	++ShotsInCurrentBurst;

	if (ShotsInCurrentBurst < WeaponData->BurstCount)
	{
		GetWorldTimerManager().SetTimer(BurstTimerHandle, this, &AWeaponBase::ContinueBurst,
			WeaponData->GetSecondsBetweenShots(), false);
	}
}

float AWeaponBase::GetCurrentSpreadDegrees() const
{
	if (!WeaponData)
	{
		return 0.f;
	}

	const bool bAiming = OwningCharacter && OwningCharacter->IsAiming();
	const float BaseSpread = bAiming ? WeaponData->SpreadADS : WeaponData->SpreadHip;

	return FMath::Min(BaseSpread + AccumulatedSpread, WeaponData->SpreadMax);
}

FVector AWeaponBase::CalculateShotDirection(const FVector& AimDirection) const
{
	const float SpreadDegrees = GetCurrentSpreadDegrees();
	if (SpreadDegrees <= 0.f)
	{
		return AimDirection;
	}

	return FMath::VRandCone(AimDirection, FMath::DegreesToRadians(SpreadDegrees));
}

void AWeaponBase::FireOnce()
{
	if (!WeaponData || !OwningCharacter)
	{
		return;
	}

	const UCameraComponent* Camera = OwningCharacter->GetFirstPersonCamera();
	if (!Camera)
	{
		return;
	}

	LastFireTime = GetWorld()->GetTimeSeconds();

	const FVector TraceStart = Camera->GetComponentLocation();
	const FVector ShotDirection = CalculateShotDirection(Camera->GetForwardVector());

	AccumulatedSpread = FMath::Min(AccumulatedSpread + WeaponData->SpreadPerShot, WeaponData->SpreadMax);
	++ConsecutiveShots;

	// Predicción local: el HUD responde al instante, el servidor confirma después.
	if (!HasAuthority())
	{
		CurrentAmmo = FMath::Max(CurrentAmmo - 1, 0);
	}

	if (OwningCharacter->IsLocallyControlled())
	{
		ApplyRecoilToOwner();
		PlayLocalFireEffects();

		// Cuenta el disparo para la precisión acumulada (RF-9.6).
		if (AShooterPlayerController* ShooterPC =
			Cast<AShooterPlayerController>(OwningCharacter->GetController()))
		{
			ShooterPC->RegisterShotFired();
		}
	}

	Server_Fire(TraceStart, ShotDirection);
}

void AWeaponBase::ApplyRecoilToOwner()
{
	if (!WeaponData || !OwningCharacter)
	{
		return;
	}

	const float ShotIndex = static_cast<float>(ConsecutiveShots);

	const float Vertical = WeaponData->RecoilVertical
		? WeaponData->RecoilVertical->GetFloatValue(ShotIndex)
		: WeaponData->BaseRecoilVertical;

	const float Horizontal = WeaponData->RecoilHorizontal
		? WeaponData->RecoilHorizontal->GetFloatValue(ShotIndex)
		: FMath::FRandRange(-WeaponData->BaseRecoilHorizontal, WeaponData->BaseRecoilHorizontal);

	OwningCharacter->AddControllerPitchInput(-Vertical);
	OwningCharacter->AddControllerYawInput(Horizontal);
}

void AWeaponBase::PlayLocalFireEffects()
{
	if (!WeaponData)
	{
		return;
	}

	if (WeaponData->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->FireSound, GetActorLocation());
	}

	if (WeaponData->MuzzleFlash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponData->MuzzleFlash, WeaponMesh, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
}

void AWeaponBase::SpawnImpactEffect(const FVector& Location) const
{
	if (WeaponData && WeaponData->ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WeaponData->ImpactEffect, Location);
	}
}

float AWeaponBase::GetRewindTimeForOwner() const
{
	const float Now = GetWorld()->GetTimeSeconds();

	if (!OwningCharacter)
	{
		return Now;
	}

	const APlayerState* PlayerState = OwningCharacter->GetPlayerState();
	if (!PlayerState)
	{
		return Now;
	}

	// Ping es ida y vuelta; el rebobinado necesita solo el trayecto de ida,
	// más el retardo de interpolación con el que el cliente ve a los demás.
	const float SingleTripSeconds = (PlayerState->GetPingInMilliseconds() / 1000.f) * 0.5f;
	const float InterpolationDelay = 0.03f;

	return Now - FMath::Min(SingleTripSeconds + InterpolationDelay, 1.f);
}

bool AWeaponBase::Server_Fire_Validate(const FVector_NetQuantize& TraceStart, const FVector_NetQuantizeNormal& ShotDirection)
{
	return ShotDirection.IsNormalized() || ShotDirection.SizeSquared() > KINDA_SMALL_NUMBER;
}

void AWeaponBase::Server_Fire_Implementation(const FVector_NetQuantize& TraceStart, const FVector_NetQuantizeNormal& ShotDirection)
{
	if (!WeaponData || !OwningCharacter || OwningCharacter->IsDead() || bIsReloading)
	{
		return;
	}

	if (CurrentAmmo <= 0)
	{
		return;
	}

	// El servidor valida que el disparo parta de donde el jugador realmente está.
	const float MaxAllowedOffset = 350.f;
	if (FVector::DistSquared(TraceStart, OwningCharacter->GetActorLocation()) > FMath::Square(MaxAllowedOffset))
	{
		return;
	}

	CurrentAmmo = FMath::Max(CurrentAmmo - 1, 0);

	const FVector Direction = ShotDirection.GetSafeNormal();
	const FVector TraceEnd = TraceStart + Direction * WeaponData->Range;

	// Trazado contra geometría estática: los personajes se resuelven aparte,
	// con sus posiciones rebobinadas.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	FHitResult GeometryHit;
	const bool bHitGeometry = GetWorld()->LineTraceSingleByChannel(GeometryHit, TraceStart, TraceEnd, ECC_Visibility, Params);
	const float GeometryDistance = bHitGeometry ? GeometryHit.Distance : WeaponData->Range;

	AShooterCharacter* HitCharacter = nullptr;
	FVector CharacterImpact = FVector::ZeroVector;
	bool bHeadshot = false;

	const bool bHitCharacter = ULagCompensationComponent::FindBestCharacterHit(
		GetWorld(), TraceStart, TraceEnd, GetRewindTimeForOwner(), OwningCharacter,
		HitCharacter, CharacterImpact, bHeadshot);

	const float CharacterDistance = bHitCharacter ? FVector::Dist(TraceStart, CharacterImpact) : TNumericLimits<float>::Max();

	Multicast_PlayFireEffects();

	if (bHitCharacter && CharacterDistance <= GeometryDistance)
	{
		float FinalDamage = WeaponData->GetDamageAtDistance(CharacterDistance);
		if (bHeadshot)
		{
			FinalDamage *= WeaponData->HeadshotMultiplier;
		}

		AController* InstigatorController = OwningCharacter->GetController();
		UGameplayStatics::ApplyDamage(HitCharacter, FinalDamage, InstigatorController, this, UDamageType::StaticClass());

		if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(InstigatorController))
		{
			ShooterPC->Client_NotifyHitConfirmed(bHeadshot);
		}

		Multicast_PlayImpactEffect(CharacterImpact);
	}
	else if (bHitGeometry)
	{
		Multicast_PlayImpactEffect(GeometryHit.ImpactPoint);
	}
}

void AWeaponBase::Multicast_PlayFireEffects_Implementation()
{
	// El dueño ya reprodujo sus efectos de forma predictiva.
	if (OwningCharacter && OwningCharacter->IsLocallyControlled())
	{
		return;
	}

	PlayLocalFireEffects();
}

void AWeaponBase::Multicast_PlayImpactEffect_Implementation(const FVector_NetQuantize& ImpactLocation)
{
	SpawnImpactEffect(ImpactLocation);
}

void AWeaponBase::TryReload()
{
	if (!WeaponData || bIsReloading)
	{
		return;
	}

	if (CurrentAmmo >= WeaponData->MagazineSize || ReserveAmmo <= 0)
	{
		return;
	}

	if (WeaponData->ReloadSound && OwningCharacter && OwningCharacter->IsLocallyControlled())
	{
		UGameplayStatics::PlaySoundAtLocation(this, WeaponData->ReloadSound, GetActorLocation());
	}

	Server_Reload();
}

void AWeaponBase::Server_Reload_Implementation()
{
	if (!WeaponData || bIsReloading || CurrentAmmo >= WeaponData->MagazineSize || ReserveAmmo <= 0)
	{
		return;
	}

	bIsReloading = true;
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, WeaponData->ReloadTime, false);
}

void AWeaponBase::FinishReload()
{
	if (!WeaponData || !HasAuthority())
	{
		return;
	}

	const int32 Needed = WeaponData->MagazineSize - CurrentAmmo;
	const int32 Taken = FMath::Min(Needed, ReserveAmmo);

	CurrentAmmo += Taken;
	ReserveAmmo -= Taken;
	bIsReloading = false;
	ConsecutiveShots = 0;
	AccumulatedSpread = 0.f;
}
