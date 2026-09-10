#include "UI/ShooterHUD.h"
#include "Player/ShooterCharacter.h"
#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameState.h"
#include "Combat/HealthComponent.h"
#include "Weapons/WeaponBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

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
	const float LineLength = 8.f;
	const float Thickness = 2.f;

	DrawLine(CenterX - Gap - LineLength, CenterY, CenterX - Gap, CenterY, CrosshairColor, Thickness);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + LineLength, CenterY, CrosshairColor, Thickness);
	DrawLine(CenterX, CenterY - Gap - LineLength, CenterX, CenterY - Gap, CrosshairColor, Thickness);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + LineLength, CrosshairColor, Thickness);
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

	const float Alpha = 1.f - (TimeSinceHit / HitMarkerDuration);
	const FLinearColor Color = Controller->WasLastHitHeadshot()
		? FLinearColor(1.f, 0.25f, 0.15f, Alpha)
		: FLinearColor(1.f, 1.f, 1.f, Alpha);

	const float Inner = 6.f;
	const float Outer = 14.f;

	DrawLine(CenterX - Outer, CenterY - Outer, CenterX - Inner, CenterY - Inner, Color, 2.f);
	DrawLine(CenterX + Inner, CenterY - Inner, CenterX + Outer, CenterY - Outer, Color, 2.f);
	DrawLine(CenterX - Outer, CenterY + Outer, CenterX - Inner, CenterY + Inner, Color, 2.f);
	DrawLine(CenterX + Inner, CenterY + Inner, CenterX + Outer, CenterY + Outer, Color, 2.f);
}

void AShooterHUD::DrawHealth(AShooterCharacter* Character)
{
	const UHealthComponent* Health = Character->GetHealthComponent();
	if (!Health)
	{
		return;
	}

	const float BarWidth = 260.f;
	const float BarHeight = 16.f;
	const float X = 40.f;
	const float Y = Canvas->ClipY - 70.f;

	const float Percent = Health->GetHealthPercent();

	const FLinearColor FillColor = Percent > 0.5f
		? FLinearColor(0.2f, 0.85f, 0.3f, 0.9f)
		: (Percent > 0.25f ? FLinearColor(0.95f, 0.75f, 0.15f, 0.9f) : FLinearColor(0.9f, 0.2f, 0.15f, 0.9f));

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 2.f, Y - 2.f, BarWidth + 4.f, BarHeight + 4.f);
	DrawRect(FillColor, X, Y, BarWidth * Percent, BarHeight);

	const FString HealthText = FString::Printf(TEXT("%d"), FMath::CeilToInt(Health->GetHealth()));
	DrawText(HealthText, FLinearColor::White, X + BarWidth + 14.f, Y - 4.f, GEngine->GetMediumFont(), 1.2f);
}

void AShooterHUD::DrawAmmo(AShooterCharacter* Character)
{
	const AWeaponBase* Weapon = Character->GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	const FString AmmoText = Weapon->IsReloading()
		? FString(TEXT("RECARGANDO"))
		: FString::Printf(TEXT("%d / %d"), Weapon->GetCurrentAmmo(), Weapon->GetReserveAmmo());

	DrawText(AmmoText, FLinearColor::White, Canvas->ClipX - 220.f, Canvas->ClipY - 70.f, GEngine->GetMediumFont(), 1.6f);
}

void AShooterHUD::DrawMatchInfo()
{
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	if (!ShooterGameState)
	{
		return;
	}

	const float TimeRemaining = ShooterGameState->GetTimeRemaining();
	const int32 Minutes = FMath::FloorToInt(TimeRemaining / 60.f);
	const int32 Seconds = FMath::FloorToInt(TimeRemaining) % 60;

	const FString TimeText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
	DrawText(TimeText, FLinearColor::White, Canvas->ClipX * 0.5f - 30.f, 24.f, GEngine->GetMediumFont(), 1.6f);

	const FString ScoreText = FString::Printf(TEXT("%d  -  %d"),
		ShooterGameState->GetTeamScore(EShooterTeam::Alpha),
		ShooterGameState->GetTeamScore(EShooterTeam::Bravo));
	DrawText(ScoreText, FLinearColor(0.85f, 0.85f, 0.9f, 1.f), Canvas->ClipX * 0.5f - 40.f, 60.f, GEngine->GetMediumFont(), 1.3f);

	if (ShooterGameState->GetMatchPhase() == EMatchPhase::Finished)
	{
		DrawText(TEXT("PARTIDA TERMINADA"), FLinearColor::Yellow,
			Canvas->ClipX * 0.5f - 120.f, Canvas->ClipY * 0.35f, GEngine->GetLargeFont(), 1.6f);
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

	const float Half = 18.f;
	DrawLine(MarkerX - Half, MarkerY + 8.f, MarkerX, MarkerY - 8.f, Color, 3.f);
	DrawLine(MarkerX, MarkerY - 8.f, MarkerX + Half, MarkerY + 8.f, Color, 3.f);
}

void AShooterHUD::DrawRespawnCountdown(AShooterPlayerController* Controller)
{
	const float Countdown = Controller->GetRespawnCountdown();
	if (Countdown <= 0.f)
	{
		return;
	}

	const FString Text = FString::Printf(TEXT("Reapareciendo en %d"), FMath::CeilToInt(Countdown));
	DrawText(Text, FLinearColor::White, Canvas->ClipX * 0.5f - 110.f, Canvas->ClipY * 0.45f, GEngine->GetLargeFont(), 1.4f);
}
