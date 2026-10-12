using UnrealBuildTool;

/// <summary>
/// The engine-agnostic core. Its code uses the C++ standard library only and also builds without
/// Unreal for its tests; the Unreal folder holds the little glue that makes it an Unreal module.
/// </summary>
public class WeltallCore : ModuleRules
{
	public WeltallCore(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		PCHUsage = PCHUsageMode.NoPCHs;
		// The core's files keep their helpers in anonymous namespaces, which unity files would merge.
		bUseUnity = false;
		PrivateDependencyModuleNames.Add("Core");

		// WELTALLCORE_API expands to DLLEXPORT, which only Unreal's headers define; the core's own
		// files include none, so the module defines it exactly as those headers do.
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PrivateDefinitions.Add("DLLEXPORT=__declspec(dllexport)");
		}
		else
		{
			PrivateDefinitions.Add("DLLEXPORT=__attribute__((visibility(\"default\")))");
		}
	}
}
