using UnrealBuildTool;

public class WeltallEditorTarget : TargetRules
{
	public WeltallEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new[] { "Weltall", "WeltallEditor" });
	}
}
