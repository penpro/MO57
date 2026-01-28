#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MODragVisualWidget.generated.h"

class UTexture2D;
class SImage;

/**
 * Simple widget used to display a drag visual during inventory drag operations.
 *
 * This widget renders using pure Slate for reliability - no Blueprint bindings required.
 * Just set the icon texture and it will display.
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
