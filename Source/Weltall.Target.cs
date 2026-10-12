using UnrealBuildTool;

public class WeltallTarget : TargetRules
{
	public WeltallTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Weltall");

		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			// The player's builds log the cases play carries past, so a report can name them.
			BuildEnvironment = TargetBuildEnvironment.Unique;
			bUseLoggingInShipping = true;
			// A single-player game without navigation: neither the engine's server code nor its navigation meshes.
			bWithServerCode = false;
			bCompileRecast = false;
		}
	}
}
