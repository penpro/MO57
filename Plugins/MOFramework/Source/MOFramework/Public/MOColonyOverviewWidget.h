/**
 * =============================================================================
 * MOColonyOverviewWidget.h - Colony overview (V1 UI half, gate b)
 * =============================================================================
 *
 * PURPOSE:
 * The "window into the community": roster rows (name, mood, housed, current
 * job) with a per-villager Assign-Craft-Job action that drives the REAL
 * CraftAtStation flow (V0) — resolve nearest station + communal storage,
 * EnqueueCraftJob on the villager's queue.
 *
 * CONSTRUCTION:
 * The widget tree is built in C++ (WidgetTree->ConstructWidget) so the whole
 * flow is testable without WBP layout. Buttons carry STABLE object names
 * ("ColonyAssignJob_<i>", "ColonyClose") — the MO.Test.ClickWidget harness
 * locates UMOCommonButton by name and SimulateClick()s through the CommonUI
 * guards. A WBP subclass can later re-skin this without touching the flow
 * (the known editor-work slice; native CommonButtons render unstyled).
 *
 * =============================================================================
 * RELATED FILES: MOColonyManagerSubsystem.h, MOGameUIManagerSubsystem.h
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"

#include "MOColonyOverviewWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class APawn;

UCLASS()
class MOFRAMEWORK_API UMOColonyOverviewWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Recipe the Assign button queues (V1 default: the V0 fun-gate recipe). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Colony|UI")
	FName DefaultCraftRecipeId = TEXT("KnapFlintFlakes");

	/** Rebuild the roster rows from the live colony state. */
	UFUNCTION(BlueprintCallable, Category="MO|Colony|UI")
	void RebuildRoster();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	void HandleAssignJob(int32 VillagerIndex);
	void HandleClose();

	UPROPERTY() TObjectPtr<UVerticalBox> RootBox;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UVerticalBox> RosterBox;

	/** Pawns backing the roster rows, index-matched to the buttons. */
	TArray<TWeakObjectPtr<APawn>> RosterPawns;
};
