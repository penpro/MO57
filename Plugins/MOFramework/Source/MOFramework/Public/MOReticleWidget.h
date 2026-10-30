/**
 * =============================================================================
 * MOReticleWidget.h - HUD Reticle: a small centred circle that tells what a click will do
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * The aim point. Wes, Oct 29 2026: "change the target reticle out to a small circle centered instead of the goofy crosshairs ... make it so the
 * circle changes colors based on context with what it's looking at, like green to interact or red to attack or blue when it wants a right click".
 * A thin ring (a rounded-box brush, no texture) with a dark edge so it reads on snow and on sky. Its colour is an EMOReticleIntent, decided ONCE
 * per focus update by UMOUIManagerComponent::ResolveReticleIntent (the focus hint's own trace); the widget only displays it.
 *
 * CLOSING CIRCLE: SetAimSpread(0..1) widens the ring (0 = at rest, 1 = AimMaxDiameter), so an aim that steadies can be shown closing in
 * (Wes: "we can also use the circle for aiming with projectiles with a 'closing circle' method for aiming before firing"). Nothing fires a
 * projectile yet (Docs/DECISIONS.md, F53): MO.UI.ReticleAim shows it.
 *
 * COLOURS (theme tokens, so colour-blind / high-contrast modes apply):
 * - None      -> color.text.primary  (white)
 * - Interact  -> color.severity.ok   (green: {key:InteractAction} does something here)
 * - Secondary -> color.severity.info (blue: a right-click menu is the way in, e.g. adding materials to a building)
 * - Threat    -> color.severity.critical (red: a predator, or an animal within reach of a swing)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-10-29] NO REBUILD PER CHANGE: colour and size are Slate attributes bound to this widget's members, so SetReticleIntent / SetAimSpread
 *   cost nothing and never flicker. The old crosshair rebuilt its whole Slate tree for every property change.
 *
 * [2026-10-29] A TINT NEVER COLOURS A RING: a rounded box's outline is drawn in the brush's own outline colour; an SImage's ColorAndOpacity
 *   reaches only the fill. The first build drew a white ring while MO.UI.Reticle reported green / blue / red (found in screenshots, not by
 *   the probe). The ring is SMOReticleRing (MOReticleWidget.cpp), which builds its brush in the colour of the moment on each paint.
 *   MO.UI.Reticle reports what the widget was TOLD; only a screenshot shows what it drew.
 *
 * =============================================================================
 * RELATED FILES: MOUIManagerComponent.h (creates it, resolves the intent), MOPlayerController.h
 * LAST UPDATED: 2026-10-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MOReticleWidget.generated.h"

/** What the thing under the reticle wants from the player (the ring's colour). */
UENUM(BlueprintType)
enum class EMOReticleIntent : uint8
{
	None		UMETA(DisplayName = "Nothing"),
	Interact	UMETA(DisplayName = "Interact"),		// green
	Secondary	UMETA(DisplayName = "Right-click"),		// blue
	Threat		UMETA(DisplayName = "Threat")			// red
};

UCLASS()
class MOFRAMEWORK_API UMOReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Colour the ring for what is under it. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleIntent(EMOReticleIntent InIntent) { Intent = InIntent; }

	UFUNCTION(BlueprintPure, Category="MO|UI|Reticle")
	EMOReticleIntent GetReticleIntent() const { return Intent; }

	/** The closing circle: 0 = the ring at rest, 1 = AimMaxDiameter (an unsteady aim). Stops a PlayClosingCircle. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetAimSpread(float InSpread01) { AimSpread = FMath::Clamp(InSpread01, 0.0f, 1.0f); ClosingSeconds = 0.0f; }

	/** The ring opens wide and closes to rest over Seconds, easing in as it settles: an aim steadying (MO.UI.ReticleAim shows it). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void PlayClosingCircle(float Seconds);

	/** The spread now (a closing circle's progress, or the spread set). */
	UFUNCTION(BlueprintPure, Category="MO|UI|Reticle")
	float GetAimSpread() const;

	/** A closing circle's spread Elapsed seconds into a close of Seconds: 1 -> 0, fast at first and settling (pure; tested). */
	static float ClosingSpread(float Elapsed, float Seconds);

	/** The ring's diameter in pixels right now. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Reticle")
	float GetCurrentDiameter() const { return FMath::Lerp(ReticleDiameter, AimMaxDiameter, GetAimSpread()); }

	/** Pin one colour (ignores the intent) until ClearReticleColor. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleColor(FLinearColor InColor) { PinnedColor = InColor; bColorPinned = true; }

	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void ClearReticleColor() { bColorPinned = false; }

	/** The ring's resting diameter (pixels). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Reticle")
	void SetReticleSize(float InDiameter) { ReticleDiameter = FMath::Max(4.0f, InDiameter); }

	/** The colour an intent shows (the theme's token for it). */
	static FLinearColor ColorForIntent(EMOReticleIntent InIntent);

	/** The colour the ring has now. */
	FLinearColor GetCurrentColor() const { return bColorPinned ? PinnedColor : ColorForIntent(Intent); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** The ring's diameter at rest (pixels). Small: an aim point, not a target box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="4.0"))
	float ReticleDiameter = 12.0f;

	/** The ring's diameter at a full aim spread (pixels): the closing circle starts here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="4.0"))
	float AimMaxDiameter = 72.0f;

	/** The ring's line width (pixels); the dark edge adds one pixel on each side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Reticle", meta=(ClampMin="1.0"))
	float RingWidth = 2.0f;

private:
	EMOReticleIntent Intent = EMOReticleIntent::None;
	float AimSpread = 0.0f;
	double ClosingStartSeconds = 0.0;   // FPlatformTime::Seconds() at PlayClosingCircle
	float ClosingSeconds = 0.0f;        // > 0 while a closing circle plays
	FLinearColor PinnedColor = FLinearColor::White;
	bool bColorPinned = false;
};
