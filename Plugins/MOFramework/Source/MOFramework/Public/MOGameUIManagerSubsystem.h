// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Widgets/CommonActivatableWidgetContainer.h"  // Full include needed for template methods
#include "MOGameUIManagerSubsystem.generated.h"

class UMOPrimaryGameLayout;
class UMOActivatableWidget;
class UCommonActivatableWidget;
class APlayerController;

/**
 * World subsystem managing UI layers and widget stacking.
 *
 * This subsystem follows the Lyra UGameUIManagerSubsystem pattern but is custom-built
 * for MOFramework. It provides:
 * - Layer-based widget stacking (HUD, Game, GameOverlay, Menu, Modal)
 * - Automatic input mode management via CommonUI
 * - Widget lifecycle management
 *
 * Usage:
 *   UMOGameUIManagerSubsystem* UISub = UMOGameUIManagerSubsystem::Get(GetWorld());
 *   UISub->PushWidgetToLayer(LayerTag, WidgetClass);
 */
UCLASS()
class MOFRAMEWORK_API UMOGameUIManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UMOGameUIManagerSubsystem();

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem Interface

	/** Get the subsystem from a world context */
	UFUNCTION(BlueprintPure, Category = "MO|UI", meta = (WorldContext = "WorldContextObject"))
	static UMOGameUIManagerSubsystem* Get(const UObject* WorldContextObject);

	/** Notify subsystem that a local player has been added */
	void NotifyPlayerAdded(APlayerController* PlayerController);

	/** Notify subsystem that a local player is being removed */
	void NotifyPlayerRemoved(APlayerController* PlayerController);

	/** Get the primary game layout for a player controller */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	UMOPrimaryGameLayout* GetRootLayoutForPlayer(APlayerController* PlayerController) const;

	/** Get the primary game layout for the first local player */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	UMOPrimaryGameLayout* GetRootLayout() const;

	/**
	 * Push a widget to a specific UI layer.
	 * The widget will be created if WidgetClass is provided, or you can push an existing instance.
	 *
	 * @param LayerTag The gameplay tag identifying the layer (e.g., "MO.UI.Layer.Game")
	 * @param WidgetClass The widget class to instantiate and push
	 * @return The created widget, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	UCommonActivatableWidget* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/**
	 * Push a widget to a layer for a specific player.
	 *
	 * @param PlayerController The player controller to push the widget for
	 * @param LayerTag The gameplay tag identifying the layer
	 * @param WidgetClass The widget class to instantiate and push
	 * @return The created widget, or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	UCommonActivatableWidget* PushWidgetToLayerForPlayer(APlayerController* PlayerController, FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/**
	 * Push a widget to a layer, creating a new instance if needed.
	 *
	 * CommonUI stacks don't support adding existing widget instances directly.
	 * If the passed widget is already activated, returns it as-is.
	 * Otherwise, creates a NEW widget of the same class via the stack.
	 *
	 * IMPORTANT: The returned widget may be different from the passed widget!
	 * Callers must update their cached references with the returned value.
	 *
	 * @param LayerTag The gameplay tag identifying the layer
	 * @param Widget The widget instance (used for class lookup if new widget needed)
	 * @return The actual widget in the stack (may be different from passed widget)
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	UCommonActivatableWidget* PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget);

	/**
	 * Pop the topmost widget from a layer.
	 *
	 * @param LayerTag The gameplay tag identifying the layer
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	void PopWidgetFromLayer(FGameplayTag LayerTag);

	/**
	 * Clear all widgets from a layer.
	 *
	 * @param LayerTag The gameplay tag identifying the layer
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	void ClearLayer(FGameplayTag LayerTag);

	/**
	 * Check if any widget is active in the specified layer.
	 *
	 * @param LayerTag The gameplay tag identifying the layer
	 * @return True if the layer has active widgets
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	bool IsLayerActive(FGameplayTag LayerTag) const;

	/**
	 * Check if any menu (non-HUD) layer has active widgets.
	 * Replaces the old IsAnyMenuOpen() boolean OR pattern.
	 *
	 * @return True if any menu layer has active widgets
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	bool IsAnyMenuOpen() const;

	/**
	 * Get the number of active widgets across all menu layers.
	 * More efficient than checking each menu individually.
	 *
	 * @return Count of active menu widgets
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	int32 GetActiveMenuCount() const;

	// =========================================================================
	// LAYER ACCESS HELPERS (Convenience methods for common patterns)
	// =========================================================================

	/**
	 * Get the Game layer stack directly.
	 * Use this for gameplay menus (inventory, crafting, building, etc.)
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	UCommonActivatableWidgetContainerBase* GetGameLayer() const;

	/**
	 * Get the Menu layer stack directly.
	 * Use this for system menus (in-game menu, possession).
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	UCommonActivatableWidgetContainerBase* GetMenuLayer() const;

	/**
	 * Get the Modal layer stack directly.
	 * Use this for modal dialogs (confirmation dialogs).
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	UCommonActivatableWidgetContainerBase* GetModalLayer() const;

	/**
	 * Push a menu widget to the Game layer.
	 * Includes dedicated server guard.
	 *
	 * @param MenuClass The menu widget class to instantiate
	 * @return The created widget, or nullptr if failed or on dedicated server
	 */
	template<typename T>
	T* PushMenu(TSubclassOf<T> MenuClass)
	{
		// Safety: no UI on dedicated server
		if (IsRunningDedicatedServer())
		{
			return nullptr;
		}

		UCommonActivatableWidgetContainerBase* Stack = GetGameLayer();
		if (!Stack)
		{
			return nullptr;
		}

		return Cast<T>(Stack->AddWidget(MenuClass));
	}

	/**
	 * Push a modal widget to the Modal layer.
	 * Includes dedicated server guard.
	 *
	 * @param ModalClass The modal widget class to instantiate
	 * @return The created widget, or nullptr if failed or on dedicated server
	 */
	template<typename T>
	T* PushModal(TSubclassOf<T> ModalClass)
	{
		// Safety: no UI on dedicated server
		if (IsRunningDedicatedServer())
		{
			return nullptr;
		}

		UCommonActivatableWidgetContainerBase* Stack = GetModalLayer();
		if (!Stack)
		{
			return nullptr;
		}

		return Cast<T>(Stack->AddWidget(ModalClass));
	}

	/**
	 * Remove the active widget from the Game layer.
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	void RemoveActiveMenu();

	/**
	 * Check if a specific menu type is currently the active widget on the Game layer.
	 * Use this for toggle key handling.
	 *
	 * @param MenuClass The menu class to check for
	 * @return True if the active widget is of this type
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI")
	bool IsMenuTypeActive(TSubclassOf<UCommonActivatableWidget> MenuClass) const;

	// =========================================================================
	// ACTIVE WIDGET TRACKING (for outside-click handling)
	// =========================================================================

	/**
	 * Register a widget as currently active. Called from UMOActivatableWidget::NativeOnActivated.
	 * Maintains a stack used by HandleFocusChanging when OldFocusedWidget has cleared.
	 */
	void RegisterActiveWidget(UMOActivatableWidget* Widget);

	/**
	 * Unregister an active widget. Called from UMOActivatableWidget::NativeOnDeactivated.
	 */
	void UnregisterActiveWidget(UMOActivatableWidget* Widget);

	/** Return the most-recently registered active widget that is still valid. */
	UMOActivatableWidget* GetTopmostActiveWidget() const;

protected:
	/** Class to use for the primary game layout */
	UPROPERTY(EditDefaultsOnly, Category = "MO|UI")
	TSubclassOf<UMOPrimaryGameLayout> PrimaryLayoutClass;

private:
	/** Create the root layout for a player */
	void CreateLayoutForPlayer(APlayerController* PlayerController);

	/** Map of player controllers to their root layouts */
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<APlayerController>, TObjectPtr<UMOPrimaryGameLayout>> PlayerLayouts;

	/** Cached reference to the first local player's layout for quick access */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOPrimaryGameLayout> CachedPrimaryLayout;

	// Global focus-change listener used to close menus on outside click.
	void HookOutsideClickHandler();
	void UnhookOutsideClickHandler();
	void HandleFocusChanging(const struct FFocusEvent& FocusEvent, const class FWeakWidgetPath& OldFocusedWidgetPath,
		const TSharedPtr<class SWidget>& OldFocusedWidget, const class FWidgetPath& NewFocusedWidgetPath,
		const TSharedPtr<class SWidget>& NewFocusedWidget);

	FDelegateHandle FocusChangingHandle;

	/**
	 * Stack of currently-active widgets (most-recently-activated last).
	 * Independent of CommonUI's layer stacks so it captures viewport-direct widgets
	 * (e.g. main menu added via AddToViewport, not on a layer stack).
	 */
	TArray<TWeakObjectPtr<UMOActivatableWidget>> ActiveWidgetStack;
};
