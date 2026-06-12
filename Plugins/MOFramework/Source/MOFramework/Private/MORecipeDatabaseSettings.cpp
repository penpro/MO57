#include "MORecipeDatabaseSettings.h"
#include "MOFramework.h"
#include "MOBuildableActor.h" // TSubclassOf<AMOBuildableActor>::Get() needs the complete type

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

// Static cache members
bool UMORecipeDatabaseSettings::bCachesDirty = true;
TMap<EMOCraftingStation, TArray<FName>> UMORecipeDatabaseSettings::RecipesByStation;
TArray<FName> UMORecipeDatabaseSettings::BuildingRecipeIds;
TArray<FName> UMORecipeDatabaseSettings::CraftableRecipeIds;
TMap<FName, TArray<FName>> UMORecipeDatabaseSettings::RecipesByCategory;
TMap<FName, FMORecipeDefinitionRow> UMORecipeDatabaseSettings::ModRecipeDefinitions;

// GC roots for the mod overlay. ModRecipeDefinitions holds ROW COPIES whose
// hard refs (TSubclassOf<AMOBuildableActor> BuildableActorClass) are invisible
// to the GC — a plain static TMap is not a UPROPERTY, so once the mod's
// source table is collected the copies hold dangling UClass pointers. Every
// registered row's class is rooted here until ClearModRecipes. Rooting per
// row (not per table) also covers direct RegisterModRecipe callers.
static TArray<TStrongObjectPtr<UObject>> GModRecipeHardRefs;

UDataTable* UMORecipeDatabaseSettings::GetRecipeDefinitionsDataTable() const
{
	return RecipeDefinitionsDataTable.LoadSynchronous();
}

const FMORecipeDefinitionRow* UMORecipeDatabaseSettings::GetRecipeDefinition(FName RecipeId)
{
	if (RecipeId.IsNone())
	{
		return nullptr;
	}

	// Mod overlay wins on ID collision so modders can override base recipes.
	if (const FMORecipeDefinitionRow* ModRow = ModRecipeDefinitions.Find(RecipeId))
	{
		return ModRow;
	}

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable();
	if (!IsValid(DataTable))
	{
		return nullptr;
	}

	return DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("GetRecipeDefinition"), false);
}

bool UMORecipeDatabaseSettings::GetRecipeDefinitionBP(FName RecipeId, FMORecipeDefinitionRow& OutDefinition)
{
	OutDefinition = FMORecipeDefinitionRow();

	const FMORecipeDefinitionRow* FoundRow = GetRecipeDefinition(RecipeId);
	if (!FoundRow)
	{
		return false;
	}

	OutDefinition = *FoundRow;
	return true;
}

UTexture2D* UMORecipeDatabaseSettings::GetRecipeIcon(FName RecipeId)
{
	const FMORecipeDefinitionRow* Definition = GetRecipeDefinition(RecipeId);
	if (!Definition)
	{
		return nullptr;
	}

	if (Definition->Icon.IsNull())
	{
		return nullptr;
	}

	return Definition->Icon.LoadSynchronous();
}

FText UMORecipeDatabaseSettings::GetRecipeDisplayName(FName RecipeId)
{
	const FMORecipeDefinitionRow* Definition = GetRecipeDefinition(RecipeId);
	if (!Definition)
	{
		return FText::GetEmpty();
	}

	return Definition->DisplayName;
}

void UMORecipeDatabaseSettings::GetAllRecipeIds(TArray<FName>& OutRecipeIds)
{
	OutRecipeIds.Empty();

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (Settings)
	{
		if (UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable())
		{
			OutRecipeIds = DataTable->GetRowNames();
		}
	}

	// Append mod IDs, deduplicating against base. Mod-only IDs get added;
	// mod-overrides-base IDs were already in OutRecipeIds from the table.
	for (const auto& Pair : ModRecipeDefinitions)
	{
		OutRecipeIds.AddUnique(Pair.Key);
	}
}

void UMORecipeDatabaseSettings::GetRecipesForStation(EMOCraftingStation Station, TArray<FName>& OutRecipeIds)
{
	EnsureCachesBuilt();

	if (const TArray<FName>* Found = RecipesByStation.Find(Station))
	{
		OutRecipeIds = *Found;
	}
	else
	{
		OutRecipeIds.Empty();
	}
}

void UMORecipeDatabaseSettings::GetBuildingRecipes(TArray<FName>& OutRecipeIds)
{
#if WITH_EDITOR
	// In editor, always scan DataTable directly to pick up reimported changes.
	// Mod entries are layered on top with the same override-wins rule used
	// in the packaged BuildCaches path.
	OutRecipeIds.Empty();
	TSet<FName> Seen;

	// Mod overlay first so mod-overrides-base ID is counted via the mod row.
	for (const auto& Pair : ModRecipeDefinitions)
	{
		if (Pair.Value.bIsBuilding && !Pair.Key.IsNone())
		{
			OutRecipeIds.Add(Pair.Key);
			Seen.Add(Pair.Key);
		}
	}

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (Settings)
	{
		if (UDataTable* DataTable = Settings->GetRecipeDefinitionsDataTable())
		{
			TArray<FName> AllRecipeIds = DataTable->GetRowNames();
			OutRecipeIds.Reserve(OutRecipeIds.Num() + AllRecipeIds.Num() / 4);

			for (const FName& RecipeId : AllRecipeIds)
			{
				if (Seen.Contains(RecipeId)) continue;  // mod already covered

				const FMORecipeDefinitionRow* Recipe = DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("GetBuildingRecipes"), false);
				if (Recipe && Recipe->bIsBuilding)
				{
					OutRecipeIds.Add(RecipeId);
				}
			}
		}
	}
#else
	// In packaged builds, use cache for performance (already includes mod entries)
	EnsureCachesBuilt();
	OutRecipeIds = BuildingRecipeIds;
#endif
}

void UMORecipeDatabaseSettings::GetCraftableRecipes(TArray<FName>& OutRecipeIds)
{
	EnsureCachesBuilt();
	OutRecipeIds = CraftableRecipeIds;
}

void UMORecipeDatabaseSettings::GetRecipesByCategory(FName Category, TArray<FName>& OutRecipeIds)
{
	EnsureCachesBuilt();

	if (const TArray<FName>* Found = RecipesByCategory.Find(Category))
	{
		OutRecipeIds = *Found;
	}
	else
	{
		OutRecipeIds.Empty();
	}
}

bool UMORecipeDatabaseSettings::IsConfigured()
{
	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	if (!Settings)
	{
		return false;
	}

	return !Settings->RecipeDefinitionsDataTable.IsNull();
}

void UMORecipeDatabaseSettings::InvalidateCache()
{
	bCachesDirty = true;
	RecipesByStation.Empty();
	BuildingRecipeIds.Empty();
	CraftableRecipeIds.Empty();
	RecipesByCategory.Empty();

	UE_LOG(LogMOFramework, Log,
		TEXT("[MORecipeDatabaseSettings] Cache invalidated (mod overlay retained: %d recipes)"),
		ModRecipeDefinitions.Num());
}

void UMORecipeDatabaseSettings::EnsureCachesBuilt()
{
	if (!bCachesDirty)
	{
		return;
	}

	BuildCaches();
}

void UMORecipeDatabaseSettings::BuildCaches()
{
	// Clear existing caches
	RecipesByStation.Empty();
	BuildingRecipeIds.Empty();
	CraftableRecipeIds.Empty();
	RecipesByCategory.Empty();

	const UMORecipeDatabaseSettings* Settings = GetDefault<UMORecipeDatabaseSettings>();
	UDataTable* DataTable = Settings ? Settings->GetRecipeDefinitionsDataTable() : nullptr;

	// Visit-once set so mod-override-base doesn't double-index.
	TSet<FName> VisitedIds;

	// Local helper — indexes one row into all four caches by ID.
	auto IndexRow = [&VisitedIds](FName RecipeId, const FMORecipeDefinitionRow* Recipe)
	{
		if (!Recipe || RecipeId.IsNone()) return;
		if (VisitedIds.Contains(RecipeId)) return;  // mod already covered this ID
		VisitedIds.Add(RecipeId);

		TArray<FName>& StationRecipes = RecipesByStation.FindOrAdd(Recipe->RequiredStation);
		StationRecipes.Add(RecipeId);

		if (Recipe->bIsBuilding) { BuildingRecipeIds.Add(RecipeId); }
		else                     { CraftableRecipeIds.Add(RecipeId); }

		if (!Recipe->Category.IsNone())
		{
			TArray<FName>& CategoryRecipes = RecipesByCategory.FindOrAdd(Recipe->Category);
			CategoryRecipes.Add(RecipeId);
		}
	};

	// Mod overlay FIRST — mod wins on ID collision. The VisitedIds guard
	// stops base from re-adding the same ID below.
	for (const auto& Pair : ModRecipeDefinitions)
	{
		IndexRow(Pair.Key, &Pair.Value);
	}

	// Base table second.
	int32 BaseRecipeCount = 0;
	if (IsValid(DataTable))
	{
		TArray<FName> AllRecipeIds = DataTable->GetRowNames();
		BaseRecipeCount = AllRecipeIds.Num();

		BuildingRecipeIds.Reserve(BuildingRecipeIds.Num() + AllRecipeIds.Num() / 4);
		CraftableRecipeIds.Reserve(CraftableRecipeIds.Num() + AllRecipeIds.Num());

		for (const FName& RecipeId : AllRecipeIds)
		{
			const FMORecipeDefinitionRow* Recipe = DataTable->FindRow<FMORecipeDefinitionRow>(RecipeId, TEXT("BuildCaches"), false);
			IndexRow(RecipeId, Recipe);
		}
	}

	bCachesDirty = false;

	if (ModRecipeDefinitions.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[MORecipeDatabaseSettings] Cache built: %d base + %d mod = %d total recipes (%d buildings, %d craftable, %d stations, %d categories)"),
			BaseRecipeCount, ModRecipeDefinitions.Num(), VisitedIds.Num(),
			BuildingRecipeIds.Num(), CraftableRecipeIds.Num(),
			RecipesByStation.Num(), RecipesByCategory.Num());
	}
	else
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[MORecipeDatabaseSettings] Cache built: %d recipes, %d buildings, %d craftable, %d stations, %d categories"),
			BaseRecipeCount,
			BuildingRecipeIds.Num(), CraftableRecipeIds.Num(),
			RecipesByStation.Num(), RecipesByCategory.Num());
	}
}

// =============================================================================
// MOD OVERLAY API
// =============================================================================

void UMORecipeDatabaseSettings::RegisterModRecipe(FName RecipeId, const FMORecipeDefinitionRow& Row)
{
	if (RecipeId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MORecipeDatabaseSettings] RegisterModRecipe refused — empty RecipeId"));
		return;
	}

	const bool bWasReplaced = ModRecipeDefinitions.Contains(RecipeId);
	ModRecipeDefinitions.Add(RecipeId, Row);

	// Keep the row's hard class ref alive — the overlay map is GC-invisible
	// (see GModRecipeHardRefs above).
	if (UClass* BuildClass = Row.PlacementData.BuildableActorClass.Get())
	{
		const bool bAlreadyRooted = GModRecipeHardRefs.ContainsByPredicate(
			[BuildClass](const TStrongObjectPtr<UObject>& Rooted)
			{
				return Rooted.Get() == BuildClass;
			});
		if (!bAlreadyRooted)
		{
			GModRecipeHardRefs.Emplace(BuildClass);
		}
	}

	// Force cache rebuild so the new recipe shows up in by-station / building /
	// category lists. Lookup-by-ID already sees it instantly via the overlay
	// check at the top of GetRecipeDefinition.
	bCachesDirty = true;

	UE_LOG(LogMOFramework, Log,
		TEXT("[MORecipeDatabaseSettings] %s mod recipe '%s' (display='%s', station=%d)"),
		bWasReplaced ? TEXT("Replaced") : TEXT("Registered"),
		*RecipeId.ToString(),
		*Row.DisplayName.ToString(),
		static_cast<int32>(Row.RequiredStation));
}

int32 UMORecipeDatabaseSettings::MergeModRecipeTable(UDataTable* SourceTable)
{
	if (!IsValid(SourceTable))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MORecipeDatabaseSettings] MergeModRecipeTable: null SourceTable"));
		return 0;
	}

	if (SourceTable->GetRowStruct() != FMORecipeDefinitionRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MORecipeDatabaseSettings] MergeModRecipeTable: '%s' has row struct '%s', expected FMORecipeDefinitionRow"),
			*SourceTable->GetName(),
			SourceTable->GetRowStruct() ? *SourceTable->GetRowStruct()->GetName() : TEXT("<null>"));
		return 0;
	}

	const TMap<FName, uint8*>& RowMap = SourceTable->GetRowMap();
	int32 Merged = 0;
	int32 Skipped = 0;

	for (const auto& Pair : RowMap)
	{
		const FMORecipeDefinitionRow* Row = reinterpret_cast<const FMORecipeDefinitionRow*>(Pair.Value);
		if (!Row)
		{
			++Skipped;
			continue;
		}
		RegisterModRecipe(Pair.Key, *Row);
		++Merged;
	}

	UE_LOG(LogMOFramework, Log,
		TEXT("[MORecipeDatabaseSettings] Merged mod table '%s': %d recipes added, %d skipped (null rows)"),
		*SourceTable->GetName(), Merged, Skipped);

	return Merged;
}

void UMORecipeDatabaseSettings::ClearModRecipes()
{
	const int32 PreviousCount = ModRecipeDefinitions.Num();
	ModRecipeDefinitions.Empty();
	GModRecipeHardRefs.Reset(); // release the GC roots with the overlay
	InvalidateCache();
	UE_LOG(LogMOFramework, Log,
		TEXT("[MORecipeDatabaseSettings] Cleared %d mod recipes + invalidated cache"),
		PreviousCount);
}

int32 UMORecipeDatabaseSettings::GetModRecipeCount()
{
	return ModRecipeDefinitions.Num();
}
