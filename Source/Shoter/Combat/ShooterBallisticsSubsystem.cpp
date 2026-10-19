#include "Combat/ShooterBallisticsSubsystem.h"
#include "Combat/ShooterPhysicalMaterial.h"
#include "Combat/LagCompensationComponent.h"
#include "Combat/HealthComponent.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/**
	 * Tramo máximo de simulación, en segundos. A 900 m/s son 15 m por tramo:
	 * la curva de caída se aproxima con rectas sin error apreciable.
	 */
	constexpr float MaxStepSeconds = 1.f / 60.f;

	/** Por debajo de esta velocidad (cm/s) la bala ya no hace daño útil. */
	constexpr float MinLethalSpeed = 15000.f;

	constexpr float MaxBulletLifetime = 4.f;

	/** Tope de superficies atravesadas, para que una ráfaga no recorra un edificio entero. */
	constexpr int32 MaxPenetrations = 3;

	/** Desviación máxima al salir de una superficie atravesada, en grados. */
	constexpr float PenetrationDeflectionDegrees = 2.5f;
}

bool UShooterBallisticsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UShooterBallisticsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShooterBallisticsSubsystem, STATGROUP_Tickables);
}

float UShooterBallisticsSubsystem::ComputeZeroingAngle(float MuzzleVelocity, float DecayDistance,
	float ZeroingDistance, float Gravity)
{
	if (ZeroingDistance <= 0.f || MuzzleVelocity <= 0.f || DecayDistance <= 0.f)
	{
		return 0.f;
	}

	// Con v(d) = v0·e^(-d/L), el tiempo hasta d es (L/v0)·(e^(d/L) - 1).
	const float FlightTime = (DecayDistance / MuzzleVelocity) * (FMath::Exp(ZeroingDistance / DecayDistance) - 1.f);
	const float Drop = 0.5f * FMath::Abs(Gravity) * FMath::Square(FlightTime);

	return FMath::RadiansToDegrees(FMath::Atan2(Drop, ZeroingDistance));
}

float UShooterBallisticsSubsystem::GetPenetrationResistance(const UPhysicalMaterial* Material)
{
	if (const UShooterPhysicalMaterial* ShooterMaterial = Cast<UShooterPhysicalMaterial>(Material))
	{
		return ShooterMaterial->PenetrationResistance;
	}

	// Sin material asignado se asume hormigón: es preferible que una pared
	// sin configurar detenga las balas a que las deje pasar.
	return DefaultPenetrationResistance;
}

void UShooterBallisticsSubsystem::FireBullet(AWeaponBase* Weapon, AShooterCharacter* Shooter,
	const FVector& Start, const FVector& Direction, float RewindOffset)
{
	if (!Weapon || !Weapon->WeaponData)
	{
		return;
	}

	const UWeaponDataAsset* Data = Weapon->WeaponData;

	FShooterBullet& Bullet = Bullets.AddDefaulted_GetRef();
	Bullet.Weapon = Weapon;
	Bullet.Shooter = Shooter;
	Bullet.InstigatorController = Shooter ? Shooter->GetController() : nullptr;
	Bullet.Position = Start;
	Bullet.Velocity = Direction.GetSafeNormal() * Data->MuzzleVelocity;
	Bullet.RewindOffset = RewindOffset;
	Bullet.RemainingPenetration = Data->PenetrationPower;
	Bullet.DamageModel = Weapon->MakeDamageSnapshot();
}

void UShooterBallisticsSubsystem::FireHitscan(AWeaponBase* Weapon, AShooterCharacter* Shooter,
	const FVector& Start, const FVector& Direction, float RewindOffset)
{
	if (!Weapon || !Weapon->WeaponData)
	{
		return;
	}

	FShooterBullet Bullet;
	Bullet.Weapon = Weapon;
	Bullet.Shooter = Shooter;
	Bullet.InstigatorController = Shooter ? Shooter->GetController() : nullptr;
	Bullet.Position = Start;
	Bullet.Velocity = Direction.GetSafeNormal();
	Bullet.RewindOffset = RewindOffset;
	Bullet.RemainingPenetration = Weapon->WeaponData->PenetrationPower;
	Bullet.DamageModel = Weapon->MakeDamageSnapshot();

	const float Range = Weapon->WeaponData->Range;

	// Recta, pero con las mismas reglas de penetración que la bala simulada.
	FVector From = Start;
	for (int32 Pass = 0; Pass <= MaxPenetrations && Bullet.DistanceTravelled < Range; ++Pass)
	{
		const FVector To = From + Bullet.Velocity * (Range - Bullet.DistanceTravelled);

		FVector ContinueFrom;
		const bool bAlive = ResolveSegment(Bullet, From, To, ContinueFrom);
		Bullet.DistanceTravelled += FVector::Dist(From, ContinueFrom);

		if (!bAlive || ContinueFrom.Equals(To))
		{
			break;
		}
		From = ContinueFrom;
	}
}

void UShooterBallisticsSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Gravity = World->GetGravityZ();

	for (int32 Index = Bullets.Num() - 1; Index >= 0; --Index)
	{
		if (!StepBullet(Bullets[Index], DeltaTime, Gravity))
		{
			Bullets.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}
}

bool UShooterBallisticsSubsystem::StepBullet(FShooterBullet& Bullet, float DeltaTime, float Gravity)
{
	float Remaining = DeltaTime;

	while (Remaining > KINDA_SMALL_NUMBER)
	{
		const float Step = FMath::Min(Remaining, MaxStepSeconds);
		Remaining -= Step;

		// Rozamiento: la velocidad decae con la distancia recorrida en el tramo.
		const float Speed = Bullet.Velocity.Size();
		const float DecayDistance = FMath::Max(Bullet.DamageModel.VelocityDecayDistance, 1.f);
		Bullet.Velocity *= FMath::Exp(-(Speed * Step) / DecayDistance);
		Bullet.Velocity.Z += Gravity * Step;

		const FVector SegmentStart = Bullet.Position;
		const FVector SegmentEnd = SegmentStart + Bullet.Velocity * Step;

		FVector ContinueFrom;
		const bool bAlive = ResolveSegment(Bullet, SegmentStart, SegmentEnd, ContinueFrom);

		Bullet.DistanceTravelled += FVector::Dist(SegmentStart, ContinueFrom);
		Bullet.Position = ContinueFrom;
		Bullet.Age += Step;

		if (!bAlive)
		{
			return false;
		}

		if (Bullet.DistanceTravelled > Bullet.DamageModel.Range
			|| Bullet.Velocity.SizeSquared() < FMath::Square(MinLethalSpeed)
			|| Bullet.Age > MaxBulletLifetime)
		{
			return false;
		}
	}

	return true;
}

bool UShooterBallisticsSubsystem::ResolveSegment(FShooterBullet& Bullet, const FVector& SegmentStart,
	const FVector& SegmentEnd, FVector& OutContinueFrom)
{
	OutContinueFrom = SegmentEnd;

	UWorld* World = GetWorld();
	const FVector Direction = (SegmentEnd - SegmentStart).GetSafeNormal();
	if (!World || Direction.IsNearlyZero())
	{
		return false;
	}

	// Escenario. Los personajes se resuelven aparte, contra sus posiciones
	// rebobinadas: con el trazado normal se probaría su posición actual.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShooterBullet), false);
	Params.bReturnPhysicalMaterial = true;
	if (AWeaponBase* Weapon = Bullet.Weapon.Get())
	{
		Params.AddIgnoredActor(Weapon);
	}
	for (TActorIterator<AShooterCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	FHitResult GeometryHit;
	const bool bHitGeometry = World->LineTraceSingleByChannel(GeometryHit, SegmentStart, SegmentEnd,
		ECC_Visibility, Params);
	const FVector EffectiveEnd = bHitGeometry ? GeometryHit.ImpactPoint : SegmentEnd;

	AShooterCharacter* HitCharacter = nullptr;
	FVector CharacterImpact = FVector::ZeroVector;
	EShooterHitZone Zone = EShooterHitZone::None;

	const float RewindTime = World->GetTimeSeconds() - Bullet.RewindOffset;
	const bool bHitCharacter = ULagCompensationComponent::FindBestCharacterHit(World, SegmentStart, EffectiveEnd,
		RewindTime, Bullet.Shooter.Get(), HitCharacter, CharacterImpact, Zone);

	NotifyNearMisses(Bullet, SegmentStart, bHitCharacter ? CharacterImpact : EffectiveEnd);

	if (bHitCharacter)
	{
		const float Distance = Bullet.DistanceTravelled + FVector::Dist(SegmentStart, CharacterImpact);
		ApplyCharacterHit(Bullet, HitCharacter, CharacterImpact, Direction, Zone, Distance);
		OutContinueFrom = CharacterImpact;
		return false;
	}

	if (!bHitGeometry)
	{
		return true;
	}

	OutContinueFrom = GeometryHit.ImpactPoint;
	const UPhysicalMaterial* SurfaceMaterial = GeometryHit.PhysMaterial.Get();

	// Blancos y drones de entrenamiento: reciben el daño como cualquier actor.
	AActor* HitActor = GeometryHit.GetActor();
	if (HitActor && HitActor->CanBeDamaged())
	{
		const float Distance = Bullet.DistanceTravelled + FVector::Dist(SegmentStart, GeometryHit.ImpactPoint);
		const float Damage = Bullet.DamageModel.GetDamage(Distance, EShooterHitZone::Torso) * Bullet.DamageScale;

		UGameplayStatics::ApplyPointDamage(HitActor, Damage, Direction, GeometryHit,
			Bullet.InstigatorController.Get(), Bullet.Weapon.Get(), UDamageType::StaticClass());
	}

	if (AWeaponBase* Weapon = Bullet.Weapon.Get())
	{
		Weapon->BroadcastImpact(GeometryHit.ImpactPoint, GeometryHit.ImpactNormal, SurfaceMaterial);
	}

	FVector ExitPoint;
	if (TryPenetrate(Bullet, GeometryHit, Direction, ExitPoint))
	{
		if (AWeaponBase* Weapon = Bullet.Weapon.Get())
		{
			Weapon->BroadcastImpact(ExitPoint, Direction, SurfaceMaterial);
		}

		OutContinueFrom = ExitPoint;
		return true;
	}

	return false;
}

bool UShooterBallisticsSubsystem::TryPenetrate(FShooterBullet& Bullet, const FHitResult& Hit,
	const FVector& Direction, FVector& OutExitPoint) const
{
	if (Bullet.Penetrations >= MaxPenetrations || Bullet.RemainingPenetration <= 0.f)
	{
		return false;
	}

	UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component)
	{
		return false;
	}

	const float Resistance = GetPenetrationResistance(Hit.PhysMaterial.Get());
	const float MaxThickness = Resistance > KINDA_SMALL_NUMBER
		? Bullet.RemainingPenetration / Resistance
		: 200.f;

	if (MaxThickness < 1.f)
	{
		return false;
	}

	// El grosor se mide trazando hacia atrás desde el otro lado: lo primero
	// que se encuentra es la cara de salida de la misma pieza.
	const FVector ProbeStart = Hit.ImpactPoint + Direction * (MaxThickness + 1.f);
	const FVector ProbeEnd = Hit.ImpactPoint + Direction * 0.1f;

	FHitResult ExitHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShooterBulletExit), true);
	if (!Component->LineTraceComponent(ExitHit, ProbeStart, ProbeEnd, Params) || ExitHit.bStartPenetrating)
	{
		// Más grueso de lo que la bala puede atravesar.
		return false;
	}

	const float Thickness = FVector::Dist(Hit.ImpactPoint, ExitHit.ImpactPoint);
	const float Cost = Thickness * Resistance;
	const float PowerBefore = Bullet.RemainingPenetration;

	if (Cost >= PowerBefore)
	{
		return false;
	}

	Bullet.RemainingPenetration -= Cost;
	++Bullet.Penetrations;

	// La energía cae en proporción al material atravesado; la velocidad, con
	// su raíz, porque la energía cinética va con el cuadrado de la velocidad.
	const float EnergyKept = 1.f - Cost / PowerBefore;
	Bullet.DamageScale *= EnergyKept;

	const float SpeedKept = FMath::Sqrt(EnergyKept);
	const FVector NewDirection = FMath::VRandCone(Direction, FMath::DegreesToRadians(PenetrationDeflectionDegrees));
	Bullet.Velocity = NewDirection * Bullet.Velocity.Size() * SpeedKept;

	OutExitPoint = ExitHit.ImpactPoint + NewDirection * 1.f;
	return true;
}

void UShooterBallisticsSubsystem::ApplyCharacterHit(FShooterBullet& Bullet, AShooterCharacter* Victim,
	const FVector& ImpactPoint, const FVector& Direction, EShooterHitZone Zone, float Distance)
{
	if (!Victim)
	{
		return;
	}

	const float Damage = Bullet.DamageModel.GetDamage(Distance, Zone) * Bullet.DamageScale;

	// La zona viaja en el nombre del hueso: así UHealthComponent la recibe
	// por la vía estándar del daño puntual, sin un canal propio.
	FHitResult HitInfo;
	HitInfo.Location = ImpactPoint;
	HitInfo.ImpactPoint = ImpactPoint;
	HitInfo.ImpactNormal = -Direction;
	HitInfo.BoneName = UHealthComponent::HitZoneToName(Zone);

	AController* InstigatorController = Bullet.InstigatorController.Get();
	UGameplayStatics::ApplyPointDamage(Victim, Damage, Direction, HitInfo, InstigatorController,
		Bullet.Weapon.Get(), UDamageType::StaticClass());

	if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(InstigatorController))
	{
		ShooterPC->Client_NotifyHitConfirmed(Zone == EShooterHitZone::Head);
	}

	if (AWeaponBase* Weapon = Bullet.Weapon.Get())
	{
		Weapon->BroadcastImpact(ImpactPoint, -Direction, nullptr, true);
	}
}

void UShooterBallisticsSubsystem::NotifyNearMisses(FShooterBullet& Bullet, const FVector& SegmentStart,
	const FVector& SegmentEnd)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AShooterCharacter* Shooter = Bullet.Shooter.Get();

	for (TActorIterator<AShooterCharacter> It(World); It; ++It)
	{
		AShooterCharacter* Character = *It;
		if (!Character || Character == Shooter || Character->IsDead())
		{
			continue;
		}

		AShooterPlayerController* PC = Cast<AShooterPlayerController>(Character->GetController());
		if (!PC || Bullet.SuppressedControllers.Contains(PC))
		{
			continue;
		}

		// Posición actual, no rebobinada: la supresión es lo que siente la
		// víctima en su pantalla, no lo que vio el tirador.
		const FVector Head = Character->GetPawnViewLocation();
		const FVector Closest = FMath::ClosestPointOnSegment(Head, SegmentStart, SegmentEnd);
		const float Distance = FVector::Dist(Head, Closest);

		if (Distance > SuppressionRadius)
		{
			continue;
		}

		Bullet.SuppressedControllers.Add(PC);

		const float Intensity = 1.f - Distance / SuppressionRadius;
		PC->Client_NotifySuppressed(Closest, Intensity);
	}
}
