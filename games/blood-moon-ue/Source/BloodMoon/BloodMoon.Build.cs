using UnrealBuildTool;

public class BloodMoon : ModuleRules
{
	public BloodMoon(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule" });
	}
}
