using UnrealBuildTool;

public class Gamebeast : ModuleRules
{
	public Gamebeast(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			// FGamebeastProperties wraps an FJsonObject, so game modules see Json types.
			"Json",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"JsonUtilities",
			// UGeneralProjectSettings (project version, stamped on caches).
			"EngineSettings",
		});
	}
}
