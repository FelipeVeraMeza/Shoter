#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, Delta, AController*, InstigatedBy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOwnerDied, AActor*, Victim, AController*, Killer);

/**
 * Salud del personaje. La autoridad es exclusivamente del servidor (RNF-3.1):
 * el cliente solo recibe el valor replicado para mostrarlo.
 */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class SHOOTERGAME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(BlueprintAssignable, Category = "Salud")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Salud")
	FOnOwnerDied OnOwnerDied;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Segundos sin recibir daño antes de que empiece la regeneración. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud", meta = (ClampMin = "0.0"))
	float RegenDelay = 5.f;

	/** Puntos de salud recuperados por segundo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud", meta = (ClampMin = "0.0"))
	float RegenRate = 25.f;

	UFUNCTION(BlueprintPure, Category = "Salud")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Salud")
	float GetHealthPercent() const { return Health / FMath::Max(MaxHealth, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Salud")
	bool IsDead() const { return bIsDead; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY(Replicated)
	bool bIsDead = false;

	float LastDamageTime = -1000.f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
