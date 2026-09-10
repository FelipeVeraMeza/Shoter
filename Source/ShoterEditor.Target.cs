using UnrealBuildTool;
using System.Collections.Generic;

public class ShoterEditorTarget : TargetRules
{
	public ShoterEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Shoter");
	}
}
