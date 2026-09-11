#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

UCLASS()
class SHOTER_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController();

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

	// --- Detección de desconexión (RNF-2.6) ---

	/**
	 * Segundos sin recibir nada del servidor.
	 *
	 * Se mide contra la última actualización de red recibida, no con un ping
	 * propio: cualquier tráfico del servidor sirve como prueba de vida, así que
	 * no hace falta añadir paquetes para detectar la caída.
	 */
	UFUNCTION(BlueprintPure, Category = "Red")
	float GetSecondsSinceServerResponse() const;

	/** True cuando el enlace lleva caído más de DisconnectWarningSeconds. */
	UFUNCTION(BlueprintPure, Category = "Red")
	bool IsConnectionLost() const;

	/**
	 * Umbral del aviso, por debajo de los 10 s que exige RNF-2.6.
	 *
	 * Tres segundos bastan para descartar un pico de latencia normal sin
	 * agotar el presupuesto del requerimiento.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Red", meta = (ClampMin = "1.0", ClampMax = "9.0"))
	float DisconnectWarningSeconds = 3.f;

	virtual void Tick(float DeltaSeconds) override;

private:
	/** Momento del mundo en que se recibió la última señal del servidor. */
	float LastServerResponseTime = 0.f;

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
