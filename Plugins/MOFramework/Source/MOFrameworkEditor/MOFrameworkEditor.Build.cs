// MOFrameworkEditor - Editor toolbox and utilities

using UnrealBuildTool;

public class MOFrameworkEditor : ModuleRules
{
	public MOFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"MOFramework",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"ToolMenus",
				"EditorStyle",
				"Projects",
				"UMG",
				"UMGEditor",
				"Blutility",
				"EditorScriptingUtilities",
				"PythonScriptPlugin",
			}
		);
	}
}
