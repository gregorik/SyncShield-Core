// Copyright (c) 2026 GregOrigin. All Rights Reserved.
using UnrealBuildTool;

public class SyncShield : ModuleRules
{
	public SyncShield(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Requirement for VS 2026 / MSVC v144+
		CppStandard = CppStandardVersion.Cpp20; 

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
			"EditorStyle",
			"ToolMenus"
		});
	}
}

