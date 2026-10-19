#include "Effects/ShooterEffects.h"
#include "Effects/ShooterTracer.h"
#include "Effects/ShooterShellCasing.h"
#include "Effects/ShooterImpactEffect.h"
#include "Effects/ShooterLightFlash.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Package.h"

namespace
{
	/**
	 * Carga perezosa con caché débil.
	 *
	 * Si el asset no existe (aún no se ejecutó el script del mapa), no se
	 * reintenta en cada disparo: buscar un paquete en disco a 12 disparos por
	 * segundo se notaría. Se vuelve a probar pasados unos segundos por si el
	 * script se ejecuta con el editor abierto.
	 */
	template <typename T>
	T* LoadCached(TWeakObjectPtr<T>& Cache, double& LastFailure, const TCHAR* Path)
	{
		if (T* Cached = Cache.Get())
		{
			return Cached;
		}

		const double Now = FPlatformTime::Seconds();
		if (Now - LastFailure < 10.0)
		{
			return nullptr;
		}

		T* Loaded = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Loaded)
		{
			Cache = Loaded;
		}
		else
		{
			LastFailure = Now;
		}
		return Loaded;
	}

#define SHOOTER_CACHED_ASSET(Type, Function, Path) \
	Type* Function() \
	{ \
		static TWeakObjectPtr<Type> Cache; \
		static double LastFailure = -100.0; \
		return LoadCached<Type>(Cache, LastFailure, Path); \
	}
}

namespace ShooterEffects
{
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetMuzzleFlashMaterial, TEXT("/Game/Realista/Efectos/M_Fogonazo.M_Fogonazo"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetTracerMaterial, TEXT("/Game/Realista/Efectos/M_Trazadora.M_Trazadora"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetReticleMaterial, TEXT("/Game/Realista/Efectos/M_Reticula.M_Reticula"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetImpactDecalMaterial, TEXT("/Game/Realista/Efectos/M_Impacto.M_Impacto"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetDustMaterial, TEXT("/Game/Realista/Efectos/M_Polvo.M_Polvo"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetBrassMaterial, TEXT("/Game/Realista/Materiales/MI_Laton.MI_Laton"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetSightBodyMaterial, TEXT("/Game/Realista/Materiales/MI_MetalNegro.MI_MetalNegro"))
	SHOOTER_CACHED_ASSET(UMaterialInterface, GetFlameMaterial, TEXT("/Game/Realista/Efectos/M_Fuego.M_Fuego"))
	SHOOTER_CACHED_ASSET(USoundBase, GetDefaultGunshot,
		TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"))

	USoundAttenuation* GetGunshotAttenuation()
	{
		static TWeakObjectPtr<USoundAttenuation> Cache;
		if (USoundAttenuation* Existing = Cache.Get())
		{
			return Existing;
		}

		// Objeto transitorio en vez de asset: así no depende de que nadie lo
		// cree en el editor. Se ancla a la raíz para que el recolector no se
		// lo lleve entre partidas.
		USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("ShooterGunshotAttenuation"));
		Attenuation->AddToRoot();

		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
		Settings.FalloffDistance = 15000.f;
		Settings.dBAttenuationAtMax = -50.f;

		// De lejos un disparo es un estampido sordo: se pierden los agudos.
		Settings.bAttenuateWithLPF = true;
		Settings.LPFRadiusMin = 1500.f;
		Settings.LPFRadiusMax = 12000.f;
		Settings.LPFFrequencyAtMax = 2200.f;

		Cache = Attenuation;
		return Attenuation;
	}

	UStaticMesh* GetBasicShape(const TCHAR* ShapeName)
	{
		const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), ShapeName, ShapeName);
		return LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	EShooterImpactKind ClassifySurface(const UPhysicalMaterial* Material, bool bFlesh)
	{
		if (bFlesh)
		{
			return EShooterImpactKind::Flesh;
		}

		if (!Material)
		{
			return EShooterImpactKind::Concrete;
		}

		// Los materiales físicos los nombra el script del mapa: PM_<Superficie>.
		const FString Name = Material->GetName();
		if (Name.Contains(TEXT("Ladrillo")))
		{
			return EShooterImpactKind::Brick;
		}
		if (Name.Contains(TEXT("Madera")))
		{
			return EShooterImpactKind::Wood;
		}
		if (Name.Contains(TEXT("Chapa")) || Name.Contains(TEXT("Contenedor")) || Name.Contains(TEXT("Vehiculo")))
		{
			return EShooterImpactKind::Metal;
		}
		if (Name.Contains(TEXT("Arena")) || Name.Contains(TEXT("Tierra")))
		{
			return EShooterImpactKind::Dirt;
		}
		return EShooterImpactKind::Concrete;
	}

	bool ShouldSpawnCosmetics(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_DedicatedServer && !World->bIsTearingDown;
	}

	bool GetViewerLocation(const UWorld* World, FVector& OutLocation)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		if (!Controller || !Controller->PlayerCameraManager)
		{
			return false;
		}

		OutLocation = Controller->PlayerCameraManager->GetCameraLocation();
		return true;
	}

	void SpawnImpact(UWorld* World, const FVector& Location, const FVector& Normal, EShooterImpactKind Kind, float Scale)
	{
		if (!ShouldSpawnCosmetics(World))
		{
			return;
		}

		// Un impacto a 120 m ocupa un píxel: no compensa crearlo.
		FVector Viewer;
		const bool bHasViewer = GetViewerLocation(World, Viewer);
		const float Distance = bHasViewer ? FVector::Dist(Viewer, Location) : 0.f;
		if (Distance > 12000.f * FMath::Max(Scale, 1.f))
		{
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AShooterImpactEffect* Effect = World->SpawnActor<AShooterImpactEffect>(
			AShooterImpactEffect::StaticClass(), Location, Normal.Rotation(), Params);
		if (Effect)
		{
			// Las esquirlas con física solo merecen la pena de cerca.
			Effect->Init(Kind, Normal, Scale, Distance < 3000.f * FMath::Max(Scale, 1.f));
		}
	}

	void SpawnTracer(UWorld* World, const FVector& Start, const FVector& Direction, float BulletSpeed, const AActor* IgnoredActor)
	{
		if (!ShouldSpawnCosmetics(World) || Direction.IsNearlyZero())
		{
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AShooterTracer* Tracer = World->SpawnActor<AShooterTracer>(
			AShooterTracer::StaticClass(), Start, Direction.Rotation(), Params);
		if (Tracer)
		{
			Tracer->Launch(Start, Direction.GetSafeNormal(), BulletSpeed, IgnoredActor);
		}
	}

	void SpawnShellCasing(UWorld* World, const FVector& Location, const FRotator& Rotation,
		const FVector& EjectVelocity, float Length)
	{
		if (!ShouldSpawnCosmetics(World) || Length <= 0.f)
		{
			return;
		}

		// Casquillos con física: solo cerca de quien mira y con un tope, o
		// ocho tiradores en automático llenarían el suelo de cuerpos rígidos.
		FVector Viewer;
		if (GetViewerLocation(World, Viewer) && FVector::DistSquared(Viewer, Location) > FMath::Square(2500.f))
		{
			return;
		}

		if (AShooterShellCasing::GetLiveCount() >= AShooterShellCasing::MaxLiveCasings)
		{
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AShooterShellCasing* Casing = World->SpawnActor<AShooterShellCasing>(
			AShooterShellCasing::StaticClass(), Location, Rotation, Params);
		if (Casing)
		{
			Casing->Eject(EjectVelocity, Length);
		}
	}

	void SpawnLightFlash(UWorld* World, const FVector& Location, const FLinearColor& Color,
		float Intensity, float Radius, float Duration)
	{
		if (!ShouldSpawnCosmetics(World))
		{
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AShooterLightFlash* Flash = World->SpawnActor<AShooterLightFlash>(
			AShooterLightFlash::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (Flash)
		{
			Flash->Init(Color, Intensity, Radius, Duration);
		}
	}
}
