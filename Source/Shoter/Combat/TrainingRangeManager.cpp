#include "Combat/TrainingRangeManager.h"
#include "Combat/TrainingDrone.h"
#include "Combat/HealthComponent.h"
#include "Player/ShooterCharacter.h"
#include "Weapons/WeaponBase.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

ATrainingRangeManager::ATrainingRangeManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ATrainingRangeManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Recoger los drones del nivel: no hace falta asignarlos a mano.
	for (TActorIterator<ATrainingDrone> It(GetWorld()); It; ++It)
	{
		ATrainingDrone* Drone = *It;
		if (!Drone)
		{
			continue;
		}

		Drones.Add(Drone);
		Drone->OnDroneDestroyed.AddDynamic(this, &ATrainingRangeManager::HandleDroneDestroyed);
	}

	UE_LOG(LogTemp, Log, TEXT("[Entrenamiento] %d drones registrados"), Drones.Num());

	// La invulnerabilidad se aplica en cuanto exista el personaje: al arrancar
	// el nivel el jugador puede no estar poseído todavía.
	if (bPlayerInvulnerable)
	{
		FTimerHandle DelayHandle;
		GetWorldTimerManager().SetTimer(DelayHandle, [this]()
		{
			SetPlayerInvulnerable(true);
		}, 0.5f, false);
	}
}

AShooterCharacter* ATrainingRangeManager::GetLocalCharacter() const
{
	const APlayerController* Controller = GEngine
		? GEngine->GetFirstLocalPlayerController(GetWorld())
		: nullptr;

	return Controller ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
}

void ATrainingRangeManager::HandleDroneDestroyed(ATrainingDrone* Drone)
{
	if (!HasAuthority() || AutoRespawnDelay <= 0.f || !Drone)
	{
		return;
	}

	// Reposición individual: la galería no se queda vacía mientras se entrena.
	FTimerHandle RespawnHandle;
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &ATrainingRangeManager::RespawnDrone, Drone);

	GetWorldTimerManager().SetTimer(RespawnHandle, RespawnDelegate, AutoRespawnDelay, false);
}

void ATrainingRangeManager::RespawnDrone(ATrainingDrone* Drone)
{
	if (Drone)
	{
		Drone->ResetDrone();
	}
}

void ATrainingRangeManager::ResetDrones()
{
	if (!HasAuthority())
	{
		return;
	}

	int32 Restored = 0;
	for (ATrainingDrone* Drone : Drones)
	{
		if (Drone)
		{
			Drone->ResetDrone();
			++Restored;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Entrenamiento] %d drones repuestos"), Restored);
}

void ATrainingRangeManager::ResetPlayer()
{
	AShooterCharacter* Character = GetLocalCharacter();
	if (!Character)
	{
		return;
	}

	if (UHealthComponent* Health = Character->GetHealthComponent())
	{
		Health->ResetHealth();
	}

	if (AWeaponBase* Weapon = Character->GetCurrentWeapon())
	{
		Weapon->ResetAmmo();
	}
}

void ATrainingRangeManager::ResetRange()
{
	ResetDrones();
	ResetPlayer();
}

void ATrainingRangeManager::SetPlayerInvulnerable(bool bInvulnerable)
{
	bPlayerInvulnerable = bInvulnerable;

	if (AShooterCharacter* Character = GetLocalCharacter())
	{
		if (UHealthComponent* Health = Character->GetHealthComponent())
		{
			Health->SetInvulnerable(bInvulnerable);
		}
	}
}

int32 ATrainingRangeManager::GetActiveDroneCount() const
{
	int32 Active = 0;
	for (const ATrainingDrone* Drone : Drones)
	{
		if (Drone && !Drone->IsDestroyed())
		{
			++Active;
		}
	}

	return Active;
}
