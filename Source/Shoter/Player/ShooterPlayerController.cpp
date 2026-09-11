#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameMode.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"

AShooterPlayerController::AShooterPlayerController()
{
	// Explícito: la vigilancia del enlace (RNF-2.6) depende del Tick, y no
	// conviene que quede a merced del valor por defecto de la clase base.
	PrimaryActorTick.bCanEverTick = true;
}

void AShooterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShooterPlayerController, RespawnWorldTime, COND_OwnerOnly);
}

void AShooterPlayerController::Client_NotifyHitConfirmed_Implementation(bool bHeadshot)
{
	LastHitConfirmedTime = GetWorld()->GetTimeSeconds();
	bLastHitWasHeadshot = bHeadshot;

	// Precisión acumulada (RF-9.6). El disparo se cuenta al efectuarlo, en
	// RegisterShotFired; aquí solo se marca el acierto.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterProgressionSubsystem* Progression =
			GameInstance->GetSubsystem<UShooterProgressionSubsystem>())
		{
			Progression->RecordShot(true);
		}
	}
}

void AShooterPlayerController::RegisterShotFired()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterProgressionSubsystem* Progression =
			GameInstance->GetSubsystem<UShooterProgressionSubsystem>())
		{
			// Un disparo sin acierto; el impacto lo confirma el servidor aparte.
			Progression->RecordShot(false);
		}
	}
}

void AShooterPlayerController::Client_NotifyDamageTaken_Implementation(const FVector_NetQuantize& DamageSourceLocation)
{
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();
	LastDamageSourceLocation = DamageSourceLocation;
}

void AShooterPlayerController::Client_NotifyKilledBy_Implementation(
	const FString& KillerName, const FString& WeaponName)
{
	LastKillerName = KillerName;
	LastKillerWeapon = WeaponName;
}

float AShooterPlayerController::GetTimeSinceHitConfirmed() const
{
	return GetWorld()->GetTimeSeconds() - LastHitConfirmedTime;
}

float AShooterPlayerController::GetTimeSinceDamageTaken() const
{
	return GetWorld()->GetTimeSeconds() - LastDamageTakenTime;
}

float AShooterPlayerController::GetRespawnCountdown() const
{
	return FMath::Max(RespawnWorldTime - GetWorld()->GetTimeSeconds(), 0.f);
}

void AShooterPlayerController::OnCharacterDied(AController* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AShooterGameMode* GameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>())
	{
		GameMode->OnPlayerKilled(this, Killer);
	}
}

void AShooterPlayerController::Client_StartKillCam_Implementation(AActor* KillerActor)
{
	// Sin asesino no hay a quién enfocar: una caída o un suicidio dejan la
	// cámara donde estaba.
	if (!bKillCamEnabled || !KillerActor || KillerActor == GetPawn())
	{
		return;
	}

	KillCamTarget = KillerActor;
	bKillCamActive = true;
}

void AShooterPlayerController::UpdateKillCam(float DeltaSeconds)
{
	// El objetivo puede morir o desconectar durante la killcam.
	if (!KillCamTarget)
	{
		StopKillCam();
		return;
	}

	const FVector TargetLocation = KillCamTarget->GetActorLocation();
	const FRotator TargetRotation = KillCamTarget->GetActorRotation();

	// Cámara detrás y por encima del asesino, mirándolo: muestra quién fue y
	// hacia dónde apuntaba.
	const FVector Offset = TargetRotation.RotateVector(
		FVector(-KillCamDistance, 0.f, 120.f));
	const FVector CameraLocation = TargetLocation + Offset;

	const FRotator LookAt =
		(TargetLocation - CameraLocation).GetSafeNormal().Rotation();

	if (APlayerCameraManager* Camera = PlayerCameraManager)
	{
		// Interpolar evita el salto brusco al morir, que desorienta.
		const FVector SmoothLocation = FMath::VInterpTo(
			Camera->GetCameraLocation(), CameraLocation, DeltaSeconds, 6.f);

		SetControlRotation(FMath::RInterpTo(
			GetControlRotation(), LookAt, DeltaSeconds, 6.f));

		Camera->SetActorLocation(SmoothLocation);
	}
}

void AShooterPlayerController::StopKillCam()
{
	bKillCamActive = false;
	KillCamTarget = nullptr;
}

void AShooterPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// La killcam corre en el cliente que murió, mientras espera reaparecer.
	if (bKillCamActive)
	{
		if (GetRespawnCountdown() <= 0.f)
		{
			StopKillCam();
		}
		else
		{
			UpdateKillCam(DeltaSeconds);
		}
	}

	// Solo el cliente detecta la caída: en el servidor no hay enlace que vigilar.
	if (HasAuthority() || !IsLocalController())
	{
		return;
	}

	const UNetConnection* Connection = GetNetConnection();
	if (!Connection)
	{
		return;
	}

	// LastReceiveTime avanza con cualquier paquete entrante, así que sirve de
	// prueba de vida sin enviar tráfico adicional (RNF-2.3).
	LastServerResponseTime = Connection->LastReceiveTime;
}

float AShooterPlayerController::GetSecondsSinceServerResponse() const
{
	const UNetConnection* Connection = GetNetConnection();
	if (!Connection)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	if (!World || !World->GetNetDriver())
	{
		return 0.f;
	}

	// El tiempo del driver de red es el mismo reloj en que se anota
	// LastReceiveTime; usar el del mundo daría una diferencia sin sentido.
	return FMath::Max(
		World->GetNetDriver()->GetElapsedTime() - Connection->LastReceiveTime, 0.f);
}

bool AShooterPlayerController::IsConnectionLost() const
{
	// Una partida local no tiene conexión que perder.
	if (HasAuthority() || !GetNetConnection())
	{
		return false;
	}

	return GetSecondsSinceServerResponse() > DisconnectWarningSeconds;
}
