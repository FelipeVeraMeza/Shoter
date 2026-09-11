#include "Core/WaveManager.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Player/ShooterCharacter.h"
#include "Combat/HealthComponent.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AWaveManager::AWaveManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AWaveManager::BeginPlay()
{
	Super::BeginPlay();
}

void AWaveManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWaveManager, CurrentWave);
	DOREPLIFETIME(AWaveManager, EnemiesRemaining);
	DOREPLIFETIME(AWaveManager, bWaveInProgress);
	DOREPLIFETIME(AWaveManager, NextWaveWorldTime);
}

float AWaveManager::GetTimeUntilNextWave() const
{
	if (bWaveInProgress || NextWaveWorldTime <= 0.f)
	{
		return 0.f;
	}

	return FMath::Max(NextWaveWorldTime - GetWorld()->GetTimeSeconds(), 0.f);
}

void AWaveManager::StartWaves()
{
	if (!HasAuthority() || !EnemyClass || !EnemyControllerClass)
	{
		return;
	}

	CurrentWave = 0;

	// La primera oleada también espera: da margen a leer el HUD antes de que
	// aparezca nada.
	NextWaveWorldTime = GetWorld()->GetTimeSeconds() + TimeBetweenWaves;
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this,
		&AWaveManager::BeginNextWave, TimeBetweenWaves, false);
}

void AWaveManager::StopWaves()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

	bWaveInProgress = false;
	PendingSpawns = 0;
}

void AWaveManager::BeginNextWave()
{
	if (!HasAuthority())
	{
		return;
	}

	// Una oleada que empieza con la partida terminada dejaría enemigos
	// deambulando sobre la pantalla de resultados.
	const AShooterGameState* GameState = GetWorld()->GetGameState<AShooterGameState>();
	if (GameState && GameState->GetMatchPhase() == EMatchPhase::Finished)
	{
		return;
	}

	++CurrentWave;
	bWaveInProgress = true;
	NextWaveWorldTime = 0.f;

	// Crecimiento lineal: la oleada N trae BaseEnemyCount + (N-1) * EnemiesPerWave.
	PendingSpawns = BaseEnemyCount + (CurrentWave - 1) * EnemiesPerWave;
	EnemiesRemaining = PendingSpawns;

	OnWaveStarted.Broadcast(CurrentWave);

	// Escalonar las apariciones evita el pico de coste de crear todos los
	// enemigos en el mismo frame (RNF-1.1).
	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this,
		&AWaveManager::SpawnEnemy, FMath::Max(SpawnInterval, 0.05f), true, 0.f);
}

void AWaveManager::SpawnEnemy()
{
	if (!HasAuthority() || PendingSpawns <= 0)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		return;
	}

	// Respetar el tope de concurrentes: se reintentará en el siguiente tick
	// del temporizador, cuando alguno haya caído.
	if (LiveEnemies.Num() >= MaxConcurrentEnemies)
	{
		return;
	}

	FTransform SpawnTransform;
	if (!FindSpawnTransform(SpawnTransform))
	{
		return;
	}

	FActorSpawnParameters ControllerParams;
	ControllerParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAIController* Controller = GetWorld()->SpawnActor<AAIController>(
		EnemyControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, ControllerParams);

	if (!Controller)
	{
		return;
	}

	FActorSpawnParameters PawnParams;
	PawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AShooterCharacter* Enemy = GetWorld()->SpawnActor<AShooterCharacter>(
		EnemyClass, SpawnTransform, PawnParams);

	if (!Enemy)
	{
		Controller->Destroy();
		return;
	}

	Controller->Possess(Enemy);

	// Todos los enemigos van al equipo Bravo para que la IA trate a los
	// jugadores (Alfa) como hostiles sin lógica adicional.
	if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
	{
		State->SetTeam(EShooterTeam::Bravo);
	}

	if (UHealthComponent* Health = Enemy->FindComponentByClass<UHealthComponent>())
	{
		Health->OnOwnerDied.AddDynamic(this, &AWaveManager::HandleEnemyDeath);
	}

	LiveEnemies.Add(Enemy);
	--PendingSpawns;
}

bool AWaveManager::FindSpawnTransform(FTransform& OutTransform) const
{
	TArray<FVector> PlayerLocations;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = It->Get())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				PlayerLocations.Add(Pawn->GetActorLocation());
			}
		}
	}

	// Se elige el punto más alejado del jugador más cercano: aparecer a la
	// espalda de alguien convierte la oleada en una emboscada injusta.
	const APlayerStart* BestStart = nullptr;
	float BestDistance = -1.f;

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		const APlayerStart* Start = *It;
		float NearestPlayer = TNumericLimits<float>::Max();

		for (const FVector& Location : PlayerLocations)
		{
			NearestPlayer = FMath::Min(NearestPlayer,
				FVector::Dist(Start->GetActorLocation(), Location));
		}

		// Sin jugadores vivos cualquier punto sirve.
		if (PlayerLocations.Num() == 0)
		{
			NearestPlayer = MinimumSpawnDistance;
		}

		if (NearestPlayer > BestDistance)
		{
			BestDistance = NearestPlayer;
			BestStart = Start;
		}
	}

	if (!BestStart)
	{
		return false;
	}

	OutTransform = BestStart->GetActorTransform();
	return true;
}

void AWaveManager::HandleEnemyDeath(AActor* DeadActor, AController* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	LiveEnemies.RemoveAll([DeadActor](const TObjectPtr<APawn>& Pawn)
	{
		return Pawn == DeadActor || Pawn == nullptr;
	});

	EnemiesRemaining = FMath::Max(EnemiesRemaining - 1, 0);

	CheckWaveCleared();
}

void AWaveManager::CheckWaveCleared()
{
	// La oleada termina cuando no queda nada por generar ni nadie vivo.
	if (PendingSpawns > 0 || EnemiesRemaining > 0)
	{
		return;
	}

	bWaveInProgress = false;
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

	OnWaveCleared.Broadcast(CurrentWave);

	NextWaveWorldTime = GetWorld()->GetTimeSeconds() + TimeBetweenWaves;
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this,
		&AWaveManager::BeginNextWave, TimeBetweenWaves, false);
}
