using UnrealBuildTool;

public class BloodMoonEditorTarget : TargetRules
{
	public BloodMoonEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("BloodMoon");
	}
}
