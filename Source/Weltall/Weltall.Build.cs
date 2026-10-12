using UnrealBuildTool;

public class Weltall : ModuleRules
{
	public Weltall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "WeltallCore", "RenderCore" });
		PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "AudioExtensions", "EnhancedInput", "InputCore", "RHI", "Slate", "SlateCore" });
	}
}
