#include "MOItemSourceAnalysis.h"

#include "MOBiomeDatabaseSettings.h"
#include "MOCarcassDefinitionRow.h"
#include "MOForagingSubsystem.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOItemActions.h"
#include "MOCoverForageSettings.h"
#include "MOPCGHISMTaggerSettings.h"
#include "MOPCGItemSpawnerSettings.h"
#include "MOPCGMeshSpawnerSettings.h"
#include "MOPCGResourceSpawnerSettings.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "MOResourceDatabaseSettings.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOSkillDatabaseSettings.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "PCGGraph.h"
#include "PCGNode.h"

namespace MOItemSources
{
	/** "Trainable to any level": a skill row, or knowledge some repeatable action keeps giving XP for. */
	constexpr int32 AnyLevel = MAX_int32;

	FString StationName(EMOCraftingStation Station)
	{
		return StaticEnum<EMOCraftingStation>()->GetNameStringByValue(static_cast<int64>(Station));
	}

	FString ToolName(EMOToolType Tool)
	{
		return StaticEnum<EMOToolType>()->GetNameStringByValue(static_cast<int64>(Tool));
	}

	/** Every DataTable of RowStruct in the project (asset registry RowStructure tag; only the matches are loaded). */
	TArray<UDataTable*> FindTablesWithRowStruct(const UScriptStruct* RowStruct)
	{
		TArray<UDataTable*> Tables;
		IAssetRegistry* Registry = IAssetRegistry::Get();
		if (!Registry || !RowStruct)
		{
			return Tables;
		}
		TArray<FAssetData> Assets;
		Registry->GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(), Assets);
		const FString StructName = RowStruct->GetName();
		for (const FAssetData& Asset : Assets)
		{
			FString RowStructure;
			if (!Asset.GetTagValue(FName(TEXT("RowStructure")), RowStructure) || !RowStructure.EndsWith(StructName))
			{
				continue;
			}
			UDataTable* Table = Cast<UDataTable>(Asset.GetAsset());
			if (Table && Table->GetRowStruct() == RowStruct)
			{
				Tables.Add(Table);
			}
		}
		return Tables;
	}

	/** Item ids an MO spawner node scatters (its ItemsToSpawn entries with weight > 0). */
	void AddSpawnEntries(const TArray<FMOPCGItemSpawnEntry>& Entries, const FString& Source, TMap<FName, FString>& OutPickups)
	{
		for (const FMOPCGItemSpawnEntry& Entry : Entries)
		{
			if (!Entry.ItemId.IsNone() && Entry.GetWeight() > 0.0f && Entry.MaxQuantity > 0)
			{
				OutPickups.FindOrAdd(Entry.ItemId, Source);
			}
		}
	}

	/**
	 * Ground pickups as the PCG scatter makes them, from every project PCG graph (whether a level runs it is not checked):
	 * - the MO Mesh Spawner / MO Item Spawner nodes list their items directly (ItemsToSpawn; the MOItem_ tag comes from that);
	 * - a graph that runs the MO HISM Tagger tags the meshes its stock weighted spawners place with the item whose
	 *   WorldVisual.StaticMesh it is, so those entries map through ItemByMesh.
	 * Disabled nodes and zero-weight entries do not count; subgraphs are not followed.
	 * Also collects the DT_Resources rows an MO Resource Spawner node places (OutPlacedResources): a row it names, with a mesh to place.
	 */
	void CollectPcgPickups(const TMap<FSoftObjectPath, FName>& ItemByMesh, TMap<FName, FString>& OutPickups, TSet<FName>& OutPlacedResources)
	{
		IAssetRegistry* Registry = IAssetRegistry::Get();
		if (!Registry)
		{
			return;
		}
		TArray<FAssetData> Graphs;
		Registry->GetAssetsByClass(UPCGGraph::StaticClass()->GetClassPathName(), Graphs, /*bSearchSubClasses=*/true);
		for (const FAssetData& Asset : Graphs)
		{
			const FString Package = Asset.PackageName.ToString();
			if (Package.StartsWith(TEXT("/Engine/")) || Package.StartsWith(TEXT("/PCG/")))
			{
				continue;   // engine samples run no MO tagger
			}
			const UPCGGraph* Graph = Cast<UPCGGraph>(Asset.GetAsset());
			if (!Graph)
			{
				continue;
			}
			bool bTags = false;
			TArray<const UPCGStaticMeshSpawnerSettings*> Spawners;
			for (const UPCGNode* Node : Graph->GetNodes())
			{
				const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
				if (!Settings || !Settings->bEnabled)
				{
					continue;
				}
				const FString Source = FString::Printf(TEXT("ground pickup (%s)"), *Asset.AssetName.ToString());
				if (Settings->IsA<UMOPCGHISMTaggerSettings>())
				{
					bTags = true;
				}
				else if (const UMOPCGMeshSpawnerSettings* MeshSpawner = Cast<UMOPCGMeshSpawnerSettings>(Settings))
				{
					AddSpawnEntries(MeshSpawner->ItemsToSpawn, Source, OutPickups);
				}
				else if (const UMOPCGItemSpawnerSettings* ItemSpawner = Cast<UMOPCGItemSpawnerSettings>(Settings))
				{
					AddSpawnEntries(ItemSpawner->ItemsToSpawn, Source, OutPickups);
				}
				else if (const UMOPCGResourceSpawnerSettings* ResourceSpawner = Cast<UMOPCGResourceSpawnerSettings>(Settings))
				{
					for (const FMOPCGResourceEntry& Entry : ResourceSpawner->ResourcesToSpawn)
					{
						const FMOResourceNodeDefinitionRow* Row = ResourceSpawner->ResourceNodeDataTable && Entry.SpawnWeight > 0.0f
							? ResourceSpawner->ResourceNodeDataTable->FindRow<FMOResourceNodeDefinitionRow>(Entry.ResourceRowName, TEXT("MOItemSources"), false) : nullptr;
						if (Row && Row->MeshVariations.Num() > 0)
						{
							OutPlacedResources.Add(Entry.ResourceRowName);
						}
					}
				}
				else if (const UPCGStaticMeshSpawnerSettings* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings))
				{
					Spawners.Add(Spawner);
				}
			}
			if (!bTags)
			{
				continue;
			}
			for (const UPCGStaticMeshSpawnerSettings* Spawner : Spawners)
			{
				const UPCGMeshSelectorWeighted* Weighted = Cast<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
				if (!Weighted)
				{
					continue;
				}
				for (const FPCGMeshSelectorWeightedEntry& Entry : Weighted->MeshEntries)
				{
					if (Entry.Weight <= 0)
					{
						continue;
					}
					if (const FName* ItemId = ItemByMesh.Find(Entry.Descriptor.StaticMesh.ToSoftObjectPath()))
					{
						OutPickups.FindOrAdd(*ItemId, FString::Printf(TEXT("ground pickup (%s)"), *Asset.AssetName.ToString()));
					}
				}
			}
		}
	}
}

FMOItemSourceReport FMOItemSourceAnalysis::Analyze(UWorld* World, const FMOItemSourceOptions& Options)
{
	using namespace MOItemSources;
	FMOItemSourceReport Report;

	// ---- Items (by id, and by world mesh for the PCG pickup mapping; a mesh shared by several items maps to none) ----
	TArray<FName> ItemIds;
	UMOItemDatabaseSettings::GetAllItemIds(ItemIds);
	TMap<FName, FMOItemDefinitionRow> Items;
	TMap<FSoftObjectPath, TArray<FName>> ItemsByMesh;
	for (const FName& ItemId : ItemIds)
	{
		FMOItemDefinitionRow Def;
		if (UMOItemDatabaseSettings::GetItemDefinition(ItemId, Def))
		{
			if (!Def.WorldVisual.StaticMesh.IsNull())
			{
				ItemsByMesh.FindOrAdd(Def.WorldVisual.StaticMesh.ToSoftObjectPath()).Add(ItemId);
			}
			Items.Add(ItemId, MoveTemp(Def));
		}
	}
	TMap<FSoftObjectPath, FName> ItemByMesh;
	for (const TPair<FSoftObjectPath, TArray<FName>>& Pair : ItemsByMesh)
	{
		if (Pair.Value.Num() == 1)
		{
			ItemByMesh.Add(Pair.Key, Pair.Value[0]);   // graybox placeholders are shared -> ambiguous -> not a pickup
		}
	}

	// ---- Recipes ----
	TArray<FName> RecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(RecipeIds);
	RecipeIds.Sort(FNameLexicalLess());
	TArray<TPair<FName, const FMORecipeDefinitionRow*>> Recipes;
	for (const FName& RecipeId : RecipeIds)
	{
		if (const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId))
		{
			Recipes.Emplace(RecipeId, Recipe);
		}
	}
	Report.RecipeCount = Recipes.Num();

	// ---- Resources + the tags harvest recipes target. Only the rows something PLACES count (filtered below, once the biome palettes and the PCG
	// graphs are read): a row on paper that no biome species and no resource spawner plants is in no world -- assuming every row placed credited clay,
	// flint and ore deposits that were never there (Oct 15 2026). ----
	TArray<TPair<FName, const FMOResourceNodeDefinitionRow*>> Resources;
	TSet<FName> TargetTags;
	if (const UMOResourceDatabaseSettings* ResourceSettings = GetDefault<UMOResourceDatabaseSettings>())
	{
		if (const UDataTable* Table = ResourceSettings->GetResourceDefinitionsDataTable())
		{
			Table->ForeachRow<FMOResourceNodeDefinitionRow>(TEXT("MOItemSources"),
				[&Resources](const FName& Key, const FMOResourceNodeDefinitionRow& Row)
				{
					Resources.Emplace(Key, &Row);
				});
		}
	}

	// ---- Unconditional sources ----
	TMap<FName, FString>& Obtainable = Report.SourceOf;
	TSet<FName> ProducedByAnything;   // any producer at all, reachable or not -- separates root orphans from consequences
	auto Produce = [&Obtainable, &ProducedByAnything](FName ItemId, const FString& Source) -> bool
	{
		if (ItemId.IsNone())
		{
			return false;
		}
		ProducedByAnything.Add(ItemId);
		if (Obtainable.Contains(ItemId))
		{
			return false;
		}
		Obtainable.Add(ItemId, Source);
		return true;
	};

	for (UDataTable* Table : FindTablesWithRowStruct(FMOCarcassDefinitionRow::StaticStruct()))
	{
		Table->ForeachRow<FMOCarcassDefinitionRow>(TEXT("MOItemSources"), [&Produce](const FName& Key, const FMOCarcassDefinitionRow& Row)
		{
			for (const FMOCarcassPartEntry& Part : Row.Parts)
			{
				Produce(Part.ItemDefinitionId, FString::Printf(TEXT("carcass %s/%s"), *Key.ToString(), *Part.PartId.ToString()));
			}
		});
	}

	if (World)
	{
		if (const UMOForagingSubsystem* Foraging = World->GetSubsystem<UMOForagingSubsystem>())
		{
			for (const FMODigDropEntry& Entry : Foraging->DigDropTable)
			{
				Produce(Entry.ItemId, TEXT("digging"));
			}
		}
	}
	{
		// A biome's own dig table (UMOForagingSubsystem::GetDigTableAt): every biome is assumed to be somewhere in the world, as every resource row is.
		TArray<FName> DigBiomeIds;
		UMOBiomeDatabaseSettings::GetAllBiomeIds(DigBiomeIds);
		for (const FName& BiomeId : DigBiomeIds)
		{
			if (const FMOBiomeDefinitionRow* Biome = UMOBiomeDatabaseSettings::GetBiomeDefinition(BiomeId))
			{
				for (const FMODigDropEntry& Entry : Biome->DigYields)
				{
					Produce(Entry.ItemId, FString::Printf(TEXT("digging in %s"), *BiomeId.ToString()));
				}
			}
		}
	}

	TMap<FName, FString> Pickups;
	TSet<FName> PlacedResources;
	CollectPcgPickups(ItemByMesh, Pickups, PlacedResources);
	TArray<FName> BiomeIds;
	UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
	for (const FName& BiomeId : BiomeIds)
	{
		const FMOBiomeDefinitionRow* Biome = UMOBiomeDatabaseSettings::GetBiomeDefinition(BiomeId);
		if (!Biome)
		{
			continue;
		}
		for (const TArray<FMOBiomeSpeciesEntry>* List : { &Biome->Species, &Biome->WaterEdgeSpecies })
		{
			for (const FMOBiomeSpeciesEntry& Species : *List)
			{
				TargetTags.Add(Species.HISMTag);   // decorative species carry their HISMTag
				// Ground cover that can be gathered (UMOCoverForageSettings): the shore variant where it is a water's-edge species.
				if (Species.HISMTag.ToString().StartsWith(TEXT("MOBiomeCover_")))
				{
					if (const FMOCoverForage* Cover = UMOCoverForageSettings::FindForMeshName(Species.Mesh.ToSoftObjectPath().GetAssetName()))
					{
						const bool bShore = List == &Biome->WaterEdgeSpecies;
						for (const FMODigDropEntry& Yield : Cover->YieldsFor(bShore))
						{
							Pickups.FindOrAdd(Yield.ItemId, FString::Printf(TEXT("gathering cover (%s in %s)"), *Cover->LabelFor(bShore).ToString(), *BiomeId.ToString()));
						}
					}
				}
				if (!Species.ResourceNodeId.IsNone())
				{
					PlacedResources.Add(Species.ResourceNodeId);
				}
				FString Tag = Species.HISMTag.ToString();
				if (Tag.RemoveFromStart(TEXT("MOItem_")) && Items.Contains(FName(*Tag)))
				{
					Pickups.FindOrAdd(FName(*Tag), FString::Printf(TEXT("biome species (%s)"), *BiomeId.ToString()));
				}
			}
		}
	}
	for (const TPair<FName, FString>& Pickup : Pickups)
	{
		Produce(Pickup.Key, Pickup.Value);
		Report.PickupItems.Add(Pickup.Key);
	}
	Report.PickupItems.Sort(FNameLexicalLess());

	// The resources that are really in the world; the rest are reported, not counted.
	Resources.RemoveAll([&PlacedResources, &Report](const TPair<FName, const FMOResourceNodeDefinitionRow*>& Resource)
	{
		if (PlacedResources.Contains(Resource.Key))
		{
			return false;
		}
		Report.UnplacedResources.Add(Resource.Key);
		return true;
	});
	Report.UnplacedResources.Sort(FNameLexicalLess());
	for (const TPair<FName, const FMOResourceNodeDefinitionRow*>& Resource : Resources)
	{
		TargetTags.Append(Resource.Value->GetAllTags());
	}

	// Producers that exist on paper whatever their gates -- for orphan detection only.
	for (const TPair<FName, const FMOResourceNodeDefinitionRow*>& Resource : Resources)
	{
		ProducedByAnything.Append(Resource.Value->YieldsItems);
		for (const FMOResourceHarvestAction& Action : Resource.Value->HarvestActions)
		{
			ProducedByAnything.Append(Action.YieldsItems);
		}
	}
	for (const TPair<FName, const FMORecipeDefinitionRow*>& Recipe : Recipes)
	{
		for (const FMORecipeOutput& Output : Recipe.Value->Outputs)
		{
			ProducedByAnything.Add(Output.ItemDefinitionId);
		}
	}

	// ---- Fixpoint ----
	TMap<FName, int32> KnowledgeLevel;          // non-skill id -> highest level some reachable source trains it to
	TMap<EMOToolType, float> BestTool;          // tool type -> best effectiveness among obtainable tools
	TSet<EMOCraftingStation> Stations = { EMOCraftingStation::None };
	TSet<FName> ReachableRecipes;
	TSet<FName> ItemsApplied;                    // obtainable items whose grants / tool capabilities are already counted

	auto IsSkill = [](FName Id) { return UMOSkillDatabaseSettings::GetSkillDefinition(Id) != nullptr; };
	auto KnowledgeOk = [&KnowledgeLevel, &IsSkill, &Options](FName Id, int32 Level)
	{
		return Options.bIgnoreKnowledge || Id.IsNone() || Level <= 0 || IsSkill(Id) || KnowledgeLevel.FindRef(Id) >= Level;
	};
	auto Train = [&KnowledgeLevel, &IsSkill](FName Id, int32 Level) -> bool
	{
		if (Id.IsNone() || IsSkill(Id))
		{
			return false;
		}
		int32& Current = KnowledgeLevel.FindOrAdd(Id, 0);
		if (Level <= Current)
		{
			return false;
		}
		Current = Level;
		return true;
	};
	auto ToolOk = [&BestTool](EMOToolType Type, float MinQuality)
	{
		if (Type == EMOToolType::None)
		{
			return true;
		}
		const float* Best = BestTool.Find(Type);
		return Best && *Best >= MinQuality;
	};

	// The gates UMOCraftingSubsystem::IsRecipeAvailable / CanCraftRecipe and the harvest path enforce, in that order.
	// Empty = performable with what is obtainable so far.
	auto FirstBlockingGate = [&](const FMORecipeDefinitionRow& Recipe) -> FString
	{
		if (Recipe.bRequiresDiscovery && !Options.bIgnoreDiscovery)
		{
			const bool bByKnowledge = !Recipe.DiscoveryKnowledgeId.IsNone() && KnowledgeOk(Recipe.DiscoveryKnowledgeId, Recipe.DiscoveryKnowledgeLevel);
			const bool bBySkill = Recipe.DiscoverySkillLevel > 0 && !Recipe.RequiredSkillId.IsNone() && KnowledgeOk(Recipe.RequiredSkillId, Recipe.DiscoverySkillLevel);
			if (!bByKnowledge && !bBySkill)
			{
				if (Recipe.DiscoveryKnowledgeId.IsNone() && Recipe.DiscoverySkillLevel <= 0)
				{
					return TEXT("requires discovery but names no knowledge or skill level to discover it by");
				}
				return FString::Printf(TEXT("discovery: %s level %d is never reached (best %d)"), *Recipe.DiscoveryKnowledgeId.ToString(),
					Recipe.DiscoveryKnowledgeLevel, KnowledgeLevel.FindRef(Recipe.DiscoveryKnowledgeId));
			}
		}
		for (const FName& Knowledge : Recipe.RequiredKnowledge)
		{
			if (!KnowledgeOk(Knowledge, 1) && !ReachableRecipes.Contains(Knowledge))
			{
				return FString::Printf(TEXT("required knowledge %s is never learned"), *Knowledge.ToString());
			}
		}
		if (!KnowledgeOk(Recipe.RequiredSkillId, Recipe.RequiredSkillLevel))
		{
			return FString::Printf(TEXT("needs %s level %d, which is not a skill and nothing trains it"), *Recipe.RequiredSkillId.ToString(), Recipe.RequiredSkillLevel);
		}
		if (!Stations.Contains(Recipe.RequiredStation))
		{
			return FString::Printf(TEXT("station %s is never available"), *StationName(Recipe.RequiredStation));
		}
		for (const FMOToolRequirement& Tool : Recipe.RequiredTools)
		{
			if (Tool.bIsRequired && !ToolOk(Tool.ToolType, Tool.MinQuality))
			{
				return FString::Printf(TEXT("tool %s (quality %.2f) is never obtainable"), *ToolName(Tool.ToolType), Tool.MinQuality);
			}
		}
		for (const FMORecipeIngredient& Ingredient : Recipe.Ingredients)
		{
			if (!Ingredient.ItemDefinitionId.IsNone() && !Obtainable.Contains(Ingredient.ItemDefinitionId))
			{
				return FString::Printf(TEXT("ingredient %s is never obtainable"), *Ingredient.ItemDefinitionId.ToString());
			}
		}
		for (const FMOBuildPart& Part : Recipe.BuildParts)
		{
			if (!Part.ItemDefinitionId.IsNone() && !Obtainable.Contains(Part.ItemDefinitionId))
			{
				return FString::Printf(TEXT("build part %s is never obtainable"), *Part.ItemDefinitionId.ToString());
			}
		}
		if (Recipe.bIsHarvestRecipe && !Recipe.RequiredTargetTag.IsNone() && !TargetTags.Contains(Recipe.RequiredTargetTag))
		{
			return FString::Printf(TEXT("harvest target tag %s is carried by no resource or species"), *Recipe.RequiredTargetTag.ToString());
		}
		return FString();
	};
	auto StationFuelled = [&Obtainable](const FMORecipeDefinitionRow& Building)
	{
		if (!Building.bRequiresFuel || Building.AcceptedFuelItems.Num() == 0)
		{
			return true;
		}
		for (const FName& Fuel : Building.AcceptedFuelItems)
		{
			if (Obtainable.Contains(Fuel))
			{
				return true;
			}
		}
		return false;
	};

	// Breadth-first rounds: a recipe whose gates are met in a round produces at the END of it, so a tier is a step count (the reachable
	// set is the same fixpoint either way).
	for (const TPair<FName, FString>& Item : Obtainable)
	{
		Report.ItemTier.Add(Item.Key, 0);
	}
	int32 Round = 0;
	for (bool bChanged = true; bChanged; )
	{
		bChanged = false;
		++Round;
		TArray<TPair<FName, const FMORecipeDefinitionRow*>> MetThisRound;

		// Obtainable items teach (inspection grants, up to each grant's MaxLevel) and work as tools.
		for (const TPair<FName, FString>& Item : Obtainable)
		{
			if (ItemsApplied.Contains(Item.Key))
			{
				continue;
			}
			ItemsApplied.Add(Item.Key);
			const FMOItemDefinitionRow* Def = Items.Find(Item.Key);
			if (!Def)
			{
				continue;
			}
			for (const FMOInspectionGrant& Grant : Def->Inspection.Grants)
			{
				if (Grant.XPAmount > 0.0f)
				{
					bChanged |= Train(Grant.Id, Grant.MaxLevel > 0 ? Grant.MaxLevel : AnyLevel);
				}
			}
			if (Def->bIsTool)
			{
				for (const FMOToolCapability& Capability : Def->ToolCapabilities)
				{
					const float Effectiveness = Def->GetToolEffectiveness(Capability.ToolType);
					float& Best = BestTool.FindOrAdd(Capability.ToolType, 0.0f);
					if (Effectiveness > Best)
					{
						Best = Effectiveness;
						bChanged = true;
					}
				}
			}
		}

		// Resource harvest actions: knowledge-gated, and tool-gated only when the action cannot be done bare-handed.
		for (const TPair<FName, const FMOResourceNodeDefinitionRow*>& Resource : Resources)
		{
			for (const FName& Yield : Resource.Value->YieldsItems)
			{
				bChanged |= Produce(Yield, FString::Printf(TEXT("resource %s"), *Resource.Key.ToString()));
			}
			for (const FMOResourceHarvestAction& Action : Resource.Value->HarvestActions)
			{
				const bool bToolOk = Action.CanPerformWithoutTool() || ToolOk(Action.RequiredToolType, Action.MinToolQuality);
				if (!bToolOk || !KnowledgeOk(Action.RequiredKnowledgeId, 1))
				{
					continue;
				}
				for (const FName& Yield : Action.YieldsItems)
				{
					bChanged |= Produce(Yield, FString::Printf(TEXT("resource %s/%s"), *Resource.Key.ToString(), *Action.ActionId.ToString()));
				}
				if (Action.SkillXPReward > 0.0f)
				{
					bChanged |= Train(Action.SkillId, AnyLevel);
				}
			}
		}

		// Water as a recipe ingredient (a liquid measure, FMOItemDefinitionRow::LiquidMeasureMl; UMOInventoryComponent::CountIngredient): any container
		// filled at water gives untreated water; safe water also needs boiling -- a vessel that goes over a fire, or stones heated in one (FMOItemActions).
		for (const TPair<FName, FMOItemDefinitionRow>& Measure : Items)
		{
			if (Measure.Value.LiquidMeasureMl <= 0.0f || Obtainable.Contains(Measure.Key))
			{
				continue;
			}
			bool bContainer = false;
			bool bHeatable = false;
			bool bStones = false;
			for (const TPair<FName, FString>& Item : Obtainable)
			{
				const FMOItemDefinitionRow* Def = Items.Find(Item.Key);
				bContainer |= Def && Def->LiquidCapacityMl > 0.0f;
				bHeatable |= Def && Def->LiquidCapacityMl > 0.0f && Def->bHeatableOverFire;
				bStones |= FMOItemActions::IsBoilingStone(Item.Key);
			}
			const bool bFire = Stations.Contains(EMOCraftingStation::Campfire);
			if (bContainer && (!Measure.Value.bMeasureNeedsSafeWater || (bFire && (bHeatable || bStones))))
			{
				bChanged |= Produce(Measure.Key, Measure.Value.bMeasureNeedsSafeWater ? TEXT("water boiled in a container") : TEXT("a container filled at water"));
			}
		}

		// Recipes whose every gate is now met.
		for (const TPair<FName, const FMORecipeDefinitionRow*>& Pair : Recipes)
		{
			const FMORecipeDefinitionRow& Recipe = *Pair.Value;
			if (!ReachableRecipes.Contains(Pair.Key))
			{
				if (!FirstBlockingGate(Recipe).IsEmpty())
				{
					continue;
				}
				MetThisRound.Add(Pair);
			}
			// A built station works once it can be fuelled (fuel may become obtainable after the building does).
			if (Recipe.bIsBuilding && Recipe.ProvidedStationType != EMOCraftingStation::None
				&& !Stations.Contains(Recipe.ProvidedStationType) && StationFuelled(Recipe))
			{
				Stations.Add(Recipe.ProvidedStationType);
				bChanged = true;
			}
		}
		for (const TPair<FName, const FMORecipeDefinitionRow*>& Met : MetThisRound)
		{
			const FName RecipeId = Met.Key;
			const FMORecipeDefinitionRow& Recipe = *Met.Value;
			ReachableRecipes.Add(RecipeId);
			Report.RecipeTier.Add(RecipeId, Round);
			bChanged = true;
			for (const FMORecipeOutput& Output : Recipe.Outputs)
			{
				Produce(Output.ItemDefinitionId, FString::Printf(TEXT("recipe %s"), *RecipeId.ToString()));
			}
			if (Recipe.SkillXPReward > 0.0f)
			{
				Train(Recipe.RequiredSkillId, AnyLevel);
			}
		}
		for (const TPair<FName, FString>& Item : Obtainable)
		{
			if (!Report.ItemTier.Contains(Item.Key))
			{
				Report.ItemTier.Add(Item.Key, Round);
			}
		}
	}

	// ---- Report ----
	Report.ReachableRecipeCount = ReachableRecipes.Num();
	TSet<FName> Consumed;
	TSet<FName> KnowledgeNeeded;
	TSet<FName> KnowledgeTrainers;   // every id anything could ever give XP / a grant for
	for (const TPair<FName, FMOItemDefinitionRow>& Item : Items)
	{
		for (const FMOInspectionGrant& Grant : Item.Value.Inspection.Grants)
		{
			KnowledgeTrainers.Add(Grant.Id);
		}
	}
	for (const TPair<FName, const FMOResourceNodeDefinitionRow*>& Resource : Resources)
	{
		for (const FMOResourceHarvestAction& Action : Resource.Value->HarvestActions)
		{
			KnowledgeTrainers.Add(Action.SkillId);
			KnowledgeNeeded.Add(Action.RequiredKnowledgeId);
		}
	}
	for (const TPair<FName, const FMORecipeDefinitionRow*>& Pair : Recipes)
	{
		const FMORecipeDefinitionRow& Recipe = *Pair.Value;
		for (const FMORecipeIngredient& Ingredient : Recipe.Ingredients)
		{
			Consumed.Add(Ingredient.ItemDefinitionId);
		}
		for (const FMOBuildPart& Part : Recipe.BuildParts)
		{
			Consumed.Add(Part.ItemDefinitionId);
		}
		KnowledgeTrainers.Add(Recipe.RequiredSkillId);
		KnowledgeNeeded.Add(Recipe.DiscoveryKnowledgeId);
		KnowledgeNeeded.Append(Recipe.RequiredKnowledge);
		if (!ReachableRecipes.Contains(Pair.Key))
		{
			Report.BlockedRecipes.Add({ Pair.Key, FirstBlockingGate(Recipe) });
		}
	}
	for (const FName& ItemId : Consumed)
	{
		if (!ItemId.IsNone() && !ProducedByAnything.Contains(ItemId))
		{
			Report.OrphanItems.Add(ItemId);
		}
	}
	for (const FName& Knowledge : KnowledgeNeeded)
	{
		if (!Knowledge.IsNone() && !IsSkill(Knowledge) && !KnowledgeTrainers.Contains(Knowledge) && !UMORecipeDatabaseSettings::GetRecipeDefinition(Knowledge))
		{
			Report.OrphanKnowledge.Add(Knowledge);
		}
	}
	Report.OrphanItems.Sort(FNameLexicalLess());
	Report.OrphanKnowledge.Sort(FNameLexicalLess());
	return Report;
}
