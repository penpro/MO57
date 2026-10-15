/**
 * MOItemPreviewWidget.h - a turning 3D view of an item (its real world mesh) for any UI panel.
 *
 * PURPOSE:
 * Wes, 2026-10-14: "the inspection/details window should show a 3D preview". The inventory details panel embeds one
 * (UMOItemInfoPanel::ItemPreview); any panel can: put WBP_MOItemPreview in its tree and call SetItem(ItemId).
 *
 * BEHAVIOUR:
 * - spawns its own AMOItemPreviewStage on first use (destroyed with the widget) and draws the stage's render target;
 * - turns slowly on its own; drag with the left mouse button to turn / tilt it yourself (auto-turn resumes 3 s after release);
 * - captures only while it is painted (NativeTick), so a hidden or cached panel costs nothing;
 * - an item without a world mesh (graybox placeholder rows included) collapses the image: the icon still shows.
 *
 * BIND: PreviewImage (UImage, required). Build the WBP with `python Tools/ue.py ui build Content/Python/ui_specs/item_preview.py`.
 */
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOItemPreviewWidget.generated.h"

class UImage;
class AMOItemPreviewStage;

UCLASS()
class MOFRAMEWORK_API UMOItemPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Show this item; returns false (and collapses) when it has no world mesh. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Preview")
	bool SetItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category="MO|UI|Preview")
	void ClearItem();

	UFUNCTION(BlueprintPure, Category="MO|UI|Preview")
	FName GetItem() const { return CurrentItem; }

	/** Degrees per second of the idle turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Preview")
	float SpinDegreesPerSecond = 24.0f;

	/** Render-target size (square). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Preview")
	int32 Resolution = 512;

	/** Treat placeholder (graybox) meshes as "no preview". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Preview")
	bool bHidePlaceholderMeshes = true;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> PreviewImage;

private:
	AMOItemPreviewStage* EnsureStage();

	UPROPERTY(Transient)
	TObjectPtr<AMOItemPreviewStage> Stage;

	FName CurrentItem = NAME_None;
	float Yaw = 35.0f;
	float Pitch = -20.0f;
	bool bDragging = false;
	double ResumeSpinAt = 0.0;
	FVector2D LastMouse = FVector2D::ZeroVector;
};
