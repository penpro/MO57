/**
 * =============================================================================
 * MOSpawnClearance.h - "does a pawn FIT here?" for every code path that picks a standing position
 * =============================================================================
 *
 * WHY THIS EXISTS: the join spawn, the first spawn, the fall-through rescue and the embedded rescue all pick a spot by tracing a
 * thin LINE down to the voxel ground. A line that passes a few centimetres from a tree trunk hits terrain, but the pawn's capsule
 * (~34 cm radius) overlaps the trunk, so the pawn spawns/teleports INTO the tree and cannot move. Reported by Wes: "the fall check
 * puts us above the world fairly reliably but it will put us into trees and then we can be stuck in them".
 *
 * ONE definition of "clear": the pawn's capsule, standing on the ground point, overlaps nothing that blocks pawns other than
 * the voxel terrain itself. Every position-picking path asks this, and re-grounds on the voxel when it has to move.
 */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class UWorld;
class AActor;
class APawn;
struct FHitResult;

namespace MOSpawnClearance
{
	/** A pawn's capsule. Defaults match the project's standard human pawn. */
	struct FCapsule
	{
		float Radius = 34.0f;
		float HalfHeight = 88.0f;
	};

	/** The capsule of a pawn class (its CDO's capsule), or the default human capsule if it has none. */
	MOFRAMEWORK_API FCapsule CapsuleOf(TSubclassOf<APawn> PawnClass);

	/** The capsule of a live pawn (scaled). */
	MOFRAMEWORK_API FCapsule CapsuleOf(const APawn* Pawn);

	/**
	 * First VOXEL-terrain hit by a vertical trace at (X, Y). Trees, rocks and buildings above the ground are skipped, not
	 * mistaken for ground. Returns false if no voxel collision lies on the line (not generated near here).
	 *
	 * IMPLEMENTATION RULE: an object-type multi trace, never a channel one. A multi trace BY CHANNEL ends at the first BLOCKING hit, so a
	 * tree/roof/prop over the ground hid the terrain behind it and this returned false -- the spawn settle and the fall-through rescue both
	 * relied on it and both failed in forests (a host pawn buried under the terrain, invisible to clients). Pinned by `nettest actions`
	 * (buried pawn under a block, old rule as control) and the `rescue_probe --hunt` over fresh worlds.
	 */
	MOFRAMEWORK_API bool TraceVoxelGround(const UWorld* World, float X, float Y, float StartZ, float EndZ, const AActor* Ignore, FHitResult& OutHit);

	/**
	 * True if a capsule centred at `Center` is blocked by something other than the voxel terrain (a tree, rock, building...).
	 * `bIgnorePawns` skips other pawns (the embedded rescue must not teleport two players apart because they overlap).
	 */
	MOFRAMEWORK_API bool IsCapsuleBlocked(const UWorld* World, const FVector& Center, const FCapsule& Capsule, const AActor* Ignore,
		bool bIgnorePawns, FString* OutBlockerName = nullptr);

	/** True if a pawn standing with its feet on `GroundLocation` fits (small lift, so the ground itself is never the obstacle). */
	MOFRAMEWORK_API bool IsStandingSpotClear(const UWorld* World, const FVector& GroundLocation, const FCapsule& Capsule, const AActor* Ignore);

	/**
	 * `GroundLocation` if a pawn fits there, else the nearest ground within `SearchRadius` where it does (rings outward; every
	 * candidate is re-grounded on the voxel and must not be too steep). Returns false if nothing fits.
	 */
	MOFRAMEWORK_API bool FindClearGround(const UWorld* World, const FVector& GroundLocation, const FCapsule& Capsule, const AActor* Ignore,
		FVector& OutGround, float SearchRadius = 2000.0f, float TraceHalfHeight = 3000.0f, float MinSurfaceNormalZ = 0.7f);
}
