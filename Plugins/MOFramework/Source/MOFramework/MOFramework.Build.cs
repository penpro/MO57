// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MOFramework : ModuleRules
{
	public MOFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
