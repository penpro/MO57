// Getting over and up things on foot (Wes, Oct 26 2026: "we need to implement climbing of cliffs / trees and we need a vault mechanic to hop / pull up on
// things"). The decisions, pure and unit-tested (MOFramework.Traversal.*); the movement itself is UMOCharacterMovementComponent (custom modes Traverse and
// Climb, predicted for co-op like every other move).
//
// Jump is the one verb:
//   - at an obstacle the body can get onto: a hop up (waist high) or a pull-up (to arm's reach with a jump), or a vault over a thin one (a fence, a log);
//   - at a climbable surface out of reach (a cliff face, a rock, a tree trunk): grab it and climb (WASD; pushing up at the top pulls over the edge);
//   - while climbing: pull up if the edge is in reach, else push off the wall. Crouch lets go.
// Heights are a 1.75 m adult's: a 45 cm step is a step (the movement component's MaxStepHeight), the hands reach ~2.2 m standing with a jump.

#pragma once

#include "CoreMinimal.h"

enum class EMOTraversal : uint8
{
	None,
	Vault,    // over a thin waist-high obstacle to the ground behind it
	HopUp,    // onto something waist high
	PullUp,   // onto a ledge at arm's reach, hands first
};

enum class EMOClimbSurface : uint8
{
	None,
	Rock,     // a cliff face, a crag, a boulder, steep natural ground
	Tree,     // a trunk: fewer holds than rock, slower
};

struct MOFRAMEWORK_API FMOTraversal
{
	/** Below this a ledge is a step (the movement component walks up 45 cm). */
	static constexpr float HopMinCm = 50.0f;
	/** Up to this the body hops onto or vaults over it (about waist to chest height). */
	static constexpr float HopMaxCm = 120.0f;
	/** Up to this the hands reach the edge (with a jump) and pull the body up; higher is climbing. */
	static constexpr float PullUpMaxCm = 220.0f;
	/** A vaulted obstacle is at most this deep (a fence, a log, a low wall): deeper is something to stand on. */
	static constexpr float VaultMaxDepthCm = 70.0f;
	/** Natural ground is climbed (not walked) where its face is steeper than this. */
	static constexpr float MinCliffSlopeDeg = 55.0f;

	/** What a ledge HeightCm above the feet allows: bRoomOnTop = the body fits standing on it; bRoomBehind = a thin obstacle with clear ground just behind. */
	static EMOTraversal Classify(float HeightCm, bool bRoomOnTop, bool bRoomBehind);

	/** Seconds the move takes: a vault 0.55 s; a hop up 0.6-0.8 s; a pull-up 1.1-1.6 s -- the higher, the longer. */
	static float DurationSeconds(EMOTraversal Kind, float HeightCm);

	/** A move's path: the body's centre from Points[0] (now) through Points[1] (risen to clear the edge) and Points[2] (over it) to Points[3] (standing). */
	struct FPath
	{
		FVector Points[4];
	};

	/** Where the body is a Fraction (0..1) of the way along the path, at an even speed along its length. */
	static FVector Evaluate(const FPath& Path, float Fraction);

	/** Climbable? (a pure classification of what the hands found): a tree trunk (the MOTreeTrunk tag), rock (MOLandmark, MOClimbable, or a rock / cliff /
	 *  boulder / crag mesh), or natural ground steeper than MinCliffSlopeDeg (the voxel terrain). Built things are not: their walls are smooth. */
	static EMOClimbSurface ClassifyClimbSurface(bool bTreeTrunkTag, bool bRockTag, bool bClimbableTag, bool bVoxelTerrain, bool bBuilt, const FString& MeshPath,
		float NormalZ);

	/** Climbing speed on a surface (cm/s): a scramble on rock 30, a trunk 22 -- hands and feet, a hold at a time. */
	static float ClimbSpeedCm(EMOClimbSurface Surface);
};
