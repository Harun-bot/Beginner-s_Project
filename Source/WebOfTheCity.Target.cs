using UnrealBuildTool;

public class WebOfTheCityTarget : TargetRules
{
	public WebOfTheCityTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("WebOfTheCity");
	}
}
