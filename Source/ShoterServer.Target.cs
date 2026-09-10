using UnrealBuildTool;
using System.Collections.Generic;

public class ShoterServerTarget : TargetRules
{
	public ShoterServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Shoter");
	}
}
