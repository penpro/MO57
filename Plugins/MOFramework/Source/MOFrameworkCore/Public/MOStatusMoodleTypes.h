/**
 * =============================================================================
 * MOStatusMoodleTypes.h - Shared types for the HUD status effect strip
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Data struct + enum carried by the moodle strip. Lives in its own header so
 * widget code, component code, and the cheat subsystem can include it without
 * pulling in the full widget classes.
 *
 * FMOStatusMoodle is intentionally lean — icon + label + tint + a stable ID.
 * Sources (vitals component, weather integration, etc.) build these and hand
 * them to UMOStatusEffectStripWidget via AddOrUpdateMoodle.
 *
 * =============================================================================
 * RELATED FILES:
 *   - MOStatusEffectStripWidget.h
 *   - MOStatusMoodleWidget.h
 *   - MOHUDRootWidget.h (host)
 * LAST UPDATED: 2026-05-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOStatusMoodleTypes.generated.h"

class UTexture2D;

/**
 * Severity buckets. Drives the moodle's tint without callers having to
 * hand-pick colors. The widget's BP layer maps these to project palette
 * colors so designers can rebrand without C++ changes.
 */
UENUM(BlueprintType)
enum class EMOStatusMoodleSeverity : uint8
{
	Info        UMETA(DisplayName="Info"),      // Neutral / informational (e.g. "Well-fed")
	Warning     UMETA(DisplayName="Warning"),   // Mild negative (e.g. "Wet", "Hungry")
	Critical    UMETA(DisplayName="Critical"),  // Serious (e.g. "Bleeding", "Hypothermia")
	Buff        UMETA(DisplayName="Buff"),      // Positive effect (e.g. "Rested")
};

/**
 * Single moodle entry — one icon slot in the status strip.
 *
 * Stable lookup by Id: AddOrUpdateMoodle uses Id to find an existing slot
 * and update in place (so an active "Wet" moodle that gets refreshed
 * doesn't flicker by being removed-and-re-added every frame).
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORKCORE_API FMOStatusMoodle
{
	GENERATED_BODY()

	/** Stable identifier — used by AddOrUpdateMoodle / RemoveMoodle. */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	FName Id;

	/** Short label shown next to the icon (or as tooltip-only). */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	FText Label;

	/** Long-form description for tooltip / inspection. */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	FText Tooltip;

	/** Display icon. */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Severity bucket — widget maps to a tint. */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	EMOStatusMoodleSeverity Severity = EMOStatusMoodleSeverity::Info;

	/** Optional stack count (e.g. "x3"). 0 = don't show count. */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	int32 Stack = 0;

	/**
	 * Intensity tier within the moodle's own scale (0 = mildest). Sources that
	 * have multiple icons per moodle (e.g. Hunger: 0=Hungry, 1=Starving;
	 * Wet: 0=Damp, 1=Wet, 2=Soaked) author one icon per level and pass the
	 * picked icon in Icon. Level is preserved here so BP-side tooltip / VFX
	 * code can react to tier transitions (pulse on level-up, etc.) without
	 * having to re-derive from severity.
	 *
	 * Severity (Info/Warning/Critical) is for VISUAL CLASSIFICATION; Level
	 * is for INTENSITY within a class. The two are independent — a Critical
	 * "Starving" can be Level 1 of a 0..1 scale OR Level 4 of a 0..4 scale.
	 */
	UPROPERTY(BlueprintReadWrite, Category="MO|HUD|Moodle")
	int32 Level = 0;
};
