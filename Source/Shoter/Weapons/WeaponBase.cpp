#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Curves/CurveFloat.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Combat/ShooterBallisticsSubsystem.h"
#include "Combat/ShooterPhysicalMaterial.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

float FShooterDamageModel::GetDamage(float Distance, EShooterHitZone Zone) const
{
	const UWeaponDataAsset* WeaponData = Data.Get();
	if (!WeaponData)
	{
		return 0.f;
	}

	// El accesorio de cañón alarga el alcance efectivo, así que la caída se
	// evalúa sobre la distancia corregida antes de escalar el daño.
	const float AdjustedDistance = (RangeMultiplier > 0.f) ? Distance / RangeMultiplier : Distance;

	return WeaponData->GetDamageAtDistance(AdjustedDistance) * DamageMultiplier
		* WeaponData->GetZoneMultiplier(Zone);
}

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCastShadow(false);

	// La munición y el selector cambian a ritmo humano: no hace falta
	// replicar el arma a la frecuencia del personaje.
	SetNetUpdateFrequency(20.f);
	SetMinNetUpdateFrequency(2.f);
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
			CurrentFireMode = WeaponData->GetFireModes()[0];
			CurrentAmmo = GetEffectiveMagazineSize() + (WeaponData->bClosedBolt ? 1 : 0);
			RefillMagazines();
		}
	}
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, CurrentAmmo);
	DOREPLIFETIME(AWeaponBase, ReserveAmmo);
	DOREPLIFETIME(AWeaponBase, bIsReloading);
	DOREPLIFETIME(AWeaponBase, Attachments);
	DOREPLIFETIME(AWeaponBase, CurrentFireMode);

	// El detalle de cada cargador solo le importa a quien lo lleva.
	DOREPLIFETIME_CONDITION(AWeaponBase, SpareMagazines, COND_OwnerOnly);
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

	CurrentAmmo = GetEffectiveMagazineSize() + (WeaponData->bClosedBolt ? 1 : 0);
	RefillMagazines();
	bIsReloading = false;
	ConsecutiveShots = 0;
	AccumulatedSpread = 0.f;
}

void AWeaponBase::RefillMagazines()
{
	SpareMagazines.Reset();

	if (!WeaponData)
	{
		UpdateReserveAmmo();
		return;
	}

	// La reserva se expresa en cargadores llenos: la munición suelta no
	// existe en el bolsillo de un soldado.
	const int32 MagazineSize = GetEffectiveMagazineSize();
	int32 Remaining = WeaponData->MaxReserveAmmo;
	while (Remaining > 0)
	{
		const int32 Rounds = FMath::Min(Remaining, MagazineSize);
		SpareMagazines.Add(Rounds);
		Remaining -= Rounds;
	}

	UpdateReserveAmmo();
}

void AWeaponBase::UpdateReserveAmmo()
{
	int32 Total = 0;
	for (const int32 Rounds : SpareMagazines)
	{
		Total += Rounds;
	}
	ReserveAmmo = Total;
}

int32 AWeaponBase::FindFullestMagazine() const
{
	int32 BestIndex = INDEX_NONE;
	int32 BestRounds = 0;

	for (int32 Index = 0; Index < SpareMagazines.Num(); ++Index)
	{
		if (SpareMagazines[Index] > BestRounds)
		{
			BestRounds = SpareMagazines[Index];
			BestIndex = Index;
		}
	}

	return BestIndex;
}

float AWeaponBase::GetTimeSinceAmmoCheck() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - LastAmmoCheckTime : 1000.f;
}

void AWeaponBase::CycleFireMode()
{
	if (!WeaponData || bIsReloading)
	{
		return;
	}

	const TArray<EWeaponFireMode> Modes = WeaponData->GetFireModes();
	if (Modes.Num() < 2)
	{
		return;
	}

	const int32 CurrentIndex = Modes.Find(CurrentFireMode);
	const EWeaponFireMode NextMode = Modes[(CurrentIndex + 1) % Modes.Num()];

	// Predicción local: el cambio se nota al instante.
	CurrentFireMode = NextMode;
	StopFire();

	if (!HasAuthority())
	{
		Server_SetFireMode(NextMode);
	}
}

void AWeaponBase::Server_SetFireMode_Implementation(EWeaponFireMode NewMode)
{
	// Solo se aceptan posiciones que el arma tenga de verdad.
	if (WeaponData && WeaponData->GetFireModes().Contains(NewMode))
	{
		CurrentFireMode = NewMode;
	}
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

	if (bWantsToFire && WeaponData && CurrentFireMode == EWeaponFireMode::Automatic && CanFire())
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

	// Al salir del sprint el arma tarda en volver a la posición de tiro.
	if (!OwningCharacter->IsWeaponReady())
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

	if (CurrentFireMode == EWeaponFireMode::Burst)
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
	float BaseSpread = bAiming ? WeaponData->SpreadADS : WeaponData->SpreadHip;

	// Postura y movimiento: disparar corriendo desde la cadera es casi a
	// ciegas; agachado, el arma se apoya y se estabiliza.
	if (OwningCharacter)
	{
		const UCharacterMovementComponent* Movement = OwningCharacter->GetCharacterMovement();
		const float Speed = OwningCharacter->GetVelocity().Size2D();
		const float SpeedFraction = FMath::Clamp(Speed / FMath::Max(OwningCharacter->WalkSpeed, 1.f), 0.f, 1.5f);

		BaseSpread *= 1.f + SpeedFraction * (bAiming ? 0.6f : 1.2f);

		if (Movement && Movement->IsFalling())
		{
			BaseSpread *= 2.5f;
		}
		else if (OwningCharacter->bIsCrouched)
		{
			BaseSpread *= 0.75f;
		}
	}

	// El accesorio escala también el tope, o una mira precisa quedaría anulada
	// por el límite del arma en cuanto se disparara en ráfaga. El tope se
	// duplica porque el movimiento puede superar la dispersión de ráfaga.
	const float Multiplier = Attachments.GetSpreadMultiplier();
	const float Capped = FMath::Min(BaseSpread + AccumulatedSpread, WeaponData->SpreadMax * 2.f);

	return Capped * Multiplier;
}

int32 AWeaponBase::GetEffectiveMagazineSize() const
{
	if (!WeaponData)
	{
		return 0;
	}

	// Al menos una bala: un multiplicador agresivo no puede dejar el arma
	// inservible por redondeo.
	return FMath::Max(1,
		FMath::RoundToInt(WeaponData->MagazineSize * Attachments.GetMagazineMultiplier()));
}

float AWeaponBase::GetEffectiveReloadTime() const
{
	return WeaponData
		? WeaponData->ReloadTime * Attachments.GetReloadTimeMultiplier()
		: 0.f;
}

float AWeaponBase::GetEffectiveADSTime() const
{
	return WeaponData
		? WeaponData->ADSTime * Attachments.GetADSTimeMultiplier()
		: 0.f;
}

float AWeaponBase::GetEffectiveDamageAtDistance(float Distance) const
{
	return MakeDamageSnapshot().GetDamage(Distance, EShooterHitZone::Torso);
}

FShooterDamageModel AWeaponBase::MakeDamageSnapshot() const
{
	FShooterDamageModel Model;
	Model.Data = WeaponData.Get();
	Model.DamageMultiplier = Attachments.GetDamageMultiplier();
	Model.RangeMultiplier = Attachments.GetEffectiveRangeMultiplier();

	if (WeaponData)
	{
		Model.Range = WeaponData->Range;
		Model.VelocityDecayDistance = WeaponData->VelocityDecayDistance;
	}

	return Model;
}

void AWeaponBase::SetAttachments(const FWeaponAttachmentSet& NewAttachments)
{
	if (!HasAuthority())
	{
		return;
	}

	Attachments = NewAttachments;
	OnRep_Attachments();
}

void AWeaponBase::OnRep_Attachments()
{
	// Un cargador ampliado se nota de inmediato; uno reducido no descarta
	// balas ya cargadas, solo limita las futuras recargas.
	const int32 Chambered = (WeaponData && WeaponData->bClosedBolt) ? 1 : 0;
	CurrentAmmo = FMath::Min(CurrentAmmo, GetEffectiveMagazineSize() + Chambered);
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

FVector AWeaponBase::ApplyZeroing(const FVector& Direction) const
{
	if (!WeaponData || !WeaponData->bUseBallistics)
	{
		return Direction;
	}

	const float GravityZ = GetWorld() ? GetWorld()->GetGravityZ() : -980.f;
	const float Elevation = UShooterBallisticsSubsystem::ComputeZeroingAngle(WeaponData->MuzzleVelocity,
		WeaponData->VelocityDecayDistance, WeaponData->ZeroingDistance, GravityZ);

	if (FMath::IsNearlyZero(Elevation))
	{
		return Direction;
	}

	// Se eleva sobre el eje lateral del disparo: vale igual mirando arriba o abajo.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return Direction;
	}

	return Direction.RotateAngleAxis(-Elevation, Right);
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
	const FVector ShotDirection = ApplyZeroing(CalculateShotDirection(Camera->GetForwardVector()));

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

		// Y para el desbloqueo de accesorios por uso del arma (RF-9.4).
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (UShooterProgressionSubsystem* Progression =
				GameInstance->GetSubsystem<UShooterProgressionSubsystem>())
			{
				Progression->RecordWeaponShot(WeaponData->GetFName());
			}
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

	// La empuñadura reduce ambos ejes por igual (RF-5.10); agachado, el
	// cuerpo absorbe parte del golpe.
	float RecoilMultiplier = Attachments.GetRecoilMultiplier();
	if (OwningCharacter->bIsCrouched)
	{
		RecoilMultiplier *= 0.8f;
	}

	// El personaje reparte el golpe en unos milisegundos y recupera parte
	// después: aplicarlo de golpe a la cámara se siente a salto, no a arma.
	OwningCharacter->AddRecoil(Vertical * RecoilMultiplier, Horizontal * RecoilMultiplier,
		WeaponData->RecoilRecoveryFraction);
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

void AWeaponBase::SpawnImpactEffect(const FVector& Location, const FVector& Normal,
	const UPhysicalMaterial* SurfaceMaterial) const
{
	// Cada superficie suena y salta a su manera; el efecto del arma queda
	// como respaldo para las que no tengan uno propio.
	const UShooterPhysicalMaterial* Surface = Cast<UShooterPhysicalMaterial>(SurfaceMaterial);

	UNiagaraSystem* Effect = (Surface && Surface->ImpactEffect)
		? Surface->ImpactEffect.Get()
		: (WeaponData ? WeaponData->ImpactEffect.Get() : nullptr);

	if (Effect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, Location, Normal.Rotation());
	}

	if (Surface && Surface->ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Surface->ImpactSound, Location);
	}
}

void AWeaponBase::BroadcastImpact(const FVector& Location, const FVector& Normal,
	const UPhysicalMaterial* SurfaceMaterial)
{
	if (HasAuthority())
	{
		Multicast_PlayImpactEffect(Location, Normal, SurfaceMaterial);
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
	// Descarte silencioso: contestar al cliente que excedió el límite le diría
	// dónde está el umbral (RNF-3.5).
	if (!FireRateLimiter.AllowCall(GetWorld()->GetTimeSeconds()))
	{
		return;
	}

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

	Multicast_PlayFireEffects();

	UShooterBallisticsSubsystem* Ballistics = GetWorld()->GetSubsystem<UShooterBallisticsSubsystem>();
	if (!Ballistics)
	{
		return;
	}

	// Se pasa el desfase y no un instante fijo: una bala que vuela 0,3 s debe
	// encontrar a los demás donde el tirador los vería en ese momento.
	const float RewindOffset = GetWorld()->GetTimeSeconds() - GetRewindTimeForOwner();
	const FVector Direction = ShotDirection.GetSafeNormal();

	if (WeaponData->bUseBallistics)
	{
		Ballistics->FireBullet(this, OwningCharacter, TraceStart, Direction, RewindOffset);
	}
	else
	{
		Ballistics->FireHitscan(this, OwningCharacter, TraceStart, Direction, RewindOffset);
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

void AWeaponBase::Multicast_PlayImpactEffect_Implementation(const FVector_NetQuantize& ImpactLocation,
	const FVector_NetQuantizeNormal& ImpactNormal, const UPhysicalMaterial* SurfaceMaterial)
{
	SpawnImpactEffect(ImpactLocation, ImpactNormal, SurfaceMaterial);
}

void AWeaponBase::TryReload()
{
	if (!WeaponData || bIsReloading)
	{
		return;
	}

	// Pulsar recargar también es mirar el cargador: el HUD realista solo
	// muestra la munición en ese momento.
	LastAmmoCheckTime = GetWorld()->GetTimeSeconds();

	const int32 Fullest = FindFullestMagazine();
	if (Fullest == INDEX_NONE)
	{
		return;
	}

	// Cambiar a un cargador con menos balas de las que ya hay en el arma no
	// tiene sentido: se trata como una simple comprobación.
	const int32 Chambered = (WeaponData->bClosedBolt && CurrentAmmo > 0) ? 1 : 0;
	if (SpareMagazines[Fullest] <= CurrentAmmo - Chambered)
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
	if (!WeaponData || bIsReloading)
	{
		return;
	}

	const int32 Fullest = FindFullestMagazine();
	const int32 Chambered = (WeaponData->bClosedBolt && CurrentAmmo > 0) ? 1 : 0;
	if (Fullest == INDEX_NONE || SpareMagazines[Fullest] <= CurrentAmmo - Chambered)
	{
		return;
	}

	// En vacío hay que montar el arma además de cambiar el cargador.
	const float BaseTime = CurrentAmmo > 0 ? WeaponData->ReloadTime : WeaponData->GetEmptyReloadTime();
	const float ReloadDuration = BaseTime * Attachments.GetReloadTimeMultiplier();

	bIsReloading = true;
	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, ReloadDuration, false);
}

void AWeaponBase::FinishReload()
{
	if (!WeaponData || !HasAuthority())
	{
		return;
	}

	bIsReloading = false;
	ConsecutiveShots = 0;
	AccumulatedSpread = 0.f;

	const int32 Fullest = FindFullestMagazine();
	if (Fullest == INDEX_NONE)
	{
		return;
	}

	const int32 NewMagazine = SpareMagazines[Fullest];
	SpareMagazines.RemoveAt(Fullest);

	// Con cerrojo cerrado, la bala de la recámara se queda en el arma; el
	// resto del cargador viejo vuelve a la bolsa: ni se pierde ni se suma.
	const int32 Chambered = (WeaponData->bClosedBolt && CurrentAmmo > 0) ? 1 : 0;
	const int32 OldMagazine = CurrentAmmo - Chambered;
	if (OldMagazine > 0)
	{
		SpareMagazines.Add(OldMagazine);
	}

	CurrentAmmo = NewMagazine + Chambered;
	UpdateReserveAmmo();
}
