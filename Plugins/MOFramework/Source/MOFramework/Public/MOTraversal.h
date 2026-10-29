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

	/** Climbable? (a pure classification of what the hands found): a tree trunk (the MOTreeTrunk tag), rock (a rock tag: IsRockTag; MOClimbable; or a rock / cliff /
	 *  boulder / crag mesh), or natural ground steeper than MinCliffSlopeDeg (the voxel terrain). Built things are not: their walls are smooth. */
	static EMOClimbSurface ClassifyClimbSurface(bool bTreeTrunkTag, bool bRockTag, bool bClimbableTag, bool bVoxelTerrain, bool bBuilt, const FString& MeshPath,
		float NormalZ);

	/** Climbing speed on a surface (cm/s): a scramble on rock 30, a trunk 22 -- hands and feet, a hold at a time. */
	static float ClimbSpeedCm(EMOClimbSurface Surface);

	/** A face's angle from level ground (degrees) from its outward normal: 90 a sheer face, less a slab leaning back, more an overhang (180 a roof). A zero
	 *  normal (no face known) reads as sheer. */
	static float FaceAngleDeg(const FVector& FaceNormal);

	/** How the climber's own body and training change a climb (F51, Wes Oct 29: "the more muscle mass percentage of the body the better at climbing and the
	 *  more skill the better"). Strength against weight: the arms and legs haul the whole body on the muscle it has, so a move's stamina scales with
	 *  ReferenceMuscleShare / MuscleShare (muscle mass over total weight; the default survivor, 30 kg of muscle at 18 % fat, is 0.51; Lillian 0.53; 25 kg at
	 *  35 % fat 0.37), clamped 0.6-1.8, and the speed with the share more gently (0.85-1.15). Technique: the climbing level saturates -- 1 - e^(-level/10):
	 *  level 5 ~40 %, 10 ~63 %, 20 ~86 % of what training gives (each level costs exponentially more XP, so ~20 is years of climbing) -- and takes up to
	 *  half the cost off (footwork puts the weight on the legs, a trained climber rests on straight arms) and adds up to 35 % speed. A share of 0 or less (no
	 *  body known) counts as the reference. */
	struct FClimbAbility
	{
		float CostScale = 1.0f;
		float SpeedScale = 1.0f;
	};
	static constexpr float ReferenceMuscleShare = 0.5f;
	static FClimbAbility ClimbAbility(int32 ClimbingLevel, float MuscleShare);

	/** A tag the world puts on rock (Oct 29: the PCG rocks whose meshes are named by an id, not "Rock"): the landmark tier (MOLandmark), the harvestable
	 *  rocks (MOResource_Rock), the biomes' rock cover (MOBiomeCover_Rock). */
	static bool IsRockTag(FName Tag);

	/** One probe dropped onto an obstacle from above arm's reach, at AlongCm past its face. */
	struct FTopSample
	{
		enum class EKind : uint8 { Miss, AboveReach, Surface };
		EKind Kind = EKind::Miss;   // Miss: nothing between a hop's height and arm's reach (past the obstacle); AboveReach: it rises higher here
		float AlongCm = 0.0f;
		float Z = 0.0f;             // Surface: its height
		bool bWalkable = false;     // Surface: a body could stand on it
	};
	/**
	 * Where a body stands on an obstacle, from probes ordered from its face outward (Oct 29, the world's own rocks: a dome's surface just past the
	 * face is its shoulder, too steep to stand on and below its crest, so one probe there read a 135 cm boulder as a 77 cm ledge with no room on
	 * top). The highest walkable surface (the nearest of equals within 2 cm: a block's top is its edge) before the obstacle rises out of reach or
	 * ends (a miss after a surface). INDEX_NONE if there is none; bOutAboveReach then says whether it rose out of reach (a climb, not a ledge).
	 */
	static int32 PickTop(TConstArrayView<FTopSample> Samples, bool& bOutAboveReach);
	/** Every walkable top PickTop would consider, best first (the highest; the nearer of two within 2 cm): a lumpy top's highest spot may have no
	 *  room for the body where a lower one does (Oct 29, the Nordic beach rock clusters). PickTop is its first. */
	static void RankTops(TConstArrayView<FTopSample> Samples, TArray<int32>& OutBestFirst, bool& bOutAboveReach);
};
