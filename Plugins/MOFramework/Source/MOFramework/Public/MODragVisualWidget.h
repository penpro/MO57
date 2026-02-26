/**
 * =============================================================================
 * MODragVisualWidget.h - Drag Visual for Inventory Operations
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Widget displayed under cursor during inventory drag operations. Shows the
 * item icon being dragged. Uses pure Slate rendering for reliability.
 *
 * USAGE:
 * Created by MOInventoryDragDropOperation. Set icon via SetIcon() and size
 * via SetVisualSize(). Widget follows cursor automatically.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PURE SLATE: Uses SImage instead of UImage widget. RebuildWidget()
 *   creates Slate hierarchy directly. Don't add Blueprint widget bindings.
 *
 * [2024-02] NULL TEXTURE: SetIcon(nullptr) clears the visual. IconTexture
 *   may be null if item has no icon.
 *
 * [2024-02] SIZE SYNC: VisualSize is cached. Call SetVisualSize() to update
 *   if widget size should change during drag.
 *
 * =============================================================================
 * RELATED FILES: MOInventoryDragDropOperation.h, MOInventorySlot.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MODragVisualWidget.generated.h"

class UTexture2D;
class SImage;

/**
 * Simple widget used to display a drag visual during inventory drag operations.
 * See file header for pitfalls.
 */
UCLASS(Blueprintable)
class MOFRAMEWORK_API UMODragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set the texture to display in the drag visual. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetIcon(UTexture2D* InTexture);

	/** Set the size of the drag visual. */
	UFUNCTION(BlueprintCallable, Category="MO|Inventory|UI")
	void SetVisualSize(FVector2D InSize);

	/** Get the current icon texture. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	UTexture2D* GetIconTexture() const { return IconTexture; }

	/** Get the visual size. */
	UFUNCTION(BlueprintPure, Category="MO|Inventory|UI")
	FVector2D GetVisualSize() const { return VisualSize; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void SynchronizeProperties() override;

	/** Called when the icon is set - override in Blueprint to update your Image widget. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|Inventory|UI")
	void OnIconChanged(UTexture2D* NewTexture);

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	FVector2D VisualSize = FVector2D(64.0f, 64.0f);

	UPROPERTY(BlueprintReadOnly, Category="MO|Inventory|UI")
	TObjectPtr<UTexture2D> IconTexture;

private:
	void UpdateBrush();

	TSharedPtr<SImage> SlateImage;
	FSlateBrush IconBrush;
};
