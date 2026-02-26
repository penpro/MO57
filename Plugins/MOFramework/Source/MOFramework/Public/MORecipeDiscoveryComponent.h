/**
 * =============================================================================
 * MORecipeDiscoveryComponent.h - Recipe Discovery and Unlocking System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Tracks which recipes a character has discovered. Recipes can be unlocked
 * through knowledge/inspection, skill level ups, schematics, experimentation,
 * or manual methods. Works with MOCraftingSubsystem for availability checks.
 *
 * KEY RESPONSIBILITIES:
 * 1. Track discovered recipes in replicated array
 * 2. Process discovery from knowledge component events
 * 3. Process discovery from skill level-ups
 * 4. Support experimentation system for recipe discovery
 * 5. Track experimentation history to prevent duplicates
 * 6. Save/load discovery state
 *
 * OWNERSHIP:
 * - Owner: AMOCharacter pawn
 * - Lifespan: Exists for pawn lifetime
 *
 * DISCOVERY METHODS:
 * - Manual: Direct unlock (debug, starting recipes)
 * - KnowledgeUnlock: From inspecting items
 * - SkillLevel: From reaching skill thresholds
 * - Schematic: From finding/using schematic items
 * - Experimentation: From combining ingredients
 *
 * DISCOVERY FLOW:
 * CheckDiscoveryFromKnowledge(KnowledgeId)
 * -> Query recipes with RequiredKnowledge containing KnowledgeId
 * -> For each matching recipe: DiscoverRecipe(Id, KnowledgeUnlock)
 * -> OnRecipeDiscovered broadcast
 *
 * EXPERIMENTATION SYSTEM:
 * TryExperiment(Ingredients)
 * -> ComputeIngredientHash() for order-independent hash
 * -> Check ExperimentationHistory for duplicates
 * -> FindRecipesMatchingIngredients()
 * -> If match and not discovered: DiscoverRecipe()
 * -> If no match: Add to history, OnExperimentFailed broadcast
 *
 * CRITICAL PATTERNS:
 * 1. REPLICATED: DiscoveredRecipes array replicated to clients
 * 2. ORDER-INDEPENDENT: Experimentation hash ignores ingredient order
 * 3. SUBSYSTEM QUERY: Uses MOCraftingSubsystem for recipe lookups
 *
 * KNOWN PITFALLS:
 * 1. DUPLICATE CHECK: DiscoverRecipe returns false if already discovered
 * 2. HASH COLLISION: Theoretical possibility with ComputeIngredientHash
 * 3. SUBSYSTEM CACHE: CachedCraftingSubsystem may be stale
 *
 * RELATED FILES:
 * - MOCraftingSubsystem.h - Recipe availability checks
 * - MOKnowledgeComponent.h - Source of knowledge events
 * - MOSkillsComponent.h - Source of skill level events
 * - MOCraftingTypes.h - EMODiscoveryMethod, FMORecipeDiscoverySaveData
 *
 * TESTING CHECKLIST:
 * [ ] Recipe discovered via knowledge unlocks correctly
 * [ ] Recipe discovered via skill level works
 * [ ] Experimentation finds matching recipes
 * [ ] Duplicate experiments prevented
 * [ ] Save/load preserves discoveries
 * [ ] IsRecipeDiscovered query works
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOCraftingTypes.h"
#include "MORecipeDiscoveryComponent.generated.h"

class UMOCraftingSubsystem;
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMORecipeDiscoveryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMORecipeDiscoveryComponent();

	// --- Delegates ---

	/** Broadcast when a recipe is discovered. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Discovery")
	FMOOnRecipeDiscoveredSignature OnRecipeDiscovered;

	/** Broadcast when an experimentation attempt fails. */
	UPROPERTY(BlueprintAssignable, Category="MO|Crafting|Discovery")
	FMOOnExperimentFailedSignature OnExperimentFailed;

	// --- Discovery Methods ---

	/** Manually discover a recipe (e.g., for starting recipes or debug). */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	bool DiscoverRecipe(FName RecipeId, EMODiscoveryMethod Method = EMODiscoveryMethod::Manual);

	/** Check if inspecting an item should unlock any recipes, and unlock them. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	void CheckDiscoveryFromKnowledge(FName KnowledgeId);

	/** Check if reaching a skill level should unlock any recipes, and unlock them. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	void CheckDiscoveryFromSkillLevel(FName SkillId, int32 NewLevel);

	/**
	 * Attempt to discover a recipe by experimenting with ingredients.
	 * @param Ingredients Array of ItemDefinitionIds to combine
	 * @param OutDiscoveredRecipe If successful, the discovered recipe ID
	 * @return True if a recipe was discovered
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	bool TryExperiment(const TArray<FName>& Ingredients, FName& OutDiscoveredRecipe);

	// --- Query ---

	/** Check if a specific recipe has been discovered. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Discovery")
	bool IsRecipeDiscovered(FName RecipeId) const;

	/** Get all discovered recipe IDs. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	void GetAllDiscoveredRecipes(TArray<FName>& OutRecipeIds) const;

	/** Get the number of discovered recipes. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Discovery")
	int32 GetDiscoveredRecipeCount() const { return DiscoveredRecipes.Num(); }

	/** Check if an ingredient combination has already been tried. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Discovery")
	bool HasTriedExperiment(const TArray<FName>& Ingredients) const;

	// --- Save/Load ---

	/** Build save data from current state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	void BuildSaveData(FMORecipeDiscoverySaveData& OutSaveData) const;

	/** Apply save data to restore state. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	bool ApplySaveData(const FMORecipeDiscoverySaveData& InSaveData);

	/** Clear all discovery progress. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Discovery")
	void ClearAllDiscoveries();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Compute a hash for an ingredient combination (order-independent). */
	int32 ComputeIngredientHash(const TArray<FName>& Ingredients) const;

	/** Find recipes that match the given ingredients exactly. */
	void FindRecipesMatchingIngredients(const TArray<FName>& Ingredients, TArray<FName>& OutMatchingRecipes) const;

private:
	/** Set of recipe IDs that have been discovered. */
	UPROPERTY(Replicated)
	TArray<FName> DiscoveredRecipes;

	/** Set of experimentation hashes that have been tried (stored as int32 for BP compatibility). */
	UPROPERTY(Replicated)
	TArray<int32> ExperimentationHistory;

	/** Cached pointer to crafting subsystem. */
	UPROPERTY()
	TWeakObjectPtr<UMOCraftingSubsystem> CachedCraftingSubsystem;
};
