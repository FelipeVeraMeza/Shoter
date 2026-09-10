#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterTypes.h"
#include "ShooterPlayerState.generated.h"

UCLASS()
class SHOTER_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Estadísticas")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "Estadísticas")
	int32 GetDeaths() const { return Deaths; }

	UFUNCTION(BlueprintPure, Category = "Estadísticas")
	EShooterTeam GetTeam() const { return Team; }

	void SetTeam(EShooterTeam NewTeam) { Team = NewTeam; }

	/** Solo servidor. */
	void AddKill();
	void AddDeath();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	UPROPERTY(Replicated)
	EShooterTeam Team = EShooterTeam::None;
};
