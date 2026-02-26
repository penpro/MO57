#include "MOHarvestSubsystem.h"
#include "MOFramework.h"
#include "MORecipeDatabaseSettings.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MOCraftingSubsystem.h"
#include "MOPCGInteractionSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

void UMOHarvestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildHarvestRecipeCache();

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Initialized with %d harvest recipes"), HarvestRecipeCache.Num());
}

void UMOHarvestSubsystem::Deinitialize()
{
	CancelHarvest();
	HarvestRecipeCache.Empty();
	Super::Deinitialize();
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
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Cached harvest recipe '%s' (tag: '%s', destroys: %s, tools: %d, knowledge: %d, discovery: %s/%s@%d)"),
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
				UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest]   Tool: '%s', Required=%s"),
					*ToolTypeName,
					Tool.bIsRequired ? TEXT("yes") : TEXT("no"));
			}

			// Log knowledge requirements
			for (const FName& Knowledge : Recipe->RequiredKnowledge)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest]   RequiredKnowledge: '%s'"), *Knowledge.ToString());
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
	// Parse "GivesX" format to extract item ID
	FString TagString = Tag.ToString();
	if (TagString.StartsWith(TEXT("Gives")))
	{
		// Strip "Gives" prefix to get item suffix
		FString ItemSuffix = TagString.RightChop(5);

		// Use PCG interaction subsystem's tag mapping
		if (UMOPCGInteractionSubsystem* PCGSubsystem = GetWorld()->GetSubsystem<UMOPCGInteractionSubsystem>())
		{
			const FName* MappedItem = PCGSubsystem->TagToItemMap.Find(Tag);
			if (MappedItem)
			{
				return *MappedItem;
			}
		}

		// Fallback: assume tag "GivesFoo" maps to "Foo01"
		// This is a convention - actual mapping should be registered in PCG subsystem
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
	FName RecipeId,
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

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || !Recipe->bIsHarvestRecipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] BeginHarvest: Recipe '%s' not found or not a harvest recipe"), *RecipeId.ToString());
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
	CurrentContext.ActiveRecipeId = RecipeId;
	CurrentContext.ElapsedTime = 0.0f;

	// Calculate adjusted craft time based on tools
	UMOCraftingSubsystem* CraftingSubsystem = GetWorld()->GetSubsystem<UMOCraftingSubsystem>();
	if (CraftingSubsystem && IsValid(Inventory))
	{
		CurrentContext.TotalTime = CraftingSubsystem->GetAdjustedCraftTime(RecipeId, Inventory);
	}
	else
	{
		CurrentContext.TotalTime = Recipe->CraftTime;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Started harvest '%s' on instance %d (time: %.1fs)"),
		*RecipeId.ToString(), InstanceIndex, CurrentContext.TotalTime);

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
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvest] Cancelled harvest '%s'"), *CurrentContext.ActiveRecipeId.ToString());
		OnHarvestCancelled.Broadcast();
	}

	CurrentContext.Reset();
}

FMOCraftResult UMOHarvestSubsystem::CompleteHarvest(
	UMOInventoryComponent* Inventory,
	UMOSkillsComponent* Skills
)
{
	FMOCraftResult Result;

	if (!CurrentContext.IsValid() || CurrentContext.ActiveRecipeId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CompleteHarvest: No valid harvest in progress"));
		return Result;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(CurrentContext.ActiveRecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvest] CompleteHarvest: Recipe '%s' not found"), *CurrentContext.ActiveRecipeId.ToString());
		CurrentContext.Reset();
		return Result;
	}

	// If bDestroysTarget, remove the instance
	if (Recipe->bDestroysTarget)
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

	// Use crafting subsystem to produce outputs and grant XP
	UMOCraftingSubsystem* CraftingSubsystem = GetWorld()->GetSubsystem<UMOCraftingSubsystem>();
	if (CraftingSubsystem)
	{
		Result = CraftingSubsystem->ProduceOutputsOnly(CurrentContext.ActiveRecipeId, Inventory, Skills);
	}

	FName CompletedRecipeId = CurrentContext.ActiveRecipeId;
	CurrentContext.Reset();

	OnHarvestComplete.Broadcast(CompletedRecipeId, Result.bSuccess);

	return Result;
}

float UMOHarvestSubsystem::GetHarvestProgress() const
{
	if (!IsHarvestInProgress() || CurrentContext.TotalTime <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(CurrentContext.ElapsedTime / CurrentContext.TotalTime, 0.0f, 1.0f);
}
