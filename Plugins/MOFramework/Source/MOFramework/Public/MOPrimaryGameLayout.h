// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "MOPrimaryGameLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;

/**
 * Layer tags for the UI system.
 * These must match the tags defined in DefaultGameplayTags.ini
 */
namespace MOUILayerTags
{
	/** HUD layer - always visible, no input capture (reticle, mode indicator, etc.) */
	MOFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_HUD);

	/** Game layer - switchable gameplay menus (inventory, crafting, skills, building) */
	MOFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_Game);

	/** Game overlay - context menus and progress widgets that overlay on game menus */
	MOFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_GameOverlay);

	/** Menu layer - system menus (in-game menu, possession) */
	MOFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_Menu);

	/** Modal layer - modal dialogs and context menus (confirmation, item context) */
	MOFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Layer_Modal);
}

/**
 * Primary game layout widget containing all UI layer stacks.
 *
 * This widget is the root of the UI hierarchy and contains UCommonActivatableWidgetStack
 * instances for each layer. Widgets are pushed/popped from layers, and CommonUI handles
 * input routing automatically based on the topmost active widget.
 *
 * Layer Z-Order (lowest to highest):
 *   HUD (0) < Game (50) < GameOverlay (100) < Menu (150) < Modal (200)
 *
 * Blueprint Setup Required:
 *   Create WBP_PrimaryGameLayout inheriting from this class with:
 *   - HUDLayer (UCommonActivatableWidgetStack)
 *   - GameLayer (UCommonActivatableWidgetStack)
 *   - GameOverlayLayer (UCommonActivatableWidgetStack)
 *   - MenuLayer (UCommonActivatableWidgetStack)
 *   - ModalLayer (UCommonActivatableWidgetStack)
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOPrimaryGameLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UMOPrimaryGameLayout(const FObjectInitializer& ObjectInitializer);

	/**
	 * Push a widget class to the specified layer.
	 * Creates a new instance of the widget and activates it on the layer stack.
	 *
	 * @param LayerTag The layer to push to
	 * @param WidgetClass The widget class to instantiate
	 * @return The created and activated widget
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Layers")
	UCommonActivatableWidget* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/**
	 * Push an existing widget instance to the specified layer.
	 *
	 * @param LayerTag The layer to push to
	 * @param Widget The widget instance to push
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Layers")
	void PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget);

	/**
	 * Pop the topmost widget from the specified layer.
	 *
	 * @param LayerTag The layer to pop from
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Layers")
	void PopWidgetFromLayer(FGameplayTag LayerTag);

	/**
	 * Clear all widgets from a layer.
	 *
	 * @param LayerTag The layer to clear
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Layers")
	void ClearLayer(FGameplayTag LayerTag);

	/**
	 * Check if a layer has any active widgets.
	 *
	 * @param LayerTag The layer to check
	 * @return True if the layer has widgets
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Layers")
	bool IsLayerActive(FGameplayTag LayerTag) const;

	/**
	 * Get the count of active widgets across all menu layers (Game, GameOverlay, Menu, Modal).
	 * Excludes HUD layer since those are always-visible and don't count as "menus".
	 *
	 * @return Total count of active menu widgets
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Layers")
	int32 GetActiveMenuCount() const;

	/**
	 * Get the widget stack for a specific layer.
	 *
	 * @param LayerTag The layer to get
	 * @return The widget stack, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Layers")
	UCommonActivatableWidgetContainerBase* GetLayerStack(FGameplayTag LayerTag) const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	/** Register a layer stack with a gameplay tag. Called from NativeConstruct. */
	void RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerStack);

	/** HUD layer - always visible elements */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> HUDLayer;

	/** Game layer - switchable gameplay menus */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> GameLayer;

	/** Game overlay - context/progress widgets over game menus */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> GameOverlayLayer;

	/** Menu layer - system menus */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> MenuLayer;

	/** Modal layer - dialogs and context menus */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "MO|UI|Layers")
	TObjectPtr<UCommonActivatableWidgetContainerBase> ModalLayer;

private:
	/** Map from layer tag to widget stack */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> LayerStacks;
};
