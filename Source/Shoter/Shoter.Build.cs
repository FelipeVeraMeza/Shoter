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
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			// Necesario para consultar las resoluciones del monitor (RNF-5.3).
			"RHI"
		});
	}
}
