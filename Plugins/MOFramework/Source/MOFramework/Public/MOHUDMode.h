/**
 * =============================================================================
 * MOHUDMode.h - How much HUD is drawn: Full, Minimal, Off (F1 cycles)
 * =============================================================================
 *
 * Wes, Oct 15 2026: "create a no hud mode for nice screen shots/video/etc ... toggle thru a few HUD modes like full, minimal, off. Minimal would hide
 * everything but vital moodles and such, so no quests etc".
 *
 * Every HUD element says what it is FOR (EMOHUDElementKind) once, where it is created (UMOUIManagerComponent::RegisterHUDElement); this policy is the
 * one place that decides which kinds each mode draws. A new HUD element = one RegisterHUDElement call, no mode code.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOHUDMode.generated.h"

UENUM(BlueprintType)
enum class EMOHUDMode : uint8
{
	/** Everything. */
	Full,
	/** What the body and the hands need: moodles, breath, the reticle, what you are looking at, timed-action progress, notifications. No quests, tutorial,
	 *  mode plate, wind dial or FPS counter. */
	Minimal,
	/** Nothing (screenshots, video). Menus still open over it. */
	Off
};

/** What a HUD element is for: decides which modes draw it. */
UENUM(BlueprintType)
enum class EMOHUDElementKind : uint8
{
	/** The body's state: the moodle strip, the breath meter. */
	Vital,
	/** The hands: the reticle, the focus / tool hint, timed-action progress, notifications. */
	Feedback,
	/** Guidance and dressing: quests, the tutorial banner, the mode plate, the wind dial, the FPS counter, skill popups. */
	Info
};

struct MOFRAMEWORK_API FMOHUDModePolicy
{
	/** Whether a mode draws an element of this kind. */
	static bool Shows(EMOHUDMode Mode, EMOHUDElementKind Kind);

	/** F1's order: Full -> Minimal -> Off -> Full. */
	static EMOHUDMode Next(EMOHUDMode Mode);

	static FText DisplayName(EMOHUDMode Mode);

	/** "full" / "minimal" / "off" (any case; also "none", "min"). */
	static bool Parse(const FString& Text, EMOHUDMode& OutMode);
};
