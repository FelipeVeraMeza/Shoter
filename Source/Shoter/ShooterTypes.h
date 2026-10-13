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

/** Modo de juego activo. Determina la condición de victoria (RF-8.1 a RF-8.4). */
UENUM(BlueprintType)
enum class EShooterGameModeType : uint8
{
	FreeForAll	UMETA(DisplayName = "Todos contra todos"),
	TeamDeathmatch	UMETA(DisplayName = "Equipos"),
	Domination	UMETA(DisplayName = "Dominación"),
	Waves		UMETA(DisplayName = "Oleadas")
};

/** Estado de captura de una zona de Dominación (RF-8.3). */
UENUM(BlueprintType)
enum class EZoneState : uint8
{
	Neutral		UMETA(DisplayName = "Neutral"),
	Capturing	UMETA(DisplayName = "Capturándose"),
	Captured	UMETA(DisplayName = "Capturada"),
	Contested	UMETA(DisplayName = "Disputada")
};

/**
 * Región del cuerpo alcanzada por un disparo.
 *
 * El daño realista depende de dónde entra la bala: la cabeza es letal, un
 * brazo no. Se deduce de la cápsula rebobinada, no de la malla, porque el
 * servidor no anima los esqueletos de los demás jugadores.
 */
UENUM(BlueprintType)
enum class EShooterHitZone : uint8
{
	None		UMETA(DisplayName = "Ninguna"),
	Head		UMETA(DisplayName = "Cabeza"),
	Torso		UMETA(DisplayName = "Torso"),
	Stomach		UMETA(DisplayName = "Abdomen"),
	Arm			UMETA(DisplayName = "Brazo"),
	Leg			UMETA(DisplayName = "Pierna")
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

	/**
	 * Desplazamiento lateral de la cabeza al asomarse (inclinación).
	 *
	 * Sin él, quien se asoma tras una esquina tendría la cabeza fuera de la
	 * cápsula: vería y dispararía sin poder recibir impactos.
	 */
	UPROPERTY()
	FVector LeanOffset = FVector::ZeroVector;
};
