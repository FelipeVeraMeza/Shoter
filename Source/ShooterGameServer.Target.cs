using UnrealBuildTool;
using System.Collections.Generic;

public class ShooterGameServerTarget : TargetRules
{
	public ShooterGameServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("ShooterGame");
	}
}
