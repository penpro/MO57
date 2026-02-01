#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOCraftingTypes.h"
#include "MORecipeDiscoveryComponent.generated.h"

class UMOCraftingSubsystem;

/**
 * Component that tracks which recipes a character has discovered.
 * Recipes can be discovered through inspection, skill level ups, schematics, or experimentation.
 */
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
