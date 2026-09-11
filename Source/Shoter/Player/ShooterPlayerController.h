#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

UCLASS()
class SHOTER_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Confirmación de impacto para el atacante: solo llega a quien disparó. */
	UFUNCTION(Client, Reliable)
	void Client_NotifyHitConfirmed(bool bHeadshot);

	/** Aviso de daño recibido, para el indicador direccional del HUD. */
	UFUNCTION(Client, Reliable)
	void Client_NotifyDamageTaken(const FVector_NetQuantize& DamageSourceLocation);

	/** Quién te eliminó y con qué, para mostrarlo al morir (RF-6.6). */
	UFUNCTION(Client, Reliable)
	void Client_NotifyKilledBy(const FString& KillerName, const FString& WeaponName);

	UFUNCTION(BlueprintPure, Category = "HUD")
	FString GetLastKillerName() const { return LastKillerName; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	FString GetLastKillerWeapon() const { return LastKillerWeapon; }

	/** Cuenta un disparo efectuado, para la precisión acumulada (RF-9.6). */
	void RegisterShotFired();

	/** Solo servidor. Informa al modo de juego y programa la reaparición. */
	void OnCharacterDied(AController* Killer);

	UFUNCTION(BlueprintPure, Category = "HUD")
	float GetTimeSinceHitConfirmed() const;

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool WasLastHitHeadshot() const { return bLastHitWasHeadshot; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	float GetTimeSinceDamageTaken() const;

	UFUNCTION(BlueprintPure, Category = "HUD")
	FVector GetLastDamageSourceLocation() const { return LastDamageSourceLocation; }

	/** Segundos restantes para reaparecer. Cero si el jugador está vivo. */
	UFUNCTION(BlueprintPure, Category = "HUD")
	float GetRespawnCountdown() const;

	void SetRespawnTime(float WorldTimeToRespawn) { RespawnWorldTime = WorldTimeToRespawn; }

private:
	float LastHitConfirmedTime = -1000.f;
	bool bLastHitWasHeadshot = false;

	float LastDamageTakenTime = -1000.f;
	FVector LastDamageSourceLocation = FVector::ZeroVector;

	FString LastKillerName;
	FString LastKillerWeapon;

	UPROPERTY(Replicated)
	float RespawnWorldTime = 0.f;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
