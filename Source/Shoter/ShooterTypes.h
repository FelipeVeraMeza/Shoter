#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.generated.h"

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	Automatic	UMETA(DisplayName = "Automático"),
	SemiAuto	UMETA(DisplayName = "Semiautomático"),
	Burst		UMETA(DisplayName = "Ráfaga")
};

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	WaitingToStart	UMETA(DisplayName = "Esperando jugadores"),
	InProgress		UMETA(DisplayName = "En curso"),
	Finished		UMETA(DisplayName = "Terminada")
};

UENUM(BlueprintType)
enum class EShooterTeam : uint8
{
	None	UMETA(DisplayName = "Sin equipo"),
	Alpha	UMETA(DisplayName = "Alfa"),
	Bravo	UMETA(DisplayName = "Bravo")
};

/** Una eliminación, para el registro en pantalla (RF-7.7) y el aviso de muerte (RF-6.6). */
USTRUCT(BlueprintType)
struct FShooterKillEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString KillerName;

	UPROPERTY(BlueprintReadOnly)
	FString VictimName;

	UPROPERTY(BlueprintReadOnly)
	FString WeaponName;

	UPROPERTY(BlueprintReadOnly)
	bool bHeadshot = false;

	/** Momento del mundo en que ocurrió: el HUD lo usa para desvanecer la línea. */
	UPROPERTY(BlueprintReadOnly)
	float WorldTime = 0.f;
};

/** Estado de un personaje en un instante pasado, para la compensación de latencia. */
USTRUCT()
struct FShooterFramePackage
{
	GENERATED_BODY()

	UPROPERTY()
	float Time = 0.f;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	float CapsuleHalfHeight = 96.f;

	UPROPERTY()
	float CapsuleRadius = 42.f;
};
