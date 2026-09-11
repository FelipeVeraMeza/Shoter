#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShooterTelemetrySubsystem.generated.h"

/**
 * Registro de eventos de partida (RF-13.1).
 *
 * Escribe líneas JSON en Saved/Telemetry. El envío a un servicio de análisis
 * (RF-13.2 en adelante) queda pendiente del backend; el formato ya es el que
 * consumiría, así que migrar es cambiar el destino, no el contenido.
 */
UCLASS()
class SHOTER_API UShooterTelemetrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Desactivar corta el registro sin tocar las llamadas (RNF-3.7). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Telemetría")
	bool bEnabled = true;

	UFUNCTION(BlueprintCallable, Category = "Telemetría")
	void RecordMatchStart(const FString& MapName, const FString& ModeName);

	UFUNCTION(BlueprintCallable, Category = "Telemetría")
	void RecordMatchEnd(float DurationSeconds, int32 Kills, int32 Deaths);

	UFUNCTION(BlueprintCallable, Category = "Telemetría")
	void RecordKill(const FString& WeaponName, float Distance, bool bHeadshot);

	UFUNCTION(BlueprintCallable, Category = "Telemetría")
	void RecordWeaponUsage(const FString& WeaponName, int32 ShotsFired, int32 ShotsHit);

	/** Errores del cliente, para diagnóstico (RF-13.4). */
	UFUNCTION(BlueprintCallable, Category = "Telemetría")
	void RecordError(const FString& Context, const FString& Message);

private:
	/** Identifica la sesión para agrupar sus eventos. */
	FString SessionId;

	FString LogFilePath;

	void WriteEvent(const FString& EventType, const TMap<FString, FString>& Fields);
};
