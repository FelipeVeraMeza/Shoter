#include "AI/ShooterBotController.h"
#include "Player/ShooterCharacter.h"
#include "Core/ShooterPlayerState.h"
#include "Core/ShooterGameState.h"
#include "Core/ControlZone.h"
#include "Combat/HealthComponent.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"

namespace
{
	/** Altura del pecho y de la cabeza sobre el centro de la cápsula, en cm. */
	constexpr float ChestHeight = 35.f;
	constexpr float HeadHeight = 68.f;

	/** Cada cuánto se comprueba la línea de visión: los trazados cuestan. */
	constexpr float VisibilityInterval = 0.15f;
}

AShooterBotController::AShooterBotController()
{
	PrimaryActorTick.bCanEverTick = true;

	// Con PlayerState el bot tiene equipo, nombre y marcador como cualquier
	// jugador. Sin él, todos los bots eran enemigos de todos.
	bWantsPlayerState = true;

	// La vista la gira este controlador (puntería con cabeceo incluido). Si
	// el controlador base la copiara de la orientación del cuerpo en cada
	// fotograma, el cabeceo volvería a cero y el bot no podría apuntar arriba
	// ni abajo.
	bSetControlRotationFromPawnOrientation = false;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = SightRange;
	SightConfig->LoseSightRadius = SightRange * 1.1f;
	SightConfig->PeripheralVisionAngleDegrees = SightAngle;
	SightConfig->SetMaxAge(MemoryDuration);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());

	SetPerceptionComponent(*Perception);
}

void AShooterBotController::BeginPlay()
{
	Super::BeginPlay();

	if (Perception)
	{
		Perception->OnTargetPerceptionUpdated.AddDynamic(
			this, &AShooterBotController::OnPerceptionUpdated);
	}
}

void AShooterBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	CurrentTarget = nullptr;
	bTargetVisible = false;
	bHasLastKnownLocation = false;
	bHasPatrolGoal = false;
	TimeSinceTargetSeen = 1000.f;
	BurstShotsRemaining = 0;

	if (AShooterCharacter* Bot = Cast<AShooterCharacter>(InPawn))
	{
		if (UHealthComponent* Health = Bot->GetHealthComponent())
		{
			Health->OnHealthChanged.AddUniqueDynamic(this, &AShooterBotController::HandleDamaged);
		}
	}
}

void AShooterBotController::OnUnPossess()
{
	if (AShooterCharacter* Bot = Cast<AShooterCharacter>(GetPawn()))
	{
		if (UHealthComponent* Health = Bot->GetHealthComponent())
		{
			Health->OnHealthChanged.RemoveDynamic(this, &AShooterBotController::HandleDamaged);
		}
	}

	Super::OnUnPossess();
}

void AShooterBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	AShooterCharacter* Sensed = Cast<AShooterCharacter>(Actor);
	if (!Sensed || Sensed->IsDead() || IsSameTeam(Sensed) || !Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	// La percepción solo alerta: disparar exige línea de visión propia.
	if (!bTargetVisible)
	{
		LastKnownTargetLocation = Sensed->GetActorLocation();
		bHasLastKnownLocation = true;
		TimeSinceTargetSeen = FMath::Min(TimeSinceTargetSeen, 1.f);
	}
}

void AShooterBotController::HandleDamaged(float NewHealth, float Delta, AController* InstigatedBy)
{
	if (Delta >= 0.f || !InstigatedBy || InstigatedBy == this)
	{
		return;
	}

	const AShooterCharacter* Attacker = Cast<AShooterCharacter>(InstigatedBy->GetPawn());
	if (!Attacker || IsSameTeam(Attacker))
	{
		return;
	}

	// Quien recibe un disparo se gira hacia él aunque no vea al tirador.
	if (!bTargetVisible)
	{
		LastKnownTargetLocation = Attacker->GetActorLocation();
		bHasLastKnownLocation = true;
		TimeSinceTargetSeen = 0.f;
	}
}

bool AShooterBotController::IsSameTeam(const AShooterCharacter* Other) const
{
	const AShooterPlayerState* MyState = GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* OtherState = Other
		? Other->GetPlayerState<AShooterPlayerState>() : nullptr;

	if (!MyState || !OtherState)
	{
		return false;
	}

	// Sin equipo asignado, todos son enemigos: es el modo todos contra todos.
	const EShooterTeam MyTeam = MyState->GetTeam();
	return MyTeam != EShooterTeam::None && MyTeam == OtherState->GetTeam();
}

bool AShooterBotController::CanSee(const AShooterCharacter* Other) const
{
	const AShooterCharacter* Bot = Cast<AShooterCharacter>(GetPawn());
	if (!Bot || !Other || Other->IsDead())
	{
		return false;
	}

	const FVector Eye = Bot->GetShotOrigin();
	const FVector Chest = Other->GetActorLocation() + FVector(0.f, 0.f, ChestHeight);
	const FVector ToOther = Chest - Eye;
	const float Distance = ToOther.Size();
	if (Distance > SightRange)
	{
		return false;
	}

	// Fuera del cono de visión no se ve, salvo al blanco que ya se sigue:
	// girarse con él no debería hacerle desaparecer.
	const bool bTracking = Other == CurrentTarget && TimeSinceTargetSeen < 1.5f;
	if (!bTracking && Distance > 300.f)
	{
		const float CosAngle = FVector::DotProduct(ToOther / Distance, GetControlRotation().Vector());
		if (CosAngle < FMath::Cos(FMath::DegreesToRadians(SightAngle)))
		{
			return false;
		}
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShooterBotSight), false, Bot);
	Params.AddIgnoredActor(Other);

	// Se ve si llega la línea al pecho o a la cabeza: asomado tras un muro
	// bajo solo se le ve la cabeza.
	const FVector Head = Other->GetActorLocation() + FVector(0.f, 0.f, HeadHeight);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chest, ECC_Visibility, Params))
	{
		return true;
	}
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Head, ECC_Visibility, Params);
}

AShooterCharacter* AShooterBotController::FindVisibleEnemy() const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return nullptr;
	}

	AShooterCharacter* Best = nullptr;
	float BestDistanceSquared = FMath::Square(SightRange);

	for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
	{
		AShooterCharacter* Candidate = *It;
		if (!Candidate || Candidate == MyPawn || Candidate->IsDead() || IsSameTeam(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(MyPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared && CanSee(Candidate))
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	return Best;
}

void AShooterBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Solo el servidor decide el comportamiento: los clientes ven el
	// resultado replicado (RNF-3.1).
	if (!HasAuthority())
	{
		return;
	}

	AShooterCharacter* Bot = Cast<AShooterCharacter>(GetPawn());
	if (!Bot || Bot->IsDead())
	{
		return;
	}

	// Con la partida terminada no se sigue peleando.
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (ShooterGameState && ShooterGameState->GetMatchPhase() == EMatchPhase::Finished)
	{
		StopMovement();
		if (AWeaponBase* Weapon = Bot->GetCurrentWeapon())
		{
			Weapon->StopFire();
		}
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastVisibilityCheck >= VisibilityInterval)
	{
		LastVisibilityCheck = Now;

		if (CurrentTarget && (CurrentTarget->IsDead() || IsSameTeam(CurrentTarget)))
		{
			CurrentTarget = nullptr;
		}

		bTargetVisible = CurrentTarget && CanSee(CurrentTarget);
		if (!bTargetVisible)
		{
			// Un enemigo nuevo exige tiempo de reacción: nadie dispara en el
			// mismo instante en que ve a alguien.
			if (AShooterCharacter* Visible = FindVisibleEnemy())
			{
				if (Visible != CurrentTarget)
				{
					CurrentTarget = Visible;
					TimeOnTarget = 0.f;
					ReactionRemaining = FMath::FRandRange(ReactionTime.X, ReactionTime.Y);
					NextAimErrorChange = 0.f;
				}
				bTargetVisible = true;
			}
		}

		if (bTargetVisible)
		{
			LastKnownTargetLocation = CurrentTarget->GetActorLocation();
			bHasLastKnownLocation = true;
			TimeSinceTargetSeen = 0.f;
		}
	}

	if (!bTargetVisible)
	{
		TimeSinceTargetSeen += DeltaSeconds;
	}

	if (bTargetVisible && CurrentTarget)
	{
		UpdateCombat(Bot, DeltaSeconds);
	}
	else if (bHasLastKnownLocation && TimeSinceTargetSeen < MemoryDuration)
	{
		UpdateSearch(Bot, DeltaSeconds);
	}
	else
	{
		CurrentTarget = nullptr;
		bHasLastKnownLocation = false;
		UpdatePatrol(Bot, DeltaSeconds);
	}
}

float AShooterBotController::RotateTowards(const FVector& Point, float DeltaSeconds, float Speed)
{
	const AShooterCharacter* Bot = Cast<AShooterCharacter>(GetPawn());
	if (!Bot)
	{
		return 180.f;
	}

	const FVector Eye = Bot->GetShotOrigin();
	const FVector Direction = (Point - Eye).GetSafeNormal();
	const FRotator Desired = Direction.Rotation();

	FRotator NewRotation = FMath::RInterpConstantTo(GetControlRotation(), Desired, DeltaSeconds, Speed);
	NewRotation.Roll = 0.f;
	SetControlRotation(NewRotation);

	const float CosError = FVector::DotProduct(NewRotation.Vector(), Direction);
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosError, -1.f, 1.f)));
}

void AShooterBotController::FaceMovementDirection(AShooterCharacter* Bot, float DeltaSeconds)
{
	const FVector Velocity = Bot->GetVelocity();
	if (Velocity.Size2D() < 50.f)
	{
		return;
	}

	const FRotator Desired(0.f, Velocity.Rotation().Yaw, 0.f);
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), Desired, DeltaSeconds, 5.f));
}

void AShooterBotController::UpdateCombat(AShooterCharacter* Bot, float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	TimeOnTarget += DeltaSeconds;
	ReactionRemaining -= DeltaSeconds;
	bHasPatrolGoal = false;

	const FVector MyLocation = Bot->GetActorLocation();
	const FVector TargetLocation = CurrentTarget->GetActorLocation();
	const float Distance = FVector::Dist(MyLocation, TargetLocation);

	// --- Movimiento: acercarse hasta su distancia y, desde ahí, pasos
	// laterales cortos para no ser un blanco fijo.
	if (Distance > PreferredCombatDistance)
	{
		Bot->UnCrouch();
		MoveToLocation(TargetLocation, PreferredCombatDistance * 0.7f);
	}
	else if (Now >= NextRepositionTime)
	{
		NextRepositionTime = Now + FMath::FRandRange(2.f, 4.5f);

		if (FMath::FRand() < 0.55f)
		{
			const FVector ToTarget = (TargetLocation - MyLocation).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, ToTarget) * (FMath::RandBool() ? 1.f : -1.f);
			const FVector Destination = MyLocation + Side * FMath::FRandRange(150.f, 400.f);

			FNavLocation Projected;
			const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			if (NavSystem && NavSystem->ProjectPointToNavigation(Destination, Projected, FVector(100.f, 100.f, 200.f)))
			{
				Bot->UnCrouch();
				MoveToLocation(Projected.Location, 40.f);
			}
		}
		else
		{
			StopMovement();
		}
	}

	// Quieto y lejos, se agacha para estabilizar el arma y ofrecer menos blanco.
	if (Now >= NextCrouchDecision)
	{
		NextCrouchDecision = Now + FMath::FRandRange(2.5f, 5.f);
		const bool bMoving = Bot->GetVelocity().Size2D() > 50.f;
		if (!bMoving && Distance > 1500.f && FMath::FRand() < 0.5f)
		{
			Bot->Crouch();
		}
		else if (bMoving)
		{
			Bot->UnCrouch();
		}
	}

	// --- Puntería: con la mira a media distancia; desde la cadera en cerca.
	Bot->SetWantsToAim(Distance > 1000.f && ReactionRemaining <= 0.f);

	// El error baja mientras sigue al blanco, y sube si alguno se mueve.
	if (Now >= NextAimErrorChange)
	{
		NextAimErrorChange = Now + FMath::FRandRange(0.35f, 0.8f);

		const float Settle = FMath::Lerp(1.f, 0.33f, FMath::Clamp(TimeOnTarget / AimSettleTime, 0.f, 1.f));
		float Magnitude = AimErrorPer10m * (Distance / 1000.f) * Settle;
		Magnitude *= 1.f + FMath::Clamp(CurrentTarget->GetVelocity().Size2D() / 400.f, 0.f, 1.f) * 0.4f;
		Magnitude *= 1.f + FMath::Clamp(Bot->GetVelocity().Size2D() / 400.f, 0.f, 1.f) * 0.5f;
		Magnitude = FMath::Max(Magnitude, 6.f);

		AimError = FMath::VRand() * Magnitude * FMath::FRandRange(0.3f, 1.f);
	}

	// Pecho casi siempre; a corta distancia y con el blanco asentado, a veces la cabeza.
	const bool bHeadshotAttempt = Distance < 1500.f && TimeOnTarget > AimSettleTime && (FMath::Frac(Now * 0.37f) < 0.25f);
	const FVector AimPoint = TargetLocation + FVector(0.f, 0.f, bHeadshotAttempt ? HeadHeight : ChestHeight) + AimError;

	const float TurnSpeed = AimSpeed * (ReactionRemaining > 0.f ? 0.6f : 1.f);
	const float ErrorDegrees = RotateTowards(AimPoint, DeltaSeconds, TurnSpeed);

	UpdateFire(Bot, Distance, ReactionRemaining <= 0.f && ErrorDegrees < 3.f);
}

void AShooterBotController::UpdateFire(AShooterCharacter* Bot, float Distance, bool bAimed)
{
	AWeaponBase* Weapon = Bot->GetCurrentWeapon();
	if (!Weapon || !Weapon->WeaponData)
	{
		return;
	}

	if (Weapon->GetCurrentAmmo() <= 0)
	{
		Weapon->StopFire();
		Weapon->TryReload();
		BurstShotsRemaining = 0;
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (!bAimed || Weapon->IsReloading() || Now < NextShotTime)
	{
		return;
	}

	const UWeaponDataAsset* Data = Weapon->WeaponData;
	const bool bSemiAuto = Weapon->GetCurrentFireMode() != EWeaponFireMode::Automatic;

	// Disciplina de tiro: ráfagas cortas en cerca, pares a media distancia y
	// tiro a tiro de lejos, como enseña cualquier instructor.
	if (BurstShotsRemaining <= 0)
	{
		if (Distance < 1500.f)
		{
			BurstShotsRemaining = FMath::RandRange(3, 6);
		}
		else if (Distance < 4000.f)
		{
			BurstShotsRemaining = FMath::RandRange(2, 3);
		}
		else
		{
			BurstShotsRemaining = 1;
		}
	}

	// Cada disparo es una pulsación: el bot controla él mismo la cadencia.
	Weapon->TryStartFire();
	Weapon->StopFire();
	--BurstShotsRemaining;

	const float Between = Data->GetSecondsBetweenShots();
	if (BurstShotsRemaining > 0)
	{
		NextShotTime = Now + (bSemiAuto ? FMath::Max(Between, FMath::FRandRange(0.22f, 0.4f)) : Between * 1.02f);
	}
	else
	{
		NextShotTime = Now + FMath::FRandRange(0.35f, 0.8f) + (Distance > 4000.f ? 0.4f : 0.f);
	}
}

void AShooterBotController::UpdateSearch(AShooterCharacter* Bot, float DeltaSeconds)
{
	Bot->SetWantsToAim(false);
	BurstShotsRemaining = 0;
	if (AWeaponBase* Weapon = Bot->GetCurrentWeapon())
	{
		Weapon->StopFire();

		// Aprovecha la pausa para recargar si el cargador va por la mitad.
		if (!Weapon->IsReloading() && Weapon->GetCurrentAmmo() < Weapon->GetEffectiveMagazineSize() / 2)
		{
			Weapon->TryReload();
		}
	}

	Bot->UnCrouch();

	if (GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		MoveToLocation(LastKnownTargetLocation, 150.f);
	}

	// Avanza mirando hacia donde vio al enemigo por última vez.
	RotateTowards(LastKnownTargetLocation + FVector(0.f, 0.f, ChestHeight), DeltaSeconds, AimSpeed * 0.5f);

	// Llegó y no hay nadie: se da por perdido pronto.
	if (FVector::DistSquared2D(Bot->GetActorLocation(), LastKnownTargetLocation) < FMath::Square(250.f))
	{
		TimeSinceTargetSeen = FMath::Max(TimeSinceTargetSeen, MemoryDuration - 1.5f);
	}
}

bool AShooterBotController::PickPatrolGoal(AShooterCharacter* Bot)
{
	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		return false;
	}

	// Objetivos que dan sentido a la patrulla: las zonas de Dominación (A, B,
	// C, uno por carril) y el lado enemigo del mapa. Sin ellos, un punto
	// cualquiera alrededor del centro.
	TArray<FVector> Objectives;
	for (TActorIterator<AControlZone> It(GetWorld()); It; ++It)
	{
		Objectives.Add(It->GetActorLocation());
	}

	const AShooterPlayerState* MyState = GetPlayerState<AShooterPlayerState>();
	const FName MyTag = MyState && MyState->GetTeam() == EShooterTeam::Alpha ? FName(TEXT("Alfa"))
		: (MyState && MyState->GetTeam() == EShooterTeam::Bravo ? FName(TEXT("Bravo")) : NAME_None);

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (!MyTag.IsNone() && It->PlayerStartTag != MyTag && FMath::FRand() < 0.35f)
		{
			Objectives.Add(It->GetActorLocation());
		}
	}

	const FVector Center = Objectives.Num() > 0
		? Objectives[FMath::RandRange(0, Objectives.Num() - 1)]
		: FVector::ZeroVector;
	const float Radius = Objectives.Num() > 0 ? PatrolRadius : 6000.f;

	FNavLocation Destination;
	if (NavSystem->GetRandomReachablePointInRadius(Center, Radius, Destination))
	{
		PatrolGoal = Destination.Location;
		bHasPatrolGoal = true;
		return true;
	}
	return false;
}

void AShooterBotController::UpdatePatrol(AShooterCharacter* Bot, float DeltaSeconds)
{
	Bot->SetWantsToAim(false);
	Bot->UnCrouch();
	BurstShotsRemaining = 0;

	AWeaponBase* Weapon = Bot->GetCurrentWeapon();
	if (Weapon)
	{
		Weapon->StopFire();
	}

	// Herido y sin nadie a la vista: vendarse primero.
	if (const UHealthComponent* Health = Bot->GetHealthComponent())
	{
		if (Health->IsBandaging())
		{
			StopMovement();
			return;
		}
		if (Health->GetBleedingWounds() > 0 && Health->GetBandageCount() > 0)
		{
			StopMovement();
			Bot->RequestBandage();
			return;
		}
	}

	if (Weapon && !Weapon->IsReloading() && Weapon->GetCurrentAmmo() < Weapon->GetEffectiveMagazineSize() * 0.7f)
	{
		Weapon->TryReload();
	}

	const bool bArrived = bHasPatrolGoal
		&& FVector::DistSquared2D(Bot->GetActorLocation(), PatrolGoal) < FMath::Square(250.f);

	if (!bHasPatrolGoal || bArrived || GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		if (PickPatrolGoal(Bot))
		{
			MoveToLocation(PatrolGoal, 150.f);
		}
		StuckTime = 0.f;
	}

	// Atascado contra algo: se elige otro destino.
	if (GetMoveStatus() == EPathFollowingStatus::Moving && Bot->GetVelocity().Size2D() < 20.f)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 3.f)
		{
			bHasPatrolGoal = false;
			StopMovement();
		}
	}
	else
	{
		StuckTime = 0.f;
	}

	FaceMovementDirection(Bot, DeltaSeconds);
}
