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
#include "AIController.h"
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
				StaticEnum<EShooterGameModeType>()->GetNameStringByValue(
					static_cast<int64>(GameModeType)));
		}
	}

	MatchStartWorldTime = GetWorld()->GetTimeSeconds();

	// Rellenar con bots tras un margen, para no crearlos antes de que los
	// jugadores humanos terminen de entrar (RF-3.6).
	if (DesiredPlayerCount > 0 && BotControllerClass)
	{
		GetWorldTimerManager().SetTimer(BotFillTimerHandle, this,
			&AShooterGameMode::FillWithBots, FMath::Max(BotFillDelay, 0.1f), false);
	}

	// En modo entrenamiento la partida no termina: sin cronómetro ni marcador final.
	if (bEndlessMatch)
	{
		ShooterGameState->SetMatchEndWorldTime(0.f);
		return;
	}

	ShooterGameState->SetMatchEndWorldTime(GetWorld()->GetTimeSeconds() + MatchDuration);

	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AShooterGameMode::FinishMatch, MatchDuration, false);

	// En Dominación los puntos los generan las zonas de forma continua, así
	// que la victoria no puede detectarse solo al morir alguien.
	if (GameModeType == EShooterGameModeType::Domination)
	{
		GetWorldTimerManager().SetTimer(DominationCheckHandle, this,
			&AShooterGameMode::CheckWinCondition, 1.f, true);
	}
}

void AShooterGameMode::FinishMatch()
{
	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->SetMatchPhase(EMatchPhase::Finished);
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	GetWorldTimerManager().ClearTimer(DominationCheckHandle);

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

void AShooterGameMode::FillWithBots()
{
	if (DesiredPlayerCount <= 0 || !BotControllerClass)
	{
		return;
	}

	const AGameStateBase* CurrentState = GameState;
	const int32 HumanCount = CurrentState ? CurrentState->PlayerArray.Num() : 0;
	const int32 Missing = DesiredPlayerCount - HumanCount - SpawnedBots.Num();

	for (int32 Index = 0; Index < Missing; ++Index)
	{
		SpawnBot();
	}

	if (Missing > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Bots] %d creados; %d humanos en partida"),
			Missing, HumanCount);
	}
}

void AShooterGameMode::SpawnBot()
{
	if (!BotControllerClass || !DefaultPawnClass)
	{
		return;
	}

	FActorSpawnParameters ControllerParams;
	ControllerParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAIController* BotController = GetWorld()->SpawnActor<AAIController>(
		BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, ControllerParams);

	if (!BotController)
	{
		return;
	}

	// ChoosePlayerStart aplica también a los bots: aparecen lejos del enemigo.
	AActor* StartSpot = ChoosePlayerStart(BotController);
	const FTransform SpawnTransform = StartSpot
		? StartSpot->GetActorTransform()
		: FTransform::Identity;

	FActorSpawnParameters PawnParams;
	PawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* BotPawn = GetWorld()->SpawnActor<APawn>(
		DefaultPawnClass, SpawnTransform, PawnParams);

	if (!BotPawn)
	{
		BotController->Destroy();
		return;
	}

	BotController->Possess(BotPawn);
	SpawnedBots.Add(BotPawn);

	// Los bots también se reparten entre equipos, igual que los humanos.
	if (bTeamBased)
	{
		if (AShooterPlayerState* BotState = BotController->GetPlayerState<AShooterPlayerState>())
		{
			BotState->SetTeam(ChooseTeamForNewPlayer());
		}
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

			// En Dominación el marcador lo llevan las zonas: sumar también por
			// baja convertiría el modo en un combate por equipos con adorno.
			if (bTeamBased && ShooterGameState
				&& GameModeType != EShooterGameModeType::Domination)
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
