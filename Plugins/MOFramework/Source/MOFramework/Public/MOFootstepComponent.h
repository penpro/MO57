// Surface-aware footsteps for any pawn: a humanoid skeleton's foot plants, or (a wolf, a deer: no ball bones) the ground it covers.
//
// WHEN a step happens: the animation decides. Each tick the component reads the world speed of both ball-of-foot bones; a foot that swung (fast against the body) and came to rest in the world has PLANTED
// (FMOFootPlantDetector), so the sound lands exactly where the animation puts the foot, whatever the gait or the clip. If the bones never report a plant (a pose without the bones, a frozen animation)
// a walked-distance fallback fires the step instead (FMOStrideFallback). Landing from a fall plays one heavy step.
//
// WHAT it sounds like: (1) an actor standing under the pawn that implements IMOFootstepSurfaceInterface names its own set (a floor, a slab); (2) otherwise the world generator decides from where the floor is
// relative to the generated surface: well above it = a structure or prop (StructureSet), well below it = a cave or a pit (UndergroundSet), on it = the BIOME there (FMOBiomeDefinitionRow::FootstepSet, resolved with the
// same biome rules the vegetation uses). The set name is the middle of the sound-bank ids "SFX.Footsteps.<Set>1" / "<Set>2": a new surface is new bank rows plus a data value, never code.
//
// Audio is cosmetic and local: nothing runs on a dedicated server, only pawns within MaxAudibleDistanceCm of the listener tick, and no replication is involved (every client animates and hears every pawn).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOFootstepComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
struct FHitResult;

/** How hard a step lands: sets the volume. */
UENUM(BlueprintType)
enum class EMOFootstepGait : uint8
{
	Sneak,
	Walk,
	Run,
};

/** A placed mesh (a rock, a log: PCG instances, props) names its footstep set by its asset path: the first rule whose PathContains is in it wins.
 *  Set None = a ground patch (forest floor, an embankment): the ground below decides (the biome). N-132. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOFootstepPropRule
{
	GENERATED_BODY()

	/** Matched case-insensitively against the mesh's path name ("/Game/Megascans/3D_Assets/MossyForestBoulder/..."). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FString PathContains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FName Set;
};

/** The decisions of the footstep system, free of world and actor state so the automation tests cover them. */
struct MOFRAMEWORK_API FMOFootstepMath
{
	/** Crouched = sneak; faster than RunThresholdCm/s = run; otherwise walk. */
	static EMOFootstepGait GaitFor(bool bCrouched, float Speed2D, float RunThresholdCm);

	/** Loudness of a step (1 = the bank row's own volume). */
	static float VolumeFor(EMOFootstepGait Gait);

	/** The distance between two steps of this gait (cm); the fallback fires after FallbackFactor times it without an animation plant. */
	static float StepLengthCm(EMOFootstepGait Gait);
	static constexpr float FallbackFactor = 1.6f;

	/** A bone-free pawn's distance between two footfall sounds (cm): its walking step, lengthening with speed past a walk (3 m/s) the way a
	 *  four-legged gait's stride does (trot, gallop). N-132. */
	static float BoneFreeStepCm(float WalkStepCm, float Speed2D);

	/** Bank id of variant 0/1 of a set: "SFX.Footsteps.<Set><1|2>". */
	static FName SoundIdFor(FName Set, int32 Variant);

	enum class EGround : uint8
	{
		Terrain,       // on the generated surface: the biome decides
		Structure,     // clearly above it: something built or placed
		Underground,   // clearly below it: a cave, a pit, a tunnel
	};

	/** Where the floor is relative to the generated terrain surface. Hand-dug ground below the surface reads as Underground (subsoil and rock), which is what it is. */
	static EGround ClassifyGround(float FloorZ, float GeneratedSurfaceZ);
	static constexpr float StructureAboveCm = 60.0f;
	static constexpr float UndergroundBelowCm = 150.0f;

	/** The first rule matching MeshPath (case-insensitive): true and its Set (None = defer to the ground), or false when none matches. */
	static bool MatchPropRule(const TArray<FMOFootstepPropRule>& Rules, const FString& MeshPath, FName& OutSet);

	/** Rocks, boulders, cliffs and cobbles ring as stone; stumps, fallen trees, branches, logs and sticks as wood; ground patches defer to the biome. */
	static TArray<FMOFootstepPropRule> DefaultPropRules();
};

/**
 * One foot's plant detector, from the foot's WORLD horizontal speed against the body's. Measured on the MO walk / sprint animations: the stance foot is nearly still in the world (12-32 cm/s at a 250 cm/s
 * walk, ~50-230 cm/s at a 700 cm/s sprint) and the swinging foot moves at 1.7-3x the body's speed; the step is the instant the speed collapses from the swing back to the stance. (The foot's HEIGHT is a poor
 * signal: the clearance is a few cm and the bone dips below the capsule floor.)
 */
struct MOFRAMEWORK_API FMOFootPlantDetector
{
	float SwingFraction = 0.65f;              // faster than this fraction of the body speed = the swing
	float PlantFraction = 0.30f;              // back under this fraction = planted
	float ReferenceFloorCmPerSec = 100.0f;    // the body speed is not taken below this: a pawn shuffling on the spot makes no step
	float MinSwingSeconds = 0.2f;             // a real swing lasts longer than this: a shorter dip in the foot's speed (measured mid-swing in the crouch walk) is not a plant
	float MaxSwingSeconds = 1.5f;             // a swing never lasts this long: a stuck "swing" is dropped without a step

	/** Feed the foot's world horizontal speed and the body's each tick. Returns true on the tick the foot plants. */
	bool Update(float FootSpeedCm, float BodySpeedCm, float DeltaSeconds);

	void Reset()
	{
		SwingSeconds = 0.0f;
		bSwinging = false;
	}

	bool IsSwinging() const { return bSwinging; }

private:
	float SwingSeconds = 0.0f;
	bool bSwinging = false;
};

/** The distance fallback: a plant from either foot resets it; walking far without one fires a step. */
struct MOFRAMEWORK_API FMOStrideFallback
{
	/** Returns true when enough ground was covered without a plant (and restarts the count). */
	bool Advance(float DistanceCm, float StepLengthCm);

	void NotePlant() { Accumulated = 0.0f; }

private:
	float Accumulated = 0.0f;
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Skeleton bones whose height above the floor times the steps (the ball of each foot; the UE mannequin and MetaHuman skeletons both have them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FName LeftFootBone = FName(TEXT("ball_l"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FName RightFootBone = FName(TEXT("ball_r"));

	/** Pawns farther than this from the listener (camera) make no step sounds and cost no bone reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps", meta=(ClampMin="0.0"))
	float MaxAudibleDistanceCm = 3500.0f;

	/** Sets used when the ground cannot be classified (no world seed yet, no biome row). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FName FallbackTerrainSet = FName(TEXT("GrassTread"));

	/** A floor or prop without IMOFootstepSurfaceInterface or a matching PropRules entry: built things are mostly timber. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FName StructureSet = FName(TEXT("BareWood"));

	/** What a placed mesh underfoot sounds like, by its asset path (first match; FMOFootstepMath::DefaultPropRules). Before Oct 20 every rock and
	 *  boulder stood on rang as bare wood (StructureSet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	TArray<FMOFootstepPropRule> PropRules;

	/** Caves, pits, tunnels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps")
	FName UndergroundSet = FName(TEXT("StoneStep"));

	/** Faster than this (cm/s) the steps are a run's: louder. Default: midway between the pawn's walk (400) and jog (550). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps", meta=(ClampMin="0.0"))
	float RunSpeedThresholdCm = 475.0f;

	/** Touching down faster than this (cm/s, ~11 cm of free fall) is a landing: one heavy step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps", meta=(ClampMin="0.0"))
	float LandingSpeedCm = 150.0f;

	/** Below this horizontal speed (cm/s) a standing pawn makes no steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps", meta=(ClampMin="0.0"))
	float MinStepSpeedCm = 40.0f;

	/** A pawn whose skeleton has no foot bones (the wolf, the deer) steps by the ground it covers instead: a footfall every this many cm at a
	 *  walk, longer as it runs (FMOFootstepMath::BoneFreeStepCm). 0 = such a pawn is silent. AMOCreature sets it (N-132, Oct 20). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps", meta=(ClampMin="0.0"))
	float BoneFreeStepLengthCm = 0.0f;

	/** How loud this pawn's steps are against the bank row (a person's foot 1; a padded paw or a light hoof less). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Footsteps", meta=(ClampMin="0.0"))
	float StepVolumeScale = 1.0f;

	/** Which sound set the floor under this pawn plays (also what the probe cheat prints). OutDescription gets a one-line reason. */
	FName ResolveSet(const FHitResult& FloorHit, const FVector& FootLocation, FString* OutDescription = nullptr);

	/** Dev: log every step (set, gait, source) for a few seconds / until switched off. */
	void SetDebugLog(bool bOn) { bDebugLog = bOn; }

	/** Dev: log both feet's heights above the floor 10 times a second for this many seconds (tuning the plant thresholds against a real animation). */
	void LogHeightsFor(float Seconds, bool bEveryFrame = false);

	int32 GetStepCount() const { return StepCount; }

private:
	enum class EStepSource : uint8 { Plant, Stride, Landing };

	void EmitStep(int32 FootIndex, EStepSource Source, float VolumeScale);
	bool EnsureBones(const USkeletalMeshComponent* Mesh);

	FMOFootPlantDetector Detectors[2];
	FMOStrideFallback Fallback;

	int32 BoneIndices[2] = { INDEX_NONE, INDEX_NONE };
	TWeakObjectPtr<const USkeletalMesh> BonesResolvedFor;   // BoneIndices belong to this mesh asset (a swapped body re-resolves)
	bool bBonesMissingLogged = false;

	double LastStepTime[2] = { -100.0, -100.0 };
	int32 NextVariant = 0;
	int32 StepCount = 0;

	bool bWasGrounded = true;
	float LastVerticalSpeed = 0.0f;

	// Biome sets are cached per ground cell: the pawn crosses a 1.5 m cell in a fraction of a second, a biome border is hundreds of metres wide.
	int32 CachedSeed = INT32_MIN;
	FIntPoint CachedCell = FIntPoint(INT32_MIN, INT32_MIN);
	FName CachedBiomeSet;
	FName CachedBiomeId;

	TSet<FName> MissingSets;   // sets with no bank rows: warned about once, then the fallback set plays

	bool bDebugLog = false;
	double HeightsLogUntil = 0.0;
	double NextHeightsLog = 0.0;
	bool bHeightsEveryFrame = false;
	FVector PrevFootLocation[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bHavePrevFoot = false;
};
