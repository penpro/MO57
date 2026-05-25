/**
 * =============================================================================
 * MOThermalComfortWidget.h - HUD Thermal Comfort Indicator
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Tiny HUD widget that shows the player's perceived thermal comfort as a
 * single icon (VeryCold → Cold → Comfortable → Hot → VeryHot). Subscribes
 * to the player pawn's UMOVitalsComponent::OnThermalComfortChanged and
 * swaps the brush only when the comfort BUCKET changes (no per-tick spam).
 *
 * USAGE (Blueprint):
 *   1. Create WBP_ThermalComfortIndicator based on this class.
 *   2. Add an Image widget called "ThermalIcon" (BindWidget — required).
 *   3. In Class Defaults, fill ComfortIcons with 5 textures in this order:
 *        [0] VeryCold    [1] Cold    [2] Comfortable    [3] Hot    [4] VeryHot
 *   4. Drop the widget on your HUD layer.
 *
 * The widget auto-finds the local player pawn's vitals component on
 * NativeConstruct and updates the brush. No additional wiring needed.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-05] ICON ARRAY SIZE: ComfortIcons MUST have exactly 5 entries matching
 *   EMOThermalComfort. Missing entries = widget logs a Warning and uses the
 *   Comfortable icon as fallback.
 *
 * [2026-05] PAWN CHANGES: If the player swaps pawns (e.g., possess another
 *   survivor), the widget re-subscribes via SetTargetVitals(). HUD wrappers
 *   should call this after OnPossessedPawnChanged.
 *
 * =============================================================================
 * RELATED FILES: MOVitalsComponent.h, MOVitalsTypes.h
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOVitalsTypes.h"
#include "MOThermalComfortWidget.generated.h"

class UImage;
class UMOVitalsComponent;
class UTexture2D;

/**
 * Tiny HUD widget — one icon, swaps on thermal comfort bucket change.
 */
UCLASS()
class MOFRAMEWORK_API UMOThermalComfortWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Five icons matching the EMOThermalComfort enum, in numeric order:
	 * [0] VeryCold, [1] Cold, [2] Comfortable, [3] Hot, [4] VeryHot.
	 * Hard refs because this widget is always on-screen during gameplay —
	 * no need for the async-load overhead of soft refs.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|HUD|Thermal")
	TArray<TObjectPtr<UTexture2D>> ComfortIcons;

	/**
	 * Switch which vitals component drives this widget. Useful when the
	 * player changes possessed pawn — call this on pawn-changed.
	 * Pass nullptr to detach.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|Thermal")
	void SetTargetVitals(UMOVitalsComponent* NewVitals);

	/**
	 * Force a re-render at a specific comfort level. Primarily for testing /
	 * preview — normal usage updates via the OnThermalComfortChanged hook.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|HUD|Thermal")
	void SetComfortLevel(EMOThermalComfort NewLevel);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Image widget that displays the icon. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UImage> ThermalIcon;

	/** Pawn vitals we're listening to. Held weakly so possess-swap doesn't keep it alive. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOVitalsComponent> TargetVitals;

	/** Last applied level — guard so we don't invalidate Slate on no-op updates. */
	EMOThermalComfort CurrentLevel = EMOThermalComfort::Comfortable;

	/** Locate the local player pawn's vitals + subscribe. Called from NativeConstruct. */
	void AutoAttachToLocalPlayerVitals();

	/** Handler bound to UMOVitalsComponent::OnThermalComfortChanged. */
	UFUNCTION()
	void HandleThermalComfortChanged(EMOThermalComfort OldComfort, EMOThermalComfort NewComfort);

	/**
	 * Handler bound to APlayerController::OnPossessedPawnChanged so we
	 * re-attach when the pawn finally exists (HUD widget activates BEFORE
	 * the initial pawn spawn — without this hook AutoAttach silently fails).
	 */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
};
