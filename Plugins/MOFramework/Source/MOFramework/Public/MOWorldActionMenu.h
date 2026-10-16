// MOWorldActionMenu.h -- the right-click menu of a world-action target (IMOWorldActionProvider: a carcass, later anything else with a list of actions).
//
// Lists the target's GetWorldActions as buttons (UMOUIUtils::RebuildActionButtons, the builder the station and ground menus use) under a heading. A click
// goes through FMOWorldActions::Submit (the server does it) and closes the menu; a disabled option shows its reason as the tooltip and, clicked anyway
// through a race, as a notification. Opened by UMOCraftingUIController::ShowWorldActionMenu on the owning client.
// Widget Blueprint: /MOFramework/UI/WorldActions/WBP_WorldActionMenu, built by Content/Python/ui_specs/kit_world_action_menu.py.

#pragma once

#include "CoreMinimal.h"
#include "MOContextMenuBase.h"
#include "MOWorldActionMenu.generated.h"

class UPanelWidget;
class UTextBlock;
class UMOCommonButton;

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOWorldActionMenu : public UMOContextMenuBase
{
	GENERATED_BODY()

public:
	/** Fill the menu for Target as seen by the owning player's pawn. False (and nothing shown) when Target offers nothing. */
	bool InitializeForTarget(AActor* Target);

	AActor* GetTarget() const { return TargetActor.Get(); }

protected:
	virtual void NativeDestruct() override;

	/** The column the action buttons go into (after Title, when Title is in it). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UPanelWidget> ButtonContainer;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The look the runtime buttons copy (kept collapsed in the Blueprint). */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> StyleButton;

private:
	void OnAction(FName ActionId);

	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMOCommonButton>> ActionButtons;
};
