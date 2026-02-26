/**
 * =============================================================================
 * MOReticleWidget.h - HUD Crosshair/Reticle Display
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Simple reticle/crosshair widget for targeting. Uses Slate SConstraintCanvas
 * for custom drawing of crosshair lines and center dot. Can be subclassed in
 * Blueprint for custom designs.
 *
 * CUSTOMIZATION:
 * - ReticleColor: Line and dot color
 * - ReticleSize: Distance from center to line ends
 * - ReticleThickness: Line width in pixels
 * - ReticleGap: Space between center and line start
 * - CenterDotSize: Size of optional center dot
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] SLATE REBUILD: RebuildReticle() is called when properties change.
 *   The widget uses RebuildWidget() override for Slate construction.
 *
 * [2024-02] PROPERTY CHANGES: SetReticleX functions modify properties and
 *   call RebuildReticle(). Changes are immediate but may cause flicker if
 *   called frequently.
 *
 * =============================================================================
 * RELATED FILES: MOPlayerController.h (adds to viewport)
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOReticleWidget.generated.h"

class SConstraintCanvas;

/**
 * Simple reticle/crosshair widget for targeting.
 * Can be used as-is for a basic crosshair or subclassed in Blueprint for custom designs.
 */
UCLASS()
class MOFRAMEWORK_API UMOReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set the reticle color. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleColor(FLinearColor InColor);

	/** Set the reticle size (width and height of the crosshair). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleSize(float InSize);

	/** Set the thickness of the crosshair lines. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleThickness(float InThickness);

	/** Set the gap in the center of the crosshair. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleGap(float InGap);

	/** Show or hide the center dot. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetShowCenterDot(bool bShow);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** Color of the reticle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle")
	FLinearColor ReticleColor = FLinearColor::White;

	/** Total size of the crosshair (distance from center to end of line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="1.0"))
	float ReticleSize = 10.0f;

	/** Thickness of the crosshair lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="1.0"))
	float ReticleThickness = 2.0f;

	/** Gap in the center (space between center and start of lines). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="0.0"))
	float ReticleGap = 3.0f;

	/** Whether to show a dot in the center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle")
	bool bShowCenterDot = true;

	/** Size of the center dot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="1.0"))
	float CenterDotSize = 2.0f;

private:
	void RebuildReticle();

	TSharedPtr<SConstraintCanvas> RootCanvas;
};
