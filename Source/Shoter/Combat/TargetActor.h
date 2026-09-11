#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Blanco de entrenamiento esférico. El servidor decide el daño y la
 * destrucción; los clientes solo reproducen el efecto visual (RNF-3.1).
 *
 * Da retroalimentación inmediata al impacto: destello, retroceso y tinte según
 * la salud. Al agotarse desaparece y vuelve tras RespawnDelay, para que la
 * galería de tiro no se quede vacía.
 */
UCLASS()
class SHOTER_API ATargetActor : public AActor
{
	GENERATED_BODY()

public:
	ATargetActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blanco")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Segundos hasta reaparecer. Cero o menos: el blanco no vuelve. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco", meta = (ClampMin = "0.0"))
	float RespawnDelay = 3.f;

	/** Oscila de lado a lado. Cero lo deja quieto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco", meta = (ClampMin = "0.0"))
	float SwingAmplitude = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco", meta = (ClampMin = "0.1"))
	float SwingSpeed = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco")
	FLinearColor HealthyColor = FLinearColor(0.05f, 0.6f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco")
	FLinearColor DamagedColor = FLinearColor(0.95f, 0.15f, 0.1f);

	UFUNCTION(BlueprintPure, Category = "Blanco")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Blanco")
	float GetHealthPercent() const { return Health / FMath::Max(MaxHealth, 1.f); }

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
	FTimerHandle RespawnTimerHandle;

	/** Segundos restantes del destello blanco al recibir un impacto. */
	float HitFlashRemaining = 0.f;

	UFUNCTION()
	void OnRep_Health();

	void UpdateVisuals();
	void HandleDestroyed();
	void Respawn();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitFeedback();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetVisible(bool bVisible);
};
