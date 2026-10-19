#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ShooterBotController.generated.h"

class AShooterCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * Bot de combate (RF-3.6, RF-8.4).
 *
 * Máquina de estados simple en vez de un árbol de comportamiento: patrulla
 * entre objetivos del mapa, combate a quien ve, investiga de dónde le
 * disparan y busca al enemigo perdido. El balance queda en propiedades
 * editables, sin assets intermedios.
 *
 * Pelea como un tirador disciplinado, no como una torreta: necesita ver al
 * enemigo, tarda en reaccionar, dispara en ráfagas cortas (tiro a tiro a
 * distancia), apunta con la mira a media distancia y su puntería mejora
 * cuanto más tiempo sigue al blanco.
 */
UCLASS()
class SHOTER_API AShooterBotController : public AAIController
{
	GENERATED_BODY()

public:
	AShooterBotController();

	/** Distancia a la que el bot deja de acercarse y combate desde donde está. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "100.0"))
	float PreferredCombatDistance = 2200.f;

	/** Alcance de visión, en cm. La avenida mide 160 m: a 90 m se distingue a alguien. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "100.0"))
	float SightRange = 9000.f;

	/** Ángulo de visión en grados, a cada lado. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "10.0", ClampMax = "180.0"))
	float SightAngle = 70.f;

	/**
	 * Grados por segundo que el bot gira para apuntar. Valores bajos dan un
	 * rival más benévolo: es la palanca principal de dificultad.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "10.0"))
	float AimSpeed = 160.f;

	/**
	 * Error de puntería inicial, en cm por cada 10 m de distancia. Se reduce
	 * mientras el bot sigue al mismo blanco.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "0.0"))
	float AimErrorPer10m = 55.f;

	/** Segundos hasta que el error de puntería se reduce a un tercio. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "0.1"))
	float AimSettleTime = 1.6f;

	/** Tiempo de reacción al ver a un enemigo nuevo, en segundos (mínimo y máximo). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "0.0"))
	FVector2D ReactionTime = FVector2D(0.35f, 0.75f);

	/** Segundos que sigue buscando tras perder de vista al objetivo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "0.0"))
	float MemoryDuration = 8.f;

	/** Radio alrededor de cada objetivo de patrulla en el que se elige destino. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Bot", meta = (ClampMin = "100.0"))
	float PatrolRadius = 1800.f;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<AShooterCharacter> CurrentTarget;

	/** Última posición conocida del objetivo, para seguir buscándolo. */
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	bool bHasLastKnownLocation = false;

	float TimeSinceTargetSeen = 1000.f;
	float TimeOnTarget = 0.f;
	float ReactionRemaining = 0.f;
	bool bTargetVisible = false;

	/** Desviación actual de la puntería respecto al blanco, en cm. */
	FVector AimError = FVector::ZeroVector;
	float NextAimErrorChange = 0.f;

	// Disciplina de tiro: ráfagas cortas separadas por pausas.
	int32 BurstShotsRemaining = 0;
	float NextShotTime = 0.f;

	// Movimiento de combate: desplazamientos laterales cortos.
	float NextRepositionTime = 0.f;
	float NextCrouchDecision = 0.f;

	// Patrulla y detección de atascos.
	FVector PatrolGoal = FVector::ZeroVector;
	bool bHasPatrolGoal = false;
	float StuckTime = 0.f;

	float LastVisibilityCheck = -100.f;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Al recibir daño, el bot mira hacia el origen aunque no vea al tirador. */
	UFUNCTION()
	void HandleDamaged(float NewHealth, float Delta, AController* InstigatedBy);

	/** Enemigo vivo más cercano a la vista: dentro del cono y sin nada en medio. */
	AShooterCharacter* FindVisibleEnemy() const;

	bool CanSee(const AShooterCharacter* Other) const;

	void UpdateCombat(AShooterCharacter* Bot, float DeltaSeconds);
	void UpdateSearch(AShooterCharacter* Bot, float DeltaSeconds);
	void UpdatePatrol(AShooterCharacter* Bot, float DeltaSeconds);
	void UpdateFire(AShooterCharacter* Bot, float Distance, bool bAimed);

	/** Gira la vista hacia un punto a AimSpeed; devuelve el error angular restante. */
	float RotateTowards(const FVector& Point, float DeltaSeconds, float Speed);

	/** Al patrullar, el bot mira hacia donde camina. */
	void FaceMovementDirection(AShooterCharacter* Bot, float DeltaSeconds);

	bool PickPatrolGoal(AShooterCharacter* Bot);

	bool IsSameTeam(const AShooterCharacter* Other) const;
};
