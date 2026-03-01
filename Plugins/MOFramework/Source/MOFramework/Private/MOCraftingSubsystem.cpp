#include "MOCraftingSubsystem.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MOEquipmentComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOFramework.h"

// ============================================================================
// CENTRALIZED AVAILABILITY CHECK
// ============================================================================

FMORecipeAvailability UMOCraftingSubsystem::IsRecipeAvailable(
	FName RecipeId,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent
) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		FMORecipeAvailability Result;
		Result.UnavailableReason = FText::FromString(TEXT("Recipe not found."));
		return Result;
	}

	return IsRecipeAvailable(Recipe, KnowledgeComponent, SkillsComponent);
}

FMORecipeAvailability UMOCraftingSubsystem::IsRecipeAvailable(
	const FMORecipeDefinitionRow* Recipe,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent
) const
{
	FMORecipeAvailability Result;

	if (!Recipe)
	{
		Result.UnavailableReason = FText::FromString(TEXT("Invalid recipe."));
		return Result;
	}

	// --- Check Discovery Requirements ---
	if (Recipe->bRequiresDiscovery)
	{
		Result.DiscoveryKnowledgeId = Recipe->DiscoveryKnowledgeId;
		Result.DiscoveryKnowledgeLevel = Recipe->DiscoveryKnowledgeLevel;

		bool bDiscovered = false;

		// Knowledge-based discovery (e.g., WoodProperties level 2 unlocks HarvestBark)
		if (!Recipe->DiscoveryKnowledgeId.IsNone() && IsValid(SkillsComponent))
		{
			Result.CurrentDiscoveryKnowledgeLevel = SkillsComponent->GetSkillLevel(Recipe->DiscoveryKnowledgeId);
			bDiscovered = Result.CurrentDiscoveryKnowledgeLevel >= Recipe->DiscoveryKnowledgeLevel;
		}

		// Skill-based discovery (alternative unlock path)
		if (!bDiscovered && Recipe->DiscoverySkillLevel > 0 && !Recipe->RequiredSkillId.IsNone() && IsValid(SkillsComponent))
		{
			int32 SkillLevel = SkillsComponent->GetSkillLevel(Recipe->RequiredSkillId);
			bDiscovered = SkillLevel >= Recipe->DiscoverySkillLevel;
		}

		Result.bIsDiscovered = bDiscovered;

		if (!bDiscovered)
		{
			Result.UnavailableReason = FText::Format(
				NSLOCTEXT("MOCrafting", "NotDiscovered", "Requires {0} level {1} (current: {2})"),
				FText::FromName(Recipe->DiscoveryKnowledgeId),
				FText::AsNumber(Recipe->DiscoveryKnowledgeLevel),
				FText::AsNumber(Result.CurrentDiscoveryKnowledgeLevel)
			);
			return Result;
		}
	}

	// --- Check Knowledge Requirements ---
	TArray<FName> MissingKnowledge;
	if (!HasRequiredKnowledge(Recipe, KnowledgeComponent, SkillsComponent, &MissingKnowledge))
	{
		Result.MissingKnowledge = MissingKnowledge;
		Result.UnavailableReason = FText::FromString(TEXT("Missing required knowledge."));
		return Result;
	}

	// --- Check Skill Requirements ---
	int32 RequiredLevel = 0;
	int32 CurrentLevel = 0;
	if (!MeetsSkillRequirements(Recipe, SkillsComponent, &RequiredLevel, &CurrentLevel))
	{
		Result.RequiredSkillLevel = RequiredLevel;
		Result.CurrentSkillLevel = CurrentLevel;
		Result.UnavailableReason = FText::Format(
			NSLOCTEXT("MOCrafting", "SkillTooLowAvail", "Requires {0} level {1} (current: {2})"),
			FText::FromName(Recipe->RequiredSkillId),
			FText::AsNumber(RequiredLevel),
			FText::AsNumber(CurrentLevel)
		);
		return Result;
	}

	// All checks passed
	Result.bIsAvailable = true;
	return Result;
}

// ============================================================================
// RECIPE QUERIES
// ============================================================================

void UMOCraftingSubsystem::GetAvailableRecipes(
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	EMOCraftingStation Station,
	TArray<FName>& OutRecipeIds
) const
{
	OutRecipeIds.Empty();

	// Get pre-filtered craftable recipes (excludes buildings) - O(1) cached lookup
	TArray<FName> CraftableRecipes;
	UMORecipeDatabaseSettings::GetCraftableRecipes(CraftableRecipes);

	OutRecipeIds.Reserve(CraftableRecipes.Num());

	for (const FName& RecipeId : CraftableRecipes)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Skip harvest recipes - they only appear in harvest context menus
		if (Recipe->bIsHarvestRecipe)
		{
			continue;
		}

		// Check station requirement (context-specific)
		if (Recipe->RequiredStation != EMOCraftingStation::None && Recipe->RequiredStation != Station)
		{
			continue;
		}

		// Use centralized availability check for discovery/knowledge/skill
		FMORecipeAvailability Availability = IsRecipeAvailable(Recipe, KnowledgeComponent, SkillsComponent);
		if (!Availability.bIsAvailable)
		{
			continue;
		}

		OutRecipeIds.Add(RecipeId);
	}
}

void UMOCraftingSubsystem::GetCraftableRecipes(
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	UMOInventoryComponent* InventoryComponent,
	EMOCraftingStation Station,
	TArray<FName>& OutRecipeIds
) const
{
	// First get available recipes
	TArray<FName> AvailableRecipes;
	GetAvailableRecipes(KnowledgeComponent, SkillsComponent, Station, AvailableRecipes);

	OutRecipeIds.Empty();

	for (const FName& RecipeId : AvailableRecipes)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Check if has all ingredients
		if (HasIngredients(Recipe, InventoryComponent, KnowledgeComponent))
		{
			OutRecipeIds.Add(RecipeId);
		}
	}
}

FMOCraftingValidation UMOCraftingSubsystem::CanCraftRecipe(
	FName RecipeId,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	UMOInventoryComponent* InventoryComponent,
	EMOCraftingStation Station
) const
{
	FMOCraftingValidation Result;

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		Result.FailureReason = FText::FromString(TEXT("Recipe not found."));
		return Result;
	}

	// --- Use centralized availability check for discovery/knowledge/skill ---
	FMORecipeAvailability Availability = IsRecipeAvailable(Recipe, KnowledgeComponent, SkillsComponent);

	// Copy availability results to validation struct
	Result.bRecipeDiscovered = Availability.bIsDiscovered;
	Result.MissingKnowledge = Availability.MissingKnowledge;
	Result.RequiredSkillLevel = Availability.RequiredSkillLevel;
	Result.CurrentSkillLevel = Availability.CurrentSkillLevel;

	if (!Availability.bIsAvailable)
	{
		Result.FailureReason = Availability.UnavailableReason;
		return Result;
	}

	// --- Station check (context-specific, not part of availability) ---
	if (Recipe->RequiredStation != EMOCraftingStation::None && Recipe->RequiredStation != Station)
	{
		Result.bCorrectStation = false;
		Result.FailureReason = FText::FromString(TEXT("Wrong crafting station."));
		return Result;
	}

	// --- Check ingredients (can craft NOW, not availability) ---
	TMap<FName, int32> MissingIngredients;
	if (!HasIngredients(Recipe, InventoryComponent, KnowledgeComponent, &MissingIngredients))
	{
		Result.MissingIngredients = MissingIngredients;
		Result.FailureReason = FText::FromString(TEXT("Missing ingredients."));
		return Result;
	}

	// Check tools - separate required vs optional
	TArray<EMOToolType> MissingRequiredTools;
	TArray<EMOToolType> MissingOptionalTools;
	float TimeMultiplier = 1.0f;
	float QualityMultiplier = 1.0f;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] CanCraftRecipe '%s': RequiredTools.Num()=%d"),
		*RecipeId.ToString(), Recipe->RequiredTools.Num());

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(ToolReq.ToolType));
		UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting]   Checking tool: Type='%s', MinQuality=%.1f, bIsRequired=%s"),
			*ToolTypeName, ToolReq.MinQuality, ToolReq.bIsRequired ? TEXT("yes") : TEXT("no"));

		FGuid ToolGuid;
		float ToolQuality;
		if (!FindBestTool(InventoryComponent, ToolReq.ToolType, ToolReq.MinQuality, ToolGuid, ToolQuality))
		{
			if (ToolReq.bIsRequired)
			{
				// Absolutely required - blocks crafting
				MissingRequiredTools.Add(ToolReq.ToolType);
			}
			else
			{
				// Optional - apply penalties
				MissingOptionalTools.Add(ToolReq.ToolType);
				TimeMultiplier *= ToolReq.MissingToolTimeMultiplier;
				QualityMultiplier *= ToolReq.MissingToolQualityMultiplier;
			}
		}
	}

	// Store all missing tools info
	Result.MissingRequiredTools = MissingRequiredTools;
	Result.MissingOptionalTools = MissingOptionalTools;
	Result.MissingToolTimeMultiplier = TimeMultiplier;
	Result.MissingToolQualityMultiplier = QualityMultiplier;

	// Backwards compatibility
	Result.MissingTools = MissingRequiredTools;

	// Only block if REQUIRED tools are missing
	if (MissingRequiredTools.Num() > 0)
	{
		Result.FailureReason = FText::FromString(TEXT("Missing required tools."));
		return Result;
	}

	Result.bCanCraft = true;
	return Result;
}

FMOCraftResult UMOCraftingSubsystem::ExecuteCraft(
	FName RecipeId,
	UMOInventoryComponent* InventoryComponent,
	UMOSkillsComponent* SkillsComponent
)
{
	FMOCraftResult Result;

	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] ExecuteCraft: Invalid inventory component"));
		return Result;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] ExecuteCraft: Recipe '%s' not found"), *RecipeId.ToString());
		return Result;
	}

	// Build a map of ItemDefId -> total quantity in inventory
	TMap<FName, int32> InventoryTotals;
	TMap<FName, TArray<FGuid>> InventoryGuidsByDefId;
	{
		TArray<FMOInventoryEntry> Entries;
		InventoryComponent->GetInventoryEntries(Entries);
		for (const FMOInventoryEntry& Entry : Entries)
		{
			InventoryTotals.FindOrAdd(Entry.ItemDefinitionId) += Entry.Quantity;
			InventoryGuidsByDefId.FindOrAdd(Entry.ItemDefinitionId).Add(Entry.ItemGuid);
		}
	}

	// Verify we have all ingredients
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		const int32* AvailablePtr = InventoryTotals.Find(Ingredient.ItemDefinitionId);
		const int32 Available = AvailablePtr ? *AvailablePtr : 0;

		if (Available < Ingredient.Quantity)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] ExecuteCraft: Not enough '%s' (have %d, need %d)"),
				*Ingredient.ItemDefinitionId.ToString(), Available, Ingredient.Quantity);
			return Result;
		}
	}

	// Consume ingredients
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		int32 RemainingToConsume = Ingredient.Quantity;
		TArray<FGuid>& Guids = InventoryGuidsByDefId.FindOrAdd(Ingredient.ItemDefinitionId);

		for (int32 i = Guids.Num() - 1; i >= 0 && RemainingToConsume > 0; --i)
		{
			FMOInventoryEntry Entry;
			if (InventoryComponent->TryGetEntryByGuid(Guids[i], Entry))
			{
				const int32 ToRemove = FMath::Min(Entry.Quantity, RemainingToConsume);
				InventoryComponent->RemoveItemByGuid(Guids[i], ToRemove);
				RemainingToConsume -= ToRemove;
			}
		}
	}

	// Produce outputs
	for (const FMORecipeOutput& Output : Recipe->Outputs)
	{
		// Check chance
		if (Output.Chance < 1.0f)
		{
			const float Roll = FMath::FRand();
			if (Roll > Output.Chance)
			{
				continue;
			}
		}

		// Add to inventory
		const FGuid NewGuid = FGuid::NewGuid();
		if (InventoryComponent->AddItemByGuid(NewGuid, Output.ItemDefinitionId, Output.Quantity))
		{
			Result.ProducedItems.FindOrAdd(Output.ItemDefinitionId) += Output.Quantity;
		}
	}

	// Grant skill XP
	if (IsValid(SkillsComponent) && !Recipe->RequiredSkillId.IsNone() && Recipe->SkillXPReward > 0.0f)
	{
		SkillsComponent->AddExperience(Recipe->RequiredSkillId, Recipe->SkillXPReward);
		Result.XPGranted.Add(Recipe->RequiredSkillId, Recipe->SkillXPReward);
	}

	// Degrade tools
	DegradeToolsForRecipe(RecipeId, InventoryComponent);

	Result.bSuccess = true;

	OnCraftCompleted.Broadcast(RecipeId, Result);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingSubsystem] Crafted '%s' successfully"), *RecipeId.ToString());

	return Result;
}

FMOCraftResult UMOCraftingSubsystem::ProduceOutputsOnly(
	FName RecipeId,
	UMOInventoryComponent* InventoryComponent,
	UMOSkillsComponent* SkillsComponent
)
{
	FMOCraftResult Result;

	if (!IsValid(InventoryComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] ProduceOutputsOnly: Invalid inventory component"));
		return Result;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] ProduceOutputsOnly: Recipe '%s' not found"), *RecipeId.ToString());
		return Result;
	}

	// Produce outputs (no ingredient consumption)
	for (const FMORecipeOutput& Output : Recipe->Outputs)
	{
		// Check chance
		if (Output.Chance < 1.0f)
		{
			const float Roll = FMath::FRand();
			if (Roll > Output.Chance)
			{
				continue;
			}
		}

		// Add to inventory
		const FGuid NewGuid = FGuid::NewGuid();
		if (InventoryComponent->AddItemByGuid(NewGuid, Output.ItemDefinitionId, Output.Quantity))
		{
			Result.ProducedItems.FindOrAdd(Output.ItemDefinitionId) += Output.Quantity;
		}
		else
		{
			// Inventory full - track failed item
			Result.FailedItems.FindOrAdd(Output.ItemDefinitionId) += Output.Quantity;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] ProduceOutputsOnly: Inventory full, couldn't add %d x '%s'"),
				Output.Quantity, *Output.ItemDefinitionId.ToString());
		}
	}

	// Grant skill XP
	if (IsValid(SkillsComponent) && !Recipe->RequiredSkillId.IsNone() && Recipe->SkillXPReward > 0.0f)
	{
		SkillsComponent->AddExperience(Recipe->RequiredSkillId, Recipe->SkillXPReward);
		Result.XPGranted.Add(Recipe->RequiredSkillId, Recipe->SkillXPReward);
	}

	// Degrade tools
	DegradeToolsForRecipe(RecipeId, InventoryComponent);

	Result.bSuccess = true;

	OnCraftCompleted.Broadcast(RecipeId, Result);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingSubsystem] Produced outputs for '%s' successfully (ingredients pre-consumed)"), *RecipeId.ToString());

	return Result;
}

float UMOCraftingSubsystem::GetRecipeCraftTime(FName RecipeId) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	return Recipe ? Recipe->CraftTime : 0.0f;
}

bool UMOCraftingSubsystem::HasRequiredKnowledge(
	const FMORecipeDefinitionRow* Recipe,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	TArray<FName>* OutMissingKnowledge
) const
{
	if (!Recipe || Recipe->RequiredKnowledge.Num() == 0)
	{
		return true;
	}

	// Try to get RecipeDiscoveryComponent from the component owner
	UMORecipeDiscoveryComponent* DiscoveryComponent = nullptr;
	if (IsValid(SkillsComponent))
	{
		if (AActor* Owner = SkillsComponent->GetOwner())
		{
			DiscoveryComponent = Owner->FindComponentByClass<UMORecipeDiscoveryComponent>();
		}
	}
	else if (IsValid(KnowledgeComponent))
	{
		if (AActor* Owner = KnowledgeComponent->GetOwner())
		{
			DiscoveryComponent = Owner->FindComponentByClass<UMORecipeDiscoveryComponent>();
		}
	}

	bool bHasAll = true;
	for (const FName& KnowledgeId : Recipe->RequiredKnowledge)
	{
		bool bHasKnowledge = false;

		// Check KnowledgeComponent first (legacy system)
		if (IsValid(KnowledgeComponent) && KnowledgeComponent->HasKnowledge(KnowledgeId))
		{
			bHasKnowledge = true;
		}

		// Fallback: Check if player has this as a skill with level >= 1
		// This bridges the gap between the unused KnowledgeComponent and active SkillsComponent
		if (!bHasKnowledge && IsValid(SkillsComponent))
		{
			if (SkillsComponent->HasSkillLevel(KnowledgeId, 1))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] Knowledge '%s' satisfied via SkillsComponent (level >= 1)"),
					*KnowledgeId.ToString());
				bHasKnowledge = true;
			}
		}

		// Fallback: Check if there's a recipe with this ID that's been discovered
		if (!bHasKnowledge && IsValid(DiscoveryComponent))
		{
			if (DiscoveryComponent->IsRecipeDiscovered(KnowledgeId))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] Knowledge '%s' satisfied via RecipeDiscoveryComponent"),
					*KnowledgeId.ToString());
				bHasKnowledge = true;
			}
		}

		if (!bHasKnowledge)
		{
			bHasAll = false;
			if (OutMissingKnowledge)
			{
				OutMissingKnowledge->Add(KnowledgeId);
			}
		}
	}

	return bHasAll;
}

bool UMOCraftingSubsystem::MeetsSkillRequirements(
	const FMORecipeDefinitionRow* Recipe,
	UMOSkillsComponent* SkillsComponent,
	int32* OutRequiredLevel,
	int32* OutCurrentLevel
) const
{
	if (!Recipe || Recipe->RequiredSkillId.IsNone() || Recipe->RequiredSkillLevel <= 0)
	{
		return true;
	}

	const int32 RequiredLevel = Recipe->RequiredSkillLevel;
	const int32 CurrentLevel = IsValid(SkillsComponent) ? SkillsComponent->GetSkillLevel(Recipe->RequiredSkillId) : 0;

	if (OutRequiredLevel)
	{
		*OutRequiredLevel = RequiredLevel;
	}
	if (OutCurrentLevel)
	{
		*OutCurrentLevel = CurrentLevel;
	}

	return CurrentLevel >= RequiredLevel;
}

bool UMOCraftingSubsystem::HasIngredients(
	const FMORecipeDefinitionRow* Recipe,
	UMOInventoryComponent* InventoryComponent,
	UMOKnowledgeComponent* KnowledgeComponent,
	TMap<FName, int32>* OutMissingIngredients
) const
{
	if (!Recipe || !IsValid(InventoryComponent))
	{
		return false;
	}

	// Build inventory totals
	TMap<FName, int32> InventoryTotals;
	{
		TArray<FMOInventoryEntry> Entries;
		InventoryComponent->GetInventoryEntries(Entries);
		for (const FMOInventoryEntry& Entry : Entries)
		{
			InventoryTotals.FindOrAdd(Entry.ItemDefinitionId) += Entry.Quantity;
		}
	}

	bool bHasAll = true;
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		// Check knowledge requirement for ingredient
		if (Ingredient.bRequiresKnowledge && IsValid(KnowledgeComponent))
		{
			// The ingredient's item must have been inspected/learned
			// For simplicity, we check if any knowledge from that item has been learned
			// A more sophisticated check would verify specific knowledge IDs
			FMOItemKnowledgeProgress Progress;
			if (!KnowledgeComponent->GetInspectionProgress(Ingredient.ItemDefinitionId, Progress) || Progress.InspectionCount == 0)
			{
				bHasAll = false;
				if (OutMissingIngredients)
				{
					// Mark as missing - quantity 0 indicates knowledge issue
					OutMissingIngredients->Add(Ingredient.ItemDefinitionId, Ingredient.Quantity);
				}
				continue;
			}
		}

		const int32* AvailablePtr = InventoryTotals.Find(Ingredient.ItemDefinitionId);
		const int32 Available = AvailablePtr ? *AvailablePtr : 0;

		if (Available < Ingredient.Quantity)
		{
			bHasAll = false;
			if (OutMissingIngredients)
			{
				OutMissingIngredients->Add(Ingredient.ItemDefinitionId, Ingredient.Quantity - Available);
			}
		}
	}

	return bHasAll;
}

bool UMOCraftingSubsystem::HasRequiredTools(
	const FMORecipeDefinitionRow* Recipe,
	UMOInventoryComponent* Inventory,
	TArray<EMOToolType>* OutMissingTools
) const
{
	if (!Recipe || Recipe->RequiredTools.Num() == 0)
	{
		return true;
	}

	if (!IsValid(Inventory))
	{
		if (OutMissingTools)
		{
			for (const FMOToolRequirement& Req : Recipe->RequiredTools)
			{
				// Only report truly required tools as missing
				if (Req.bIsRequired)
				{
					OutMissingTools->Add(Req.ToolType);
				}
			}
		}
		return false;
	}

	bool bHasAllRequired = true;
	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		FGuid FoundGuid;
		float FoundQuality;
		if (!FindBestTool(Inventory, ToolReq.ToolType, ToolReq.MinQuality, FoundGuid, FoundQuality))
		{
			// Only fail for required tools, optional tools just get penalties
			if (ToolReq.bIsRequired)
			{
				bHasAllRequired = false;
				if (OutMissingTools)
				{
					OutMissingTools->Add(ToolReq.ToolType);
				}
			}
		}
	}

	return bHasAllRequired;
}

bool UMOCraftingSubsystem::FindBestTool(
	UMOInventoryComponent* Inventory,
	EMOToolType ToolType,
	float MinEffectiveness,
	FGuid& OutItemGuid,
	float& OutEffectiveness
) const
{
	OutItemGuid.Invalidate();
	OutEffectiveness = 0.0f;

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(ToolType));

	UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: Looking for ToolType='%s', MinEffectiveness=%.2f"),
		*ToolTypeName, MinEffectiveness);

	if (!IsValid(Inventory) || ToolType == EMOToolType::None)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCrafting] FindBestTool: Invalid inventory or empty tool type"));
		return false;
	}

	// Helper lambda to check if an item matches tool requirements
	auto CheckToolItem = [&](FName ItemDefinitionId, const FGuid& ItemGuid, int32 CurrentDurability, const TCHAR* Source) -> bool
	{
		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] Item '%s' not in database"), Source, *ItemDefinitionId.ToString());
			return false;
		}

		// Check if this item has the required tool capability
		float ItemEffectiveness = ItemDef.GetToolEffectiveness(ToolType);

		UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] Checking '%s' - bIsTool=%s, HasCapability=%s, Effectiveness=%.2f, Durability=%d"),
			Source,
			*ItemDefinitionId.ToString(),
			ItemDef.bIsTool ? TEXT("yes") : TEXT("no"),
			ItemEffectiveness > 0.0f ? TEXT("yes") : TEXT("no"),
			ItemEffectiveness,
			CurrentDurability);

		// Check if this item can function as the required tool type
		if (!ItemDef.bIsTool || ItemEffectiveness <= 0.0f)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - no capability for '%s'"),
				Source, *ItemDefinitionId.ToString(), *ToolTypeName);
			return false;
		}

		// Check effectiveness threshold
		if (ItemEffectiveness < MinEffectiveness)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - effectiveness too low (%.2f < %.2f)"),
				Source, *ItemDefinitionId.ToString(), ItemEffectiveness, MinEffectiveness);
			return false;
		}

		// Check if tool has durability remaining (if applicable)
		if (CurrentDurability == 0)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - broken (durability=0)"), Source, *ItemDefinitionId.ToString());
			return false; // Tool is broken
		}

		// Track the best effectiveness tool
		if (ItemEffectiveness > OutEffectiveness)
		{
			OutItemGuid = ItemGuid;
			OutEffectiveness = ItemEffectiveness;
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - MATCHED! Effectiveness=%.2f"),
				Source, *ItemDefinitionId.ToString(), ItemEffectiveness);
			return true;
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - effectiveness not better than current (%.2f vs %.2f)"),
			Source, *ItemDefinitionId.ToString(), ItemEffectiveness, OutEffectiveness);
		return false;
	};

	// First, check equipped items (hand slots) - these should have priority
	AActor* Owner = Inventory->GetOwner();
	if (IsValid(Owner))
	{
		UMOEquipmentComponent* EquipComp = Owner->FindComponentByClass<UMOEquipmentComponent>();
		if (IsValid(EquipComp))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: Found equipment component on '%s'"), *Owner->GetName());

			// Check left hand
			FMOEquippedItem LeftHand = EquipComp->GetEquippedItem(EMOEquipmentSlot::LeftHand);
			if (LeftHand.IsValid())
			{
				CheckToolItem(LeftHand.ItemDefinitionId, LeftHand.ItemGuid, LeftHand.CurrentDurability, TEXT("LeftHand"));
			}
			else
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: LeftHand empty"));
			}

			// Check right hand
			FMOEquippedItem RightHand = EquipComp->GetEquippedItem(EMOEquipmentSlot::RightHand);
			if (RightHand.IsValid())
			{
				CheckToolItem(RightHand.ItemDefinitionId, RightHand.ItemGuid, RightHand.CurrentDurability, TEXT("RightHand"));
			}
			else
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: RightHand empty"));
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: No equipment component on '%s'"), *Owner->GetName());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCrafting] FindBestTool: Inventory has no valid owner"));
	}

	// Then check inventory for potentially better tools
	TArray<FMOInventoryEntry> Entries;
	Inventory->GetInventoryEntries(Entries);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: Checking %d inventory entries"), Entries.Num());

	for (const FMOInventoryEntry& Entry : Entries)
	{
		CheckToolItem(Entry.ItemDefinitionId, Entry.ItemGuid, Entry.CurrentDurability, TEXT("Inventory"));
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] FindBestTool: Result - Found=%s, BestEffectiveness=%.2f"),
		OutItemGuid.IsValid() ? TEXT("yes") : TEXT("no"), OutEffectiveness);

	return OutItemGuid.IsValid();
}

void UMOCraftingSubsystem::DegradeToolsForRecipe(FName RecipeId, UMOInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || Recipe->RequiredTools.Num() == 0)
	{
		return;
	}

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		if (ToolReq.DurabilityConsumed <= 0)
		{
			continue;
		}

		FGuid ToolGuid;
		float ToolQuality;
		if (FindBestTool(Inventory, ToolReq.ToolType, ToolReq.MinQuality, ToolGuid, ToolQuality))
		{
			bool bDestroyed = false;
			Inventory->ReduceDurability(ToolGuid, ToolReq.DurabilityConsumed, bDestroyed);

			if (bDestroyed)
			{
				FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(ToolReq.ToolType));
				UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingSubsystem] Tool '%s' was destroyed during crafting"), *ToolTypeName);
			}
		}
	}
}

float UMOCraftingSubsystem::GetAdjustedCraftTime(FName RecipeId, UMOInventoryComponent* Inventory) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return 0.0f;
	}

	float BaseCraftTime = Recipe->CraftTime;

	// No tools required means no adjustment
	if (Recipe->RequiredTools.Num() == 0 || !IsValid(Inventory))
	{
		return BaseCraftTime;
	}

	// Calculate tool quality multiplier and missing optional tool penalty
	// Higher quality = faster crafting
	// Missing optional tools = time penalty (MissingToolTimeMultiplier)
	float TotalQuality = 0.0f;
	int32 ToolsWithQuality = 0;
	float MissingToolPenalty = 1.0f;

	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		FGuid ToolGuid;
		float ToolQuality;
		if (FindBestTool(Inventory, ToolReq.ToolType, ToolReq.MinQuality, ToolGuid, ToolQuality))
		{
			// Tool found - factor in its quality
			TotalQuality += ToolQuality;
			ToolsWithQuality++;
		}
		else if (!ToolReq.bIsRequired)
		{
			// Optional tool missing - apply time penalty
			MissingToolPenalty *= ToolReq.MissingToolTimeMultiplier;
		}
		// Required tools missing would have blocked crafting in CanCraftRecipe
	}

	// Calculate quality multiplier from tools that were found
	float QualityMultiplier = 1.0f;
	if (ToolsWithQuality > 0)
	{
		const float AverageQuality = TotalQuality / ToolsWithQuality;
		// Quality of 1.0 = base time, Quality of 2.0 = half time, etc.
		// Clamp minimum to 0.5 to prevent unreasonably fast crafting
		QualityMultiplier = (AverageQuality > KINDA_SMALL_NUMBER)
			? FMath::Max(0.5f, 1.0f / AverageQuality)
			: 1.0f;
	}

	// Apply both quality bonus and missing tool penalty
	// Example: Good knife (quality 1.5) but no shovel (5x penalty)
	// Result: BaseCraftTime * (1/1.5) * 5.0 = BaseCraftTime * 3.33
	return BaseCraftTime * QualityMultiplier * MissingToolPenalty;
}
