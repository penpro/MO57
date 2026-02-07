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
