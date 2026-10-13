#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShooterTypes.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, Delta, AController*, InstigatedBy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOwnerDied, AActor*, Victim, AController*, Killer);

/**
 * Salud del personaje. La autoridad es exclusivamente del servidor (RNF-3.1):
 * el cliente solo recibe el valor replicado para mostrarlo.
 */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class SHOTER_API UHealthComponent : public UActorComponent
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
	float RegenDelay = 8.f;

	/**
	 * Puntos de salud recuperados por segundo.
	 *
	 * Cero por defecto: en un juego realista las heridas no se cierran solas,
	 * se vendan. La regeneración queda como opción para modos arcade.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud", meta = (ClampMin = "0.0"))
	float RegenRate = 0.f;

	// --- Heridas ---

	/** Daño mínimo de un impacto para abrir una herida que sangra. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "0.0"))
	float BleedDamageThreshold = 15.f;

	/** Salud perdida por segundo por cada herida abierta. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "0.0"))
	float BleedRatePerWound = 1.2f;

	/** Heridas simultáneas como máximo: más allá, el jugador ya estaría muerto. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "1"))
	int32 MaxBleedingWounds = 3;

	/** Velocidad conservada con una pierna herida. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LegInjurySpeedMultiplier = 0.7f;

	/** Vendas con las que aparece el jugador. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "0"))
	int32 MaxBandages = 2;

	/** Segundos que se tarda en vendar, sin poder disparar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "0.1"))
	float BandageDuration = 3.5f;

	/** Salud recuperada al terminar de vendar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Salud|Heridas", meta = (ClampMin = "0.0"))
	float BandageHealAmount = 15.f;

	/**
	 * Empieza a vendar. Detiene las hemorragias, cura la pierna y devuelve
	 * algo de salud al terminar. Válido en cliente: reenvía al servidor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Salud")
	void StartBandage();

	UFUNCTION(BlueprintPure, Category = "Salud")
	bool IsBandaging() const { return bIsBandaging; }

	UFUNCTION(BlueprintPure, Category = "Salud")
	int32 GetBandageCount() const { return BandageCount; }

	UFUNCTION(BlueprintPure, Category = "Salud")
	int32 GetBleedingWounds() const { return BleedingWounds; }

	UFUNCTION(BlueprintPure, Category = "Salud")
	bool HasLegInjury() const { return bLegInjured; }

	/** Solo servidor. Lesión de pierna sin herida abierta (caídas). */
	void ApplyLegInjury();

	/** Multiplicador de velocidad por heridas. */
	UFUNCTION(BlueprintPure, Category = "Salud")
	float GetMovementMultiplier() const { return bLegInjured ? LegInjurySpeedMultiplier : 1.f; }

	/**
	 * La zona del impacto viaja como nombre de hueso en el daño puntual: así
	 * no hace falta un tipo de daño por zona.
	 */
	static FName HitZoneToName(EShooterHitZone Zone);
	static EShooterHitZone NameToHitZone(FName Name);

	/**
	 * Ignora todo el daño recibido (RF3). Pensado para la zona de
	 * entrenamiento, donde morir interrumpe la práctica.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salud")
	bool bInvulnerable = false;

	UFUNCTION(BlueprintCallable, Category = "Salud")
	void SetInvulnerable(bool bNewInvulnerable);

	/** Devuelve la salud al máximo y revive si estaba muerto (RF6). */
	UFUNCTION(BlueprintCallable, Category = "Salud")
	void ResetHealth();

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

	UPROPERTY(Replicated)
	int32 BleedingWounds = 0;

	UPROPERTY(Replicated)
	bool bLegInjured = false;

	UPROPERTY(Replicated)
	bool bIsBandaging = false;

	UPROPERTY(Replicated)
	int32 BandageCount = 0;

	float LastDamageTime = -1000.f;

	/** Zona del impacto puntual en curso, leída por HandleTakeAnyDamage. */
	EShooterHitZone PendingHitZone = EShooterHitZone::None;

	/** Quién abrió la última herida: se lleva la baja si la víctima se desangra. */
	TWeakObjectPtr<AController> LastWoundInstigator;

	FTimerHandle BandageTimerHandle;

	UFUNCTION(Server, Reliable)
	void Server_StartBandage();

	void FinishBandage();

	/** Resta salud por una causa sin atacante directo (hemorragia). */
	void ApplyHealthLoss(float Amount, AController* Instigator);

	UFUNCTION()
	void HandleTakePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
		FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
		FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
