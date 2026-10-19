#include "UI/ShooterHUD.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Core/ControlZone.h"
#include "Core/WaveManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Combat/HealthComponent.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Settings/ShooterSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Player/ShooterFirstPersonComponent.h"
#include "UI/ShooterMenuSubsystem.h"

// Todo texto visible pasa por LOCTEXT: así se extrae a las tablas de
// localización sin tocar el código (RNF-6.1).
#define LOCTEXT_NAMESPACE "ShooterHUD"

namespace
{
	const FLinearColor PanelBackground(0.02f, 0.03f, 0.05f, 0.55f);
	const FLinearColor PanelBorder(1.f, 1.f, 1.f, 0.12f);
	const FLinearColor TextPrimary(0.96f, 0.97f, 1.f, 1.f);
	const FLinearColor TextMuted(0.62f, 0.66f, 0.74f, 1.f);
	const FLinearColor AccentLow(0.95f, 0.28f, 0.22f, 1.f);

	/** Panel con fondo translúcido y un borde fino, para que el texto se lea sobre cualquier fondo. */
	void DrawPanel(AHUD* HUD, float X, float Y, float Width, float Height)
	{
		HUD->DrawRect(PanelBackground, X, Y, Width, Height);
		HUD->DrawRect(PanelBorder, X, Y, Width, 1.f);
		HUD->DrawRect(PanelBorder, X, Y + Height - 1.f, Width, 1.f);
	}
}

void AShooterHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(PlayerOwner);
	AShooterCharacter* Character = ShooterPC ? Cast<AShooterCharacter>(ShooterPC->GetPawn()) : nullptr;

	DrawMatchInfo();

	// En el modo realista la información sale del mundo, no de la pantalla:
	// sin mira, sin minimapa y sin barra de vida. La salud se intuye por la
	// visión (UpdateCameraEffects) y la munición, mirando el cargador.
	const bool bRealistic = IsRealisticHUD();

	// El visor va debajo de todo lo demás: es lo que ve el ojo, no interfaz.
	if (Character && !Character->IsDead())
	{
		if (const UShooterFirstPersonComponent* FirstPerson = Character->GetFirstPersonView())
		{
			if (FirstPerson->IsScopeViewActive())
			{
				DrawScopeOverlay();
			}
		}
	}

	if (Character && !Character->IsDead())
	{
		if (!bRealistic)
		{
			DrawCrosshair(Character);
			DrawHealth(Character);
			DrawMinimap(Character);
		}

		DrawAmmo(Character, bRealistic);
		DrawStatus(Character);
		DrawDamageIndicator(Character, ShooterPC);
	}

	DrawKillFeed();
	DrawControlZones();
	DrawWaveInfo();
	DrawConnectionWarning(ShooterPC);

	if (ShooterPC)
	{
		if (!bRealistic)
		{
			DrawHitMarker(ShooterPC);
		}
		DrawRespawnCountdown(ShooterPC);
	}

	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	const bool bMatchOver = ShooterGameState
		&& ShooterGameState->GetMatchPhase() == EMatchPhase::Finished;

	if (ShooterPC)
	{
		DrawHints(ShooterPC);
	}

	if (bMatchOver)
	{
		DrawScoreboard();
	}
	else if (ShooterPC && ShooterPC->IsInputKeyDown(EKeys::Tab))
	{
		DrawLiveScoreboard();
	}
	else if (ShooterPC && ShooterPC->IsControlsHelpVisible())
	{
		DrawControlsHelp();
	}
}

void AShooterHUD::DrawScopeOverlay()
{
	const float Width = Canvas->ClipX;
	const float Height = Canvas->ClipY;
	const float CenterX = Width * 0.5f;
	const float CenterY = Height * 0.5f;
	const float Radius = Height * 0.46f;

	const FLinearColor Black(0.f, 0.f, 0.f, 1.f);

	// Fuera del campo, negro. Se rellena por franjas horizontales: cada una
	// tapa a izquierda y derecha del círculo a esa altura.
	DrawRect(Black, 0.f, 0.f, Width, CenterY - Radius);
	DrawRect(Black, 0.f, CenterY + Radius, Width, Height - (CenterY + Radius));

	const float Step = 2.f;
	for (float Y = CenterY - Radius; Y < CenterY + Radius; Y += Step)
	{
		const float Dy = (Y + Step * 0.5f) - CenterY;
		const float HalfWidth = FMath::Sqrt(FMath::Max(Radius * Radius - Dy * Dy, 0.f));
		DrawRect(Black, 0.f, Y, CenterX - HalfWidth + 1.f, Step);
		DrawRect(Black, CenterX + HalfWidth - 1.f, Y, Width - (CenterX + HalfWidth) + 1.f, Step);

		// Viñeta en el borde del campo: la lente oscurece hacia fuera.
		const float Edge = HalfWidth * 0.08f + 4.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), CenterX - HalfWidth, Y, Edge, Step);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), CenterX + HalfWidth - Edge, Y, Edge, Step);
	}

	// Retícula dúplex: hilos finos en el centro y postes gruesos hacia fuera.
	const FLinearColor Reticle(0.02f, 0.02f, 0.02f, 0.95f);
	const float Thin = FMath::Max(1.f, Height / 900.f);
	const float Thick = Thin * 4.f;
	const float Inner = Radius * 0.45f;

	DrawRect(Reticle, CenterX - Inner, CenterY - Thin * 0.5f, Inner * 2.f, Thin);
	DrawRect(Reticle, CenterX - Thin * 0.5f, CenterY - Inner, Thin, Inner * 2.f);

	DrawRect(Reticle, CenterX - Radius, CenterY - Thick * 0.5f, Radius - Inner, Thick);
	DrawRect(Reticle, CenterX + Inner, CenterY - Thick * 0.5f, Radius - Inner, Thick);
	DrawRect(Reticle, CenterX - Thick * 0.5f, CenterY + Inner, Thick, Radius - Inner);
	DrawRect(Reticle, CenterX - Thick * 0.5f, CenterY - Radius, Thick, Radius - Inner);

	// Marcas de medición en el hilo vertical inferior, para compensar la
	// caída a larga distancia.
	for (int32 Mark = 1; Mark <= 4; ++Mark)
	{
		const float MarkY = CenterY + Inner * 0.2f * Mark;
		const float MarkWidth = (Mark % 2 == 0) ? Inner * 0.12f : Inner * 0.07f;
		DrawRect(Reticle, CenterX - MarkWidth * 0.5f, MarkY - Thin * 0.5f, MarkWidth, Thin);
	}
}

void AShooterHUD::DrawControlsHelp()
{
	struct FKeyLine
	{
		FText Action;
		FText Keys;
	};

	const FKeyLine Lines[] = {
		{ LOCTEXT("HelpMove", "Moverse"), LOCTEXT("HelpMoveKeys", "W A S D") },
		{ LOCTEXT("HelpSprint", "Correr"), LOCTEXT("HelpSprintKeys", "Mayús") },
		{ LOCTEXT("HelpCrouch", "Agacharse"), LOCTEXT("HelpCrouchKeys", "Ctrl / C") },
		{ LOCTEXT("HelpJump", "Saltar"), LOCTEXT("HelpJumpKeys", "Espacio") },
		{ LOCTEXT("HelpLean", "Asomarse"), LOCTEXT("HelpLeanKeys", "Q / E") },
		{ LOCTEXT("HelpFire", "Disparar"), LOCTEXT("HelpFireKeys", "Clic izquierdo") },
		{ LOCTEXT("HelpAim", "Apuntar"), LOCTEXT("HelpAimKeys", "Clic derecho") },
		{ LOCTEXT("HelpBreath", "Contener la respiración"), LOCTEXT("HelpBreathKeys", "Mayús apuntando") },
		{ LOCTEXT("HelpReload", "Recargar / mirar cargador"), LOCTEXT("HelpReloadKeys", "R") },
		{ LOCTEXT("HelpFireMode", "Selector de fuego"), LOCTEXT("HelpFireModeKeys", "B") },
		{ LOCTEXT("HelpSwitch", "Cambiar de arma"), LOCTEXT("HelpSwitchKeys", "Rueda / 1") },
		{ LOCTEXT("HelpGrenade", "Granada"), LOCTEXT("HelpGrenadeKeys", "G") },
		{ LOCTEXT("HelpBandage", "Vendarse"), LOCTEXT("HelpBandageKeys", "H") },
		{ LOCTEXT("HelpScore", "Tabla de posiciones"), LOCTEXT("HelpScoreKeys", "Tab") },
		{ LOCTEXT("HelpLoadout", "Equipamiento"), LOCTEXT("HelpLoadoutKeys", "L") },
		{ LOCTEXT("HelpPause", "Pausa"), LOCTEXT("HelpPauseKeys", "Esc / P") },
	};

	const float RowHeight = 24.f;
	const float Width = 420.f;
	const float Height = 64.f + RowHeight * UE_ARRAY_COUNT(Lines);
	const float X = Canvas->ClipX - Width - 42.f;
	const float Y = FMath::Max((Canvas->ClipY - Height) * 0.5f, 20.f);

	DrawPanel(this, X, Y, Width, Height);
	DrawText(LOCTEXT("HelpTitle", "CONTROLES  (F1 para cerrar)").ToString(), TextPrimary,
		X + 22.f, Y + 16.f, GEngine->GetMediumFont(), 1.f);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
	{
		const float RowY = Y + 52.f + Index * RowHeight;
		DrawText(Lines[Index].Action.ToString(), TextMuted, X + 22.f, RowY, GEngine->GetSmallFont(), 1.1f);
		DrawText(Lines[Index].Keys.ToString(), TextPrimary, X + Width * 0.58f, RowY, GEngine->GetSmallFont(), 1.1f);
	}
}

void AShooterHUD::DrawHints(AShooterPlayerController* Controller)
{
	// Con un menú abierto, los avisos sobran.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UShooterMenuSubsystem* Menu = GameInstance->GetSubsystem<UShooterMenuSubsystem>())
		{
			if (Menu->IsMenuOpen())
			{
				return;
			}
		}
	}

	const float CenterX = Canvas->ClipX * 0.5f;

	if (Controller->GetTimeSinceLoadoutChange() < 4.f)
	{
		const FString Text = LOCTEXT("LoadoutSaved", "Equipamiento guardado: se aplicará al reaparecer").ToString();
		float TextWidth = 0.f;
		float TextHeight = 0.f;
		GetTextSize(Text, TextWidth, TextHeight, GEngine->GetMediumFont(), 1.f);
		DrawText(Text, FLinearColor(0.95f, 0.75f, 0.3f, 1.f), CenterX - TextWidth * 0.5f, Canvas->ClipY * 0.72f,
			GEngine->GetMediumFont(), 1.f);
	}

	// Los primeros segundos de cada vida, un recordatorio discreto de las
	// teclas que no son evidentes.
	const float SinceSpawn = Controller->GetTimeSinceSpawn();
	if (Controller->GetPawn() && SinceSpawn < 10.f && !Controller->IsControlsHelpVisible())
	{
		const float Alpha = FMath::Clamp((10.f - SinceSpawn) / 1.5f, 0.f, 1.f) * 0.85f;
		const FString Text = LOCTEXT("SpawnHint", "F1 controles   ·   L equipamiento   ·   Esc pausa").ToString();
		float TextWidth = 0.f;
		float TextHeight = 0.f;
		GetTextSize(Text, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.1f);
		DrawText(Text, FLinearColor(0.9f, 0.9f, 0.88f, Alpha), CenterX - TextWidth * 0.5f, Canvas->ClipY - 70.f,
			GEngine->GetSmallFont(), 1.1f);
	}
}

namespace
{
	/** Jugadores ordenados por bajas, de más a menos. */
	TArray<const AShooterPlayerState*> GetSortedPlayers(const AGameStateBase* GameState)
	{
		TArray<const AShooterPlayerState*> Players;

		if (!GameState)
		{
			return Players;
		}

		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState))
			{
				Players.Add(ShooterState);
			}
		}

		Players.Sort([](const AShooterPlayerState& A, const AShooterPlayerState& B)
		{
			if (A.GetKills() != B.GetKills())
			{
				return A.GetKills() > B.GetKills();
			}
			return A.GetDeaths() < B.GetDeaths();
		});

		return Players;
	}

	FText TeamName(EShooterTeam Team)
	{
		switch (Team)
		{
		case EShooterTeam::Alpha: return LOCTEXT("TeamAlpha", "ALFA");
		case EShooterTeam::Bravo: return LOCTEXT("TeamBravo", "BRAVO");
		default: return LOCTEXT("TeamNone", "-");
		}
	}

	/**
	 * Nombre para mostrar de un jugador sin nombre propio.
	 *
	 * El índice se pasa como argumento con nombre en vez de concatenarse: hay
	 * idiomas que anteponen el número al sustantivo, y solo el traductor puede
	 * decidir ese orden (RNF-6.1).
	 */
	FText FallbackPlayerName(int32 Index)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Number"), FText::AsNumber(Index));
		return FText::Format(LOCTEXT("UnnamedPlayer", "Jugador {Number}"), Args);
	}
}

void AShooterHUD::DrawScoreboard()
{
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!ShooterGameState)
	{
		return;
	}

	const TArray<const AShooterPlayerState*> Players = GetSortedPlayers(ShooterGameState);

	const float Width = FMath::Min(Canvas->ClipX * 0.7f, 720.f);
	const float RowHeight = 30.f;
	const float HeaderHeight = 124.f;
	const float Height = HeaderHeight + RowHeight * FMath::Max(Players.Num(), 1) + 24.f;

	const float X = (Canvas->ClipX - Width) * 0.5f;
	const float Y = FMath::Max((Canvas->ClipY - Height) * 0.5f, 20.f);

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawPanel(this, X, Y, Width, Height);

	DrawText(LOCTEXT("MatchOver", "PARTIDA TERMINADA").ToString(), FLinearColor(1.f, 0.85f, 0.25f, 1.f),
		X + 28.f, Y + 22.f, GEngine->GetLargeFont(), 1.7f);

	// En modo por equipos gana el equipo con más puntos; en todos contra todos,
	// el jugador que encabeza la tabla.
	const bool bTeamMode = ShooterGameState->GetTeamScore(EShooterTeam::Alpha) > 0
		|| ShooterGameState->GetTeamScore(EShooterTeam::Bravo) > 0;

	FText ResultText;
	if (bTeamMode)
	{
		const EShooterTeam Winner = ShooterGameState->GetWinningTeam();
		if (Winner == EShooterTeam::None)
		{
			ResultText = LOCTEXT("ResultDraw", "EMPATE");
		}
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Team"), TeamName(Winner));
			ResultText = FText::Format(LOCTEXT("ResultTeamWins", "GANA EQUIPO {Team}"), Args);
		}

		FFormatNamedArguments ScoreArgs;
		ScoreArgs.Add(TEXT("Alpha"), FText::AsNumber(ShooterGameState->GetTeamScore(EShooterTeam::Alpha)));
		ScoreArgs.Add(TEXT("Bravo"), FText::AsNumber(ShooterGameState->GetTeamScore(EShooterTeam::Bravo)));
		DrawText(FText::Format(LOCTEXT("TeamScoreLine", "{Alpha}  -  {Bravo}"), ScoreArgs).ToString(),
			TextMuted, X + Width - 130.f, Y + 58.f, GEngine->GetMediumFont(), 1.3f);
	}
	else if (Players.Num() > 0)
	{
		const FText Name = Players[0]->GetPlayerName().IsEmpty()
			? FallbackPlayerName(1)
			: FText::FromString(Players[0]->GetPlayerName());

		FFormatNamedArguments Args;
		Args.Add(TEXT("Player"), Name);
		ResultText = FText::Format(LOCTEXT("ResultPlayerWins", "GANA {Player}"), Args);

		// Plural gestionado por el motor: hay idiomas con más de dos formas,
		// así que la concordancia no puede resolverse concatenando en código.
		DrawText(FText::Format(
			LOCTEXT("KillCount", "{Kills} {Kills}|plural(one=baja,other=bajas)"),
			FFormatNamedArguments{ { TEXT("Kills"), FText::AsNumber(Players[0]->GetKills()) } }).ToString(),
			TextMuted, X + Width - 130.f, Y + 58.f, GEngine->GetMediumFont(), 1.3f);
	}
	else
	{
		ResultText = LOCTEXT("ResultNone", "SIN RESULTADO");
	}

	DrawText(ResultText.ToString(), TextPrimary, X + 28.f, Y + 58.f, GEngine->GetMediumFont(), 1.3f);

	DrawColumns(X, Y + HeaderHeight - 26.f, Width);
	DrawPlayerRows(Players, X, Y + HeaderHeight, Width, RowHeight);
}

void AShooterHUD::DrawLiveScoreboard()
{
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!ShooterGameState)
	{
		return;
	}

	const TArray<const AShooterPlayerState*> Players = GetSortedPlayers(ShooterGameState);

	const float Width = FMath::Min(Canvas->ClipX * 0.6f, 640.f);
	const float RowHeight = 28.f;
	const float HeaderHeight = 52.f;
	const float Height = HeaderHeight + RowHeight * FMath::Max(Players.Num(), 1) + 18.f;

	const float X = (Canvas->ClipX - Width) * 0.5f;
	const float Y = Canvas->ClipY * 0.18f;

	DrawPanel(this, X, Y, Width, Height);

	DrawText(LOCTEXT("Scoreboard", "TABLA DE POSICIONES").ToString(), TextPrimary,
		X + 24.f, Y + 14.f, GEngine->GetMediumFont(), 1.2f);

	DrawColumns(X, Y + HeaderHeight - 22.f, Width);
	DrawPlayerRows(Players, X, Y + HeaderHeight, Width, RowHeight);
}

void AShooterHUD::DrawKillFeed()
{
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!ShooterGameState)
	{
		return;
	}

	const TArray<FShooterKillEvent>& Feed = ShooterGameState->GetKillFeed();
	if (Feed.Num() == 0)
	{
		return;
	}

	const FString LocalName = (PlayerOwner && PlayerOwner->PlayerState)
		? PlayerOwner->PlayerState->GetPlayerName() : FString();

	const float Now = GetWorld()->GetTimeSeconds();
	const float RowHeight = 22.f;
	const float Right = Canvas->ClipX - 42.f;

	int32 Visible = 0;
	for (const FShooterKillEvent& Event : Feed)
	{
		const float Age = Now - Event.WorldTime;
		if (Age > KillFeedDuration)
		{
			continue;
		}

		// Las últimas décimas se desvanecen en vez de desaparecer de golpe.
		const float Alpha = FMath::Min((KillFeedDuration - Age) / 0.6f, 1.f);
		const float RowY = 96.f + Visible * RowHeight;

		// Las líneas que involucran al jugador local se destacan.
		const bool bInvolved = !LocalName.IsEmpty()
			&& (Event.KillerName == LocalName || Event.VictimName == LocalName);

		// El orden verdugo/víctima se deja al traductor: no todos los idiomas
		// leen de izquierda a derecha ni ponen el sujeto primero.
		FFormatNamedArguments LineArgs;
		LineArgs.Add(TEXT("Killer"), FText::FromString(Event.KillerName));
		LineArgs.Add(TEXT("Victim"), FText::FromString(Event.VictimName));
		LineArgs.Add(TEXT("Weapon"), FText::FromString(Event.WeaponName));

		const FString Line = Event.WeaponName.IsEmpty()
			? FText::Format(LOCTEXT("KillFeedNoWeapon", "{Killer}  ->  {Victim}"), LineArgs).ToString()
			: FText::Format(LOCTEXT("KillFeedWeapon", "{Killer}  [{Weapon}]  {Victim}"), LineArgs).ToString();

		// Ancho aproximado: la fuente pequeña ocupa unos 7 px por carácter.
		const float Width = Line.Len() * 7.4f + 24.f;
		const float X = Right - Width;

		DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.45f * Alpha),
			X, RowY - 3.f, Width, RowHeight - 3.f);

		const FLinearColor LineColor = bInvolved
			? FLinearColor(1.f, 0.85f, 0.3f, Alpha)
			: FLinearColor(0.9f, 0.92f, 0.96f, Alpha);

		DrawText(Line, LineColor, X + 12.f, RowY, GEngine->GetSmallFont(), 1.f);

		++Visible;
		if (Visible >= 5)
		{
			break;
		}
	}
}

void AShooterHUD::DrawColumns(float X, float Y, float Width)
{
	DrawText(LOCTEXT("ColPlayer", "JUGADOR").ToString(), TextMuted, X + 28.f, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(LOCTEXT("ColTeam", "EQUIPO").ToString(), TextMuted, X + Width - 250.f, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(LOCTEXT("ColKills", "BAJAS").ToString(), TextMuted, X + Width - 160.f, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(LOCTEXT("ColDeaths", "MUERTES").ToString(), TextMuted, X + Width - 92.f, Y, GEngine->GetSmallFont(), 1.f);

	DrawRect(PanelBorder, X + 20.f, Y + 18.f, Width - 40.f, 1.f);
}

void AShooterHUD::DrawPlayerRows(const TArray<const AShooterPlayerState*>& Players,
	float X, float Y, float Width, float RowHeight)
{
	if (Players.Num() == 0)
	{
		DrawText(LOCTEXT("NoPlayers", "Sin jugadores").ToString(), TextMuted, X + 28.f, Y + 6.f, GEngine->GetSmallFont(), 1.1f);
		return;
	}

	const APlayerState* LocalState = PlayerOwner ? PlayerOwner->PlayerState : nullptr;

	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const AShooterPlayerState* Player = Players[Index];
		const float RowY = Y + Index * RowHeight;
		const bool bIsLocal = (Player == LocalState);

		// La fila propia se resalta para encontrarse de un vistazo.
		if (bIsLocal)
		{
			DrawRect(FLinearColor(0.25f, 0.55f, 0.95f, 0.18f), X + 20.f, RowY - 2.f, Width - 40.f, RowHeight - 2.f);
		}

		const FLinearColor RowColor = bIsLocal ? FLinearColor(0.65f, 0.85f, 1.f, 1.f) : TextPrimary;
		const FString Name = Player->GetPlayerName().IsEmpty()
			? FallbackPlayerName(Index + 1).ToString()
			: Player->GetPlayerName();

		// Los números se formatean con FText::AsNumber porque el separador de
		// millares depende de la configuración regional.
		DrawText(FText::AsNumber(Index + 1).ToString() + TEXT("."), TextMuted,
			X + 28.f, RowY + 4.f, GEngine->GetSmallFont(), 1.1f);
		DrawText(Name, RowColor, X + 58.f, RowY + 4.f, GEngine->GetSmallFont(), 1.2f);
		DrawText(TeamName(Player->GetTeam()).ToString(), TextMuted,
			X + Width - 250.f, RowY + 4.f, GEngine->GetSmallFont(), 1.1f);
		DrawText(FText::AsNumber(Player->GetKills()).ToString(), RowColor,
			X + Width - 150.f, RowY + 4.f, GEngine->GetSmallFont(), 1.2f);
		DrawText(FText::AsNumber(Player->GetDeaths()).ToString(), TextMuted,
			X + Width - 74.f, RowY + 4.f, GEngine->GetSmallFont(), 1.2f);
	}
}

float AShooterHUD::SpreadDegreesToPixels(float SpreadDegrees) const
{
	if (!Canvas || !PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return 0.f;
	}

	const float FOVDegrees = PlayerOwner->PlayerCameraManager->GetFOVAngle();
	const float HalfFOVRadians = FMath::DegreesToRadians(FOVDegrees * 0.5f);
	const float TanHalfFOV = FMath::Tan(HalfFOVRadians);

	if (TanHalfFOV <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float ScreenDistance = (Canvas->ClipX * 0.5f) / TanHalfFOV;

	return ScreenDistance * FMath::Tan(FMath::DegreesToRadians(SpreadDegrees));
}

void AShooterHUD::DrawCrosshair(AShooterCharacter* Character)
{
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	// El jugador puede elegir el color de su mira (RF-11.8).
	FLinearColor ActiveCrosshairColor = CrosshairColor;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UShooterSettingsSubsystem* Settings =
			GameInstance->GetSubsystem<UShooterSettingsSubsystem>())
		{
			ActiveCrosshairColor = Settings->GetCrosshairColor();
		}
	}

	const AWeaponBase* Weapon = Character->GetCurrentWeapon();
	const float SpreadDegrees = Weapon ? Weapon->GetCurrentSpreadDegrees() : 0.f;

	const float Gap = FMath::Clamp(SpreadDegreesToPixels(SpreadDegrees), 4.f, Canvas->ClipY * 0.25f);
	const float LineLength = 7.f;
	const float Thickness = 2.f;

	// Contorno oscuro bajo cada trazo: la mira se ve sobre fondos claros.
	const FLinearColor Outline(0.f, 0.f, 0.f, 0.55f);
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bOutlinePass = (Pass == 0);
		const FLinearColor Color = bOutlinePass ? Outline : ActiveCrosshairColor;
		const float Extra = bOutlinePass ? 1.f : 0.f;
		const float Weight = Thickness + (bOutlinePass ? 2.f : 0.f);

		DrawLine(CenterX - Gap - LineLength - Extra, CenterY, CenterX - Gap + Extra, CenterY, Color, Weight);
		DrawLine(CenterX + Gap - Extra, CenterY, CenterX + Gap + LineLength + Extra, CenterY, Color, Weight);
		DrawLine(CenterX, CenterY - Gap - LineLength - Extra, CenterX, CenterY - Gap + Extra, Color, Weight);
		DrawLine(CenterX, CenterY + Gap - Extra, CenterX, CenterY + Gap + LineLength + Extra, Color, Weight);
	}

	// Punto central fijo: referencia estable mientras la mira se abre y cierra.
	DrawRect(ActiveCrosshairColor, CenterX - 1.f, CenterY - 1.f, 2.f, 2.f);
}

void AShooterHUD::DrawHitMarker(AShooterPlayerController* Controller)
{
	const float TimeSinceHit = Controller->GetTimeSinceHitConfirmed();
	if (TimeSinceHit > HitMarkerDuration)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	const float Progress = TimeSinceHit / HitMarkerDuration;
	const float Alpha = 1.f - Progress;
	const bool bHeadshot = Controller->WasLastHitHeadshot();

	const FLinearColor Color = bHeadshot
		? FLinearColor(1.f, 0.3f, 0.15f, Alpha)
		: FLinearColor(1.f, 1.f, 1.f, Alpha);

	// Las aspas se separan al desvanecerse: da sensación de impacto.
	const float Inner = 6.f + Progress * 4.f;
	const float Outer = 14.f + Progress * 4.f;
	const float Weight = bHeadshot ? 3.f : 2.f;

	DrawLine(CenterX - Outer, CenterY - Outer, CenterX - Inner, CenterY - Inner, Color, Weight);
	DrawLine(CenterX + Inner, CenterY - Inner, CenterX + Outer, CenterY - Outer, Color, Weight);
	DrawLine(CenterX - Outer, CenterY + Outer, CenterX - Inner, CenterY + Inner, Color, Weight);
	DrawLine(CenterX + Inner, CenterY + Inner, CenterX + Outer, CenterY + Outer, Color, Weight);

	if (bHeadshot)
	{
		DrawText(LOCTEXT("Headshot", "CABEZA").ToString(), FLinearColor(1.f, 0.45f, 0.2f, Alpha),
			CenterX - 32.f, CenterY - 46.f, GEngine->GetMediumFont(), 1.1f);
	}
}

void AShooterHUD::DrawHealth(AShooterCharacter* Character)
{
	const UHealthComponent* Health = Character->GetHealthComponent();
	if (!Health)
	{
		return;
	}

	const float BarWidth = 240.f;
	const float BarHeight = 10.f;
	const float X = 42.f;
	const float Y = Canvas->ClipY - 62.f;

	const float Percent = Health->GetHealthPercent();
	const int32 Points = FMath::CeilToInt(Health->GetHealth());

	const FLinearColor FillColor = Percent > 0.5f
		? FLinearColor(0.25f, 0.85f, 0.45f, 0.95f)
		: (Percent > 0.25f ? FLinearColor(0.98f, 0.75f, 0.2f, 0.95f) : AccentLow);

	DrawPanel(this, X - 14.f, Y - 26.f, BarWidth + 96.f, 52.f);

	DrawText(LOCTEXT("Health", "SALUD").ToString(), TextMuted, X, Y - 18.f, GEngine->GetSmallFont(), 1.f);

	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), X, Y, BarWidth, BarHeight);
	DrawRect(FillColor, X, Y, BarWidth * Percent, BarHeight);

	// Marcas cada 25 puntos: da referencia de cuánta vida queda de un vistazo.
	for (int32 Mark = 1; Mark < 4; ++Mark)
	{
		const float MarkX = X + BarWidth * (Mark * 0.25f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), MarkX, Y, 1.f, BarHeight);
	}

	DrawText(FText::AsNumber(Points).ToString(), TextPrimary,
		X + BarWidth + 16.f, Y - 10.f, GEngine->GetLargeFont(), 1.2f);
}

namespace
{
	FText FireModeName(EWeaponFireMode Mode)
	{
		switch (Mode)
		{
		case EWeaponFireMode::SemiAuto:	return LOCTEXT("FireModeSemi", "TIRO A TIRO");
		case EWeaponFireMode::Burst:	return LOCTEXT("FireModeBurst", "RÁFAGA");
		default:						return LOCTEXT("FireModeAuto", "AUTOMÁTICO");
		}
	}

	/**
	 * Lo que un soldado sabe de su cargador al sopesarlo: una estimación,
	 * no un número exacto.
	 */
	FText MagazineEstimate(int32 Current, int32 Capacity)
	{
		const float Ratio = Capacity > 0 ? static_cast<float>(Current) / Capacity : 0.f;

		if (Current <= 0)		return LOCTEXT("MagEmpty", "Vacío");
		if (Ratio >= 0.9f)		return LOCTEXT("MagFull", "Lleno");
		if (Ratio >= 0.6f)		return LOCTEXT("MagNearlyFull", "Casi lleno");
		if (Ratio >= 0.35f)		return LOCTEXT("MagHalf", "Medio");
		return LOCTEXT("MagNearlyEmpty", "Casi vacío");
	}
}

bool AShooterHUD::IsRealisticHUD() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UShooterSettingsSubsystem* Settings = GameInstance->GetSubsystem<UShooterSettingsSubsystem>())
		{
			return Settings->GetRealisticHUD();
		}
	}
	return true;
}

void AShooterHUD::DrawAmmo(AShooterCharacter* Character, bool bRealistic)
{
	const AWeaponBase* Weapon = Character->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	// En modo realista el panel solo aparece al comprobar el cargador
	// (pulsar recargar) o al recargar; el resto del tiempo, nada.
	const float ShowSeconds = 2.5f;
	if (bRealistic && !Weapon->IsReloading() && Weapon->GetTimeSinceAmmoCheck() > ShowSeconds)
	{
		return;
	}

	const float PanelWidth = 250.f;
	const float X = Canvas->ClipX - PanelWidth - 42.f;
	const float Y = Canvas->ClipY - 88.f;

	DrawPanel(this, X - 14.f, Y - 8.f, PanelWidth + 28.f, 78.f);

	// Nombre del arma y posición del selector.
	const UWeaponDataAsset* Data = Weapon->WeaponData;
	const FString WeaponName = (Data && !Data->DisplayName.IsEmpty())
		? Data->DisplayName.ToString()
		: LOCTEXT("WeaponFallback", "ARMA").ToString();
	DrawText(WeaponName.ToUpper(), TextMuted, X, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(FireModeName(Weapon->GetCurrentFireMode()).ToString(), TextMuted,
		X + PanelWidth - 96.f, Y, GEngine->GetSmallFont(), 1.f);

	if (Weapon->IsReloading())
	{
		DrawText(LOCTEXT("Reloading", "RECARGANDO").ToString(), FLinearColor(0.98f, 0.75f, 0.2f, 1.f),
			X, Y + 24.f, GEngine->GetMediumFont(), 1.1f);
		return;
	}

	const int32 Current = Weapon->GetCurrentAmmo();
	const int32 Magazine = Weapon->GetEffectiveMagazineSize();

	// Los cargadores de reserva se cuentan en ambos modos: es lo que el
	// jugador sabe palpando el chaleco.
	int32 SpareCount = 0;
	for (const int32 Rounds : Weapon->GetSpareMagazines())
	{
		SpareCount += Rounds > 0 ? 1 : 0;
	}

	FFormatNamedArguments MagArgs;
	MagArgs.Add(TEXT("Count"), FText::AsNumber(SpareCount));
	const FText SpareText = FText::Format(
		LOCTEXT("SpareMagazines", "{Count} {Count}|plural(one=cargador,other=cargadores)"), MagArgs);

	if (bRealistic)
	{
		const FLinearColor EstimateColor = Current > 0 ? TextPrimary : AccentLow;
		DrawText(MagazineEstimate(Current, Magazine).ToString(), EstimateColor,
			X, Y + 20.f, GEngine->GetMediumFont(), 1.3f);
		DrawText(SpareText.ToString(), TextMuted, X, Y + 48.f, GEngine->GetSmallFont(), 1.f);
		return;
	}

	// El cargador se tiñe cuando queda poco: avisa antes de quedarse seco.
	const float MagazinePercent = Magazine > 0 ? static_cast<float>(Current) / Magazine : 0.f;
	const FLinearColor CurrentColor = MagazinePercent > 0.25f
		? TextPrimary
		: (Current > 0 ? FLinearColor(0.98f, 0.75f, 0.2f, 1.f) : AccentLow);

	DrawText(FText::AsNumber(Current).ToString(), CurrentColor,
		X, Y + 18.f, GEngine->GetLargeFont(), 1.6f);

	const float CurrentWidth = Current >= 10 ? 46.f : 26.f;
	DrawText(FText::Format(LOCTEXT("AmmoReserve", "/ {Reserve}"),
		FFormatNamedArguments{ { TEXT("Reserve"), FText::AsNumber(Weapon->GetReserveAmmo()) } }).ToString(),
		TextMuted, X + CurrentWidth, Y + 36.f, GEngine->GetMediumFont(), 1.f);

	DrawText(SpareText.ToString(), TextMuted, X + 140.f, Y + 40.f, GEngine->GetSmallFont(), 1.f);

	// Granadas restantes (RF-5.11).
	const int32 Grenades = Character->GetGrenadeCount();
	if (Grenades > 0)
	{
		// La "G" es la inicial de granada: el traductor necesita poder cambiarla.
		DrawText(FText::Format(LOCTEXT("GrenadeCount", "G x{Count}"),
			FFormatNamedArguments{ { TEXT("Count"), FText::AsNumber(Grenades) } }).ToString(),
			TextMuted, X + 140.f, Y + 22.f, GEngine->GetSmallFont(), 1.1f);
	}
}

void AShooterHUD::DrawStatus(AShooterCharacter* Character)
{
	const UHealthComponent* Health = Character->GetHealthComponent();

	const float X = 42.f;
	float Y = Canvas->ClipY - (IsRealisticHUD() ? 40.f : 112.f);

	const float BarWidth = 160.f;
	const float BarHeight = 4.f;

	// Resistencia: solo cuando falta, para no cargar la pantalla en reposo.
	const float StaminaPercent = Character->GetStaminaPercent();
	if (StaminaPercent < 0.98f)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.10f), X, Y, BarWidth, BarHeight);
		DrawRect(FLinearColor(0.85f, 0.88f, 0.92f, 0.75f), X, Y, BarWidth * StaminaPercent, BarHeight);
		Y -= 10.f;
	}

	// Aliento: mientras se contiene, cuánto queda antes de que el pulso se dispare.
	if (Character->IsHoldingBreath())
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.10f), X, Y, BarWidth, BarHeight);
		DrawRect(FLinearColor(0.45f, 0.7f, 1.f, 0.85f), X, Y, BarWidth * Character->GetBreathPercent(), BarHeight);
		Y -= 10.f;
	}

	if (!Health)
	{
		return;
	}

	TArray<TPair<FText, FLinearColor>> Lines;

	if (Health->IsBandaging())
	{
		Lines.Emplace(LOCTEXT("Bandaging", "VENDANDO…"), FLinearColor(0.98f, 0.75f, 0.2f, 1.f));
	}
	else if (Health->GetBleedingWounds() > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Bandages"), FText::AsNumber(Health->GetBandageCount()));
		const FText Text = Health->GetBandageCount() > 0
			? FText::Format(LOCTEXT("BleedingWithBandages", "SANGRANDO · vendas: {Bandages}"), Args)
			: LOCTEXT("BleedingNoBandages", "SANGRANDO · sin vendas");

		// El aviso late para que se perciba sin leerlo.
		const float Pulse = 0.65f + 0.35f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f);
		Lines.Emplace(Text, FLinearColor(AccentLow.R, AccentLow.G, AccentLow.B, Pulse));
	}

	if (Health->HasLegInjury())
	{
		Lines.Emplace(LOCTEXT("LegInjury", "PIERNA HERIDA"), FLinearColor(0.98f, 0.75f, 0.2f, 1.f));
	}

	for (const TPair<FText, FLinearColor>& Line : Lines)
	{
		Y -= 22.f;
		DrawText(Line.Key.ToString(), Line.Value, X, Y, GEngine->GetSmallFont(), 1.2f);
	}
}

void AShooterHUD::DrawControlZones()
{
	// Sin zonas en el nivel, el modo no es Dominación y no hay nada que dibujar.
	TArray<AActor*> Zones;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AControlZone::StaticClass(), Zones);
	if (Zones.Num() == 0)
	{
		return;
	}

	// Orden estable por etiqueta: si dependiera del orden de búsqueda, las
	// zonas bailarían en pantalla entre partidas.
	Zones.Sort([](const AActor& A, const AActor& B)
	{
		return CastChecked<AControlZone>(&A)->ZoneLabel
			 < CastChecked<AControlZone>(&B)->ZoneLabel;
	});

	const float BoxSize = 46.f;
	const float Spacing = 12.f;
	const float TotalWidth = Zones.Num() * BoxSize + (Zones.Num() - 1) * Spacing;
	const float StartX = (Canvas->ClipX - TotalWidth) * 0.5f;
	const float Y = 92.f;

	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		const AControlZone* Zone = CastChecked<AControlZone>(Zones[Index]);
		const float X = StartX + Index * (BoxSize + Spacing);

		FLinearColor BoxColor;
		switch (Zone->GetOwningTeam())
		{
		case EShooterTeam::Alpha: BoxColor = FLinearColor(0.15f, 0.5f, 1.f, 0.85f); break;
		case EShooterTeam::Bravo: BoxColor = FLinearColor(1.f, 0.3f, 0.15f, 0.85f); break;
		default: BoxColor = FLinearColor(0.35f, 0.35f, 0.4f, 0.75f); break;
		}

		DrawRect(BoxColor, X, Y, BoxSize, BoxSize);

		// Una zona en disputa parpadea: es la señal de que hay combate allí.
		if (Zone->GetZoneState() == EZoneState::Contested)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f);
			DrawRect(FLinearColor(1.f, 0.9f, 0.2f, Pulse * 0.6f), X, Y, BoxSize, BoxSize);
		}

		// Barra de progreso mientras alguien captura.
		const float Progress = Zone->GetCaptureProgress();
		if (Progress > 0.f && Progress < 1.f)
		{
			const FLinearColor ProgressColor =
				(Zone->GetCapturingTeam() == EShooterTeam::Alpha)
					? FLinearColor(0.3f, 0.7f, 1.f, 1.f)
					: FLinearColor(1.f, 0.5f, 0.3f, 1.f);

			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y + BoxSize + 2.f, BoxSize, 5.f);
			DrawRect(ProgressColor, X, Y + BoxSize + 2.f, BoxSize * Progress, 5.f);
		}

		DrawText(Zone->ZoneLabel, TextPrimary, X + BoxSize * 0.5f - 6.f, Y + 14.f,
			GEngine->GetMediumFont(), 1.3f);
	}
}

void AShooterHUD::DrawMinimap(AShooterCharacter* Character)
{
	if (!bShowMinimap || !Character)
	{
		return;
	}

	const AShooterPlayerState* LocalState = Character->GetPlayerState<AShooterPlayerState>();
	if (!LocalState)
	{
		return;
	}

	const float CenterX = Canvas->ClipX - MinimapScreenRadius - 42.f;
	const float CenterY = MinimapScreenRadius + 42.f;
	const float Diameter = MinimapScreenRadius * 2.f;

	// Fondo del minimapa.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.05f, 0.55f),
		CenterX - MinimapScreenRadius, CenterY - MinimapScreenRadius, Diameter, Diameter);

	// Borde: dos líneas arriba y abajo, como el resto de paneles.
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f),
		CenterX - MinimapScreenRadius, CenterY - MinimapScreenRadius, Diameter, 1.f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f),
		CenterX - MinimapScreenRadius, CenterY + MinimapScreenRadius - 1.f, Diameter, 1.f);

	const FVector LocalLocation = Character->GetActorLocation();
	const float LocalYaw = Character->GetActorRotation().Yaw;
	const float Scale = MinimapScreenRadius / MinimapWorldRadius;

	// Proyecta una posición del mundo al minimapa, rotada para que el frente
	// del jugador quede siempre arriba.
	auto WorldToMinimap = [&](const FVector& WorldLocation, float& OutX, float& OutY) -> bool
	{
		const FVector Delta = WorldLocation - LocalLocation;

		if (Delta.SizeSquared2D() > FMath::Square(MinimapWorldRadius))
		{
			return false;
		}

		const float Radians = FMath::DegreesToRadians(-LocalYaw);
		const float RotatedX = Delta.X * FMath::Cos(Radians) - Delta.Y * FMath::Sin(Radians);
		const float RotatedY = Delta.X * FMath::Sin(Radians) + Delta.Y * FMath::Cos(Radians);

		// El eje X del mundo apunta al frente, que en pantalla es hacia arriba.
		OutX = CenterX + RotatedY * Scale;
		OutY = CenterY - RotatedX * Scale;
		return true;
	};

	// Zonas de Dominación: objetivos, no jugadores, así que mostrarlas no
	// filtra información de posición enemiga.
	TArray<AActor*> Zones;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AControlZone::StaticClass(), Zones);

	for (const AActor* ZoneActor : Zones)
	{
		const AControlZone* Zone = CastChecked<AControlZone>(ZoneActor);

		float ZoneX, ZoneY;
		if (!WorldToMinimap(Zone->GetActorLocation(), ZoneX, ZoneY))
		{
			continue;
		}

		FLinearColor ZoneColor;
		switch (Zone->GetOwningTeam())
		{
		case EShooterTeam::Alpha: ZoneColor = FLinearColor(0.15f, 0.5f, 1.f, 0.8f); break;
		case EShooterTeam::Bravo: ZoneColor = FLinearColor(1.f, 0.3f, 0.15f, 0.8f); break;
		default: ZoneColor = FLinearColor(0.5f, 0.5f, 0.55f, 0.8f); break;
		}

		DrawRect(ZoneColor, ZoneX - 6.f, ZoneY - 6.f, 12.f, 12.f);
		DrawText(Zone->ZoneLabel, TextPrimary, ZoneX - 3.f, ZoneY - 7.f,
			GEngine->GetTinyFont(), 1.f);
	}

	// Solo aliados (RF-7.9). Dibujar enemigos sería un wallhack incorporado y
	// contradiría RNF-3.6, que exige no darle al cliente su posición.
	for (TActorIterator<AShooterCharacter> It(GetWorld()); It; ++It)
	{
		const AShooterCharacter* Other = *It;
		if (Other == Character || Other->IsDead())
		{
			continue;
		}

		const AShooterPlayerState* OtherState = Other->GetPlayerState<AShooterPlayerState>();
		if (!OtherState || OtherState->GetTeam() != LocalState->GetTeam())
		{
			continue;
		}

		// En todos contra todos nadie es aliado: sin equipo no hay puntos que
		// mostrar, o el modo se volvería trivial.
		if (LocalState->GetTeam() == EShooterTeam::None)
		{
			continue;
		}

		float AllyX, AllyY;
		if (WorldToMinimap(Other->GetActorLocation(), AllyX, AllyY))
		{
			DrawRect(FLinearColor(0.3f, 0.85f, 0.4f, 0.95f), AllyX - 3.f, AllyY - 3.f, 6.f, 6.f);
		}
	}

	// El jugador local, siempre en el centro y apuntando hacia arriba.
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 1.f), CenterX - 2.f, CenterY - 2.f, 4.f, 4.f);
	DrawLine(CenterX, CenterY, CenterX, CenterY - 10.f, FLinearColor(1.f, 1.f, 1.f, 0.9f), 1.5f);
}

void AShooterHUD::DrawConnectionWarning(AShooterPlayerController* Controller)
{
	if (!Controller || !Controller->IsConnectionLost())
	{
		return;
	}

	const float PanelWidth = 420.f;
	const float X = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float Y = Canvas->ClipY * 0.24f;

	// Fondo rojo sólido: el aviso debe leerse sobre cualquier escena.
	DrawRect(FLinearColor(0.35f, 0.03f, 0.03f, 0.9f), X, Y, PanelWidth, 64.f);

	DrawText(LOCTEXT("ConnectionLost", "SE PERDIÓ LA CONEXIÓN").ToString(),
		FLinearColor(1.f, 0.92f, 0.92f, 1.f),
		X + 20.f, Y + 10.f, GEngine->GetMediumFont(), 1.3f);

	// Mensaje sin códigos técnicos, como exige RNF-4.6.
	FFormatNamedArguments Args;
	Args.Add(TEXT("Seconds"),
		FText::AsNumber(FMath::FloorToInt(Controller->GetSecondsSinceServerResponse())));

	DrawText(FText::Format(
		LOCTEXT("ConnectionLostDetail", "Sin respuesta del servidor desde hace {Seconds} s"),
		Args).ToString(),
		FLinearColor(1.f, 0.8f, 0.8f, 1.f),
		X + 20.f, Y + 38.f, GEngine->GetSmallFont(), 1.1f);
}

void AShooterHUD::DrawWaveInfo()
{
	// Sin gestor en el nivel no es una partida de Oleadas.
	AActor* ManagerActor = UGameplayStatics::GetActorOfClass(GetWorld(), AWaveManager::StaticClass());
	const AWaveManager* Manager = Cast<AWaveManager>(ManagerActor);
	if (!Manager || Manager->GetCurrentWave() <= 0)
	{
		return;
	}

	const float PanelWidth = 210.f;
	const float X = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float Y = 92.f;

	DrawPanel(this, X, Y, PanelWidth, 54.f);

	FFormatNamedArguments WaveArgs;
	WaveArgs.Add(TEXT("Wave"), FText::AsNumber(Manager->GetCurrentWave()));
	DrawText(FText::Format(LOCTEXT("WaveNumber", "OLEADA {Wave}"), WaveArgs).ToString(),
		TextPrimary, X + 18.f, Y + 8.f, GEngine->GetMediumFont(), 1.2f);

	if (Manager->IsWaveInProgress())
	{
		FFormatNamedArguments EnemyArgs;
		EnemyArgs.Add(TEXT("Count"), FText::AsNumber(Manager->GetEnemiesRemaining()));
		DrawText(FText::Format(
			LOCTEXT("EnemiesLeft", "{Count} {Count}|plural(one=enemigo,other=enemigos)"),
			EnemyArgs).ToString(),
			TextMuted, X + 18.f, Y + 32.f, GEngine->GetSmallFont(), 1.1f);
	}
	else
	{
		// Entre oleadas se muestra la cuenta atrás: es el momento de recargar.
		FFormatNamedArguments NextArgs;
		NextArgs.Add(TEXT("Seconds"),
			FText::AsNumber(FMath::CeilToInt(Manager->GetTimeUntilNextWave())));
		DrawText(FText::Format(LOCTEXT("NextWaveIn", "Siguiente en {Seconds}"), NextArgs).ToString(),
			FLinearColor(0.98f, 0.75f, 0.2f, 1.f), X + 18.f, Y + 32.f, GEngine->GetSmallFont(), 1.1f);
	}
}

void AShooterHUD::DrawMatchInfo()
{
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!ShooterGameState)
	{
		return;
	}

	const float TimeRemaining = ShooterGameState->GetTimeRemaining();
	const bool bInProgress = ShooterGameState->GetMatchPhase() == EMatchPhase::InProgress;

	// Una partida en curso sin tiempo restante es entrenamiento: mostrar 00:00
	// haría pensar que ya terminó.
	const bool bTraining = bInProgress && TimeRemaining <= 0.f;

	const float PanelWidth = bTraining ? 220.f : 130.f;
	const float PanelX = Canvas->ClipX * 0.5f - PanelWidth * 0.5f;

	DrawPanel(this, PanelX, 16.f, PanelWidth, bTraining ? 40.f : 62.f);

	if (bTraining)
	{
		DrawText(LOCTEXT("TrainingMode", "ENTRENAMIENTO").ToString(), TextPrimary,
			PanelX + 24.f, 27.f, GEngine->GetMediumFont(), 1.1f);
	}
	else
	{
		const FString TimeText = FString::Printf(TEXT("%02d:%02d"),
			FMath::FloorToInt(TimeRemaining / 60.f),
			FMath::FloorToInt(TimeRemaining) % 60);

		// El reloj se tiñe en el último minuto.
		const FLinearColor TimeColor = (bInProgress && TimeRemaining <= 60.f)
			? AccentLow : TextPrimary;

		DrawText(TimeText, TimeColor, PanelX + 32.f, 24.f, GEngine->GetLargeFont(), 1.3f);

		FFormatNamedArguments ScoreArgs;
		ScoreArgs.Add(TEXT("Alpha"), FText::AsNumber(ShooterGameState->GetTeamScore(EShooterTeam::Alpha)));
		ScoreArgs.Add(TEXT("Bravo"), FText::AsNumber(ShooterGameState->GetTeamScore(EShooterTeam::Bravo)));
		DrawText(FText::Format(LOCTEXT("TeamScoreLive", "{Alpha}  -  {Bravo}"), ScoreArgs).ToString(),
			TextMuted, PanelX + 32.f, 52.f, GEngine->GetSmallFont(), 1.1f);
	}

	if (ShooterGameState->GetMatchPhase() == EMatchPhase::Finished)
	{
		const float BannerY = Canvas->ClipY * 0.35f;
		DrawPanel(this, Canvas->ClipX * 0.5f - 190.f, BannerY - 14.f, 380.f, 56.f);
		DrawText(LOCTEXT("MatchOver", "PARTIDA TERMINADA").ToString(), FLinearColor(1.f, 0.85f, 0.25f, 1.f),
			Canvas->ClipX * 0.5f - 150.f, BannerY, GEngine->GetLargeFont(), 1.6f);
	}
}

void AShooterHUD::DrawDamageIndicator(AShooterCharacter* Character, AShooterPlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	const float TimeSinceDamage = Controller->GetTimeSinceDamageTaken();
	if (TimeSinceDamage > DamageIndicatorDuration)
	{
		return;
	}

	const FVector ToSource = Controller->GetLastDamageSourceLocation() - Character->GetActorLocation();
	if (ToSource.IsNearlyZero())
	{
		return;
	}

	const float CameraYaw = Character->GetControlRotation().Yaw;
	const float SourceYaw = ToSource.Rotation().Yaw;
	const float RelativeYaw = FMath::DegreesToRadians(FRotator::NormalizeAxis(SourceYaw - CameraYaw));

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const float Radius = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.22f;

	const float Alpha = 1.f - (TimeSinceDamage / DamageIndicatorDuration);
	const FLinearColor Color(0.95f, 0.15f, 0.1f, Alpha);

	const float MarkerX = CenterX + FMath::Sin(RelativeYaw) * Radius;
	const float MarkerY = CenterY - FMath::Cos(RelativeYaw) * Radius;

	const float Half = 20.f;
	DrawLine(MarkerX - Half, MarkerY + 9.f, MarkerX, MarkerY - 9.f, Color, 4.f);
	DrawLine(MarkerX, MarkerY - 9.f, MarkerX + Half, MarkerY + 9.f, Color, 4.f);

	// Destello rojo en los bordes: se percibe sin mirar el indicador.
	const float EdgeAlpha = Alpha * 0.28f;
	const FLinearColor Edge(0.85f, 0.05f, 0.05f, EdgeAlpha);
	const float EdgeSize = 90.f;
	DrawRect(Edge, 0.f, 0.f, EdgeSize, Canvas->ClipY);
	DrawRect(Edge, Canvas->ClipX - EdgeSize, 0.f, EdgeSize, Canvas->ClipY);
}

void AShooterHUD::DrawRespawnCountdown(AShooterPlayerController* Controller)
{
	const float Countdown = Controller->GetRespawnCountdown();
	if (Countdown <= 0.f)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float PanelY = Canvas->ClipY * 0.42f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawPanel(this, CenterX - 180.f, PanelY - 16.f, 360.f, 104.f);

	DrawText(LOCTEXT("Eliminated", "ELIMINADO").ToString(), AccentLow, CenterX - 62.f, PanelY, GEngine->GetLargeFont(), 1.5f);

	// Quién te eliminó y con qué arma (RF-6.6).
	const FString KillerName = Controller->GetLastKillerName();
	if (!KillerName.IsEmpty())
	{
		const FString Weapon = Controller->GetLastKillerWeapon();

		FFormatNamedArguments Args;
		Args.Add(TEXT("Killer"), FText::FromString(KillerName));
		Args.Add(TEXT("Weapon"), FText::FromString(Weapon));

		const FText Text = Weapon.IsEmpty()
			? FText::Format(LOCTEXT("KilledBy", "Te eliminó {Killer}"), Args)
			: FText::Format(LOCTEXT("KilledByWeapon", "Te eliminó {Killer} con {Weapon}"), Args);

		DrawText(Text.ToString(), TextPrimary, CenterX - 120.f, PanelY + 34.f, GEngine->GetSmallFont(), 1.2f);
	}

	FFormatNamedArguments CountdownArgs;
	CountdownArgs.Add(TEXT("Seconds"), FText::AsNumber(FMath::CeilToInt(Countdown)));
	DrawText(FText::Format(LOCTEXT("RespawnIn", "Reapareces en {Seconds}"), CountdownArgs).ToString(),
		TextMuted, CenterX - 78.f, PanelY + 56.f, GEngine->GetMediumFont(), 1.2f);
}

#undef LOCTEXT_NAMESPACE
