// Copyright (c) 2026 GregOrigin. All Rights Reserved.
using UnrealBuildTool;

public class SyncShield : ModuleRules
{
	public SyncShield(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"CoreUObject",
			"Engine",
			"Json",
			"Slate",
			"SlateCore",
			"Settings",
			"SourceControl",
			"SourceControlWindows",
			"UnrealEd",
			"AssetTools",
			"ToolMenus"
		});
	}
}
