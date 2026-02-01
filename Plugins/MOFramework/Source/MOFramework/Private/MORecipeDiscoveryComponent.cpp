#include "MORecipeDiscoveryComponent.h"
#include "MOFramework.h"
#include "MOCraftingSubsystem.h"
#include "MORecipeDatabaseSettings.h"
#include "Net/UnrealNetwork.h"

UMORecipeDiscoveryComponent::UMORecipeDiscoveryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMORecipeDiscoveryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache crafting subsystem
	if (UWorld* World = GetWorld())
	{
		CachedCraftingSubsystem = World->GetSubsystem<UMOCraftingSubsystem>();
	}
}

void UMORecipeDiscoveryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMORecipeDiscoveryComponent, DiscoveredRecipes);
	DOREPLIFETIME(UMORecipeDiscoveryComponent, ExperimentationHistory);
}

bool UMORecipeDiscoveryComponent::DiscoverRecipe(FName RecipeId, EMODiscoveryMethod Method)
{
	if (RecipeId.IsNone())
	{
		return false;
	}

	// Check if already discovered
	if (DiscoveredRecipes.Contains(RecipeId))
	{
		return false;
	}

	// Verify recipe exists
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MORecipeDiscovery] Attempted to discover non-existent recipe: %s"), *RecipeId.ToString());
		return false;
	}

	// Add to discovered list
	DiscoveredRecipes.Add(RecipeId);

	UE_LOG(LogMOFramework, Log, TEXT("[MORecipeDiscovery] Discovered recipe '%s' via %d"), *RecipeId.ToString(), (int32)Method);

	// Broadcast discovery
	OnRecipeDiscovered.Broadcast(RecipeId, Method);

	return true;
}

void UMORecipeDiscoveryComponent::CheckDiscoveryFromKnowledge(FName KnowledgeId)
{
	if (KnowledgeId.IsNone())
	{
		return;
	}

	// Get all recipe IDs and check which ones should be unlocked by this knowledge
	TArray<FName> AllRecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(AllRecipeIds);

	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Check if this recipe requires discovery and is unlocked by this knowledge
		if (Recipe->bRequiresDiscovery && Recipe->DiscoveryKnowledgeId == KnowledgeId)
		{
			DiscoverRecipe(RecipeId, EMODiscoveryMethod::Inspection);
		}
	}
}

void UMORecipeDiscoveryComponent::CheckDiscoveryFromSkillLevel(FName SkillId, int32 NewLevel)
{
	if (SkillId.IsNone() || NewLevel <= 0)
	{
		return;
	}

	// Get all recipe IDs and check which ones should be unlocked at this skill level
	TArray<FName> AllRecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(AllRecipeIds);

	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Check if this recipe requires discovery, uses this skill, and is unlocked at this level
		if (Recipe->bRequiresDiscovery &&
			Recipe->RequiredSkillId == SkillId &&
			Recipe->DiscoverySkillLevel > 0 &&
			Recipe->DiscoverySkillLevel <= NewLevel)
		{
			DiscoverRecipe(RecipeId, EMODiscoveryMethod::SkillLevel);
		}
	}
}

bool UMORecipeDiscoveryComponent::TryExperiment(const TArray<FName>& Ingredients, FName& OutDiscoveredRecipe)
{
	OutDiscoveredRecipe = NAME_None;

	if (Ingredients.Num() == 0)
	{
		return false;
	}

	// Compute hash for this ingredient combination
	int32 IngredientHash = ComputeIngredientHash(Ingredients);

	// Check if we've already tried this combination
	if (ExperimentationHistory.Contains(IngredientHash))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MORecipeDiscovery] Already tried this ingredient combination"));
		return false;
	}

	// Record this attempt
	ExperimentationHistory.Add(IngredientHash);

	// Find recipes that match these ingredients
	TArray<FName> MatchingRecipes;
	FindRecipesMatchingIngredients(Ingredients, MatchingRecipes);

	// Filter to undiscovered recipes that require discovery
	for (const FName& RecipeId : MatchingRecipes)
	{
		if (DiscoveredRecipes.Contains(RecipeId))
		{
			continue;
		}

		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe || !Recipe->bRequiresDiscovery)
		{
			continue;
		}

		// Found a recipe! Discover it.
		DiscoverRecipe(RecipeId, EMODiscoveryMethod::Experimentation);
		OutDiscoveredRecipe = RecipeId;
		return true;
	}

	// No recipe found - broadcast failure
	OnExperimentFailed.Broadcast(Ingredients);
	return false;
}

bool UMORecipeDiscoveryComponent::IsRecipeDiscovered(FName RecipeId) const
{
	if (RecipeId.IsNone())
	{
		return false;
	}

	// Check if the recipe requires discovery at all
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return false;
	}

	// If it doesn't require discovery, it's always "discovered"
	if (!Recipe->bRequiresDiscovery)
	{
		return true;
	}

	return DiscoveredRecipes.Contains(RecipeId);
}

void UMORecipeDiscoveryComponent::GetAllDiscoveredRecipes(TArray<FName>& OutRecipeIds) const
{
	OutRecipeIds = DiscoveredRecipes;
}

bool UMORecipeDiscoveryComponent::HasTriedExperiment(const TArray<FName>& Ingredients) const
{
	int32 IngredientHash = ComputeIngredientHash(Ingredients);
	return ExperimentationHistory.Contains(IngredientHash);
}

void UMORecipeDiscoveryComponent::BuildSaveData(FMORecipeDiscoverySaveData& OutSaveData) const
{
	OutSaveData.DiscoveredRecipes = DiscoveredRecipes;
	OutSaveData.ExperimentationHistory = ExperimentationHistory;
}

bool UMORecipeDiscoveryComponent::ApplySaveData(const FMORecipeDiscoverySaveData& InSaveData)
{
	DiscoveredRecipes = InSaveData.DiscoveredRecipes;
	ExperimentationHistory = InSaveData.ExperimentationHistory;

	UE_LOG(LogMOFramework, Log, TEXT("[MORecipeDiscovery] Loaded %d discovered recipes and %d experiment attempts"),
		DiscoveredRecipes.Num(), ExperimentationHistory.Num());

	return true;
}

void UMORecipeDiscoveryComponent::ClearAllDiscoveries()
{
	DiscoveredRecipes.Empty();
	ExperimentationHistory.Empty();
}

int32 UMORecipeDiscoveryComponent::ComputeIngredientHash(const TArray<FName>& Ingredients) const
{
	// Sort ingredients to make hash order-independent
	TArray<FName> SortedIngredients = Ingredients;
	SortedIngredients.Sort([](const FName& A, const FName& B) {
		return A.LexicalLess(B);
	});

	// Combine hashes of all ingredients
	uint32 Hash = 0;
	for (const FName& Ingredient : SortedIngredients)
	{
		Hash = HashCombine(Hash, GetTypeHash(Ingredient));
	}

	// Cast to int32 for Blueprint compatibility (hash collision is acceptable for this use case)
	return static_cast<int32>(Hash);
}

void UMORecipeDiscoveryComponent::FindRecipesMatchingIngredients(const TArray<FName>& Ingredients, TArray<FName>& OutMatchingRecipes) const
{
	OutMatchingRecipes.Empty();

	// Build a map of ingredient counts
	TMap<FName, int32> IngredientCounts;
	for (const FName& Ingredient : Ingredients)
	{
		IngredientCounts.FindOrAdd(Ingredient)++;
	}

	// Get all recipe IDs
	TArray<FName> AllRecipeIds;
	UMORecipeDatabaseSettings::GetAllRecipeIds(AllRecipeIds);

	for (const FName& RecipeId : AllRecipeIds)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Build ingredient count map for this recipe
		TMap<FName, int32> RecipeIngredients;
		for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
		{
			RecipeIngredients.FindOrAdd(Ingredient.ItemDefinitionId) += Ingredient.Quantity;
		}

		// Check if the provided ingredients match the recipe exactly
		if (RecipeIngredients.Num() != IngredientCounts.Num())
		{
			continue;
		}

		bool bMatches = true;
		for (const auto& Pair : RecipeIngredients)
		{
			const int32* ProvidedCount = IngredientCounts.Find(Pair.Key);
			if (!ProvidedCount || *ProvidedCount != Pair.Value)
			{
				bMatches = false;
				break;
			}
		}

		if (bMatches)
		{
			OutMatchingRecipes.Add(RecipeId);
		}
	}
}
