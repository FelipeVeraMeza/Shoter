#include "AI/ShooterBotController.h"
#include "Player/ShooterCharacter.h"
#include "Core/ShooterPlayerState.h"
#include "Weapons/WeaponBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"

AShooterBotController::AShooterBotController()
{
	PrimaryActorTick.bCanEverTick = true;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = SightRange;
	SightConfig->LoseSightRadius = SightRange * 1.2f;
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

	// La patrulla se mueve alrededor de donde el bot apareció.
	if (InPawn)
	{
		PatrolOrigin = InPawn->GetActorLocation();
	}
}

void AShooterBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	AShooterCharacter* Sensed = Cast<AShooterCharacter>(Actor);
	if (!Sensed || Sensed->IsDead() || IsSameTeam(Sensed))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		CurrentTarget = Sensed;
		LastKnownTargetLocation = Sensed->GetActorLocation();
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

AShooterCharacter* AShooterBotController::FindNearestEnemy() const
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
		if (!Candidate || Candidate == MyPawn || Candidate->IsDead()
			|| IsSameTeam(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			MyPawn->GetActorLocation(), Candidate->GetActorLocation());

		if (DistanceSquared < BestDistanceSquared)
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

	// La percepción puede no haber disparado todavía: buscar por proximidad
	// evita que el bot se quede inerte al empezar la partida.
	if (!CurrentTarget || CurrentTarget->IsDead())
	{
		CurrentTarget = FindNearestEnemy();
		if (CurrentTarget)
		{
			LastKnownTargetLocation = CurrentTarget->GetActorLocation();
			TimeSinceTargetSeen = 0.f;
		}
	}

	if (CurrentTarget && !CurrentTarget->IsDead())
	{
		TimeSinceTargetSeen += DeltaSeconds;
		UpdateCombat(DeltaSeconds);
		return;
	}

	// Sin objetivo: olvidarlo tras MemoryDuration y volver a patrullar.
	if (TimeSinceTargetSeen > MemoryDuration)
	{
		CurrentTarget = nullptr;
		UpdatePatrol();
		return;
	}

	MoveToLocation(LastKnownTargetLocation, 120.f);
	TimeSinceTargetSeen += DeltaSeconds;
}

void AShooterBotController::UpdateCombat(float DeltaSeconds)
{
	AShooterCharacter* Bot = Cast<AShooterCharacter>(GetPawn());
	if (!Bot || !CurrentTarget)
	{
		return;
	}

	LastKnownTargetLocation = CurrentTarget->GetActorLocation();

	const float Distance = FVector::Dist(
		Bot->GetActorLocation(), CurrentTarget->GetActorLocation());

	// Acercarse solo hasta la distancia de combate: pegarse al enemigo hace
	// que el bot sea trivial de esquivar.
	if (Distance > PreferredCombatDistance)
	{
		MoveToActor(CurrentTarget, PreferredCombatDistance * 0.8f);
	}
	else
	{
		StopMovement();
	}

	const bool bFacingTarget = AimAtTarget(DeltaSeconds);

	AWeaponBase* Weapon = Bot->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	if (Weapon->GetCurrentAmmo() <= 0)
	{
		Weapon->TryReload();
		return;
	}

	if (bFacingTarget)
	{
		Weapon->TryStartFire();
	}
	else
	{
		Weapon->StopFire();
	}
}

bool AShooterBotController::AimAtTarget(float DeltaSeconds)
{
	const APawn* Bot = GetPawn();
	if (!Bot || !CurrentTarget)
	{
		return false;
	}

	// Apuntar al pecho, no a los pies.
	const FVector TargetLocation = CurrentTarget->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FRotator DesiredRotation = (TargetLocation - Bot->GetActorLocation()).Rotation();

	const FRotator CurrentRotation = GetControlRotation();
	const FRotator NewRotation = FMath::RInterpConstantTo(
		CurrentRotation, DesiredRotation, DeltaSeconds, AimSpeed);

	SetControlRotation(NewRotation);

	// Se considera encarado si el error residual cabe en el margen de puntería:
	// así el bot falla de vez en cuando en lugar de acertar siempre.
	const float AngleError = FMath::Abs(
		FRotator::NormalizeAxis(DesiredRotation.Yaw - NewRotation.Yaw));

	return AngleError <= FMath::Max(AimErrorDegrees, 1.f);
}

void AShooterBotController::UpdatePatrol()
{
	// Solo se elige un destino nuevo al llegar al anterior.
	if (IsFollowingAPath())
	{
		return;
	}

	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		return;
	}

	FNavLocation Destination;
	if (NavSystem->GetRandomReachablePointInRadius(PatrolOrigin, PatrolRadius, Destination))
	{
		MoveToLocation(Destination.Location, 100.f);
	}
}
