#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponAttachmentDataAsset.generated.h"

class UStaticMesh;

/** Ranura que ocupa un accesorio. Una por ranura y arma (RF-5.10). */
UENUM(BlueprintType)
enum class EAttachmentSlot : uint8
{
	Optic		UMETA(DisplayName = "Mira"),
	Barrel		UMETA(DisplayName = "Cañón"),
	Magazine	UMETA(DisplayName = "Cargador")
};

/**
 * Accesorio de arma (RF-5.10).
 *
 * Los efectos son multiplicadores, no valores absolutos: así un mismo
 * accesorio sirve para todas las armas sin recalibrarlo por cada una, y el
 * balance sigue viviendo en el DataAsset del arma (RNF-9.3).
 *
 * Todos los campos valen 1.0 por defecto, de modo que un accesorio recién
 * creado no altera nada hasta que se le configure un efecto.
 */
UCLASS(BlueprintType)
class SHOTER_API UWeaponAttachmentDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identidad")
	EAttachmentSlot Slot = EAttachmentSlot::Optic;

	/**
	 * Disparos con el arma necesarios para desbloquearlo (RF-9.4).
	 *
	 * Se cuenta por arma, no global: un accesorio de rifle se gana usando el
	 * rifle, que es lo que el requerimiento pide ("según el uso de cada arma").
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Desbloqueo", meta = (ClampMin = "0"))
	int32 ShotsToUnlock = 0;

	// --- Modificadores. 1.0 significa "sin efecto". ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float DamageMultiplier = 1.f;

	/** Menor que 1 mejora la precisión: la dispersión baja. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SpreadMultiplier = 1.f;

	/** Menor que 1 reduce el retroceso. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float RecoilMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float MagazineMultiplier = 1.f;

	/** Menor que 1 acelera la recarga. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float ReloadTimeMultiplier = 1.f;

	/** Menor que 1 acelera el apuntado. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float ADSTimeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Efectos", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EffectiveRangeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMesh> AttachmentMesh;

	/** Socket del arma donde se engancha la malla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FName AttachSocket = NAME_None;
};

/**
 * Conjunto de accesorios equipados en un arma.
 *
 * Se guarda en el perfil y se replica al servidor, que es quien aplica los
 * modificadores: si el cliente los calculara, bastaría con editar la memoria
 * para multiplicar el daño (RNF-3.1, RNF-3.2).
 */
USTRUCT(BlueprintType)
struct FWeaponAttachmentSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accesorios")
	TObjectPtr<UWeaponAttachmentDataAsset> Optic = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accesorios")
	TObjectPtr<UWeaponAttachmentDataAsset> Barrel = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accesorios")
	TObjectPtr<UWeaponAttachmentDataAsset> Magazine = nullptr;

	/** Producto de los modificadores de las tres ranuras. */
	float GetDamageMultiplier() const;
	float GetSpreadMultiplier() const;
	float GetRecoilMultiplier() const;
	float GetMagazineMultiplier() const;
	float GetReloadTimeMultiplier() const;
	float GetADSTimeMultiplier() const;
	float GetEffectiveRangeMultiplier() const;
};
