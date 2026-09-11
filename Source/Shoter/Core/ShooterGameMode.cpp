#include "Core/ShooterGameMode.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHUD.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Telemetry/ShooterTelemetrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
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

	// Telemetría de partida (RF-13.1).
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterTelemetrySubsystem* Telemetry =
			GameInstance->GetSubsystem<UShooterTelemetrySubsystem>())
		{
			Telemetry->RecordMatchStart(
				GetWorld()->GetMapName(),
				bTeamBased ? TEXT("Equipos") : TEXT("TodosContraTodos"));
		}
	}

	MatchStartWorldTime = GetWorld()->GetTimeSeconds();

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

	AwardEndOfMatchExperience();
}

void AShooterGameMode::AwardEndOfMatchExperience()
{
	// La progresión es local a cada cliente; en una partida en escucha solo el
	// anfitrión tiene GameInstance con el subsistema. Cuando exista el backend
	// (RNF-3.4), el servidor reportará el resultado de todos.
	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UShooterProgressionSubsystem* Progression =
		GameInstance->GetSubsystem<UShooterProgressionSubsystem>();
	if (!Progression)
	{
		return;
	}

	const APlayerController* LocalController = GEngine
		? GEngine->GetFirstLocalPlayerController(GetWorld())
		: nullptr;
	if (!LocalController)
	{
		return;
	}

	const AShooterPlayerState* State = LocalController->GetPlayerState<AShooterPlayerState>();
	if (!State)
	{
		return;
	}

	Progression->RecordMatchResult(State->GetKills(), State->GetDeaths(), 0);

	if (UShooterTelemetrySubsystem* Telemetry =
		GameInstance->GetSubsystem<UShooterTelemetrySubsystem>())
	{
		Telemetry->RecordMatchEnd(
			GetWorld()->GetTimeSeconds() - MatchStartWorldTime,
			State->GetKills(), State->GetDeaths());
	}
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

	AShooterPlayerState* VictimState = Victim->GetPlayerState<AShooterPlayerState>();
	if (VictimState)
	{
		VictimState->AddDeath();
	}

	FString KillerName = TEXT("El entorno");
	FString WeaponName = TEXT("");

	// Un jugador no suma por eliminarse a sí mismo.
	if (Killer && Killer != Victim)
	{
		if (AShooterPlayerState* KillerState = Killer->GetPlayerState<AShooterPlayerState>())
		{
			KillerState->AddKill();
			KillerName = KillerState->GetPlayerName();

			if (const AShooterCharacter* KillerPawn = Cast<AShooterCharacter>(Killer->GetPawn()))
			{
				if (const AWeaponBase* Weapon = KillerPawn->GetCurrentWeapon())
				{
					if (const UWeaponDataAsset* Data = Weapon->WeaponData)
					{
						WeaponName = Data->DisplayName.ToString();
					}
				}
			}

			if (bTeamBased && ShooterGameState)
			{
				ShooterGameState->AddTeamScore(KillerState->GetTeam(), 1);
			}
		}
	}
	else if (Killer == Victim)
	{
		KillerName = TEXT("Suicidio");
	}

	// Registro visible para todos (RF-7.7) y aviso a la víctima (RF-6.6).
	if (ShooterGameState)
	{
		const FString VictimName = VictimState
			? VictimState->GetPlayerName() : FString(TEXT("Jugador"));

		ShooterGameState->AddKillEvent(KillerName, VictimName, WeaponName, false);
	}

	Victim->Client_NotifyKilledBy(KillerName, WeaponName);

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
