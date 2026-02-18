#pragma once

#include "CoreMinimal.h"
#include "MOUIControllerBase.h"
#include "MOBuildingUIController.generated.h"

class UMOBuildingMenu;
class UMOBuildWidget;
class UMOGhostContextMenu;
class AMOBuildableActor;

/**
 * Specialized UI controller for building-related UI.
 *
 * Handles:
 * - Building menu (toggle, open, close)
 * - Ghost context menu (for building ghost interaction)
 * - Build widget (construction progress)
 *
 * This controller is extracted from MOUIManagerComponent to reduce its size
 * and provide clear ownership of the building UI subsystem.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOBuildingUIController : public UMOUIControllerBase
{
	GENERATED_BODY()

public:
	UMOBuildingUIController();

	// ==========================================================================
	// BUILDING MENU
	// ==========================================================================

	/** Toggle building menu visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ToggleBuildingMenu();

	/** Open the building menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void OpenBuildingMenu();

	/** Close the building menu. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void CloseBuildingMenu();

	/** Check if building menu is open. */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsBuildingMenuOpen() const;

	/** Get the building menu widget (may be null if not open). */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	UMOBuildingMenu* GetBuildingMenu() const;

	// ==========================================================================
	// GHOST CONTEXT MENU (BUILD WIDGET)
	// ==========================================================================

	/** Show the build widget for a ghost building. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void ShowBuildWidget(AMOBuildableActor* Target);

	/** Hide the build widget. */
	UFUNCTION(BlueprintCallable, Category="MO|Building")
	void HideBuildWidget();

	/** Check if build widget is open. */
	UFUNCTION(BlueprintPure, Category="MO|Building")
	bool IsBuildWidgetOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Building Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOBuildingMenu> BuildingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 BuildingMenuZOrder = 50;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOBuildingMenu> BuildingMenuWidget;

	UFUNCTION()
	void HandleBuildingMenuRequestClose();

	UFUNCTION()
	void HandleBuildingSelected(FName RecipeId);

	// --- Ghost Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOGhostContextMenu> GhostContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Building", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 GhostContextMenuZOrder = 60;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOGhostContextMenu> GhostContextMenuWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AMOBuildableActor> CurrentBuildTarget;

	UFUNCTION()
	void HandleGhostContextMenuRequestClose();

	UFUNCTION()
	void HandleGhostContextMenuBuildStarted();

	UFUNCTION()
	void HandleGhostContextMenuCancelled();
};
