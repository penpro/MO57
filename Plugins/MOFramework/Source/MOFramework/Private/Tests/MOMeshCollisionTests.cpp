// Every mesh the data tables can PLACE as a solid thing (a tree, a rock, an ore deposit) must have collision geometry the pawn can hit. A HISM instance with a blocking profile and a mesh
// that has no collision shapes (and is not "use complex as simple") is invisible to the pawn's movement: the player walks through boulders and tree trunks (found 2026-10-10: 12 of
// the 27 meshes in DT_Biomes / DT_Resources, plus the biome spawner's single NoCollision profile on top).
// Fix tool: python Tools/ue.py py --file Content/Python/fix_mesh_collision.py   Audit: Content/Python/audit_table_mesh_collision.py

#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "PhysicsEngine/BodySetup.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOResourceDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOPCGCaveSpawnerSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The pawn can hit it: simple shapes, or the render triangles are the collision. */
	bool MeshBlocksPawn(const UStaticMesh* Mesh)
	{
		const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
		return Body && (Body->AggGeom.GetElementCount() > 0 || Body->CollisionTraceFlag == CTF_UseComplexAsSimple);
	}

	bool IsSolidKind(EMOResourceType Type)
	{
		return Type == EMOResourceType::Tree || Type == EMOResourceType::Rock || Type == EMOResourceType::Ore;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOData_SolidMeshCollision,
	"MOFramework.Data.SolidMeshCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOData_SolidMeshCollision::RunTest(const FString& Parameters)
{
	if (!UMOBiomeDatabaseSettings::IsConfigured() || !UMOResourceDatabaseSettings::IsConfigured())
	{
		AddWarning(TEXT("biome / resource databases are not configured in this context; nothing to check"));
		return true;
	}

	TMap<FString, FString> Solid;   // mesh path -> who places it
	TArray<FName> BiomeIds;
	UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
	for (const FName& BiomeId : BiomeIds)
	{
		const FMOBiomeDefinitionRow* Biome = UMOBiomeDatabaseSettings::GetBiomeDefinition(BiomeId);
		for (const FMOBiomeSpeciesEntry& Species : Biome ? Biome->Species : TArray<FMOBiomeSpeciesEntry>())
		{
			const FMOResourceNodeDefinitionRow* Resource = Species.ResourceNodeId.IsNone() ? nullptr : UMOResourceDatabaseSettings::GetResourceDefinition(Species.ResourceNodeId);
			if (Resource && IsSolidKind(Resource->ResourceType) && !Species.Mesh.IsNull())
			{
				Solid.Add(Species.Mesh.ToString(), FString::Printf(TEXT("biome %s (%s)"), *BiomeId.ToString(), *Species.ResourceNodeId.ToString()));
			}
		}
	}

	// The cave scatter: rows that block the pawn -- Solid (fallen blocks), or Auto with a solid-kind resource (ore lumps, coal seams).
	if (const UDataTable* Cave = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_CaveScatter.DT_CaveScatter")); Cave && Cave->GetRowStruct() == FMOCaveScatterRow::StaticStruct())
	{
		Cave->ForeachRow<FMOCaveScatterRow>(TEXT("SolidMeshCollision"), [&Solid](const FName& Name, const FMOCaveScatterRow& Row)
		{
			const FMOResourceNodeDefinitionRow* Resource = Row.ResourceNodeId.IsNone() ? nullptr : UMOResourceDatabaseSettings::GetResourceDefinition(Row.ResourceNodeId);
			const bool bBlocks = Row.Collision == EMOSpeciesCollision::Solid || (Row.Collision == EMOSpeciesCollision::Auto && Resource && IsSolidKind(Resource->ResourceType));
			if (bBlocks && !Row.Mesh.IsNull())
			{
				Solid.Add(Row.Mesh.ToString(), FString::Printf(TEXT("cave scatter %s"), *Name.ToString()));
			}
		});
	}

	int32 Checked = 0, Missing = 0;
	for (const TPair<FString, FString>& Pair : Solid)
	{
		const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Pair.Key);
		if (!Mesh)
		{
			AddError(FString::Printf(TEXT("%s: the mesh %s does not load"), *Pair.Value, *Pair.Key));
			++Missing;
			continue;
		}
		++Checked;
		if (!MeshBlocksPawn(Mesh))
		{
			AddError(FString::Printf(TEXT("%s has no collision geometry (%s): the pawn walks through it. Fix: Content/Python/fix_mesh_collision.py"), *Pair.Key, *Pair.Value));
			++Missing;
		}
	}
	AddInfo(FString::Printf(TEXT("%d solid-kind meshes placed by the biome and cave scatter tables checked, %d without collision"), Checked, Missing));
	TestTrue(TEXT("the biome table places at least one solid-kind mesh (the check is not vacuous)"), Checked > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
