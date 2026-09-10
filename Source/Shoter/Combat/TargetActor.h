#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class SHOTER_API ATargetActor : public AActor
{
	GENERATED_BODY()

public:
	ATargetActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	float Health = 100.f;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
};
