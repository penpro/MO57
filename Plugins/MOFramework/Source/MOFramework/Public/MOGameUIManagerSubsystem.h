// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "MOGameUIManagerSubsystem.generated.h"

class UMOPrimaryGameLayout;
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
	 * Push an existing widget instance to a layer.
	 *
	 * @param LayerTag The gameplay tag identifying the layer
	 * @param Widget The widget instance to push
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI")
	void PushWidgetToLayerInstance(FGameplayTag LayerTag, UCommonActivatableWidget* Widget);

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
};
