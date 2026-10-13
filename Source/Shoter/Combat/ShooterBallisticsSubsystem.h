#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShooterTypes.h"
#include "Weapons/WeaponBase.h"
#include "ShooterBallisticsSubsystem.generated.h"

class AWeaponBase;
class AShooterCharacter;
class AController;
class UPhysicalMaterial;

/** Una bala en vuelo. Solo existe en el servidor. */
struct FShooterBullet
{
	TWeakObjectPtr<AWeaponBase> Weapon;
	TWeakObjectPtr<AShooterCharacter> Shooter;
	TWeakObjectPtr<AController> InstigatorController;

	/**
	 * Copia del modelo de daño del arma. Si el tirador muere con la bala en
	 * el aire, el arma se destruye, pero el disparo debe seguir contando:
	 * los intercambios simultáneos son parte del combate realista.
	 */
	FShooterDamageModel DamageModel;

	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;

	/**
	 * Segundos que se rebobina a los personajes al resolver cada tramo.
	 *
	 * Es constante durante todo el vuelo: el tirador vio a los demás con ese
	 * retraso, así que una bala que tarda 0,3 s en llegar debe encontrarlos
	 * donde él los vería 0,3 s después de disparar, no donde estaban al
	 * apretar el gatillo.
	 */
	float RewindOffset = 0.f;

	float DistanceTravelled = 0.f;
	float Age = 0.f;

	/** Capacidad de penetración restante, que se gasta al atravesar materiales. */
	float RemainingPenetration = 0.f;

	/** Daño conservado tras atravesar obstáculos (1 = intacto). */
	float DamageScale = 1.f;

	int32 Penetrations = 0;

	/** Jugadores a los que ya se avisó de que la bala pasó cerca. */
	TArray<TWeakObjectPtr<AController>> SuppressedControllers;
};

/**
 * Balística simulada (realismo).
 *
 * Sustituye el disparo instantáneo por balas con velocidad finita, caída por
 * gravedad, pérdida de velocidad y penetración de materiales. El servidor
 * simula cada bala por tramos; cada tramo se resuelve como un trazado contra
 * el escenario y contra los personajes rebobinados por la compensación de
 * latencia, así que la autoridad sigue siendo del servidor (RNF-3.2).
 */
UCLASS()
class SHOTER_API UShooterBallisticsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Solo servidor. Lanza una bala desde Start en la dirección Direction
	 * (ya con dispersión y puesta a cero aplicadas).
	 */
	void FireBullet(AWeaponBase* Weapon, AShooterCharacter* Shooter, const FVector& Start,
		const FVector& Direction, float RewindOffset);

	/**
	 * Resuelve un disparo instantáneo con las mismas reglas de daño y
	 * penetración. Para armas con bUseBallistics desactivado.
	 */
	void FireHitscan(AWeaponBase* Weapon, AShooterCharacter* Shooter, const FVector& Start,
		const FVector& Direction, float RewindOffset);

	/**
	 * Ángulo de elevación, en grados, que hace que la bala cruce la línea de
	 * mira a la distancia de puesta a cero.
	 */
	static float ComputeZeroingAngle(float MuzzleVelocity, float DecayDistance, float ZeroingDistance, float Gravity);

	/** Resistencia de una superficie sin UShooterPhysicalMaterial (hormigón). */
	static constexpr float DefaultPenetrationResistance = 4.f;

	/** Distancia a la que una bala que pasa cerca suprime a un jugador, en cm. */
	static constexpr float SuppressionRadius = 300.f;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Bullets.Num() > 0; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<FShooterBullet> Bullets;

	/**
	 * Avanza una bala un tramo. Devuelve false si la bala terminó (impacto
	 * sin penetración, sin energía o fuera de alcance).
	 */
	bool StepBullet(FShooterBullet& Bullet, float DeltaTime, float Gravity);

	/**
	 * Resuelve un segmento recto. Devuelve false si la bala se detuvo en él.
	 * Si atraviesa algo, OutContinueFrom indica desde dónde sigue.
	 */
	bool ResolveSegment(FShooterBullet& Bullet, const FVector& SegmentStart, const FVector& SegmentEnd,
		FVector& OutContinueFrom);

	/** Intenta atravesar la superficie impactada. Devuelve el punto de salida si lo logra. */
	bool TryPenetrate(FShooterBullet& Bullet, const FHitResult& Hit, const FVector& Direction, FVector& OutExitPoint) const;

	/** Daño, confirmación al tirador y efecto de impacto sobre un personaje. */
	void ApplyCharacterHit(FShooterBullet& Bullet, AShooterCharacter* Victim, const FVector& ImpactPoint,
		const FVector& Direction, EShooterHitZone Zone, float Distance);

	/** Avisa a quien tenga una bala pasándole cerca (supresión). */
	void NotifyNearMisses(FShooterBullet& Bullet, const FVector& SegmentStart, const FVector& SegmentEnd);

	static float GetPenetrationResistance(const UPhysicalMaterial* Material);
};
