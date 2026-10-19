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
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Etiqueta de los puntos de aparición de cada equipo (PlayerStartTag). */
	FName GetTeamStartTag(EShooterTeam Team)
	{
		switch (Team)
		{
		case EShooterTeam::Alpha: return TEXT("Alfa");
		case EShooterTeam::Bravo: return TEXT("Bravo");
		default:                  return NAME_None;
		}
	}

	/** Indicativos de radio para los bots: un nombre se recuerda mejor que "Bot 3". */
	const TCHAR* const BotCallsigns[] = {
		TEXT("Halcón"), TEXT("Lobo"), TEXT("Cuervo"), TEXT("Tejón"), TEXT("Zorro"), TEXT("Puma"),
		TEXT("Búho"), TEXT("Lince"), TEXT("Jabalí"), TEXT("Águila"), TEXT("Coyote"), TEXT("Oso"),
		TEXT("Víbora"), TEXT("Gavilán"), TEXT("Toro"), TEXT("Cóndor")
	};
}

AShooterGameMode::AShooterGameMode()
{
	DefaultPawnClass = AShooterCharacter::StaticClass();
	PlayerControllerClass = AShooterPlayerController::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	GameStateClass = AShooterGameState::StaticClass();
	HUDClass = AShooterHUD::StaticClass();
}

void AShooterGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Opciones que llegan desde el menú en la URL del nivel
	// ("Lvl_Distrito?modo=dominacion?jugadores=8").
	const FString Mode = UGameplayStatics::ParseOption(Options, TEXT("modo"));
	if (Mode == TEXT("dominacion"))
	{
		GameModeType = EShooterGameModeType::Domination;
		bTeamBased = true;

		// Las zonas suman un punto por segundo cada una: 300 es dominar dos
		// zonas durante dos minutos y medio largos.
		ScoreToWin = 300;
	}
	else if (Mode == TEXT("todos"))
	{
		GameModeType = EShooterGameModeType::FreeForAll;
		bTeamBased = false;
		ScoreToWin = 25;
	}
	else if (Mode == TEXT("equipos"))
	{
		GameModeType = EShooterGameModeType::TeamDeathmatch;
		bTeamBased = true;
	}

	if (UGameplayStatics::HasOption(Options, TEXT("jugadores")))
	{
		DesiredPlayerCount = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("jugadores"), DesiredPlayerCount), 0, 16);
	}
}

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->SetPrimaryWeaponOptions(PrimaryWeaponOptions);
	}

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

	// Los bots dejan de pelear con la tabla final en pantalla.
	for (AAIController* BotController : BotControllers)
	{
		if (BotController)
		{
			BotController->StopMovement();
			if (AShooterCharacter* Bot = Cast<AShooterCharacter>(BotController->GetPawn()))
			{
				if (AWeaponBase* Weapon = Bot->GetCurrentWeapon())
				{
					Weapon->StopFire();
				}
			}
		}
	}

	if (PostMatchRestartDelay > 0.f && !bEndlessMatch)
	{
		GetWorldTimerManager().SetTimer(RestartTimerHandle, this, &AShooterGameMode::RestartMatch,
			PostMatchRestartDelay, false);
	}
}

void AShooterGameMode::RestartMatch()
{
	// Recarga el nivel con las mismas opciones: misma modalidad, mismos
	// jugadores. Abandonar la partida sigue disponible en el menú de pausa.
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}

int32 AShooterGameMode::CountHumanPlayers() const
{
	int32 Count = 0;
	if (const AGameStateBase* CurrentState = GameState)
	{
		for (const APlayerState* PlayerState : CurrentState->PlayerArray)
		{
			if (PlayerState && !PlayerState->IsABot())
			{
				++Count;
			}
		}
	}
	return Count;
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

	// Los bots también tienen PlayerState (para equipo, nombre y marcador):
	// los humanos se cuentan aparte.
	const int32 HumanCount = CountHumanPlayers();
	const int32 Missing = DesiredPlayerCount - HumanCount - BotControllers.Num();

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

	// Equipo, nombre y arma se deciden antes de aparecer: el punto de
	// aparición depende del equipo, y el arma se crea al poseer el pawn.
	if (AShooterPlayerState* BotState = BotController->GetPlayerState<AShooterPlayerState>())
	{
		if (bTeamBased)
		{
			BotState->SetTeam(ChooseTeamForNewPlayer());
		}

		const int32 NameCount = UE_ARRAY_COUNT(BotCallsigns);
		const int32 Round = BotNameIndex / NameCount;
		BotState->SetPlayerName(Round == 0
			? FString(BotCallsigns[BotNameIndex % NameCount])
			: FString::Printf(TEXT("%s %d"), BotCallsigns[BotNameIndex % NameCount], Round + 1));
		++BotNameIndex;

		if (PrimaryWeaponOptions.Num() > 1 && FMath::FRand() < MarksmanBotChance)
		{
			BotState->SetPreferredPrimary(PrimaryWeaponOptions[1]);
		}
	}

	BotControllers.Add(BotController);

	if (!SpawnPawnForBot(BotController))
	{
		BotControllers.Remove(BotController);
		BotController->Destroy();
	}
}

bool AShooterGameMode::SpawnPawnForBot(AAIController* BotController)
{
	if (!BotController || !DefaultPawnClass)
	{
		return false;
	}

	// ChoosePlayerStart aplica también a los bots: aparecen en su lado y
	// lejos del enemigo.
	AActor* StartSpot = ChoosePlayerStart(BotController);
	const FTransform SpawnTransform = StartSpot
		? StartSpot->GetActorTransform()
		: FTransform::Identity;

	FActorSpawnParameters PawnParams;
	PawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* BotPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, SpawnTransform, PawnParams);
	if (!BotPawn)
	{
		return false;
	}

	BotController->Possess(BotPawn);
	BotController->SetControlRotation(SpawnTransform.Rotator());
	return true;
}

void AShooterGameMode::RespawnBot(AAIController* BotController)
{
	if (!BotController)
	{
		return;
	}

	const AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();
	if (ShooterGameState && ShooterGameState->GetMatchPhase() != EMatchPhase::InProgress)
	{
		return;
	}

	if (APawn* OldPawn = BotController->GetPawn())
	{
		BotController->UnPossess();
		OldPawn->Destroy();
	}

	SpawnPawnForBot(BotController);
}

void AShooterGameMode::OnBotKilled(AController* Victim, AController* Killer)
{
	AAIController* BotController = Cast<AAIController>(Victim);
	if (!BotController)
	{
		return;
	}

	RegisterKill(Victim, Killer);

	// Solo reaparecen los bots de relleno: los enemigos de Oleadas los
	// gestiona su propio gestor.
	if (!BotControllers.Contains(BotController))
	{
		CheckWinCondition();
		return;
	}

	FTimerHandle RespawnTimer;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AShooterGameMode::RespawnBot, BotController);
	GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnDelay, false);

	CheckWinCondition();
}

void AShooterGameMode::PostLogin(APlayerController* NewPlayer)
{
	// El equipo se asigna antes de Super: Super hace aparecer al jugador, y
	// el punto de aparición depende del equipo.
	if (bTeamBased)
	{
		if (AShooterPlayerState* ShooterPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr)
		{
			ShooterPlayerState->SetTeam(ChooseTeamForNewPlayer());
		}
	}

	Super::PostLogin(NewPlayer);
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

	RegisterKill(Victim, Killer);

	FString KillerName = TEXT("El entorno");
	FString WeaponName = TEXT("");
	if (Killer && Killer != Victim)
	{
		if (const AShooterPlayerState* KillerState = Killer->GetPlayerState<AShooterPlayerState>())
		{
			KillerName = KillerState->GetPlayerName();
		}
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
	}
	else if (Killer == Victim)
	{
		KillerName = TEXT("Suicidio");
	}

	Victim->Client_NotifyKilledBy(KillerName, WeaponName);

	// Enfocar al asesino durante la espera (RF-6.7). Se envía el pawn y no el
	// controlador porque el cliente solo recibe actores relevantes para él.
	if (Killer && Killer != Victim)
	{
		Victim->Client_StartKillCam(Killer->GetPawn());
	}

	const float RespawnWorldTime = GetWorld()->GetTimeSeconds() + RespawnDelay;
	Victim->SetRespawnTime(RespawnWorldTime);

	FTimerHandle RespawnTimer;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AShooterGameMode::RespawnPlayer, Victim);
	GetWorldTimerManager().SetTimer(RespawnTimer, RespawnDelegate, RespawnDelay, false);

	CheckWinCondition();
}

void AShooterGameMode::RegisterKill(AController* Victim, AController* Killer)
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

	// Un jugador no suma por eliminarse a sí mismo ni por abatir a un
	// compañero: el fuego amigo existe, pero no puntúa.
	if (Killer && Killer != Victim)
	{
		AShooterPlayerState* KillerState = Killer->GetPlayerState<AShooterPlayerState>();
		const bool bTeamKill = bTeamBased && KillerState && VictimState
			&& KillerState->GetTeam() != EShooterTeam::None
			&& KillerState->GetTeam() == VictimState->GetTeam();

		if (KillerState && bTeamKill)
		{
			KillerName = KillerState->GetPlayerName();
		}
		else if (KillerState)
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

	// Registro visible para todos (RF-7.7).
	if (ShooterGameState)
	{
		const FString VictimName = VictimState
			? VictimState->GetPlayerName() : FString(TEXT("Jugador"));

		ShooterGameState->AddKillEvent(KillerName, VictimName, WeaponName, false);
	}
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

	// Con equipos, cada uno aparece en su lado del mapa: los puntos llevan la
	// etiqueta del equipo. Si el mapa no los etiqueta, valen todos.
	const FName TeamTag = bTeamBased ? GetTeamStartTag(PlayerTeam) : NAME_None;
	if (!TeamTag.IsNone())
	{
		TArray<APlayerStart*> TeamStarts = Candidates.FilterByPredicate(
			[TeamTag](const APlayerStart* Start) { return Start && Start->PlayerStartTag == TeamTag; });
		if (TeamStarts.Num() > 0)
		{
			Candidates = MoveTemp(TeamStarts);
		}
	}

	// Un punto con alguien encima haría aparecer a dos personajes solapados.
	TArray<APlayerStart*> FreeStarts = Candidates.FilterByPredicate([this, PlayerPawn](const APlayerStart* Start)
	{
		for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != PlayerPawn && !It->IsDead()
				&& FVector::DistSquared2D(It->GetActorLocation(), Start->GetActorLocation()) < FMath::Square(150.f))
			{
				return false;
			}
		}
		return true;
	});
	if (FreeStarts.Num() > 0)
	{
		Candidates = MoveTemp(FreeStarts);
	}

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
