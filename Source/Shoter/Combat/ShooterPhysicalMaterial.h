#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShooterPhysicalMaterial.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * Material físico con propiedades balísticas.
 *
 * Se asigna a los materiales del escenario (o a sus mallas) para decidir qué
 * atraviesa una bala y cómo suena y se ve el impacto. Una superficie sin él
 * usa la resistencia por defecto de UShooterBallisticsSubsystem.
 */
UCLASS(BlueprintType)
class SHOTER_API UShooterPhysicalMaterial : public UPhysicalMaterial
{
	GENERATED_BODY()

public:
	/**
	 * Coste de atravesar 1 cm de este material, en unidades de
	 * PenetrationPower. Referencias: cristal 0,1 · chapa fina 0,3 · madera 1 ·
	 * ladrillo 3 · hormigón 4 · acero 8.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balística", meta = (ClampMin = "0.0"))
	float PenetrationResistance = 1.f;

	/** Efecto del impacto en esta superficie. Sin asignar, se usa el del arma. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacto")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacto")
	TObjectPtr<USoundBase> ImpactSound;
};
