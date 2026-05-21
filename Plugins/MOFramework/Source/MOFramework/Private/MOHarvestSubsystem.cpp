#include "MOHarvestSubsystem.h"
#include "MOFramework.h"
#include "MORecipeDatabaseSettings.h"
#include "MOResourceDatabaseSettings.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOResourceDepletionSubsystem.h"
#include "MOHarvestDebugSubsystem.h"
#include "MOCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/DataTable.h"

void UMOHarvestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildHarvestRecipeCache();
	CacheResourceDataTable();

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Initialized with %d harvest recipes, ResourceTable=%s"),
		HarvestRecipeCache.Num(),
		CachedResourceDataTable.IsValid() ? TEXT("loaded") : TEXT("NOT FOUND"));
}

void UMOHarvestSubsystem::Deinitialize()
{
	CancelHarvest();
	HarvestRecipeCache.Empty();
	CachedResourceDataTable.Reset();
	Super::Deinitialize();
}

void UMOHarvestSubsystem::CacheResourceDataTable()
{
	const UMOResourceDatabaseSettings* ResourceSettings = GetDefault<UMOResourceDatabaseSettings>();
	if (ResourceSettings)
	{
		CachedResourceDataTable = ResourceSettings->GetResourceDefinitionsDataTable();
	}
}

FName UMOHarvestSubsystem::ExtractResourceNodeId(const TArray<FName>& Tags) const
{
	static const FString Prefix = TEXT("ResourceNode_");

	for (const FName& Tag : Tags)
	{
		FString TagString = Tag.ToString();
		if (TagString.StartsWith(Prefix))
		{
			// Extract the row name after the prefix
			return FName(*TagString.RightChop(Prefix.Len()));
		}
	}

	return NAME_None;
}

const FMOResourceNodeDefinitionRow* UMOHarvestSubsystem::GetResourceDefinition(FName ResourceNodeId) const
{
	if (ResourceNodeId.IsNone())
	{
		return nullptr;
	}

	UDataTable* DataTable = CachedResourceDataTable.Get();
	if (!DataTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] GetResourceDefinition: No resource DataTable cached"));
		return nullptr;
	}

	static const FString ContextString(TEXT("MOHarvestSubsystem::GetResourceDefinition"));
	return DataTable->FindRow<FMOResourceNodeDefinitionRow>(ResourceNodeId, ContextString);
}

const FMOResourceNodeDefinitionRow* UMOHarvestSubsystem::GetResourceDefinitionForComponent(UInstancedStaticMeshComponent* ISMComponent) const
{
	if (!IsValid(ISMComponent))
	{
		return nullptr;
	}

	TArray<FName> Tags = CollectTargetTags(ISMComponent);
	FName ResourceNodeId = ExtractResourceNodeId(Tags);

	if (ResourceNodeId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] GetResourceDefinitionForComponent: No ResourceNode_ tag found on component"));
		return nullptr;
	}

	return GetResourceDefinition(ResourceNodeId);
}

void UMOHarvestSubsystem::BuildHarvestRecipeCache()
{
	HarvestRecipeCache.Empty();

	const UMORecipeDatabaseSettings* RecipeSettings = GetDefault<UMORecipeDatabaseSettings>();
	if (!RecipeSettings)
	{
		return;
	}

	UDataTable* DataTable = RecipeSettings->GetRecipeDefinitionsDataTable();
	if (!DataTable)
	{
		return;
	}

	static const FString ContextString(TEXT("MOHarvestSubsystem"));
	TArray<FMORecipeDefinitionRow*> AllRecipes;
	DataTable->GetAllRows<FMORecipeDefinitionRow>(ContextString, AllRecipes);

	for (const FMORecipeDefinitionRow* Recipe : AllRecipes)
	{
		if (Recipe && Recipe->bIsHarvestRecipe)
		{
			HarvestRecipeCache.Add(Recipe);
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOHarvest] Cached harvest recipe '%s' (tag: '%s', destroys: %s, tools: %d, knowledge: %d, discovery: %s/%s@%d)"),
				*Recipe->RecipeId.ToString(),
				*Recipe->RequiredTargetTag.ToString(),
				Recipe->bDestroysTarget ? TEXT("yes") : TEXT("no"),
				Recipe->RequiredTools.Num(),
				Recipe->RequiredKnowledge.Num(),
				Recipe->bRequiresDiscovery ? TEXT("yes") : TEXT("no"),
				*Recipe->DiscoveryKnowledgeId.ToString(),
				Recipe->DiscoveryKnowledgeLevel);

			// Log tool requirements
			const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
			for (const FMOToolRequirement& Tool : Recipe->RequiredTools)
			{
				FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(Tool.ToolType));
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOHarvest]   Tool: '%s', Required=%s"),
					*ToolTypeName,
					Tool.bIsRequired ? TEXT("yes") : TEXT("no"));
			}

			// Log knowledge requirements
			for (const FName& Knowledge : Recipe->RequiredKnowledge)
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOHarvest]   RequiredKnowledge: '%s'"), *Knowledge.ToString());
			}
		}
	}
}

TArray<FName> UMOHarvestSubsystem::CollectTargetTags(UInstancedStaticMeshComponent* ISMComponent) const
{
	TArray<FName> Tags;

	if (!IsValid(ISMComponent))
	{
		return Tags;
	}

	// Collect component tags
	Tags.Append(ISMComponent->ComponentTags);

	// Collect owner actor tags
	if (AActor* Owner = ISMComponent->GetOwner())
	{
		Tags.Append(Owner->Tags);
	}

	return Tags;
}

FName UMOHarvestSubsystem::GetItemIdForGivesTag(FName Tag) const
{
	// Parse "Gives_X" or "GivesX" format to extract item ID
	FString TagString = Tag.ToString();

	// Use PCG interaction subsystem's tag mapping first
	if (UMOPCGInteractionSubsystem* PCGSubsystem = GetWorld()->GetSubsystem<UMOPCGInteractionSubsystem>())
	{
		const FName* MappedItem = PCGSubsystem->TagToItemMap.Find(Tag);
		if (MappedItem)
		{
			return *MappedItem;
		}
	}

	// Handle "Gives_ItemId" format (e.g., "Gives_Stick01" -> "Stick01")
	// This is the current format from GetYieldTags() in MOResourceNodeDefinitionRow
	if (TagString.StartsWith(TEXT("Gives_")))
	{
		// Strip "Gives_" prefix (6 chars) - the remainder IS the item ID
		return FName(*TagString.RightChop(6));
	}

	// Handle legacy "GivesX" format (e.g., "GivesBark" -> "Bark01")
	if (TagString.StartsWith(TEXT("Gives")))
	{
		FString ItemSuffix = TagString.RightChop(5);
		// Fallback: assume tag "GivesFoo" maps to "Foo01"
		return FName(*FString::Printf(TEXT("%s01"), *ItemSuffix));
	}

	return NAME_None;
}

TArray<FName> UMOHarvestSubsystem::GetItemIdsFromGivesTags(const TArray<FName>& Tags) const
{
	TArray<FName> ItemIds;

	for (const FName& Tag : Tags)
	{
		FString TagString = Tag.ToString();
		if (TagString.StartsWith(TEXT("Gives")))
		{
			FName ItemId = GetItemIdForGivesTag(Tag);
			if (!ItemId.IsNone())
			{
				ItemIds.AddUnique(ItemId);
			}
		}
	}

	return ItemIds;
}

FName UMOHarvestSubsystem::GetSmartInspectItemId(
	const TArray<FName>& TargetTags,
	UMOKnowledgeComponent* Knowledge,
	UMOSkillsComponent* Skills
) const
{
	if (!IsValid(Knowledge))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] GetSmartInspectItemId: Invalid knowledge component"));
		return NAME_None;
	}

	// Get all item IDs from GivesX tags
	TArray<FName> ItemIds = GetItemIdsFromGivesTags(TargetTags);

	if (ItemIds.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] GetSmartInspectItemId: No GivesX tags found"));
		return NAME_None;
	}

	// Find first item with learning potential
	for (const FName& ItemId : ItemIds)
	{
		EMOLearningPotential Potential = Knowledge->GetLearningPotential(ItemId, Skills);
		if (Potential != EMOLearningPotential::None)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] GetSmartInspectItemId: '%s' has learning potential"), *ItemId.ToString());
			return ItemId;
		}
	}

	// All items maxed out, return first one anyway (for viewing knowledge)
	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] GetSmartInspectItemId: All items maxed, returning first: '%s'"), *ItemIds[0].ToString());
	return ItemIds[0];
}

void UMOHarvestSubsystem::GetHarvestRecipesForTags(
	const TArray<FName>& TargetTags,
	UMOKnowledgeComponent* Knowledge,
	UMOSkillsComponent* Skills,
	UMOInventoryComponent* Inventory,
	TArray<FName>& OutRecipeIds
) const
{
	OutRecipeIds.Empty();

	UMOCraftingSubsystem* CraftingSubsystem = GetWorld()->GetSubsystem<UMOCraftingSubsystem>();
	if (!CraftingSubsystem)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] GetHarvestRecipesForTags: Checking %d cached recipes against %d target tags"),
		HarvestRecipeCache.Num(), TargetTags.Num());

	for (const FMORecipeDefinitionRow* Recipe : HarvestRecipeCache)
	{
		if (!Recipe)
		{
			continue;
		}

		// Skip destroy recipes (handled separately)
		if (Recipe->bDestroysTarget)
		{
			continue;
		}

		// Check if target has the required tag
		if (!Recipe->RequiredTargetTag.IsNone())
		{
			bool bHasTag = TargetTags.Contains(Recipe->RequiredTargetTag);
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest]   Recipe '%s' requires tag '%s': %s"),
				*Recipe->RecipeId.ToString(),
				*Recipe->RequiredTargetTag.ToString(),
				bHasTag ? TEXT("FOUND") : TEXT("not found"));
			if (!bHasTag)
			{
				continue;
			}
		}

		// Use centralized availability check (discovery/knowledge/skill)
		FMORecipeAvailability Availability = CraftingSubsystem->IsRecipeAvailable(Recipe, Knowledge, Skills);

		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest]   Recipe '%s' availability: Available=%s, Discovered=%s, Reason='%s'"),
			*Recipe->RecipeId.ToString(),
			Availability.bIsAvailable ? TEXT("yes") : TEXT("no"),
			Availability.bIsDiscovered ? TEXT("yes") : TEXT("no"),
			*Availability.UnavailableReason.ToString());

		if (!Availability.bIsAvailable)
		{
			continue;
		}

		// Check tool requirements (harvest recipes may need tools like axes)
		TArray<EMOToolType> MissingTools;
		bool bHasTools = CraftingSubsystem->HasRequiredTools(Recipe, Inventory, &MissingTools);

		if (!bHasTools)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest]   Recipe '%s' missing required tools: %d"),
				*Recipe->RecipeId.ToString(), MissingTools.Num());
			continue;
		}

		OutRecipeIds.Add(Recipe->RecipeId);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Found %d valid harvest recipes for target"), OutRecipeIds.Num());
}

FName UMOHarvestSubsystem::GetDestroyRecipeForTags(
	const TArray<FName>& TargetTags,
	UMOKnowledgeComponent* Knowledge,
	UMOSkillsComponent* Skills,
	UMOInventoryComponent* Inventory
) const
{
	UMOCraftingSubsystem* CraftingSubsystem = GetWorld()->GetSubsystem<UMOCraftingSubsystem>();
	if (!CraftingSubsystem)
	{
		return NAME_None;
	}

	for (const FMORecipeDefinitionRow* Recipe : HarvestRecipeCache)
	{
		if (!Recipe || !Recipe->bDestroysTarget)
		{
			continue;
		}

		// Check if target has the required tag
		if (!Recipe->RequiredTargetTag.IsNone())
		{
			bool bHasTag = TargetTags.Contains(Recipe->RequiredTargetTag);
			if (!bHasTag)
			{
				continue;
			}
		}

		// Use centralized availability check (discovery/knowledge/skill)
		FMORecipeAvailability Availability = CraftingSubsystem->IsRecipeAvailable(Recipe, Knowledge, Skills);
		if (!Availability.bIsAvailable)
		{
			continue;
		}

		// Check tool requirements
		TArray<EMOToolType> MissingTools;
		bool bHasTools = CraftingSubsystem->HasRequiredTools(Recipe, Inventory, &MissingTools);
		if (!bHasTools)
		{
			continue;
		}

		return Recipe->RecipeId;
	}

	return NAME_None;
}

FName UMOHarvestSubsystem::FindDestroyRecipeForTags(const TArray<FName>& TargetTags) const
{
	for (const FMORecipeDefinitionRow* Recipe : HarvestRecipeCache)
	{
		if (!Recipe || !Recipe->bDestroysTarget)
		{
			continue;
		}

		// Check if target has the required tag (or recipe has no tag requirement)
		if (!Recipe->RequiredTargetTag.IsNone())
		{
			bool bHasTag = TargetTags.Contains(Recipe->RequiredTargetTag);
			if (!bHasTag)
			{
				continue;
			}
		}

		// Found a matching destroy recipe (don't check tool requirements)
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] FindDestroyRecipeForTags: Found '%s' for tag '%s'"),
			*Recipe->RecipeId.ToString(), *Recipe->RequiredTargetTag.ToString());
		return Recipe->RecipeId;
	}

	return NAME_None;
}

bool UMOHarvestSubsystem::CanExecuteDestroyRecipe(FName RecipeId, UMOInventoryComponent* Inventory) const
{
	if (RecipeId.IsNone())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] CanExecuteDestroyRecipe: No recipe ID"));
		return false;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CanExecuteDestroyRecipe: Recipe '%s' not found"), *RecipeId.ToString());
		return false;
	}

	if (!IsValid(Inventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CanExecuteDestroyRecipe: Invalid inventory"));
		return false;
	}

	// Directly check tool requirements - skip knowledge/skill checks for destroy recipe availability
	UMOCraftingSubsystem* CraftingSubsystem = GetWorld()->GetSubsystem<UMOCraftingSubsystem>();
	if (!CraftingSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CanExecuteDestroyRecipe: No crafting subsystem"));
		return false;
	}

	TArray<EMOToolType> MissingTools;
	bool bHasTools = CraftingSubsystem->HasRequiredTools(Recipe, Inventory, &MissingTools);

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] CanExecuteDestroyRecipe '%s': RequiredTools=%d, HasTools=%s, MissingTools=%d"),
		*RecipeId.ToString(), Recipe->RequiredTools.Num(), bHasTools ? TEXT("yes") : TEXT("no"), MissingTools.Num());

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	for (const EMOToolType& MissingTool : MissingTools)
	{
		FString ToolName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(MissingTool));
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest]   Missing tool: %s"), *ToolName);
	}

	return bHasTools;
}

bool UMOHarvestSubsystem::BeginHarvest(
	UInstancedStaticMeshComponent* ISMComponent,
	int32 InstanceIndex,
	FName ActionId,
	UMOInventoryComponent* Inventory
)
{
	// Cancel any existing harvest
	if (IsHarvestInProgress())
	{
		CancelHarvest();
	}

	if (!IsValid(ISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] BeginHarvest: Invalid ISM component"));
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= ISMComponent->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] BeginHarvest: Invalid instance index %d"), InstanceIndex);
		return false;
	}

	// Setup context
	CurrentContext.Reset();
	CurrentContext.ISMComponent = ISMComponent;

	// Check if it's actually an HISM
	if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(ISMComponent))
	{
		CurrentContext.HISMComponent = HISM;
	}

	CurrentContext.InstanceIndex = InstanceIndex;
	ISMComponent->GetInstanceTransform(InstanceIndex, CurrentContext.InstanceTransform, true);
	CurrentContext.TargetTags = CollectTargetTags(ISMComponent);
	CurrentContext.ActiveActionId = ActionId;
	CurrentContext.ElapsedTime = 0.0f;

	// Extract ResourceNodeId and look up the HarvestAction from resource definition
	CurrentContext.ResourceNodeId = ExtractResourceNodeId(CurrentContext.TargetTags);

	const FMOResourceNodeDefinitionRow* ResourceDef = nullptr;
	const FMOResourceHarvestAction* HarvestAction = nullptr;

	if (!CurrentContext.ResourceNodeId.IsNone())
	{
		ResourceDef = GetResourceDefinition(CurrentContext.ResourceNodeId);
		if (ResourceDef)
		{
			HarvestAction = ResourceDef->FindAction(ActionId);
		}
	}

	if (!HarvestAction)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] BeginHarvest: Action '%s' not found in resource '%s'"),
			*ActionId.ToString(), *CurrentContext.ResourceNodeId.ToString());
		CurrentContext.Reset();
		return false;
	}

	// Calculate action time - base time from HarvestAction, potentially modified by tool quality
	float BaseTime = HarvestAction->BaseActionTime;

	// If player doesn't have the tool but action allows it with penalty, apply penalty
	if (HarvestAction->RequiresTool() && !HarvestAction->bToolRequired && IsValid(Inventory))
	{
		if (!Inventory->HasToolOfType(HarvestAction->RequiredToolType))
		{
			BaseTime *= HarvestAction->MissingToolTimeMultiplier;
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] No tool - applying time penalty (x%.1f)"),
				HarvestAction->MissingToolTimeMultiplier);
		}
	}

	CurrentContext.TotalTime = BaseTime;

	// Subscribe to the harvester's "I started moving" broadcast. Movement
	// cancels the harvest entirely — no partial progress kept (gathering is a
	// stay-put activity).
	RegisterWithHarvesterForInterrupts(Inventory);

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Started harvest action '%s' on resource '%s' instance %d (time: %.1fs)"),
		*ActionId.ToString(), *CurrentContext.ResourceNodeId.ToString(), InstanceIndex, CurrentContext.TotalTime);

	return true;
}

bool UMOHarvestSubsystem::UpdateHarvest(float DeltaTime)
{
	if (!IsHarvestInProgress())
	{
		return false;
	}

	// Verify target still exists
	if (!CurrentContext.ISMComponent.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] UpdateHarvest: Target component no longer valid"));
		CancelHarvest();
		return false;
	}

	// Update elapsed time
	CurrentContext.ElapsedTime += DeltaTime;

	// Broadcast progress
	float Progress = GetHarvestProgress();
	float TimeRemaining = FMath::Max(0.0f, CurrentContext.TotalTime - CurrentContext.ElapsedTime);
	OnHarvestProgress.Broadcast(Progress, TimeRemaining);

	// Check if complete
	if (CurrentContext.ElapsedTime >= CurrentContext.TotalTime)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Harvest complete after %.1fs"), CurrentContext.ElapsedTime);
		return false; // No longer in progress
	}

	return true;
}

void UMOHarvestSubsystem::CancelHarvest()
{
	if (IsHarvestInProgress())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Cancelled harvest action '%s'"), *CurrentContext.ActiveActionId.ToString());
		OnHarvestCancelled.Broadcast();
	}

	UnregisterFromHarvesterInterrupts();
	CurrentContext.Reset();
}

FMOCraftResult UMOHarvestSubsystem::CompleteHarvest(
	UMOInventoryComponent* Inventory,
	UMOSkillsComponent* Skills
)
{
	FMOCraftResult Result;

	if (!CurrentContext.IsValid() || CurrentContext.ActiveActionId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CompleteHarvest: No valid harvest in progress"));
		return Result;
	}

	// Look up the HarvestAction from resource definition
	const FMOResourceNodeDefinitionRow* ResourceDef = GetResourceDefinition(CurrentContext.ResourceNodeId);
	const FMOResourceHarvestAction* HarvestAction = nullptr;

	if (ResourceDef)
	{
		HarvestAction = ResourceDef->FindAction(CurrentContext.ActiveActionId);
	}

	if (!HarvestAction)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CompleteHarvest: Action '%s' not found in resource '%s'"),
			*CurrentContext.ActiveActionId.ToString(), *CurrentContext.ResourceNodeId.ToString());
		CurrentContext.Reset();
		return Result;
	}

	// If bDestroysResource, remove the instance
	if (HarvestAction->bDestroysResource)
	{
		if (CurrentContext.HISMComponent.IsValid())
		{
			CurrentContext.HISMComponent->RemoveInstance(CurrentContext.InstanceIndex);
		}
		else if (CurrentContext.ISMComponent.IsValid())
		{
			CurrentContext.ISMComponent->RemoveInstance(CurrentContext.InstanceIndex);
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Destroyed target instance %d"), CurrentContext.InstanceIndex);
	}

	// Produce yields from the HarvestAction
	Result.bSuccess = true;

	// Compute a stable per-node key for the depletion subsystem. Identifies a single
	// HISM/ISM instance (e.g. a particular tree) across game sessions via its
	// world-space location.
	FString NodeKey;
	UInstancedStaticMeshComponent* MeshCompForLog = nullptr;
	{
		UInstancedStaticMeshComponent* MeshComp =
			CurrentContext.HISMComponent.IsValid() ?
				static_cast<UInstancedStaticMeshComponent*>(CurrentContext.HISMComponent.Get()) :
				CurrentContext.ISMComponent.Get();
		MeshCompForLog = MeshComp;
		if (MeshComp)
		{
			NodeKey = UMOResourceDepletionSubsystem::MakeNodeKey(MeshComp, CurrentContext.InstanceIndex);
		}
	}
	UMOResourceDepletionSubsystem* Depletion = UMOResourceDepletionSubsystem::Get(this);

	// Log the harvest attempt so we can see what's happening — particularly whether
	// the NodeKey is non-empty and whether the depletion subsystem was found.
	{
		FString YieldList;
		for (const FName& Y : HarvestAction->YieldsItems)
		{
			YieldList += Y.ToString() + TEXT(" ");
		}
		MOHARVEST_LOG(this, "Harvest",
			"CompleteHarvest action='%s' resource='%s' MeshComp=%s InstanceIndex=%d NodeKey='%s' Depletion=%s yields=[%s]",
			*CurrentContext.ActiveActionId.ToString(),
			*CurrentContext.ResourceNodeId.ToString(),
			MeshCompForLog ? *MeshCompForLog->GetClass()->GetName() : TEXT("<null>"),
			CurrentContext.InstanceIndex,
			*NodeKey,
			Depletion ? TEXT("OK") : TEXT("NULL"),
			*YieldList);
	}

	if (IsValid(Inventory))
	{
		for (const FName& ItemId : HarvestAction->YieldsItems)
		{
			if (ItemId.IsNone())
			{
				continue;
			}

			// Check depletion limit. If this yield is configured with a count range,
			// ConsumeYield decrements remaining and returns false when exhausted.
			// Unconfigured items (no entry in InitialCountByItem) are treated as
			// unlimited and return true.
			//
			// IMPORTANT: depletion-skip is NOT an inventory failure — it must land in
			// DepletedItems, not FailedItems, so the UI does not show "Inventory Full!"
			// when the player's inventory has plenty of room. Misclassifying these as
			// inventory-full is the root cause of "I picked up a few things and it says
			// the inventory is full — dropping doesn't help."
			if (Depletion && !NodeKey.IsEmpty())
			{
				if (!Depletion->ConsumeYield(NodeKey, ItemId))
				{
					Result.DepletedItems.FindOrAdd(ItemId) += 1;
					MOHARVEST_LOG(this, "Harvest",
						"  Yield '%s' DEPLETED on node '%s' — skipping (NOT an inventory failure)",
						*ItemId.ToString(), *NodeKey);
					continue;
				}
			}

			// Generate a new GUID for the item
			FGuid NewItemGuid = FGuid::NewGuid();

			// Add 1 of each yielded item (quantity can be enhanced later with tool quality/skill bonuses)
			int32 Quantity = 1;

			if (Inventory->AddItemByGuid(NewItemGuid, ItemId, Quantity))
			{
				Result.ProducedItems.FindOrAdd(ItemId) += Quantity;
				MOHARVEST_LOG(this, "Harvest", "  Produced '%s' x%d", *ItemId.ToString(), Quantity);
			}
			else
			{
				// Real inventory rejection. AddItemByGuid currently always returns true
				// for valid inputs, so reaching this branch means invalid GUID / no
				// authority / empty ItemId — not actually a full inventory.
				Result.FailedItems.FindOrAdd(ItemId) += Quantity;
				MOHARVEST_LOG(this, "Harvest",
					"  AddItemByGuid REJECTED '%s' x%d — invalid input or no authority (not inventory-full)",
					*ItemId.ToString(), Quantity);
			}
		}
	}

	// Grant skill XP
	if (IsValid(Skills) && !HarvestAction->SkillId.IsNone() && HarvestAction->SkillXPReward > 0.0f)
	{
		Skills->AddExperience(HarvestAction->SkillId, HarvestAction->SkillXPReward);
		Result.XPGranted.FindOrAdd(HarvestAction->SkillId) += HarvestAction->SkillXPReward;
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Granted XP: %s +%.0f"),
			*HarvestAction->SkillId.ToString(), HarvestAction->SkillXPReward);
	}

	FName CompletedActionId = CurrentContext.ActiveActionId;
	CurrentContext.Reset();

	// Harvest is done — stop listening for harvester movement.
	UnregisterFromHarvesterInterrupts();

	OnHarvestComplete.Broadcast(CompletedActionId, Result.bSuccess);

	return Result;
}

// =============================================================================
// INTERRUPT HANDLING (IMOInterruptibleInterface)
// =============================================================================

void UMOHarvestSubsystem::NotifyInterrupt_Implementation(const FMOInterruptContext& Context)
{
	if (!IsHarvestInProgress())
	{
		return;
	}

	// Any meaningful disturbance cancels — gathering has no resumable state.
	// UserCancel skipped to avoid double-fire with the UI's direct CancelHarvest call.
	switch (Context.Reason)
	{
	case EMOInterruptReason::Movement:
	case EMOInterruptReason::Damage:
	case EMOInterruptReason::Knockdown:
	case EMOInterruptReason::Unconscious:
	case EMOInterruptReason::Death:
	case EMOInterruptReason::EnteredCombat:
	case EMOInterruptReason::LostControl:
	case EMOInterruptReason::External:
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOHarvest] Interrupt reason=%d — cancelling harvest (no progress preserved)"),
			(int32)Context.Reason);
		// CancelHarvest broadcasts OnHarvestCancelled (the progress widget
		// listens to that to tear itself down) and unregisters us.
		CancelHarvest();
		break;

	case EMOInterruptReason::UserCancel:
	case EMOInterruptReason::None:
	default:
		break;
	}
}

void UMOHarvestSubsystem::RegisterWithHarvesterForInterrupts(UMOInventoryComponent* HarvesterInventory)
{
	if (!IsValid(HarvesterInventory))
	{
		return;
	}

	AMOCharacter* Harvester = Cast<AMOCharacter>(HarvesterInventory->GetOwner());
	if (!IsValid(Harvester))
	{
		return;
	}

	Harvester->RegisterInterruptListener(this);
	RegisteredHarvesterCharacter = Harvester;
}

void UMOHarvestSubsystem::UnregisterFromHarvesterInterrupts()
{
	if (AMOCharacter* Harvester = RegisteredHarvesterCharacter.Get())
	{
		Harvester->UnregisterInterruptListener(this);
	}
	RegisteredHarvesterCharacter.Reset();
}

float UMOHarvestSubsystem::GetHarvestProgress() const
{
	if (!IsHarvestInProgress() || CurrentContext.TotalTime <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(CurrentContext.ElapsedTime / CurrentContext.TotalTime, 0.0f, 1.0f);
}
