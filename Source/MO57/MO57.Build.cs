// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MO57 : ModuleRules
{
	public MO57(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MO57",
			"MO57/Variant_Platforming",
			"MO57/Variant_Platforming/Animation",
			"MO57/Variant_Combat",
			"MO57/Variant_Combat/AI",
			"MO57/Variant_Combat/Animation",
			"MO57/Variant_Combat/Gameplay",
			"MO57/Variant_Combat/Interfaces",
			"MO57/Variant_Combat/UI",
			"MO57/Variant_SideScrolling",
			"MO57/Variant_SideScrolling/AI",
			"MO57/Variant_SideScrolling/Gameplay",
			"MO57/Variant_SideScrolling/Interfaces",
			"MO57/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
