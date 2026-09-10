#include "Core/ShooterGameMode.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHUD.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AShooterGameMode::AShooterGameMode()
{
	DefaultPawnClass = AShooterCharacter::StaticClass();
	PlayerControllerClass = AShooterPlayerController::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	GameStateClass = AShooterGameState::StaticClass();
	HUDClass = AShooterHUD::StaticClass();
}

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	StartMatch();
}

void AShooterGameMode::StartMatch()
{
	AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();
	if (!ShooterGameState)
	{
		return;
	}

	ShooterGameState->SetMatchPhase(EMatchPhase::InProgress);

	// En modo entrenamiento la partida no termina: sin cronómetro ni marcador final.
	if (bEndlessMatch)
	{
		ShooterGameState->SetMatchEndWorldTime(0.f);
		return;
	}

	ShooterGameState->SetMatchEndWorldTime(GetWorld()->GetTimeSeconds() + MatchDuration);

	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AShooterGameMode::FinishMatch, MatchDuration, false);
}

void AShooterGameMode::FinishMatch()
{
	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->SetMatchPhase(EMatchPhase::Finished);
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
}

void AShooterGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!bTeamBased)
	{
		return;
	}

	if (AShooterPlayerState* ShooterPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		ShooterPlayerState->SetTeam(ChooseTeamForNewPlayer());
	}
}

EShooterTeam AShooterGameMode::ChooseTeamForNewPlayer() const
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return EShooterTeam::Alpha;
	}

	int32 CountAlpha = 0;
	int32 CountBravo = 0;

	for (const APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!ShooterPlayerState)
		{
			continue;
		}

		if (ShooterPlayerState->GetTeam() == EShooterTeam::Alpha) { ++CountAlpha; }
		else if (ShooterPlayerState->GetTeam() == EShooterTeam::Bravo) { ++CountBravo; }
	}

	return CountAlpha <= CountBravo ? EShooterTeam::Alpha : EShooterTeam::Bravo;
}

void AShooterGameMode::OnPlayerKilled(AShooterPlayerController* Victim, AController* Killer)
{
	if (!Victim)
	{
		return;
	}

	AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();

	if (AShooterPlayerState* VictimState = Victim->GetPlayerState<AShooterPlayerState>())
	{
		VictimState->AddDeath();
	}

	// Un jugador no suma por eliminarse a sí mismo.
	if (Killer && Killer != Victim)
	{
		if (AShooterPlayerState* KillerState = Killer->GetPlayerState<AShooterPlayerState>())
		{
			KillerState->AddKill();

			if (bTeamBased && ShooterGameState)
			{
				ShooterGameState->AddTeamScore(KillerState->GetTeam(), 1);
			}
		}
	}

	const float RespawnWorldTime = GetWorld()->GetTimeSeconds() + RespawnDelay;
	Victim->SetRespawnTime(RespawnWorldTime);

	FTimerHandle RespawnTimer;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AShooterGameMode::RespawnPlayer, Victim);
	GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnDelay, false);

	CheckWinCondition();
}

void AShooterGameMode::CheckWinCondition()
{
	if (bEndlessMatch || ScoreToWin <= 0)
	{
		return;
	}

	AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();
	if (!ShooterGameState || ShooterGameState->GetMatchPhase() != EMatchPhase::InProgress)
	{
		return;
	}

	if (bTeamBased)
	{
		if (ShooterGameState->GetTeamScore(EShooterTeam::Alpha) >= ScoreToWin ||
			ShooterGameState->GetTeamScore(EShooterTeam::Bravo) >= ScoreToWin)
		{
			FinishMatch();
		}
		return;
	}

	for (const APlayerState* PlayerState : ShooterGameState->PlayerArray)
	{
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (ShooterPlayerState && ShooterPlayerState->GetKills() >= ScoreToWin)
		{
			FinishMatch();
			return;
		}
	}
}

void AShooterGameMode::RespawnPlayer(AShooterPlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();
	if (ShooterGameState && ShooterGameState->GetMatchPhase() != EMatchPhase::InProgress)
	{
		return;
	}

	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	Controller->SetRespawnTime(0.f);
	RestartPlayer(Controller);
}

AActor* AShooterGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> Candidates;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Candidates.Add(*It);
	}

	if (Candidates.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const AShooterPlayerState* PlayerTeamState = Player ? Player->GetPlayerState<AShooterPlayerState>() : nullptr;
	const EShooterTeam PlayerTeam = PlayerTeamState ? PlayerTeamState->GetTeam() : EShooterTeam::None;
	const APawn* PlayerPawn = Player ? Player->GetPawn() : nullptr;

	APlayerStart* BestStart = nullptr;
	float BestDistanceToEnemy = -1.f;

	for (APlayerStart* Start : Candidates)
	{
		float NearestEnemyDistance = TNumericLimits<float>::Max();

		for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
		{
			const AShooterCharacter* Other = *It;
			if (!Other || Other->IsDead() || Other == PlayerPawn)
			{
				continue;
			}

			const AShooterPlayerState* OtherState = Other->GetPlayerState<AShooterPlayerState>();
			const bool bSameTeam = bTeamBased && OtherState && PlayerTeam != EShooterTeam::None
				&& OtherState->GetTeam() == PlayerTeam;

			if (bSameTeam)
			{
				continue;
			}

			NearestEnemyDistance = FMath::Min(NearestEnemyDistance,
				FVector::Dist(Start->GetActorLocation(), Other->GetActorLocation()));
		}

		// Sin enemigos vivos, cualquier punto sirve.
		if (NearestEnemyDistance == TNumericLimits<float>::Max())
		{
			return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		}

		if (NearestEnemyDistance > BestDistanceToEnemy)
		{
			BestDistanceToEnemy = NearestEnemyDistance;
			BestStart = Start;
		}
	}

	return BestStart ? BestStart : Super::ChoosePlayerStart_Implementation(Player);
}
