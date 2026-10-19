#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class USoundBase;
class USoundAttenuation;
class UStaticMesh;
class UPhysicalMaterial;
class UWorld;
class AActor;

/** Tipo de superficie alcanzada: decide el color del polvo y si saltan esquirlas. */
enum class EShooterImpactKind : uint8
{
	Concrete,
	Brick,
	Wood,
	Metal,
	Dirt,
	Flesh
};

/**
 * Efectos visuales y sonoros del combate, sin Niagara.
 *
 * El proyecto no tiene sistemas de partículas propios: fogonazos, trazadoras,
 * casquillos, impactos y fuego se construyen con mallas básicas del motor y
 * los materiales que genera build_realistic_map.py en /Game/Realista/Efectos.
 * Si esos materiales faltan, los efectos se dibujan igual con el material por
 * defecto: se ven peor, pero el juego no falla.
 *
 * Nada de esto existe en un servidor dedicado: allí no hay nadie que mire.
 */
namespace ShooterEffects
{
	// --- Recursos ---

	SHOTER_API UMaterialInterface* GetMuzzleFlashMaterial();
	SHOTER_API UMaterialInterface* GetTracerMaterial();
	SHOTER_API UMaterialInterface* GetReticleMaterial();
	SHOTER_API UMaterialInterface* GetImpactDecalMaterial();
	SHOTER_API UMaterialInterface* GetDustMaterial();
	SHOTER_API UMaterialInterface* GetBrassMaterial();
	SHOTER_API UMaterialInterface* GetSightBodyMaterial();
	SHOTER_API UMaterialInterface* GetFlameMaterial();

	/** Disparo de la plantilla: el único sonido de arma que trae el proyecto. */
	SHOTER_API USoundBase* GetDefaultGunshot();

	/**
	 * Atenuación de un disparo: se oye a 150 m y pierde agudos con la
	 * distancia, que es lo que permite saber si un tiroteo está cerca.
	 */
	SHOTER_API USoundAttenuation* GetGunshotAttenuation();

	/** Malla básica del motor: "Cube", "Cylinder", "Sphere", "Plane" o "Cone". */
	SHOTER_API UStaticMesh* GetBasicShape(const TCHAR* ShapeName);

	// --- Utilidades ---

	/** Deduce el tipo de superficie del material físico (PM_Ladrillo, PM_Madera...). */
	SHOTER_API EShooterImpactKind ClassifySurface(const UPhysicalMaterial* Material, bool bFlesh);

	/** Posición de la cámara local, para no gastar efectos que nadie verá. */
	SHOTER_API bool GetViewerLocation(const UWorld* World, FVector& OutLocation);

	/** True si la máquina dibuja algo (no es un servidor dedicado). */
	SHOTER_API bool ShouldSpawnCosmetics(const UWorld* World);

	// --- Efectos ---

	/** Polvo, esquirlas y marca en la superficie. Scale agranda todo (explosiones). */
	SHOTER_API void SpawnImpact(UWorld* World, const FVector& Location, const FVector& Normal,
		EShooterImpactKind Kind, float Scale = 1.f);

	/** Trazadora que sale de Start en Direction y vuela hasta lo primero que encuentre. */
	SHOTER_API void SpawnTracer(UWorld* World, const FVector& Start, const FVector& Direction,
		float BulletSpeed, const AActor* IgnoredActor);

	/** Casquillo con física que sale despedido con la velocidad indicada. */
	SHOTER_API void SpawnShellCasing(UWorld* World, const FVector& Location, const FRotator& Rotation,
		const FVector& EjectVelocity, float Length);

	/** Destello de luz breve: fogonazo visto de lejos o explosión. */
	SHOTER_API void SpawnLightFlash(UWorld* World, const FVector& Location, const FLinearColor& Color,
		float Intensity, float Radius, float Duration);
}
