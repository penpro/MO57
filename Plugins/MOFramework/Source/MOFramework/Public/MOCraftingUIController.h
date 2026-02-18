#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOInteractorComponent.h"
#include "MOCraftingUIController.generated.h"

class UMOCraftingMenu;
class UMOStationContextMenu;
class UMOKeepOnHarvestContextMenu;
class UMOHarvestProgressWidget;
class AMOCraftingStationActor;
struct FMOCraftResult;

/**
 * Specialized UI controller for crafting-related UI.
 *
 * Handles:
 * - Crafting menu (toggle, open, close)
 * - Station context menus (open, craft, light actions)
 * - Keep-on-harvest context menus (ISM/HISM targets)
 * - Harvest operations and progress
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the crafting UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCraftingUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOCraftingUIController();

	// ==========================================================================
	// CRAFTING MENU
	// ==========================================================================

	/** Toggle crafting menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void ToggleCraftingMenu();

	/** Open the crafting menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void OpenCraftingMenu();

	/** Close the crafting menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	void CloseCraftingMenu();

	/** Check if crafting menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting")
	bool IsCraftingMenuOpen() const;

	/** Get the crafting menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|Crafting")
	UMOCraftingMenu* GetCraftingMenu() const;

	// ==========================================================================
	// STATION CONTEXT MENU
	// ==========================================================================

	/** Show the station context menu for a crafting station. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Station")
	void ShowStationContextMenu(AActor* StationActor, FVector WorldPosition);

	/** Hide the station context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Station")
	void HideStationContextMenu();

	/** Check if station context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Station")
	bool IsStationContextMenuOpen() const;

	// ==========================================================================
	// KEEPONHARVEST CONTEXT MENU
	// ==========================================================================

	/** Show the keep-on-harvest context menu for an ISM/HISM target. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void ShowKeepOnHarvestContextMenu(const FMOInteractionTarget& Target);

	/** Hide the keep-on-harvest context menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void HideKeepOnHarvestContextMenu();

	/** Check if keep-on-harvest context menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Harvest")
	bool IsKeepOnHarvestContextMenuOpen() const;

	// ==========================================================================
	// HARVEST OPERATIONS
	// ==========================================================================

	/** Start a harvest operation on the current target. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void StartHarvestOperation(FName RecipeId);

	/** Cancel any active harvest operation. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting|Harvest")
	void CancelHarvestOperation();

	/** Check if a harvest operation is in progress. */
	UFUNCTION(BlueprintPure, Category="MO|Crafting|Harvest")
	bool IsHarvestInProgress() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Crafting Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOCraftingMenu> CraftingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 CraftingMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOCraftingMenu> CraftingMenuWidget;

	UFUNCTION()
	void HandleCraftingMenuRequestClose();

	// --- Station Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Station", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOStationContextMenu> StationContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Station", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 StationContextMenuZOrder = 60;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOStationContextMenu> StationContextMenuWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AMOCraftingStationActor> CurrentStationTarget;

	UFUNCTION()
	void HandleStationContextMenuRequestClose();

	UFUNCTION()
	void HandleStationContextMenuOpen();

	UFUNCTION()
	void HandleStationContextMenuCraft();

	UFUNCTION()
	void HandleStationContextMenuLight();

	// --- KeepOnHarvest Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOKeepOnHarvestContextMenu> KeepOnHarvestContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOHarvestProgressWidget> HarvestProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 KeepOnHarvestContextMenuZOrder = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Crafting|Harvest", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 HarvestProgressZOrder = 200;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOKeepOnHarvestContextMenu> KeepOnHarvestContextMenuWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOHarvestProgressWidget> HarvestProgressWidget;

	/** The current interaction target for harvest operations. */
	FMOInteractionTarget CurrentHarvestTarget;

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuRequestClose();

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuInspectClicked();

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuHarvestClicked(FName RecipeId);

	UFUNCTION()
	void HandleKeepOnHarvestContextMenuChopDownClicked();

	UFUNCTION()
	void HandleHarvestCompleted(bool bCompleted, const FMOCraftResult& Result);

	UFUNCTION()
	void HandleHarvestCancelled();
};
