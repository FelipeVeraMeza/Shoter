#include "UI/ShooterHUD.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Weapons/WeaponBase.h"
#include "Weapons/WeaponDataAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

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

	if (Character && !Character->IsDead())
	{
		DrawCrosshair(Character);
		DrawHealth(Character);
		DrawAmmo(Character);
		DrawDamageIndicator(Character, ShooterPC);
	}

	if (ShooterPC)
	{
		DrawHitMarker(ShooterPC);
		DrawRespawnCountdown(ShooterPC);
	}

	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	const bool bMatchOver = ShooterGameState
		&& ShooterGameState->GetMatchPhase() == EMatchPhase::Finished;

	if (bMatchOver)
	{
		DrawScoreboard();
	}
	else if (ShooterPC && ShooterPC->IsInputKeyDown(EKeys::Tab))
	{
		DrawLiveScoreboard();
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

	FString TeamName(EShooterTeam Team)
	{
		switch (Team)
		{
		case EShooterTeam::Alpha: return TEXT("ALFA");
		case EShooterTeam::Bravo: return TEXT("BRAVO");
		default: return TEXT("-");
		}
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

	DrawText(TEXT("PARTIDA TERMINADA"), FLinearColor(1.f, 0.85f, 0.25f, 1.f),
		X + 28.f, Y + 22.f, GEngine->GetLargeFont(), 1.7f);

	// En modo por equipos gana el equipo con más puntos; en todos contra todos,
	// el jugador que encabeza la tabla.
	const bool bTeamMode = ShooterGameState->GetTeamScore(EShooterTeam::Alpha) > 0
		|| ShooterGameState->GetTeamScore(EShooterTeam::Bravo) > 0;

	FString ResultText;
	if (bTeamMode)
	{
		const EShooterTeam Winner = ShooterGameState->GetWinningTeam();
		ResultText = (Winner == EShooterTeam::None)
			? FString(TEXT("EMPATE"))
			: FString::Printf(TEXT("GANA EQUIPO %s"), *TeamName(Winner));

		DrawText(FString::Printf(TEXT("%d  -  %d"),
			ShooterGameState->GetTeamScore(EShooterTeam::Alpha),
			ShooterGameState->GetTeamScore(EShooterTeam::Bravo)),
			TextMuted, X + Width - 130.f, Y + 58.f, GEngine->GetMediumFont(), 1.3f);
	}
	else if (Players.Num() > 0)
	{
		const FString Name = Players[0]->GetPlayerName().IsEmpty()
			? FString(TEXT("Jugador 1"))
			: Players[0]->GetPlayerName();
		ResultText = FString::Printf(TEXT("GANA %s"), *Name);

		DrawText(FString::Printf(TEXT("%d bajas"), Players[0]->GetKills()),
			TextMuted, X + Width - 130.f, Y + 58.f, GEngine->GetMediumFont(), 1.3f);
	}
	else
	{
		ResultText = TEXT("SIN RESULTADO");
	}

	DrawText(ResultText, TextPrimary, X + 28.f, Y + 58.f, GEngine->GetMediumFont(), 1.3f);

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

	DrawText(TEXT("TABLA DE POSICIONES"), TextPrimary,
		X + 24.f, Y + 14.f, GEngine->GetMediumFont(), 1.2f);

	DrawColumns(X, Y + HeaderHeight - 22.f, Width);
	DrawPlayerRows(Players, X, Y + HeaderHeight, Width, RowHeight);
}

void AShooterHUD::DrawColumns(float X, float Y, float Width)
{
	DrawText(TEXT("JUGADOR"), TextMuted, X + 28.f, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(TEXT("EQUIPO"), TextMuted, X + Width - 250.f, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(TEXT("BAJAS"), TextMuted, X + Width - 160.f, Y, GEngine->GetSmallFont(), 1.f);
	DrawText(TEXT("MUERTES"), TextMuted, X + Width - 92.f, Y, GEngine->GetSmallFont(), 1.f);

	DrawRect(PanelBorder, X + 20.f, Y + 18.f, Width - 40.f, 1.f);
}

void AShooterHUD::DrawPlayerRows(const TArray<const AShooterPlayerState*>& Players,
	float X, float Y, float Width, float RowHeight)
{
	if (Players.Num() == 0)
	{
		DrawText(TEXT("Sin jugadores"), TextMuted, X + 28.f, Y + 6.f, GEngine->GetSmallFont(), 1.1f);
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
			? FString::Printf(TEXT("Jugador %d"), Index + 1)
			: Player->GetPlayerName();

		DrawText(FString::Printf(TEXT("%d."), Index + 1), TextMuted,
			X + 28.f, RowY + 4.f, GEngine->GetSmallFont(), 1.1f);
		DrawText(Name, RowColor, X + 58.f, RowY + 4.f, GEngine->GetSmallFont(), 1.2f);
		DrawText(TeamName(Player->GetTeam()), TextMuted,
			X + Width - 250.f, RowY + 4.f, GEngine->GetSmallFont(), 1.1f);
		DrawText(FString::Printf(TEXT("%d"), Player->GetKills()), RowColor,
			X + Width - 150.f, RowY + 4.f, GEngine->GetSmallFont(), 1.2f);
		DrawText(FString::Printf(TEXT("%d"), Player->GetDeaths()), TextMuted,
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
		const FLinearColor Color = bOutlinePass ? Outline : CrosshairColor;
		const float Extra = bOutlinePass ? 1.f : 0.f;
		const float Weight = Thickness + (bOutlinePass ? 2.f : 0.f);

		DrawLine(CenterX - Gap - LineLength - Extra, CenterY, CenterX - Gap + Extra, CenterY, Color, Weight);
		DrawLine(CenterX + Gap - Extra, CenterY, CenterX + Gap + LineLength + Extra, CenterY, Color, Weight);
		DrawLine(CenterX, CenterY - Gap - LineLength - Extra, CenterX, CenterY - Gap + Extra, Color, Weight);
		DrawLine(CenterX, CenterY + Gap - Extra, CenterX, CenterY + Gap + LineLength + Extra, Color, Weight);
	}

	// Punto central fijo: referencia estable mientras la mira se abre y cierra.
	DrawRect(CrosshairColor, CenterX - 1.f, CenterY - 1.f, 2.f, 2.f);
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
		DrawText(TEXT("CABEZA"), FLinearColor(1.f, 0.45f, 0.2f, Alpha),
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

	DrawText(TEXT("SALUD"), TextMuted, X, Y - 18.f, GEngine->GetSmallFont(), 1.f);

	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), X, Y, BarWidth, BarHeight);
	DrawRect(FillColor, X, Y, BarWidth * Percent, BarHeight);

	// Marcas cada 25 puntos: da referencia de cuánta vida queda de un vistazo.
	for (int32 Mark = 1; Mark < 4; ++Mark)
	{
		const float MarkX = X + BarWidth * (Mark * 0.25f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), MarkX, Y, 1.f, BarHeight);
	}

	DrawText(FString::Printf(TEXT("%d"), Points), TextPrimary,
		X + BarWidth + 16.f, Y - 8.f, GEngine->GetMediumFont(), 1.5f);
}

void AShooterHUD::DrawAmmo(AShooterCharacter* Character)
{
	const AWeaponBase* Weapon = Character->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	const float PanelWidth = 250.f;
	const float X = Canvas->ClipX - PanelWidth - 42.f;
	const float Y = Canvas->ClipY - 88.f;

	DrawPanel(this, X - 14.f, Y - 8.f, PanelWidth + 28.f, 78.f);

	// Nombre del arma: útil al alternar entre primaria y secundaria.
	const UWeaponDataAsset* Data = Weapon->WeaponData;
	const FString WeaponName = (Data && !Data->DisplayName.IsEmpty())
		? Data->DisplayName.ToString()
		: FString(TEXT("ARMA"));
	DrawText(WeaponName.ToUpper(), TextMuted, X, Y, GEngine->GetSmallFont(), 1.f);

	if (Weapon->IsReloading())
	{
		DrawText(TEXT("RECARGANDO"), FLinearColor(0.98f, 0.75f, 0.2f, 1.f),
			X, Y + 22.f, GEngine->GetMediumFont(), 1.5f);
		return;
	}

	const int32 Current = Weapon->GetCurrentAmmo();
	const int32 Reserve = Weapon->GetReserveAmmo();
	const int32 Magazine = Data ? Data->MagazineSize : 30;

	// El cargador se tiñe cuando queda poco: avisa antes de quedarse seco.
	const float MagazinePercent = Magazine > 0 ? static_cast<float>(Current) / Magazine : 0.f;
	const FLinearColor CurrentColor = MagazinePercent > 0.25f
		? TextPrimary
		: (Current > 0 ? FLinearColor(0.98f, 0.75f, 0.2f, 1.f) : AccentLow);

	DrawText(FString::Printf(TEXT("%d"), Current), CurrentColor,
		X, Y + 20.f, GEngine->GetMediumFont(), 2.2f);

	const float CurrentWidth = Current >= 10 ? 46.f : 26.f;
	DrawText(FString::Printf(TEXT("/ %d"), Reserve), TextMuted,
		X + CurrentWidth, Y + 34.f, GEngine->GetMediumFont(), 1.2f);

	if (Current <= 0)
	{
		DrawText(TEXT("PULSA R"), AccentLow, X + 140.f, Y + 34.f, GEngine->GetSmallFont(), 1.1f);
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
		DrawText(TEXT("ENTRENAMIENTO"), TextPrimary,
			PanelX + 26.f, 26.f, GEngine->GetMediumFont(), 1.4f);
	}
	else
	{
		const FString TimeText = FString::Printf(TEXT("%02d:%02d"),
			FMath::FloorToInt(TimeRemaining / 60.f),
			FMath::FloorToInt(TimeRemaining) % 60);

		// El reloj se tiñe en el último minuto.
		const FLinearColor TimeColor = (bInProgress && TimeRemaining <= 60.f)
			? AccentLow : TextPrimary;

		DrawText(TimeText, TimeColor, PanelX + 30.f, 24.f, GEngine->GetMediumFont(), 1.7f);

		const FString ScoreText = FString::Printf(TEXT("%d  -  %d"),
			ShooterGameState->GetTeamScore(EShooterTeam::Alpha),
			ShooterGameState->GetTeamScore(EShooterTeam::Bravo));
		DrawText(ScoreText, TextMuted, PanelX + 32.f, 52.f, GEngine->GetSmallFont(), 1.1f);
	}

	if (ShooterGameState->GetMatchPhase() == EMatchPhase::Finished)
	{
		const float BannerY = Canvas->ClipY * 0.35f;
		DrawPanel(this, Canvas->ClipX * 0.5f - 190.f, BannerY - 14.f, 380.f, 56.f);
		DrawText(TEXT("PARTIDA TERMINADA"), FLinearColor(1.f, 0.85f, 0.25f, 1.f),
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
	DrawPanel(this, CenterX - 170.f, PanelY - 16.f, 340.f, 80.f);

	DrawText(TEXT("ELIMINADO"), AccentLow, CenterX - 62.f, PanelY, GEngine->GetLargeFont(), 1.5f);
	DrawText(FString::Printf(TEXT("Reapareces en %d"), FMath::CeilToInt(Countdown)), TextMuted,
		CenterX - 78.f, PanelY + 36.f, GEngine->GetMediumFont(), 1.2f);
}
