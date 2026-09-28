using UnrealBuildTool;

public class WebOfTheCityEditorTarget : TargetRules
{
	public WebOfTheCityEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("WebOfTheCity");
	}
}
