#pragma once

#include "CoreMinimal.h"
#include "MOItemInstanceState.h"
#include "MOIngredientReservation.generated.h"

struct FMOInventoryEntry;
struct FMOItemDefinitionRow;

UENUM(BlueprintType)
enum class EMOIngredientFailure : uint8
{
	None, InvalidInput, UnknownDefinition, MissingIngredients, Capacity, IdentityConflict, NotAuthority
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOIngredientOperationResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="MO|Ingredients") EMOIngredientFailure Failure = EMOIngredientFailure::None;
	UPROPERTY(BlueprintReadOnly, Category="MO|Ingredients") FName IngredientId;
	UPROPERTY(BlueprintReadOnly, Category="MO|Ingredients") int32 MissingUnits = 0;
	bool IsComplete() const { return Failure == EMOIngredientFailure::None; }
};

/** One owned ingredient lot. Physical stacks carry quantity/identity/state; drawn liquid carries its actual profile and volume. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOIngredientLot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") FName IngredientId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") FGuid ItemGuid;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") FName ItemDefinitionId;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") int32 Quantity = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") FMOItemInstanceState State;
	/** Drawn liquid only: original vessel, preferred when returning it. This is a reference, not ownership of the vessel. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") FGuid SourceContainerGuid;
	/** Zero for physical items. Positive = the measure size when the liquid was reserved; survives definition changes. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") float MeasureMl = 0.0f;
	/** Fresh products follow the existing same-safety pouring policy; captured refunds require an exact profile. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") bool bMayMixCompatibleLiquids = false;
	bool IsLiquid() const { return MeasureMl > 0.0f; }
	bool HasContents() const { return IsLiquid() ? State.Liquid.Millilitres > 0.0f : Quantity > 0; }
};

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOIngredientReservation
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="MO|Ingredients") TArray<FMOIngredientLot> Lots;
	bool IsEmpty() const { return Lots.IsEmpty(); }
};

/** Pure plans over entry snapshots: no world, delegates, inventory mutation or DataTable dependency. */
namespace MOIngredientPlans
{
	using FDefinitionLookup = TFunctionRef<bool(FName, FMOItemDefinitionRow&)>;
	/** Definition-driven fresh products. Refunds must always use captured lots, never this factory. */
	MOFRAMEWORK_API FMOIngredientOperationResult CreateProducts(const TMap<FName, int32>& Quantities,
		FMOIngredientReservation& OutProducts, FDefinitionLookup Definition);
	/** Preserve the existing 0.5 ml tolerance for 250 ml measures, bounded for smaller modded measures. */
	inline double MeasureToleranceMl(float MeasureMl) { return FMath::Min(0.5, double(MeasureMl) * 0.002); }
	/** Atomic: failure changes neither Entries nor OutReservation. Safe-water requirements allocate before unrestricted water. */
	MOFRAMEWORK_API FMOIngredientOperationResult Reserve(TArray<FMOInventoryEntry>& Entries, const TMap<FName, int32>& Requirements,
		FMOIngredientReservation& OutReservation, FDefinitionLookup Definition, const TSet<FGuid>* ExcludedPhysicalItems = nullptr);
	/** Atomic removal of one craft's original requirements from its owned lots. No live recipe/definition lookup. */
	MOFRAMEWORK_API FMOIngredientOperationResult Consume(FMOIngredientReservation& Reservation, const TMap<FName, int32>& Requirements);
	/** Convert drawn lots into transport of their actual source vessels. Moves all liquid in those vessels, including unrequested remainder.
	 * SourceEntries is the post-reservation snapshot. Failure changes no input or output; callers still plan destination capacity before committing. */
	MOFRAMEWORK_API FMOIngredientOperationResult CarrySourceContainers(TArray<FMOInventoryEntry>& SourceEntries,
		FMOIngredientReservation& Transfer, TArray<FGuid>& OutCarriedContainers, FDefinitionLookup Definition);
	/** Add specific remaining physical entries to a transport plan, or recognize vessels already carried by that plan. */
	MOFRAMEWORK_API FMOIngredientOperationResult CarryItems(TArray<FMOInventoryEntry>& SourceEntries,
		FMOIngredientReservation& Transfer, const TArray<FGuid>& ItemGuids);
	/** Places what fits; every unplaced unit/ml stays owned in Reservation. Never invents liquid from an ingredient name. */
	MOFRAMEWORK_API FMOIngredientOperationResult Return(TArray<FMOInventoryEntry>& Entries, int32 SlotCount,
		FMOIngredientReservation& Reservation, FDefinitionLookup Definition);
}
