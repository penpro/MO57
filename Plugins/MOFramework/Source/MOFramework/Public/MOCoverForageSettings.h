// What gathering a clump of ground cover gives: dry grass from a tussock, reeds where the tall grass stands at the water's edge, bracken, a few loose stones,
// a seedling pulled up for a green stick, a hazel rod. Wes (Oct 15 2026): "every world thing gets an action".
//
// Cover is planted as decorative instances (the biome spawner's MOBiomeCover_* tags, the native grass spawner's "grass"; no collision), many thousand per hectare, so it cannot be a
// DT_Resources node with a body per instance. It is matched by its MESH instead -- a name fragment covers every variant (SM_vlkhcbxia_VarA..F) -- so one entry
// serves every biome that plants it. Read by UMOForagingSubsystem::FindGatherableCover (the ground menu on the client, the timed action on the server, a
// survivor alike) and by the progression analysis. Defaults are set in the constructor; DefaultGame.ini can override the list.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MODigDropEntry.h"
#include "MOItemDefinitionRow.h"
#include "MOCoverForageSettings.generated.h"

class UStaticMesh;
class UMOInventoryComponent;

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOCoverForage
{
	GENERATED_BODY()

	/** A fragment of the cover mesh's name: "vlkhcbxia" matches SM_vlkhcbxia_VarA .. VarF. First matching entry wins. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	FString MeshNameContains;

	/** Other mesh families that are the SAME cover (both grass meshes): one entry, so the forage menu offers the gathering once. Two entries
	 *  with one label listed "Gather dry grass" twice wherever both grasses grew (Oct 18 2026). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	TArray<FString> MoreMeshNamesContaining;

	bool MatchesMeshName(const FString& MeshName) const
	{
		return (!MeshNameContains.IsEmpty() && MeshName.Contains(MeshNameContains))
			|| MoreMeshNamesContaining.ContainsByPredicate([&MeshName](const FString& Fragment) { return !Fragment.IsEmpty() && MeshName.Contains(Fragment); });
	}

	/** The menu's action ("Gather dry grass"). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	FText Label;

	/** What one clump gives: one roll per entry (chance, quantity range, minimum Foraging level). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	TArray<FMODigDropEntry> Yields;

	/** At the water's edge (the biome spawner's water-edge band: FMOPCGWaterMask::ShoreFactor > 0) this cover is something else -- the tall grass there is
	 *  reed. Empty = the same as anywhere. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	FText ShoreLabel;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	TArray<FMODigDropEntry> ShoreYields;

	/** How long gathering one clump takes (game seconds, on the pawn's timed action: stand still). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover", meta=(ClampMin="0.5"))
	float Seconds = 5.0f;

	/** The clump is taken (pulled up, cut down): the instance goes, through the world removal ledger (it stays gone, in co-op and in saves). False for
	 *  something worked but left standing: a dead tree's branches, an outcrop's stone. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	bool bRemovesInstance = true;

	/** A tool the work needs (a hammer to break stone from an outcrop); None = bare hands. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover")
	EMOToolType RequiredTool = EMOToolType::None;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Cover", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinToolQuality = 0.0f;

	/** Whether the bearer of Inventory can do it (carries the tool at its quality); OutReason says what is missing. */
	bool CanBeWorkedBy(const UMOInventoryComponent* Inventory, FText* OutReason = nullptr) const;

	bool HasShoreVariant() const { return ShoreYields.Num() > 0; }
	const TArray<FMODigDropEntry>& YieldsFor(bool bAtShore) const { return bAtShore && HasShoreVariant() ? ShoreYields : Yields; }
	const FText& LabelFor(bool bAtShore) const { return bAtShore && HasShoreVariant() && !ShoreLabel.IsEmpty() ? ShoreLabel : Label; }
};

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Cover Foraging"))
class MOFRAMEWORK_API UMOCoverForageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMOCoverForageSettings();

	UPROPERTY(Config, EditAnywhere, Category="Cover")
	TArray<FMOCoverForage> Entries;

	/** The entry for a cover mesh name (first entry whose fragment it contains), or null: nothing to gather from it. */
	static const FMOCoverForage* FindForMeshName(const FString& MeshName);

	static const FMOCoverForage* FindForMesh(const UStaticMesh* Mesh);

	/** The index of that entry (the same on every machine: it is how a client names the kind of cover it asks the server to gather), or INDEX_NONE. */
	static int32 FindIndexForMeshName(const FString& MeshName);

	static const FMOCoverForage* GetEntry(int32 Index);

	virtual FName GetCategoryName() const override { return TEXT("MO57"); }
};
