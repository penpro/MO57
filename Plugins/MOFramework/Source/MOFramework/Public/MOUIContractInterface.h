/**
 * =============================================================================
 * MOUIContractInterface.h - UI Operation Contract Interface
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Interface for controllers/actors that can provide UI operations. Decouples
 * building/container/crafting systems from specific controller types. Any
 * controller implementing this can show build widgets, open containers, etc.
 *
 * INTERFACE METHODS:
 * - RequestShowBuildWidget: Show UI for ghost building placement
 * - RequestOpenContainerInventory: Open inventory with container
 * - RequestOpenCraftingMenu: Open crafting (hand or station)
 * - RequestShowGhostContextMenu: Show context menu for ghost
 * - RequestShowStationContextMenu: Show context menu for station
 *
 * IMPLEMENTORS:
 * - AMOPlayerController: Primary implementor
 * - AI controllers may implement for NPC UI (future)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] BLUEPRINT NATIVE EVENT: All interface methods use BlueprintNativeEvent.
 *   Implement as virtual with _Implementation suffix in C++.
 *
 * [2024-02] WORLD POSITION: Context menu methods take WorldPosition, not screen
 *   position. Caller converts to screen space as needed.
 *
 * [2024-02] NULL STATION: RequestOpenCraftingMenu with nullptr = hand crafting.
 *
 * =============================================================================
 * RELATED FILES: MOPlayerController.h, MOBuildableActor.h, MOCraftingStationActor.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOUIContractInterface.generated.h"

/**
 * Interface for controllers/actors that can provide UI operations.
 * Decouples building/container/crafting systems from specific controller types.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UMOUIContractInterface : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API IMOUIContractInterface
{
	GENERATED_BODY()

public:
	/**
	 * Request to show the build widget for a ghost building.
	 * @param BuildingActor - The ghost building to show UI for
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|UI")
	void RequestShowBuildWidget(AActor* BuildingActor);

	/**
	 * Request to open inventory UI with a container.
	 * @param ContainerActor - The container to open
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|UI")
	void RequestOpenContainerInventory(AActor* ContainerActor);

	/**
	 * Request to open the crafting menu.
	 * @param StationActor - Optional crafting station (nullptr for hand crafting)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|UI")
	void RequestOpenCraftingMenu(AActor* StationActor);

	/**
	 * Request to show the ghost context menu for a building.
	 * @param GhostActor - The ghost building
	 * @param ScreenPosition - Where to show the menu
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|UI")
	void RequestShowGhostContextMenu(AActor* GhostActor, FVector WorldPosition);

	/**
	 * Request to show the station context menu for a crafting station.
	 * @param StationActor - The crafting station
	 * @param WorldPosition - World position for menu placement
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|UI")
	void RequestShowStationContextMenu(AActor* StationActor, FVector WorldPosition);
};
