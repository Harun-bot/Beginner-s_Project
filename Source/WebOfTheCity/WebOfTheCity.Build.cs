using UnrealBuildTool;

public class WebOfTheCity : ModuleRules
{
	public WebOfTheCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		// RenderCore/RHI: per-frame game/render/GPU timings for the perf harness (Perf/PerfRouteRunner).
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI"
		});
	}
}
