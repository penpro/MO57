#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MOResourceNodeDefinitionRow.generated.h"

/**
 * Resource type enum for specialized spawner behavior and tag generation.
 * Determines base behavior and auto-generates "MOResource_{Type}" tags.
 */
UENUM(BlueprintType)
enum class EMOResourceType : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	Tree UMETA(DisplayName = "Tree"),
	Bush UMETA(DisplayName = "Bush"),
	Rock UMETA(DisplayName = "Rock"),
	Ore UMETA(DisplayName = "Ore Deposit"),
	Plant UMETA(DisplayName = "Plant/Herb")
};

/**
 * A single mesh variation for a resource node.
 * Allows multiple visual representations for the same resource type.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOResourceMeshVariation
{
	GENERATED_BODY()

	/** The static mesh for this variation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Weight for random selection among variations (higher = more common). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode", meta=(ClampMin="0.01"))
	float Weight = 1.0f;

	/** Optional material override for this variation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;
};

/**
 * A harvest action that can be performed on a resource node.
 * Each action specifies what items are yielded, what tool is required,
 * and whether the action destroys the resource.
 *
 * Example actions for a tree:
 * - "Gather Sticks": Yields Stick, no tool required, keeps resource
 * - "Strip Bark": Yields Bark, no tool required, keeps resource
 * - "Chop Down": Yields Log, requires Axe, destroys resource
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOResourceHarvestAction
{
	GENERATED_BODY()

	/**
	 * Identifier for this action, used for tag generation.
	 * Example: "GatherSticks", "StripBark", "ChopDown", "Mine"
	 * Auto-generates tag "Action_{ActionId}" for recipe matching.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest")
	FName ActionId = NAME_None;

	/** Display name shown in context menu (e.g., "Gather Sticks", "Chop Down"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest")
	FText DisplayName;

	/**
	 * Items yielded by this action.
	 * Each entry auto-generates a "Gives_{ItemId}" tag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest")
	TArray<FName> YieldsItems;

	// ============================================================================
	// TOOL REQUIREMENT
	// ============================================================================

	/**
	 * Tool type required for this action (e.g., "Axe", "Pickaxe", "Hammer").
	 * Leave as NAME_None for actions that don't require a tool.
	 * Matches against FMOItemDefinitionRow::ToolType.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Tool")
	FName RequiredToolType = NAME_None;

	/**
	 * If true, the tool is absolutely required - action cannot be performed without it.
	 * If false, action can be done without tool but with penalties (slower, worse quality).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Tool")
	bool bToolRequired = true;

	/**
	 * Minimum tool quality required (0 = any quality).
	 * Higher quality tools may provide bonuses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Tool", meta=(ClampMin="0.0"))
	float MinToolQuality = 0.0f;

	/**
	 * When tool is optional (bToolRequired=false) but missing, multiply action time by this value.
	 * Example: Mining with hands instead of pickaxe takes 10x longer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Tool", meta=(ClampMin="1.0", EditCondition="!bToolRequired"))
	float MissingToolTimeMultiplier = 3.0f;

	// ============================================================================
	// BEHAVIOR
	// ============================================================================

	/**
	 * If true, performing this action destroys/removes the resource node.
	 * Example: Chopping down a tree destroys it. Gathering sticks does not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Behavior")
	bool bDestroysResource = false;

	/**
	 * Base time in seconds to complete this action.
	 * Modified by tool quality and skill level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Behavior", meta=(ClampMin="0.1"))
	float BaseActionTime = 3.0f;

	/**
	 * Skill ID that affects this action (speed, quality bonuses).
	 * Example: "Woodcutting" for chopping trees, "Mining" for ore.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Behavior")
	FName SkillId = NAME_None;

	/**
	 * XP awarded to SkillId when this action is completed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest|Behavior", meta=(ClampMin="0.0"))
	float SkillXPReward = 5.0f;

	// ============================================================================
	// HELPERS
	// ============================================================================

	/** Returns true if this action requires a tool. */
	bool RequiresTool() const { return !RequiredToolType.IsNone(); }

	/** Returns true if this action can be performed without a tool (optionally with penalties). */
	bool CanPerformWithoutTool() const { return RequiredToolType.IsNone() || !bToolRequired; }

	/** Get all "Gives_{ItemId}" tags for this action. */
	TArray<FName> GetYieldTags() const
	{
		TArray<FName> Tags;
		for (const FName& ItemId : YieldsItems)
		{
			if (!ItemId.IsNone())
			{
				Tags.Add(FName(*FString::Printf(TEXT("Gives_%s"), *ItemId.ToString())));
			}
		}
		return Tags;
	}

	/** Get the action tag for recipe matching (e.g., "Action_ChopDown"). */
	FName GetActionTag() const
	{
		if (ActionId.IsNone())
		{
			return NAME_None;
		}
		return FName(*FString::Printf(TEXT("Action_%s"), *ActionId.ToString()));
	}

	/** Get the required tool tag (e.g., "RequiresTool_Axe"). */
	FName GetToolRequirementTag() const
	{
		if (RequiredToolType.IsNone())
		{
			return FName("RequiresTool_None");
		}
		return FName(*FString::Printf(TEXT("RequiresTool_%s"), *RequiredToolType.ToString()));
	}
};

/**
 * DataTable row that defines a harvestable resource node (tree, rock, ore deposit, etc.).
 * The row name is the canonical ResourceNodeId (example: "BlackAlder", "IronOreDeposit").
 *
 * This is the single source of truth for resource node definitions.
 * PCG spawners select resources via FDataTableRowHandle dropdown.
 * All tags are auto-generated from this definition.
 *
 * Auto-generated tags:
 * - "Name {DisplayName}" - for harvest context menu display
 * - "MOResource_{ResourceType}" - type classification (Tree, Rock, etc.)
 * - "Gives_{ItemId}" - for each entry in YieldsItems array
 * - All entries in ResourceTags array
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOResourceNodeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// ============================================================================
	// IDENTITY
	// ============================================================================

	/** Display name shown in harvest context menu (e.g., "Black Alder", "Iron Ore"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Identity")
	FText DisplayName;

	/** Optional description for tooltips/UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Identity", meta=(MultiLine=true))
	FText Description;

	/** Resource type classification. Auto-generates "MOResource_{Type}" tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Identity")
	EMOResourceType ResourceType = EMOResourceType::Generic;

	// ============================================================================
	// HARVEST ACTIONS
	// ============================================================================

	/**
	 * Harvest actions available for this resource.
	 * Each action defines yields, tool requirements, and behavior.
	 *
	 * Example for a tree:
	 * - Action: "GatherSticks", Yields: [Stick], Tool: None, Destroys: false
	 * - Action: "StripBark", Yields: [Bark], Tool: None, Destroys: false
	 * - Action: "ChopDown", Yields: [Log, Log, Stick], Tool: Axe, Destroys: true
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest")
	TArray<FMOResourceHarvestAction> HarvestActions;

	/**
	 * Default behavior when no specific action destroys the resource.
	 * If true, the resource mesh remains after any non-destructive harvest.
	 * Typically true for trees/bushes (can gather multiple times), false for ore (mine once).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Harvest")
	bool bKeepOnHarvest = false;

	// ============================================================================
	// LEGACY YIELDS (for simple resources without complex harvest actions)
	// ============================================================================

	/**
	 * Simple yield list for resources that don't need multiple harvest actions.
	 * If HarvestActions is empty, use this for basic "Gives_{ItemId}" tag generation.
	 * Prefer using HarvestActions for full tool requirement support.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Yields", AdvancedDisplay)
	TArray<FName> YieldsItems;

	// ============================================================================
	// TAGGING
	// ============================================================================

	/**
	 * Additional resource classification tags.
	 * Use for job system matching: "Wood", "Stone", "Fiber", "Crop", "Harvestable", etc.
	 * These are added directly without prefix modification.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Tags")
	TArray<FName> ResourceTags;

	// ============================================================================
	// VISUALS
	// ============================================================================

	/** Mesh variations for this resource. Random selection based on weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Visuals")
	TArray<FMOResourceMeshVariation> MeshVariations;

	/** Minimum scale for random variation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Visuals")
	FVector MinScale = FVector(0.9f);

	/** Maximum scale for random variation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Visuals")
	FVector MaxScale = FVector(1.1f);

	/** If true, apply random rotation on Z axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|ResourceNode|Visuals")
	bool bRandomizeRotation = true;

	// ============================================================================
	// HELPERS
	// ============================================================================

	/** Get all auto-generated tags for this resource node. */
	TArray<FName> GetAllTags() const
	{
		TArray<FName> Tags;

		// Display name tag
		if (!DisplayName.IsEmpty())
		{
			Tags.Add(FName(*FString::Printf(TEXT("Name %s"), *DisplayName.ToString())));
		}

		// Resource type tag
		static const TMap<EMOResourceType, FName> TypeTags = {
			{ EMOResourceType::Generic, FName("MOResource_Generic") },
			{ EMOResourceType::Tree, FName("MOResource_Tree") },
			{ EMOResourceType::Bush, FName("MOResource_Bush") },
			{ EMOResourceType::Rock, FName("MOResource_Rock") },
			{ EMOResourceType::Ore, FName("MOResource_Ore") },
			{ EMOResourceType::Plant, FName("MOResource_Plant") }
		};
		if (const FName* TypeTag = TypeTags.Find(ResourceType))
		{
			Tags.Add(*TypeTag);
		}

		// Harvest action tags
		for (const FMOResourceHarvestAction& Action : HarvestActions)
		{
			// Action tag (e.g., "Action_ChopDown")
			FName ActionTag = Action.GetActionTag();
			if (!ActionTag.IsNone() && !Tags.Contains(ActionTag))
			{
				Tags.Add(ActionTag);
			}

			// Yield tags from this action (e.g., "Gives_Log")
			for (const FName& YieldTag : Action.GetYieldTags())
			{
				if (!Tags.Contains(YieldTag))
				{
					Tags.Add(YieldTag);
				}
			}

			// Tool requirement tag (e.g., "RequiresTool_Axe")
			FName ToolTag = Action.GetToolRequirementTag();
			if (!Tags.Contains(ToolTag))
			{
				Tags.Add(ToolTag);
			}
		}

		// Legacy yields tags (for simple resources)
		for (const FName& ItemId : YieldsItems)
		{
			if (!ItemId.IsNone())
			{
				FName YieldTag = FName(*FString::Printf(TEXT("Gives_%s"), *ItemId.ToString()));
				if (!Tags.Contains(YieldTag))
				{
					Tags.Add(YieldTag);
				}
			}
		}

		// KeepOnHarvest tag
		if (bKeepOnHarvest)
		{
			Tags.Add(FName("KeepOnHarvest"));
		}

		// Direct resource tags
		for (const FName& Tag : ResourceTags)
		{
			if (!Tag.IsNone() && !Tags.Contains(Tag))
			{
				Tags.Add(Tag);
			}
		}

		return Tags;
	}

	/** Get all harvest actions that don't require a tool. */
	TArray<FMOResourceHarvestAction> GetNoToolActions() const
	{
		TArray<FMOResourceHarvestAction> Result;
		for (const FMOResourceHarvestAction& Action : HarvestActions)
		{
			if (!Action.RequiresTool())
			{
				Result.Add(Action);
			}
		}
		return Result;
	}

	/** Get all harvest actions that require a specific tool type. */
	TArray<FMOResourceHarvestAction> GetActionsForTool(FName ToolType) const
	{
		TArray<FMOResourceHarvestAction> Result;
		for (const FMOResourceHarvestAction& Action : HarvestActions)
		{
			if (Action.RequiredToolType == ToolType)
			{
				Result.Add(Action);
			}
		}
		return Result;
	}

	/** Find a harvest action by ID. Returns nullptr if not found. */
	const FMOResourceHarvestAction* FindAction(FName ActionId) const
	{
		for (const FMOResourceHarvestAction& Action : HarvestActions)
		{
			if (Action.ActionId == ActionId)
			{
				return &Action;
			}
		}
		return nullptr;
	}

	/** Check if any harvest action destroys the resource. */
	bool HasDestructiveAction() const
	{
		for (const FMOResourceHarvestAction& Action : HarvestActions)
		{
			if (Action.bDestroysResource)
			{
				return true;
			}
		}
		return false;
	}

	/** Select a random mesh variation based on weights. Returns nullptr if no variations. */
	const FMOResourceMeshVariation* SelectMeshVariation(FRandomStream& RandomStream) const
	{
		if (MeshVariations.Num() == 0)
		{
			return nullptr;
		}

		if (MeshVariations.Num() == 1)
		{
			return &MeshVariations[0];
		}

		// Calculate total weight
		float TotalWeight = 0.0f;
		for (const FMOResourceMeshVariation& Var : MeshVariations)
		{
			TotalWeight += Var.Weight;
		}

		// Select based on weight
		float Selection = RandomStream.FRandRange(0.0f, TotalWeight);
		float Accumulated = 0.0f;
		for (const FMOResourceMeshVariation& Var : MeshVariations)
		{
			Accumulated += Var.Weight;
			if (Selection <= Accumulated)
			{
				return &Var;
			}
		}

		// Fallback to last
		return &MeshVariations.Last();
	}
};
