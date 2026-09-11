#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingDrone.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDroneDestroyed, ATrainingDrone*, Drone);

/**
 * Dron de entrenamiento (RF2).
 *
 * Flota, reacciona al impacto con chispas y se destruye al agotar su salud.
 * El servidor decide el daño; los clientes reproducen el efecto (RNF-3.1).
 *
 * La malla y los efectos se asignan desde el Blueprint: sin ellos usa la
 * esfera del motor, de modo que el sistema funciona antes de tener arte.
 */
UCLASS()
class SHOTER_API ATrainingDrone : public AActor
{
	GENERATED_BODY()

public:
	ATrainingDrone();

	UPROPERTY(BlueprintAssignable, Category = "Dron")
	FOnDroneDestroyed OnDroneDestroyed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dron")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Amplitud del flotado vertical, en cm. Cero lo deja inmóvil. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Movimiento", meta = (ClampMin = "0.0"))
	float HoverAmplitude = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Movimiento", meta = (ClampMin = "0.1"))
	float HoverSpeed = 1.6f;

	/** Desplazamiento lateral. Cero mantiene el dron en su sitio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Movimiento", meta = (ClampMin = "0.0"))
	float StrafeAmplitude = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Movimiento", meta = (ClampMin = "0.1"))
	float StrafeSpeed = 0.8f;

	/** Giro sobre sí mismo, en grados por segundo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Movimiento")
	float SpinSpeed = 45.f;

	/** Chispas al recibir un impacto (RF2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Efectos")
	TObjectPtr<UNiagaraSystem> HitSparks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Efectos")
	TObjectPtr<UNiagaraSystem> DestructionEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Efectos")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Efectos")
	TObjectPtr<USoundBase> DestructionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Efectos")
	FLinearColor HealthyColor = FLinearColor(0.1f, 0.75f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dron|Efectos")
	FLinearColor DamagedColor = FLinearColor(0.95f, 0.2f, 0.12f);

	UFUNCTION(BlueprintPure, Category = "Dron")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Dron")
	float GetHealthPercent() const { return Health / FMath::Max(MaxHealth, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Dron")
	bool IsDestroyed() const { return Health <= 0.f; }

	/** Devuelve el dron a su estado inicial, sin recrearlo (RF6). */
	UFUNCTION(BlueprintCallable, Category = "Dron")
	void ResetDrone();

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	FVector HomeLocation = FVector::ZeroVector;

	/** Segundos restantes del destello de impacto. */
	float HitFlashRemaining = 0.f;

	/** Desfase por dron, para que no floten todos al unísono. */
	float PhaseOffset = 0.f;

	UFUNCTION()
	void OnRep_Health();

	void UpdateVisuals();
	void HandleDestruction(AController* InstigatedBy);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitEffects(const FVector_NetQuantize& ImpactLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDestruction();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetActive(bool bActive);
};
