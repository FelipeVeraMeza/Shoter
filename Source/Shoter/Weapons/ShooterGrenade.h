#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterGrenade.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * Granada de fragmentación (RF-5.11).
 *
 * Vuela con física, rebota y detona tras FuseTime. El daño lo aplica el
 * servidor con caída radial; los clientes solo reproducen el efecto (RNF-3.1).
 */
UCLASS()
class SHOTER_API AShooterGrenade : public AActor
{
	GENERATED_BODY()

public:
	AShooterGrenade();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Granada")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Granada")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Granada")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Segundos hasta la detonación desde que se lanza. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada", meta = (ClampMin = "0.1"))
	float FuseTime = 3.f;

	/** Daño en el centro de la explosión. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada", meta = (ClampMin = "0.0"))
	float MaxDamage = 120.f;

	/** Radio en el que el daño es máximo, en cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada", meta = (ClampMin = "0.0"))
	float InnerRadius = 200.f;

	/** Radio en el que el daño llega a cero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada", meta = (ClampMin = "0.0"))
	float OuterRadius = 600.f;

	/** Fuerza con la que sale despedida al lanzarla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada", meta = (ClampMin = "0.0"))
	float ThrowSpeed = 1400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada|Efectos")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granada|Efectos")
	TObjectPtr<USoundBase> ExplosionSound;

	/** Lanza la granada en la dirección indicada. Solo servidor. */
	UFUNCTION(BlueprintCallable, Category = "Granada")
	void Launch(const FVector& Direction);

protected:
	virtual void BeginPlay() override;

private:
	FTimerHandle FuseTimerHandle;

	void Explode();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayExplosion(const FVector_NetQuantize& Location);
};
