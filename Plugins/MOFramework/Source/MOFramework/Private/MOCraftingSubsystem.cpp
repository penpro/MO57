#include "MOCraftingSubsystem.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOFramework.h"

void UMOCraftingSubsystem::GetAvailableRecipes(
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	EMOCraftingStation Station,
	TArray<FName>& OutRecipeIds
) const
{
	OutRecipeIds.Empty();

	TArray<FName> AllRecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(AllRecipeIds);

	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Check station requirement
		if (Recipe->RequiredStation != EMOCraftingStation::None && Recipe->RequiredStation != Station)
		{
			continue;
		}

		// Check knowledge requirements
		if (!HasRequiredKnowledge(Recipe, KnowledgeComponent))
		{
			continue;
		}

		// Check skill requirements
		if (!MeetsSkillRequirements(Recipe, SkillsComponent))
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

	// Check station
	if (Recipe->RequiredStation != EMOCraftingStation::None && Recipe->RequiredStation != Station)
	{
		Result.bCorrectStation = false;
		Result.FailureReason = FText::FromString(TEXT("Wrong crafting station."));
		return Result;
	}

	// Check knowledge
	TArray<FName> MissingKnowledge;
	if (!HasRequiredKnowledge(Recipe, KnowledgeComponent, &MissingKnowledge))
	{
		Result.MissingKnowledge = MissingKnowledge;
		Result.FailureReason = FText::FromString(TEXT("Missing required knowledge."));
		return Result;
	}

	// Check skill
	int32 RequiredLevel = 0;
	int32 CurrentLevel = 0;
	if (!MeetsSkillRequirements(Recipe, SkillsComponent, &RequiredLevel, &CurrentLevel))
	{
		Result.RequiredSkillLevel = RequiredLevel;
		Result.CurrentSkillLevel = CurrentLevel;
		Result.FailureReason = FText::Format(
			NSLOCTEXT("MOCrafting", "SkillTooLow", "Skill level too low ({0}/{1})."),
			FText::AsNumber(CurrentLevel),
			FText::AsNumber(RequiredLevel)
		);
		return Result;
	}

	// Check ingredients
	TMap<FName, int32> MissingIngredients;
	if (!HasIngredients(Recipe, InventoryComponent, KnowledgeComponent, &MissingIngredients))
	{
		Result.MissingIngredients = MissingIngredients;
		Result.FailureReason = FText::FromString(TEXT("Missing ingredients."));
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

	Result.bSuccess = true;

	OnCraftCompleted.Broadcast(RecipeId, Result);

	UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingSubsystem] Crafted '%s' successfully"), *RecipeId.ToString());

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
	TArray<FName>* OutMissingKnowledge
) const
{
	if (!Recipe || Recipe->RequiredKnowledge.Num() == 0)
	{
		return true;
	}

	if (!IsValid(KnowledgeComponent))
	{
		if (OutMissingKnowledge)
		{
			*OutMissingKnowledge = Recipe->RequiredKnowledge;
		}
		return false;
	}

	bool bHasAll = true;
	for (const FName& KnowledgeId : Recipe->RequiredKnowledge)
	{
		if (!KnowledgeComponent->HasKnowledge(KnowledgeId))
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
