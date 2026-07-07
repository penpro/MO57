// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Reflection;
using UnrealBuildTool;

public class MOFramework : ModuleRules
{
	public MOFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// =====================================================================
		// PreBuildStep: refresh MOBuildInfoGenerated.h with current git data
		// so FMOBuildInfo / the main menu version label match the build.
		//
		// PreBuildSteps lives on TargetRules, not ModuleRules — we receive a
		// ReadOnlyTargetRules and have to reach the inner TargetRules via
		// reflection. Same trick VoxelCore.Build.cs uses (UE's officially
		// supported "do something at build time from a plugin" pattern).
		//
		// The script is idempotent and degrades gracefully if git is missing
		// (writes "unknown" sentinels that the C++ side handles). We invoke
		// it on Windows only — other platforms still compile against the
		// committed stub header.
		// =====================================================================
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// ModuleDirectory = .../Plugins/MOFramework/Source/MOFramework
			// Project root    = ../../../../  (four levels up)
			string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
			string ScriptPath  = Path.Combine(ProjectRoot, "Tools", "Update-BuildInfo.ps1");

			if (File.Exists(ScriptPath))
			{
				FieldInfo InnerField = typeof(ReadOnlyTargetRules).GetField("Inner",
					BindingFlags.NonPublic | BindingFlags.Instance);
				TargetRules MutableRules = InnerField != null
					? InnerField.GetValue(Target) as TargetRules
					: null;

				if (MutableRules != null)
				{
					string Command = string.Format(
						"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"{0}\"",
						ScriptPath);

					if (!MutableRules.PreBuildSteps.Contains(Command))
					{
						MutableRules.PreBuildSteps.Add(Command);
					}
				}
			}
		}
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"MOFrameworkCore",          // C1 split: types/interfaces/rows layer
				"Core",
				"UMG",
				"InputCore",
				"EnhancedInput",
				"CommonUI",
				"CommonInput",
				"GameplayTags",
				"AIModule",
				"NavigationSystem",
				"GameplayTasks",
				"Niagara",
				"PCG",                      // Procedural Content Generation
				"Voxel",                    // Voxel plugin - terrain sculpting
				"VoxelCore",                // Voxel plugin - core types
				"VoxelGraph",               // Voxel plugin - graph types (FVoxelExposedSeed)
				"VoxelPCG",                 // Voxel plugin - PCG point nodes
				"ProceduralMeshComponent",  // Water mesh generation
				"HairStrandsCore"           // MetaHuman groom/hair components
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"EngineSettings",  // UGeneralProjectSettings for build info / project version
				"Slate",
				"SlateCore",
				"NetCore",     // Required for FastArraySerializer + push model symbols
				"Networking",  // Recommended when you are doing replication-heavy work
				"DeveloperSettings", // Required for UDeveloperSettings (MOItemDatabaseSettings)
				"ImageWrapper", // Required for PNG encoding/decoding (save thumbnails)
				"RenderCore",   // Required for viewport pixel reading
				"Media",        // Required for UMediaPlayer (intro video)
				"MediaAssets",  // Required for UMediaSource, UMediaTexture
				"AudioMixer",   // Required for UMediaSoundComponent (inherits USynthComponent)
				"MoviePlayer"   // Required for loading screen during level transitions
			}
			);

		// Editor-only dependencies
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",        // Thumbnail rendering, asset tools, FBlueprintEditorUtils
					"UMGEditor",       // UWidgetBlueprint access for widget editor utilities
					"ContentBrowser",  // Content browser selection
					"AssetTools",      // Asset creation utilities
					"ToolMenus"        // Context menu registration
				}
			);
		}
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
