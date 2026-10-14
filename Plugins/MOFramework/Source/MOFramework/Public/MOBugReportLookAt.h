/**
 * =============================================================================
 * MOBugReportLookAt.h - "What was I looking at?" rows for a bug report
 * =============================================================================
 *
 * Wes, Oct 14 2026: "add to the bug report an 'item looked at' so it can narrow down what asset is causing issues". When the report form opens
 * (the same moment as its screenshot) a trace runs from the player's view point through the screen centre, against the real geometry, and the
 * report gets LookAt.* rows: the mesh asset (and instance + its scale: mirrored instances are flagged), actor / component / their tags (a world
 * feature's id), the material and physical material of the face, the collision profile AND the mesh's actual collision (simple shapes,
 * complex-as-simple: "no collision" reports answer themselves), distance, slope, the generator's surface biome there, and the exact view --
 * `LookAt.Reproduce` is an MO.Cam.LookAt command that rebuilds it.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOBugReport.h"

struct FHitResult;

struct MOFRAMEWORK_API FMOBugReportLookAt
{
	/** How far the look ray goes (cm): far terrain and mountains are worth naming too. */
	static constexpr double RangeCm = 200000.0;

	/** Trace from the first local player's view point through the screen centre; the rows describe what it hits (or that it hit nothing). */
	static void Capture(const UWorld* World, FMOBugReportFields& Out);

	/** The rows for one look ray (the view always; the hit's asset, collision, material and place when it hit something). */
	static void Describe(const UWorld* World, const FVector& ViewLocation, const FRotator& ViewRotation, const FHitResult& Hit, FMOBugReportFields& Out);
};
