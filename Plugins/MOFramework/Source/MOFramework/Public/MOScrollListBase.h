/**
 * =============================================================================
 * MOScrollListBase.h - Generic Scrollable List Widget Base
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Abstract base class for scrollable list widgets. Manages a collection of
 * entry widgets with selection, filtering, and population. Subclass for
 * recipe lists, skill lists, character lists, etc.
 *
 * SUBCLASSES:
 * - UMORecipeListWidget (existing - consider migrating)
 * - UMOBuildingRecipeListWidget (existing - consider migrating)
 * - UMOColonyCharacterList (planned)
 *
 * WIDGET BINDINGS (in Blueprint):
 * - ContentScrollBox (UScrollBox, optional) - Scrollable container
 * - ContentContainer (UVerticalBox, optional) - Alternative container
 *
 * ENTRY WIDGET:
 * - EntryWidgetClass must be set to a UMOListEntryBase subclass
 * - Entries are created automatically when PopulateList() is called
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] ENTRY CLASS: EntryWidgetClass MUST be set before PopulateList().
 *   If null, entries won't be created. Set in Blueprint defaults.
 *
 * [2026-03] WIDGET POOLING: Current implementation creates/destroys widgets.
 *   For 100+ entries, consider overriding with pooled implementation.
 *
 * [2026-03] REFRESH vs REPOPULATE: RefreshEntryStates() updates existing
 *   entries. PopulateList() recreates all entries. Use Refresh when only
 *   state changes, Populate when data changes.
 *
 * =============================================================================
 * RELATED FILES: MOListEntryBase.h, MORecipeListWidget.h, MOUIDelegates.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOUIDelegates.h"
#include "MOScrollListBase.generated.h"

class UMOListEntryBase;
class UScrollBox;
class UVerticalBox;

/**
 * Delegate fired when an entry is selected in the list.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOScrollListSelectionDelegate, FName, SelectedId);

/**
 * Base class for scrollable list widgets.
 * See file header for usage and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOScrollListBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOScrollListBase(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// POPULATION
	// ============================================================================

	/**
	 * Populate the list with entry IDs.
	 * Creates entry widgets for each ID.
	 * @param EntryIds Array of entry identifiers
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void PopulateList(const TArray<FName>& EntryIds);

	/**
	 * Clear all entries from the list.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void ClearList();

	/**
	 * Refresh the visual state of all entries without recreating them.
	 * Subclasses override RefreshEntryState() to update individual entries.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void RefreshEntryStates();

	/**
	 * Get the number of entries in the list.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	int32 GetEntryCount() const { return EntryWidgets.Num(); }

	/**
	 * Get all current entry IDs.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	const TArray<FName>& GetEntryIds() const { return CurrentEntryIds; }

	// ============================================================================
	// SELECTION
	// ============================================================================

	/**
	 * Select an entry by ID.
	 * @param EntryId ID of entry to select (NAME_None to clear selection)
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|List")
	virtual void SelectEntry(FName EntryId);

	/**
	 * Get the currently selected entry ID.
	 * @return Selected entry ID, or NAME_None if nothing selected
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	FName GetSelectedEntryId() const { return SelectedEntryId; }

	/**
	 * Check if any entry is selected.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	bool HasSelection() const { return SelectedEntryId != NAME_None; }

	// ============================================================================
	// ENTRY ACCESS
	// ============================================================================

	/**
	 * Get an entry widget by ID.
	 * @param EntryId ID of the entry to find
	 * @return Entry widget, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	UMOListEntryBase* GetEntryById(FName EntryId) const;

	/**
	 * Get all entry widgets.
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|List")
	const TArray<UMOListEntryBase*>& GetAllEntries() const;

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when an entry is selected. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|List")
	FMOScrollListSelectionDelegate OnEntrySelected;

	/** Broadcast when selection is cleared. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|List")
	FMOUIRequestClose OnSelectionCleared;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Widget class to use for entries. Must be a subclass of UMOListEntryBase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|List")
	TSubclassOf<UMOListEntryBase> EntryWidgetClass;

protected:
	virtual void NativeConstruct() override;

	/**
	 * Called to configure an entry after creation.
	 * Override in subclasses to pass additional data.
	 * @param Entry The entry widget to configure
	 * @param EntryId The entry's identifier
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|List")
	void ConfigureEntry(UMOListEntryBase* Entry, FName EntryId);

	/**
	 * Called to refresh a single entry's state.
	 * Override in subclasses to update entry visuals.
	 * @param Entry The entry widget to refresh
	 * @param EntryId The entry's identifier
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|List")
	void RefreshEntryState(UMOListEntryBase* Entry, FName EntryId);

	/**
	 * Called when an entry is clicked.
	 */
	UFUNCTION()
	void HandleEntryClicked(FName EntryId);

	/**
	 * Get the container widget where entries should be added.
	 * Prefers ContentScrollBox, falls back to ContentContainer.
	 * Override in subclasses that use different widget bindings.
	 */
	virtual UPanelWidget* GetContainer() const;

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ContentScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ContentContainer;

private:
	/** Current entry IDs. */
	TArray<FName> CurrentEntryIds;

	/** Created entry widgets. */
	UPROPERTY()
	TArray<TObjectPtr<UMOListEntryBase>> EntryWidgets;

	/** Temp array for GetAllEntries() return value. */
	mutable TArray<UMOListEntryBase*> CachedEntryPtrs;

	/** Currently selected entry ID. */
	FName SelectedEntryId = NAME_None;
};
