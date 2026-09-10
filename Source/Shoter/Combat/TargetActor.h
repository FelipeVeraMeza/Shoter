#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Blanco de entrenamiento. El servidor decide el daño y la destrucción; los
 * clientes solo reproducen el efecto visual (RNF-3.1).
 *
 * Al agotarse la salud desaparece y vuelve tras RespawnDelay, de modo que la
 * zona de tiro no se queda vacía. Con RespawnDelay <= 0 no reaparece.
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

	/** Color con el blanco intacto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco")
	FLinearColor HealthyColor = FLinearColor(0.05f, 0.55f, 0.9f);

	/** Color al que vira según baja la salud. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blanco")
	FLinearColor DamagedColor = FLinearColor(0.9f, 0.15f, 0.1f);

	UFUNCTION(BlueprintPure, Category = "Blanco")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Blanco")
	float GetHealthPercent() const { return Health / FMath::Max(MaxHealth, 1.f); }

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	FTimerHandle RespawnTimerHandle;

	UFUNCTION()
	void OnRep_Health();

	/** Tiñe el blanco según la salud restante. */
	void UpdateVisuals();

	void HandleDestroyed();
	void Respawn();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetVisible(bool bVisible);
};
