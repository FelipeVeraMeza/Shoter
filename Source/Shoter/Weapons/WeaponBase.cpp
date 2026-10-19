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
#include "Effects/ShooterEffects.h"
#include "Player/ShooterFirstPersonComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "AnimationRuntime.h"

namespace
{
	constexpr int32 SightPartCount = 8;

	/** Posición de un socket en el espacio de la malla, en la pose de referencia. */
	bool GetRefPoseSocketLocation(const USkeletalMesh* Mesh, FName SocketName, FVector& OutLocation)
	{
		FTransform SocketLocal;
		int32 BoneIndex = INDEX_NONE;
		int32 SocketIndex = INDEX_NONE;
		if (!Mesh || !Mesh->FindSocketInfo(SocketName, SocketLocal, BoneIndex, SocketIndex))
		{
			return false;
		}

		FTransform BoneTransform = FTransform::Identity;
		if (BoneIndex != INDEX_NONE)
		{
			BoneTransform = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), BoneIndex);
		}

		OutLocation = (SocketLocal * BoneTransform).GetLocation();
		return true;
	}

	/** Pieza del visor o del fogonazo dentro de la primera persona del tirador. */
	void ConfigureFirstPersonPart(UPrimitiveComponent* Part)
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		Part->SetOnlyOwnerSee(true);
		Part->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	}

	/** Caja o cilindro colocado en un marco local: posición del centro y tamaño en cm. */
	FTransform MakePartTransform(const FVector& Center, const FVector& Size, bool bCylinderAlongX)
	{
		if (bCylinderAlongX)
		{
			// El cilindro del motor tiene el eje en Z: se tumba a lo largo de X.
			return FTransform(FRotator(-90.f, 0.f, 0.f), Center, FVector(Size.Y / 100.f, Size.Z / 100.f, Size.X / 100.f));
		}
		return FTransform(FRotator::ZeroRotator, Center, Size / 100.f);
	}

	/** Planos del fogonazo en el marco de la boca: una estrella de frente y dos lenguas a lo largo del cañón. */
	void MakeFlashTransforms(float Scale, float Roll, FTransform OutLocal[3])
	{
		OutLocal[0] = FTransform(FRotator(90.f, 0.f, 0.f) + FRotator(0.f, 0.f, Roll), FVector(1.5f, 0.f, 0.f) * Scale,
			FVector(0.1f, 0.1f, 1.f) * Scale);
		OutLocal[1] = FTransform(FRotator(0.f, 0.f, Roll), FVector(8.f, 0.f, 0.f) * Scale,
			FVector(0.16f, 0.07f, 1.f) * Scale);
		OutLocal[2] = FTransform(FRotator(0.f, 0.f, Roll + 90.f), FVector(8.f, 0.f, 0.f) * Scale,
			FVector(0.16f, 0.07f, 1.f) * Scale);
	}
}

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

	// Representación en el mundo: los demás la ven en la mano del cuerpo y
	// proyecta sombra; el propio tirador no la ve (usa FirstPersonMesh).
	WeaponMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	WeaponMesh->SetOwnerNoSee(true);
	WeaponMesh->SetCastShadow(true);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(WeaponMesh);
	ConfigureFirstPersonPart(FirstPersonMesh);

	for (int32 Index = 0; Index < SightPartCount; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("SightPart%d"), Index));
		Part->SetupAttachment(FirstPersonMesh);
		ConfigureFirstPersonPart(Part);
		Part->SetHiddenInGame(true);
		SightParts.Add(Part);
	}

	SightGlass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightGlass"));
	SightGlass->SetupAttachment(FirstPersonMesh);
	ConfigureFirstPersonPart(SightGlass);
	SightGlass->SetHiddenInGame(true);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* FirstPersonPlane = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FlashFP%d"), Index));
		FirstPersonPlane->SetupAttachment(FirstPersonMesh);
		ConfigureFirstPersonPart(FirstPersonPlane);
		FirstPersonPlane->SetHiddenInGame(true);
		FirstPersonFlash.Add(FirstPersonPlane);

		UStaticMeshComponent* WorldPlane = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FlashTP%d"), Index));
		WorldPlane->SetupAttachment(WeaponMesh);
		WorldPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WorldPlane->SetGenerateOverlapEvents(false);
		WorldPlane->SetCastShadow(false);
		WorldPlane->SetOwnerNoSee(true);
		WorldPlane->SetHiddenInGame(true);
		ThirdPersonFlash.Add(WorldPlane);
	}

	// La luz del fogonazo cuelga de la malla del mundo: ilumina la escena
	// para todos, también para el tirador, aunque él no vea esa malla.
	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(WeaponMesh);
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(0.f);
	FlashLight->SetAttenuationRadius(650.f);
	FlashLight->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
	FlashLight->SetCastShadows(false);
	FlashLight->SetVisibility(false);

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
			FirstPersonMesh->SetSkeletalMeshAsset(WeaponData->WeaponMesh);
		}

		BuildViewFrame();

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

	UpdateMuzzleFlash(DeltaSeconds);

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

	LastFireTime = GetWorld()->GetTimeSeconds();

	// Origen y dirección salen de la vista del personaje, no de la cámara:
	// la cámara de un bot no gira con su controlador (solo lo hace la del
	// jugador que mira a través de ella), y sus disparos saldrían siempre
	// horizontales.
	const FVector TraceStart = OwningCharacter->GetShotOrigin();
	const FVector ShotDirection = ApplyZeroing(CalculateShotDirection(OwningCharacter->GetAimRotation().Vector()));

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
		PlayLocalFireEffects(ShotDirection);

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

void AWeaponBase::PlayLocalFireEffects(const FVector& Direction)
{
	if (!WeaponData || !ShooterEffects::ShouldSpawnCosmetics(GetWorld()))
	{
		return;
	}

	USkeletalMeshComponent* VisibleMesh = GetVisibleMesh();
	const FVector MuzzleLocation = (VisibleMesh && ViewFrame.bValid)
		? VisibleMesh->GetComponentTransform().TransformPosition(ViewFrame.MuzzleLocation)
		: GetActorLocation();

	// Sin sonido propio se usa el disparo de la plantilla, afinado por
	// calibre: es el único sonido de arma que trae el proyecto.
	USoundBase* FireSound = WeaponData->FireSound ? WeaponData->FireSound.Get() : ShooterEffects::GetDefaultGunshot();
	if (FireSound)
	{
		const float Pitch = WeaponData->FireSoundPitch * FMath::FRandRange(0.96f, 1.04f);
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation, WeaponData->FireSoundVolume, Pitch,
			0.f, ShooterEffects::GetGunshotAttenuation());
	}

	if (WeaponData->MuzzleFlash && VisibleMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(WeaponData->MuzzleFlash, VisibleMesh, NAME_None,
			ViewFrame.MuzzleLocation, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		ShowMuzzleFlash();
	}

	SpawnShotCosmetics(Direction);

	// El golpe visual del arma contra el hombro solo lo ve quien dispara.
	if (IsViewedInFirstPerson())
	{
		if (UShooterFirstPersonComponent* FirstPerson = OwningCharacter->GetFirstPersonView())
		{
			FirstPerson->NotifyShotFired(WeaponData);
		}
	}
}

bool AWeaponBase::IsViewedInFirstPerson() const
{
	return OwningCharacter && OwningCharacter->IsLocallyControlled() && OwningCharacter->IsPlayerControlled();
}

USkeletalMeshComponent* AWeaponBase::GetVisibleMesh() const
{
	return IsViewedInFirstPerson() ? FirstPersonMesh.Get() : WeaponMesh.Get();
}

void AWeaponBase::SpawnShotCosmetics(const FVector& Direction)
{
	USkeletalMeshComponent* Mesh = GetVisibleMesh();
	if (!WeaponData || !Mesh || !ViewFrame.bValid)
	{
		return;
	}

	const FTransform MeshTransform = Mesh->GetComponentTransform();
	const FVector Muzzle = MeshTransform.TransformPosition(ViewFrame.MuzzleLocation);

	// Una de cada N balas es trazadora, como en los cargadores militares.
	if (WeaponData->TracerEveryNShots > 0 && ++ShotsSinceTracer >= WeaponData->TracerEveryNShots)
	{
		ShotsSinceTracer = 0;
		ShooterEffects::SpawnTracer(GetWorld(), Muzzle + Direction * 40.f, Direction,
			WeaponData->MuzzleVelocity, OwningCharacter);
	}

	if (WeaponData->ShellLength > 0.f)
	{
		const FVector Forward = MeshTransform.TransformVectorNoScale(ViewFrame.Forward);
		const FVector Right = MeshTransform.TransformVectorNoScale(ViewFrame.Right);
		const FVector Up = MeshTransform.TransformVectorNoScale(ViewFrame.Up);

		// Sale a la derecha y algo hacia arriba y atrás, como de una ventana
		// de expulsión; hereda la velocidad del tirador.
		FVector Velocity = Right * FMath::FRandRange(170.f, 260.f) + Up * FMath::FRandRange(90.f, 170.f)
			- Forward * FMath::FRandRange(0.f, 50.f);
		if (OwningCharacter)
		{
			Velocity += OwningCharacter->GetVelocity();
		}

		ShooterEffects::SpawnShellCasing(GetWorld(), MeshTransform.TransformPosition(ViewFrame.EjectLocation),
			FRotationMatrix::MakeFromZ(Forward).Rotator(), Velocity, WeaponData->ShellLength);
	}
}

float AWeaponBase::GetCurrentReloadDuration() const
{
	if (!WeaponData)
	{
		return 0.f;
	}

	const float BaseTime = CurrentAmmo > 0 ? WeaponData->ReloadTime : WeaponData->GetEmptyReloadTime();
	return BaseTime * Attachments.GetReloadTimeMultiplier();
}

void AWeaponBase::BuildViewFrame()
{
	ViewFrame = FWeaponViewFrame();

	const USkeletalMesh* Mesh = WeaponData ? WeaponData->WeaponMesh.Get() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
	const FVector Extent = Bounds.BoxExtent;
	const FVector Center = Bounds.Origin;

	// El origen de la malla es la empuñadura: es por donde se ancla a la mano.
	FVector Muzzle;
	const bool bHasMuzzle = GetRefPoseSocketLocation(Mesh, TEXT("Muzzle"), Muzzle);

	// Eje del cañón: el más largo de la caja, orientado hacia la boca.
	int32 ForwardAxis = 0;
	for (int32 Axis = 1; Axis < 3; ++Axis)
	{
		if (Extent[Axis] > Extent[ForwardAxis])
		{
			ForwardAxis = Axis;
		}
	}

	if (!bHasMuzzle)
	{
		// Sin socket, la boca está en el extremo más alejado de la empuñadura.
		Muzzle = Center;
		Muzzle[ForwardAxis] = Center[ForwardAxis] >= 0.f
			? Center[ForwardAxis] + Extent[ForwardAxis]
			: Center[ForwardAxis] - Extent[ForwardAxis];
	}

	FVector Forward = FVector::ZeroVector;
	Forward[ForwardAxis] = Muzzle[ForwardAxis] >= Center[ForwardAxis] ? 1.f : -1.f;

	// Eje vertical: de los otros dos, el más alto (un arma es más alta que
	// ancha), orientado de la empuñadura hacia la boca, que va por encima.
	const int32 AxisA = (ForwardAxis + 1) % 3;
	const int32 AxisB = (ForwardAxis + 2) % 3;
	const int32 UpAxis = Extent[AxisA] >= Extent[AxisB] ? AxisA : AxisB;

	FVector Up = FVector::ZeroVector;
	Up[UpAxis] = FMath::Abs(Muzzle[UpAxis]) > 0.5f
		? FMath::Sign(Muzzle[UpAxis])
		: (Center[UpAxis] >= 0.f ? 1.f : -1.f);

	const FVector Right = FVector::CrossProduct(Up, Forward);

	const float Top = FVector::DotProduct(Center, Up) + Extent[UpAxis];
	const float BarrelSide = FVector::DotProduct(Muzzle, Right);
	const float BarrelHeight = FVector::DotProduct(Muzzle, Up);
	const float MuzzleForward = FVector::DotProduct(Muzzle, Forward);

	ViewFrame.Forward = Forward;
	ViewFrame.Up = Up;
	ViewFrame.Right = Right;
	ViewFrame.MeshToView = FQuat(FRotationMatrix::MakeFromXZ(Forward, Up)).Inverse();
	ViewFrame.MuzzleLocation = Muzzle;

	// Visor algo por delante de la empuñadura, sobre el borde superior.
	ViewFrame.MountLocation = Forward * (MuzzleForward * 0.12f) + Up * Top + Right * BarrelSide;

	// Ventana de expulsión: por encima de la empuñadura, en el lado derecho.
	ViewFrame.EjectLocation = Forward * (MuzzleForward * 0.18f) + Up * (BarrelHeight - 1.f) + Right * (BarrelSide + 2.f);

	if (WeaponData->bOverrideSightLocation)
	{
		ViewFrame.SightLocation = WeaponData->SightLocation;
	}
	else
	{
		// Miras metálicas: línea de mira justo bajo el borde superior, algo
		// por detrás de la empuñadura.
		ViewFrame.SightLocation = Forward * (-3.f) + Up * (Top - WeaponData->IronSightDrop) + Right * BarrelSide;
	}

	ViewFrame.bValid = true;

	LayoutSight();
	LayoutFlash(FirstPersonFlash, FirstPersonMesh);
	LayoutFlash(ThirdPersonFlash, WeaponMesh);

	if (FlashLight)
	{
		FlashLight->SetRelativeLocation(Muzzle + Forward * 10.f);
	}
}

void AWeaponBase::SetFirstPersonHidden(bool bShouldHide)
{
	if (bFirstPersonHidden == bShouldHide)
	{
		return;
	}

	bFirstPersonHidden = bShouldHide;
	FirstPersonMesh->SetHiddenInGame(bShouldHide);
	for (int32 Index = 0; Index < SightParts.Num(); ++Index)
	{
		SightParts[Index]->SetHiddenInGame(bShouldHide || Index >= ActiveSightParts);
	}
	SightGlass->SetHiddenInGame(bShouldHide || !bSightGlassActive);
}

void AWeaponBase::LayoutSight()
{
	for (UStaticMeshComponent* Part : SightParts)
	{
		Part->SetHiddenInGame(true);
	}
	SightGlass->SetHiddenInGame(true);
	ActiveSightParts = 0;
	bSightGlassActive = false;

	if (!WeaponData || !ViewFrame.bValid || WeaponData->SightType == EWeaponSightType::Iron)
	{
		return;
	}

	// Marco del visor: X a lo largo del cañón, Z hacia arriba, origen en el
	// borde superior del arma.
	const FTransform Mount(FQuat(FRotationMatrix::MakeFromXZ(ViewFrame.Forward, ViewFrame.Up)), ViewFrame.MountLocation);

	UStaticMesh* Cube = ShooterEffects::GetBasicShape(TEXT("Cube"));
	UStaticMesh* Cylinder = ShooterEffects::GetBasicShape(TEXT("Cylinder"));
	UMaterialInterface* BodyMaterial = ShooterEffects::GetSightBodyMaterial();

	struct FSightPart
	{
		bool bCylinder;
		FVector Center;
		FVector Size;
	};

	TArray<FSightPart> Parts;
	FVector SightPoint;

	if (WeaponData->SightType == EWeaponSightType::Holographic)
	{
		// Visor holográfico: base, dos montantes, capucha y la caja de la
		// batería delante. La ventana queda entre los montantes.
		Parts.Add({ false, FVector(0.f, 0.f, 0.6f), FVector(8.f, 3.4f, 1.2f) });
		Parts.Add({ false, FVector(0.8f, -1.95f, 3.6f), FVector(4.6f, 0.5f, 4.8f) });
		Parts.Add({ false, FVector(0.8f, 1.95f, 3.6f), FVector(4.6f, 0.5f, 4.8f) });
		Parts.Add({ false, FVector(0.8f, 0.f, 6.25f), FVector(4.6f, 4.4f, 0.5f) });
		Parts.Add({ false, FVector(4.4f, 0.f, 1.9f), FVector(2.6f, 3.f, 2.2f) });
		SightPoint = FVector(0.6f, 0.f, 3.7f);

		// Cristal con la retícula, de cara al ojo: el plano del motor mira a
		// +Z y, girado 90°, pasa a mirar hacia atrás.
		SightGlass->SetStaticMesh(ShooterEffects::GetBasicShape(TEXT("Plane")));
		if (UMaterialInterface* Reticle = ShooterEffects::GetReticleMaterial())
		{
			SightGlass->SetMaterial(0, Reticle);
			const FTransform GlassLocal(FRotator(90.f, 0.f, 0.f), SightPoint, FVector(3.6f / 100.f, 3.6f / 100.f, 1.f));
			SightGlass->SetRelativeTransform(GlassLocal * Mount);
			SightGlass->SetHiddenInGame(bFirstPersonHidden);
			bSightGlassActive = true;
		}
	}
	else
	{
		// Visor de aumentos: tubo con campana delantera y ocular, sobre dos
		// anillas. Al apuntar se mira a través de él a pantalla completa.
		Parts.Add({ false, FVector(1.f, 0.f, 0.5f), FVector(20.f, 2.4f, 1.f) });
		Parts.Add({ false, FVector(-4.f, 0.f, 2.3f), FVector(1.6f, 3.2f, 3.6f) });
		Parts.Add({ false, FVector(7.f, 0.f, 2.3f), FVector(1.6f, 3.2f, 3.6f) });
		Parts.Add({ true, FVector(1.5f, 0.f, 4.6f), FVector(24.f, 3.f, 3.f) });
		Parts.Add({ true, FVector(15.f, 0.f, 4.6f), FVector(6.f, 4.6f, 4.6f) });
		Parts.Add({ true, FVector(-12.5f, 0.f, 4.6f), FVector(5.f, 4.f, 4.f) });
		SightPoint = FVector(-15.5f, 0.f, 4.6f);
	}

	for (int32 Index = 0; Index < Parts.Num() && Index < SightParts.Num(); ++Index)
	{
		UStaticMeshComponent* Part = SightParts[Index];
		Part->SetStaticMesh(Parts[Index].bCylinder ? Cylinder : Cube);
		if (BodyMaterial)
		{
			Part->SetMaterial(0, BodyMaterial);
		}
		Part->SetRelativeTransform(MakePartTransform(Parts[Index].Center, Parts[Index].Size, Parts[Index].bCylinder) * Mount);
		Part->SetHiddenInGame(bFirstPersonHidden);
		ActiveSightParts = Index + 1;
	}

	if (!WeaponData->bOverrideSightLocation)
	{
		ViewFrame.SightLocation = Mount.TransformPosition(SightPoint);
	}
}

void AWeaponBase::LayoutFlash(const TArray<TObjectPtr<UStaticMeshComponent>>& Planes, USkeletalMeshComponent* Parent)
{
	if (!ViewFrame.bValid || Planes.Num() < 3)
	{
		return;
	}

	// Sin material aditivo, un plano gris opaco delante del cañón sería peor
	// que no tener fogonazo.
	UMaterialInterface* FlashMaterial = ShooterEffects::GetMuzzleFlashMaterial();
	if (!FlashMaterial)
	{
		return;
	}

	UStaticMesh* Plane = ShooterEffects::GetBasicShape(TEXT("Plane"));
	const FTransform Muzzle(FQuat(FRotationMatrix::MakeFromXZ(ViewFrame.Forward, ViewFrame.Up)), ViewFrame.MuzzleLocation);

	FTransform Local[3];
	MakeFlashTransforms(WeaponData ? WeaponData->MuzzleFlashScale : 1.f, 0.f, Local);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Component = Planes[Index];
		Component->SetStaticMesh(Plane);
		Component->SetMaterial(0, FlashMaterial);
		Component->SetRelativeTransform(Local[Index] * Muzzle);
	}
}

void AWeaponBase::ShowMuzzleFlash()
{
	if (!ViewFrame.bValid)
	{
		return;
	}

	FlashTimeRemaining = 0.05f;

	const bool bFirstPerson = IsViewedInFirstPerson();
	const TArray<TObjectPtr<UStaticMeshComponent>>& Planes = bFirstPerson ? FirstPersonFlash : ThirdPersonFlash;

	// Mirando por el visor de aumentos el arma no se ve: tampoco su fogonazo.
	const bool bShowPlanes = !(bFirstPerson && bFirstPersonHidden);

	// Cada fogonazo es distinto: giro y tamaño al azar.
	const FTransform Muzzle(FQuat(FRotationMatrix::MakeFromXZ(ViewFrame.Forward, ViewFrame.Up)), ViewFrame.MuzzleLocation);
	const float BaseScale = WeaponData ? WeaponData->MuzzleFlashScale : 1.f;

	FTransform Local[3];
	MakeFlashTransforms(BaseScale * FMath::FRandRange(0.8f, 1.25f), FMath::FRandRange(0.f, 360.f), Local);

	for (int32 Index = 0; Index < Planes.Num() && Index < 3; ++Index)
	{
		if (bShowPlanes && Planes[Index]->GetStaticMesh())
		{
			Planes[Index]->SetRelativeTransform(Local[Index] * Muzzle);
			Planes[Index]->SetHiddenInGame(false);
		}
	}

	if (FlashLight)
	{
		FlashLight->SetVisibility(true);
		FlashLight->SetIntensity(450.f * BaseScale);
	}
}

void AWeaponBase::UpdateMuzzleFlash(float DeltaSeconds)
{
	if (FlashTimeRemaining <= 0.f)
	{
		return;
	}

	FlashTimeRemaining -= DeltaSeconds;
	if (FlashTimeRemaining > 0.f)
	{
		return;
	}

	for (UStaticMeshComponent* Plane : FirstPersonFlash)
	{
		Plane->SetHiddenInGame(true);
	}
	for (UStaticMeshComponent* Plane : ThirdPersonFlash)
	{
		Plane->SetHiddenInGame(true);
	}
	if (FlashLight)
	{
		FlashLight->SetIntensity(0.f);
		FlashLight->SetVisibility(false);
	}
}

void AWeaponBase::SpawnImpactEffect(const FVector& Location, const FVector& Normal,
	const UPhysicalMaterial* SurfaceMaterial, bool bFlesh) const
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
	else
	{
		// Sin sistema de partículas: polvo, esquirlas y marca construidos con
		// mallas básicas (ShooterEffects).
		ShooterEffects::SpawnImpact(GetWorld(), Location, Normal,
			ShooterEffects::ClassifySurface(SurfaceMaterial, bFlesh));
	}

	if (Surface && Surface->ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Surface->ImpactSound, Location);
	}
}

void AWeaponBase::BroadcastImpact(const FVector& Location, const FVector& Normal,
	const UPhysicalMaterial* SurfaceMaterial, bool bFlesh)
{
	if (HasAuthority())
	{
		Multicast_PlayImpactEffect(Location, Normal, SurfaceMaterial, bFlesh);
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

	Multicast_PlayFireEffects(ShotDirection);

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

void AWeaponBase::Multicast_PlayFireEffects_Implementation(const FVector_NetQuantizeNormal& Direction)
{
	// El dueño ya reprodujo sus efectos de forma predictiva.
	if (OwningCharacter && OwningCharacter->IsLocallyControlled())
	{
		return;
	}

	PlayLocalFireEffects(Direction);
}

void AWeaponBase::Multicast_PlayImpactEffect_Implementation(const FVector_NetQuantize& ImpactLocation,
	const FVector_NetQuantizeNormal& ImpactNormal, const UPhysicalMaterial* SurfaceMaterial, bool bFlesh)
{
	SpawnImpactEffect(ImpactLocation, ImpactNormal, SurfaceMaterial, bFlesh);
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
