#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOUIManagerComponent.generated.h"

class APlayerController;
class UMOInventoryComponent;
class UMOInventoryMenu;
class UMOReticleWidget;
class UMOInGameMenu;
class UMOItemContextMenu;
class UMOConfirmationDialog;
class UCommonActivatableWidget;
class UMOPlayerStatusWidget;
class UMOModalBackground;

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOUIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOUIManagerComponent();

	// Called by your IA_Inventory binding (or any other UI action).
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void ToggleInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void OpenInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CloseInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	bool IsInventoryMenuOpen() const;

	/** Show or hide the reticle. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void SetReticleVisible(bool bVisible);

	/** Check if the reticle is currently visible. */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	bool IsReticleVisible() const;

	/** Get the reticle widget (may be null if not created yet). */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	UMOReticleWidget* GetReticleWidget() const;

	// --- Player Status HUD ---

	/** Toggle player status HUD visibility. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void TogglePlayerStatus();

	/** Get the player status widget (may be null if not created yet). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Status")
	UMOPlayerStatusWidget* GetPlayerStatusWidget() const;

	/** Show or hide the player status HUD. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Status")
	void SetPlayerStatusVisible(bool bVisible);

	/** Check if player status HUD is visible. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Status")
	bool IsPlayerStatusVisible() const;

	// --- In-Game Menu ---

	/** Toggle in-game menu (Tab key behavior). Closes other menus first. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void ToggleInGameMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void OpenInGameMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI|InGameMenu")
	void CloseInGameMenu();

	UFUNCTION(BlueprintPure, Category="MO|UI|InGameMenu")
	bool IsInGameMenuOpen() const;

	// --- Item Context Menu ---

	/** Show context menu for an inventory item at the given screen position. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void ShowItemContextMenu(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid, int32 SlotIndex, FVector2D ScreenPosition);

	UFUNCTION(BlueprintCallable, Category="MO|UI|ContextMenu")
	void CloseItemContextMenu();

	UFUNCTION(BlueprintPure, Category="MO|UI|ContextMenu")
	bool IsItemContextMenuOpen() const;

	// --- Confirmation Dialogs ---

	/** Show a confirmation dialog. Returns immediately; result comes via delegates. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Confirmation")
	void ShowConfirmationDialog(const FText& Title, const FText& Message, const FText& ConfirmText, const FText& CancelText);

	/** Called when any confirmation is confirmed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationConfirmedSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Confirmation")
	FMOConfirmationConfirmedSignature OnConfirmationConfirmed;

	/** Called when any confirmation is cancelled. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOConfirmationCancelledSignature);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Confirmation")
	FMOConfirmationCancelledSignature OnConfirmationCancelled;

	// --- Menu Stack ---

	/** Check if any menu is currently open. */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	bool IsAnyMenuOpen() const;

	/** Close all open menus. */
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CloseAllMenus();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* ResolveOwningPlayerController() const;
	bool IsLocalOwningPlayerController() const;

	UMOInventoryComponent* ResolveCurrentPawnInventoryComponent() const;

	void ApplyInputModeForMenuClosed(APlayerController* PlayerController) const;

	UFUNCTION()
	void HandleInventoryMenuRequestClose();

	UFUNCTION()
	void HandleInventoryMenuSlotRightClicked(int32 SlotIndex, const FGuid& ItemGuid, FVector2D ScreenPosition);

private:
	// Set this in the component defaults (or on the component instance in your PlayerController BP).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInventoryMenu> InventoryMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InventoryMenuZOrder = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	bool bShowMouseCursorWhileMenuOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	bool bLockMovementWhileMenuOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true"))
	bool bLockLookWhileMenuOpen = true;

	// Weak pointer so we do not keep dead widgets alive.
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInventoryMenu> InventoryMenuWidget;

	// --- Reticle ---

	/** Widget class for the reticle. If not set, uses the default UMOReticleWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOReticleWidget> ReticleWidgetClass;

	/** Z-order for the reticle widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ReticleZOrder = 0;

	/** Whether to create the reticle automatically on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(AllowPrivateAccess="true"))
	bool bCreateReticleOnBeginPlay = true;

	/** Whether to hide the reticle when inventory menu is open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(AllowPrivateAccess="true"))
	bool bHideReticleWhenMenuOpen = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOReticleWidget> ReticleWidget;

	void CreateReticle();

	// --- Player Status HUD ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOPlayerStatusWidget> PlayerStatusWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 PlayerStatusZOrder = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	bool bCreatePlayerStatusOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Status", meta=(AllowPrivateAccess="true"))
	bool bHidePlayerStatusWhenMenuOpen = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOPlayerStatusWidget> PlayerStatusWidget;

	void CreatePlayerStatus();

	UFUNCTION()
	void HandlePlayerStatusRequestClose();

	// --- In-Game Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InGameMenu", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOInGameMenu> InGameMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|InGameMenu", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 InGameMenuZOrder = 100;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInGameMenu> InGameMenuWidget;

	UFUNCTION()
	void HandleInGameMenuRequestClose();

	UFUNCTION()
	void HandleInGameMenuExitToMainMenu();

	UFUNCTION()
	void HandleInGameMenuExitGame();

	UFUNCTION()
	void HandleSaveRequested(const FString& SlotName);

	UFUNCTION()
	void HandleLoadRequested(const FString& SlotName);

	// --- Item Context Menu ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ContextMenu", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOItemContextMenu> ItemContextMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|ContextMenu", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ItemContextMenuZOrder = 150;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOItemContextMenu> ItemContextMenuWidget;

	UFUNCTION()
	void HandleContextMenuClosed();

	UFUNCTION()
	void HandleContextMenuAction(FName ActionId, const FGuid& ItemGuid);

	// --- Confirmation Dialog ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Confirmation", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMOConfirmationDialog> ConfirmationDialogClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Confirmation", meta=(ClampMin="0", AllowPrivateAccess="true"))
	int32 ConfirmationDialogZOrder = 200;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOConfirmationDialog> ConfirmationDialogWidget;

	/** Stores context for pending confirmations. */
	FString PendingConfirmationContext;

	UFUNCTION()
	void HandleConfirmationConfirmed();

	UFUNCTION()
	void HandleConfirmationCancelled();

	// --- Helpers ---

	void ApplyInputModeForMenuOpen(APlayerController* PlayerController, UUserWidget* MenuWidget) const;
	void UpdateReticleVisibility();

	/** Drop item from inventory into world in front of player by GUID. */
	void DropItemToWorldByGuid(UMOInventoryComponent* InventoryComponent, const FGuid& ItemGuid);

	// --- Modal Background ---

	/** Shows the modal background behind menus. Click to close all menus. */
	void ShowModalBackground();

	/** Hides the modal background. */
	void HideModalBackground();

	UPROPERTY(Transient)
	TWeakObjectPtr<UMOModalBackground> ModalBackgroundWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 ModalBackgroundZOrder = 10;

	UFUNCTION()
	void HandleModalBackgroundClicked();
};
