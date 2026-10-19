#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterFireSource.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Fuego con humo para ambientar el escenario: coches calcinados, bidones,
 * escombros que arden.
 *
 * Sin Niagara: las llamas son planos cruzados con un material emisivo que se
 * desplaza, la luz parpadea con ruido y el humo es una cadena de esferas
 * translúcidas que suben, crecen y se disipan con el viento. El humo tapa la
 * vista de verdad, así que también es un elemento táctico.
 */
UCLASS()
class SHOTER_API AShooterFireSource : public AActor
{
	GENERATED_BODY()

public:
	AShooterFireSource();

	/** Tamaño del fuego: 1 es un bidón; 3, un coche en llamas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuego", meta = (ClampMin = "0.2", ClampMax = "6.0"))
	float FireScale = 1.f;

	/** Intensidad de la luz, en candelas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuego", meta = (ClampMin = "0.0"))
	float LightIntensity = 80.f;

	/** Si sale humo. Un bidón en interior puede no tenerlo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuego")
	bool bEmitSmoke = true;

	/** Oscuridad del humo: 0 blanco, 1 negro de goma quemada. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuego", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmokeDarkness = 0.8f;

	/** Dirección y fuerza del viento que arrastra el humo, en cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuego")
	FVector Wind = FVector(60.f, 25.f, 0.f);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Fuego")
	TObjectPtr<UPointLightComponent> FireLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Flames;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SmokePuffs;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SmokeMaterials;

	/** Edad de cada bocanada de humo; negativa mientras espera turno. */
	TArray<float> SmokeAges;
	TArray<FVector> SmokeOffsets;

	float Time = 0.f;
	float SmokeLifetime = 6.f;

	static constexpr int32 FlameCount = 3;
	static constexpr int32 SmokeCount = 12;

	void UpdateFlames();
	void UpdateSmoke(float DeltaSeconds);
};
