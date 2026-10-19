#include "Player/ShooterPlayerController.h"
#include "Core/ShooterGameMode.h"
#include "Core/ShooterGameState.h"
#include "Core/ShooterPlayerState.h"
#include "Weapons/WeaponBase.h"
#include "UI/ShooterMenuSubsystem.h"
#include "InputCoreTypes.h"
#include "Progression/ShooterProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

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

void AShooterPlayerController::Client_NotifySuppressed_Implementation(const FVector_NetQuantize& BulletLocation,
	float Intensity)
{
	// Se acumula con techo: una ráfaga cercana pesa más que una bala suelta,
	// pero nunca deja la pantalla inservible.
	SuppressionLevel = FMath::Min(SuppressionLevel + FMath::Clamp(Intensity, 0.f, 1.f) * 0.5f, 1.f);

	if (NearMissSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, NearMissSound, BulletLocation);
	}
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

void AShooterPlayerController::Server_SelectPrimaryWeapon_Implementation(TSubclassOf<AWeaponBase> WeaponClass)
{
	const AShooterGameState* ShooterGameState = GetWorld()->GetGameState<AShooterGameState>();
	AShooterPlayerState* ShooterState = GetPlayerState<AShooterPlayerState>();
	if (!ShooterGameState || !ShooterState)
	{
		return;
	}

	// Un cliente modificado podría pedir cualquier clase: solo valen las que
	// ofrece el modo.
	if (!WeaponClass || !ShooterGameState->GetPrimaryWeaponOptions().Contains(WeaponClass))
	{
		return;
	}

	ShooterState->SetPreferredPrimary(WeaponClass);
}

void AShooterPlayerController::RequestPrimaryWeapon(TSubclassOf<AWeaponBase> WeaponClass)
{
	LastLoadoutChangeTime = GetWorld()->GetTimeSeconds();
	Server_SelectPrimaryWeapon(WeaponClass);
}

float AShooterPlayerController::GetTimeSinceLoadoutChange() const
{
	return GetWorld()->GetTimeSeconds() - LastLoadoutChangeTime;
}

float AShooterPlayerController::GetTimeSinceSpawn() const
{
	return GetWorld()->GetTimeSeconds() - LastSpawnTime;
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	LastSpawnTime = GetWorld()->GetTimeSeconds();
}

void AShooterPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	LastSpawnTime = GetWorld()->GetTimeSeconds();
}

void AShooterPlayerController::PollMenuKeys()
{
	// Se leen directamente: son teclas de interfaz, no de juego, y no deben
	// depender de que el contexto de combate esté activo (en la muerte, por
	// ejemplo, no hay personaje que lo registre).
	if (WasInputKeyJustPressed(EKeys::F1))
	{
		bShowControlsHelp = !bShowControlsHelp;
	}

	if (WasInputKeyJustPressed(EKeys::L))
	{
		if (UShooterMenuSubsystem* Menu = GetGameInstance() ? GetGameInstance()->GetSubsystem<UShooterMenuSubsystem>() : nullptr)
		{
			if (Menu->GetCurrentScreen() == EShooterMenuScreen::Loadout)
			{
				Menu->CloseMenu();
			}
			else
			{
				Menu->OpenScreen(EShooterMenuScreen::Loadout);
			}
		}
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

	if (SuppressionLevel > 0.f)
	{
		SuppressionLevel = FMath::Max(SuppressionLevel - SuppressionDecayRate * DeltaSeconds, 0.f);
	}

	if (IsLocalController())
	{
		PollMenuKeys();
	}

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
