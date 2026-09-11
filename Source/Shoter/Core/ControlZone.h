#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "ControlZone.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class AShooterCharacter;

/**
 * Zona de captura del modo Dominación (RF-8.3).
 *
 * El progreso de captura avanza mientras un equipo tiene más ocupantes vivos
 * que el contrario, y se detiene —sin retroceder— cuando ambos están presentes.
 * Congelar en vez de revertir premia al equipo que llegó primero y evita que
 * dos jugadores se anulen indefinidamente en un empate estéril.
 *
 * El servidor es el único que calcula: los clientes reciben el estado ya
 * resuelto (RNF-3.1).
 */
UCLASS()
class SHOTER_API AControlZone : public AActor
{
	GENERATED_BODY()

public:
	AControlZone();

	/** Letra que identifica la zona en el HUD ("A", "B", "C"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zona")
	FString ZoneLabel = TEXT("A");

	/** Segundos que tarda un solo jugador en capturarla desde neutral. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zona", meta = (ClampMin = "1.0"))
	float CaptureTime = 8.f;

	/**
	 * Cuánto acelera cada ocupante extra, como fracción del ritmo base.
	 *
	 * Por debajo de 1 para que un pelotón no capture instantáneamente: el
	 * segundo jugador aporta la mitad que el primero, el tercero un tercio.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zona", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExtraOccupantBonus = 0.5f;

	/** Puntos por segundo que otorga al equipo que la controla. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zona", meta = (ClampMin = "0.0"))
	float PointsPerSecond = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zona", meta = (ClampMin = "50.0"))
	float ZoneRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zona", meta = (ClampMin = "50.0"))
	float ZoneHeight = 300.f;

	UFUNCTION(BlueprintPure, Category = "Zona")
	EShooterTeam GetOwningTeam() const { return OwningTeam; }

	UFUNCTION(BlueprintPure, Category = "Zona")
	EZoneState GetZoneState() const { return ZoneState; }

	/** Progreso de captura de 0 a 1, para la barra del HUD. */
	UFUNCTION(BlueprintPure, Category = "Zona")
	float GetCaptureProgress() const { return CaptureProgress; }

	/** Equipo que está capturándola ahora mismo, distinto del que la posee. */
	UFUNCTION(BlueprintPure, Category = "Zona")
	EShooterTeam GetCapturingTeam() const { return CapturingTeam; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zona")
	TObjectPtr<UCapsuleComponent> CaptureVolume;

	/** Marca visual del suelo. Se tiñe con el color del equipo que controla. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zona")
	TObjectPtr<UStaticMeshComponent> ZoneMarker;

private:
	UPROPERTY(ReplicatedUsing = OnRep_OwningTeam)
	EShooterTeam OwningTeam = EShooterTeam::None;

	UPROPERTY(Replicated)
	EZoneState ZoneState = EZoneState::Neutral;

	UPROPERTY(Replicated)
	float CaptureProgress = 0.f;

	UPROPERTY(Replicated)
	EShooterTeam CapturingTeam = EShooterTeam::None;

	/** Acumula fracciones de punto entre ticks para no perderlas al truncar. */
	float PointAccumulator = 0.f;

	UFUNCTION()
	void OnRep_OwningTeam();

	/** Cuenta ocupantes vivos por equipo dentro del volumen. */
	void CountOccupants(int32& OutAlpha, int32& OutBravo) const;

	/** Avanza o retrocede la captura según quién domine el volumen. */
	void UpdateCapture(float DeltaSeconds, int32 AlphaCount, int32 BravoCount);

	/** Otorga puntos al equipo que la controla (RF-8.3). */
	void AwardPoints(float DeltaSeconds);

	void RefreshMarkerColor();
};
