using UnrealBuildTool;

public class MOBodyShapeEditor : ModuleRules
{
	public MOBodyShapeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"MeshDescription",
			"SkeletalMeshDescription",     // the morph goes into the mesh description; the engine's build makes the morph target
			"MOBodyShape",
			"MetaHumanCharacter",          // UMetaHumanCharacter
			"MetaHumanCharacterEditor",    // the Creator's subsystem: import the body DNA, the body state
			"MetaHumanCoreTechLib",        // FMetaHumanCharacterBodyIdentity::FState: constraints, StateToDna
			"RigLogicModule",              // USkelMeshDNAUtils, FDNAToSkelMeshMap, IDNAReader
			"RigLogicLib",
		});
	}
}
