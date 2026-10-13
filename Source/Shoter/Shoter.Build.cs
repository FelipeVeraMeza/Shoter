using UnrealBuildTool;

public class Shoter : ModuleRules
{
	public Shoter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Los includes se escriben relativos a la raíz del módulo
		// ("Combat/HealthComponent.h"), no a cada subcarpeta.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Niagara",
			// Menús y HUD en widgets (RF-2).
			"UMG",
			// Bots: navegación y percepción (RF-3.6, RF-8.4).
			"AIModule",
			"NavigationSystem",
			// UPhysicalMaterial: la resistencia balística de cada superficie.
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			// Necesario para consultar las resoluciones del monitor (RNF-5.3).
			"RHI",
			// Buscar los DataAssets de accesorios sin cargarlos todos (RF-9.4).
			// Engine ya lo arrastra, pero depender de eso de forma implícita
			// rompería si esa dependencia transitiva cambiara.
			"AssetRegistry"
		});
	}
}
