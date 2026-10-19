#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterShellCasing.generated.h"

class UStaticMeshComponent;

/** Casquillo expulsado: un cilindro de latón con física que rebota y se queda en el suelo. */
UCLASS(NotBlueprintable)
class SHOTER_API AShooterShellCasing : public AActor
{
	GENERATED_BODY()

public:
	AShooterShellCasing();

	void Eject(const FVector& Velocity, float Length);

	static int32 GetLiveCount() { return LiveCount; }

	/** Tope de casquillos simultáneos en la escena. */
	static constexpr int32 MaxLiveCasings = 48;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	static int32 LiveCount;
};
