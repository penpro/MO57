// The character movement component of every MO pawn: stock UCharacterMovementComponent plus WATER.
//
// The engine decides "swimming" from the APhysicsVolume a character overlaps; MO's water is generated (see UMOWaterSubsystem), so this component asks the water oracle instead:
//   - Immersion = the fraction of the capsule below the water surface at its position (feet .. feet + capsule height), refreshed at the start of every movement update (so a client replaying a
//     saved move gets the same answer as the server: the oracle is deterministic in the position).
//   - WADING: walking in water is slowed by the drag of the water (about half speed at full immersion).
//   - SWIMMING is decided by the DEPTH OF THE WATER where the character is: the surface down to the first thing it could stand on (a trace on its own collision channel: the
//     real floor, dug pits included, a submerged rock, a dock). It starts at SwimEnterDepthCm (1 m) and ends below SwimExitDepthCm (0.9 m; the gap is hysteresis so the mode does not
//     flicker at the line). Wes, Oct 16 2026: it used to go by how much of the capsule was under water, and a floating swimmer stayed a swimmer in waist-deep water until the capsule
//     touched the bottom. While swimming GetPhysicsVolume() returns the world's shared water volume, which is what makes the engine's PhysSwimming keep the mode, apply fluid friction and buoyancy.
//   - BUOYANCY: stock swimming floats a body whose Buoyancy x Immersion reaches 1. A human with lungs full floats a little (Buoyancy 1.12: head and shoulders out of the water at rest); a heavy load
//     lowers Buoyancy (the owning character sets it) until the swimmer sinks.
//   - The capsule stays UPRIGHT while swimming (ShouldRemainVertical): immersion math and collision stay simple; the character mesh is what lies down (AMOCharacter).
//
// The pure functions (ComputeImmersion, ShouldSwim) are static so the automation tests exercise the decisions without a world.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MOTraversal.h"
#include "MOCharacterMovementComponent.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UMOCharacterMovementComponent();

	// ---- pure decisions (tested) ----

	/** Fraction (0..1) of a capsule whose feet are at FeetZ and which is CapsuleHeight tall that lies under a water surface at SurfaceZ. */
	static float ComputeImmersion(float SurfaceZ, float FeetZ, float CapsuleHeight);

	/**
	 * Swim-mode decision on the depth of the water where the character is (surface to the bottom it could stand on), with hysteresis: not swimming -> start at WaterDepthCm >= Enter;
	 * swimming -> stop only once WaterDepthCm < Exit. Water with no reachable bottom is passed as a huge depth, so a swimmer at the surface of deep water never pops out of the mode.
	 */
	static bool ShouldSwim(bool bWasSwimming, float WaterDepthCm, float EnterDepthCm, float ExitDepthCm);

	/** Pure: ground speed for a gait (sprint beats jog beats walk). */
	static float GaitSpeed(bool bJog, bool bSprint, float WalkSpeed, float JogSpeed, float SprintSpeed) { return bSprint ? SprintSpeed : (bJog ? JogSpeed : WalkSpeed); }

	// ---- gait (walk / jog / sprint) ----
	//
	// CO-OP: the gait is MOVEMENT INPUT, so it travels inside every saved move (FLAG_Custom_0 = jog, FLAG_Custom_1 = sprint) and the server simulates the
	// joiner at the same speed the joiner predicts. It used to be set on the pressing machine only (AMOCharacter wrote MaxWalkSpeed there): on a joiner the
	// server kept simulating a walk and corrected every faster step back -- "the second person can't jog or sprint" (Steam playtest, Oct 16 2026).

	/** The character's speeds for each gait (it owns the tuning; MaxWalkSpeed is the walk). */
	void SetGaitSpeeds(float Walk, float Jog, float Sprint);

	/** The gait this character asks for. Set where the input happens (the owning client, or the server for AI / the host); the server takes it from the moves. */
	void SetWantedGait(bool bJog, bool bSprint);

	bool WantsToJog() const { return bWantsToJog; }
	bool WantsToSprint() const { return bWantsToSprint; }

	// ---- dev glider (Wes, Oct 18 2026: "hang glider mode for devs", BR18-17) ----
	//
	// A developer's way to look at the world from the air, never the simulation (Principle 11): while the world's HOST is in developer mode
	// (FMODevMode::IsHostInDevMode), a FALLING pawn that wants to glide flies like a hang glider at trim (MO.Dev.GlideSpeed forward, MO.Dev.GlideSink
	// down), steered by the movement input. It is movement input like the gait: it rides in the saved moves (FLAG_Custom_2) and the server judges the
	// developer gate itself, so it simulates a joiner's flight the way the joiner predicts it. Toggle: MO.Dev.Glider.

	/** Pure: a glider's next falling velocity. The horizontal speed eases to TrimSpeedCm along Heading (zero = keep the current heading); gravity
	 *  acts until the descent reaches SinkCm, and a faster fall (a dive, the launch) eases back to it -- the wing's lift. */
	static FVector GlideVelocity(const FVector& Velocity, const FVector& Heading, float GravityZ, float DeltaSeconds, float TrimSpeedCm, float SinkCm);

	/** Set where the input happens (the owning client, or the host for its own pawn); the server takes it from the moves. */
	void SetWantsToGlide(bool bGlide) { bWantsToGlide = bGlide; }
	bool WantsToGlide() const { return bWantsToGlide; }

	/** Flying the glider right now: wants to, falling, and the world's host is in developer mode. */
	bool IsGliding() const;

	// ---- traversal: vault / hop up / pull up, and climbing (MOTraversal.h; Wes, Oct 26 2026) ----
	//
	// Two custom movement modes. Traverse follows the path worked out at the jump (FMOTraversal::FPath) for the move's duration; Climb holds the body
	// to a climbable face and moves it along the face with the input (W up, S down, A / D across), pulling up over the edge at the top. Both are
	// predicted like every move: they start from the jump (DoJump) on the joiner and, from the same input, on the server; the traversal's state and
	// the face held ride in the saved moves, so a replay after a correction runs them the same way. Crouch lets go of a climb.

	enum ECustomMovement : uint8
	{
		CMOVE_None = 0,
		CMOVE_Traverse = 1,
		CMOVE_Climb = 2,
	};

	UFUNCTION(BlueprintPure, Category="MO|Traversal")
	bool IsClimbing() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Climb; }

	UFUNCTION(BlueprintPure, Category="MO|Traversal")
	bool IsTraversing() const { return MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Traverse; }

	/** What the climber holds (the speed depends on it). */
	EMOClimbSurface GetClimbSurface() const { return ClimbSurface; }

	/** What a jump would do in front of the body right now (also MO.Test.Traversal). */
	struct FTraversalPlan
	{
		EMOTraversal Kind = EMOTraversal::None;
		float HeightCm = 0.0f;
		float Duration = 0.0f;
		FVector Facing = FVector::ForwardVector;
		FMOTraversal::FPath Path;
	};
	bool FindTraversal(FTraversalPlan& OutPlan, FString* OutWhyNot = nullptr) const;

	/** A climbable face in front of the body within reach (bRequireInputTowardIt: on the ground, only when the input pushes at it -- a jump beside a trunk stays a jump). */
	bool FindClimbableWall(FHitResult& OutHit, EMOClimbSurface& OutSurface, bool bRequireInputTowardIt) const;

	/** Let go of the face (crouch, exhaustion): falls, or stands when Next is walking. */
	void StopClimbing(EMovementMode Next = MOVE_Falling);

	/** How far from the face the climber's capsule is held (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Traversal", meta=(ClampMin="0"))
	float ClimbWallGapCm = 6.0f;

	/** How far past the capsule the hands find a face to grab (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Traversal", meta=(ClampMin="5"))
	float ClimbReachCm = 30.0f;

	/** Effort scale on the climbing speed (the character lowers it when tired). */
	UPROPERTY(BlueprintReadWrite, Category="MO|Traversal")
	float ClimbSpeedScale = 1.0f;

	/** A jump off the face: away from it and up (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Traversal")
	float ClimbPushOffSpeed = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Traversal")
	float ClimbPushOffUpSpeed = 180.0f;

	// ---- water state (valid after the first movement update) ----

	/** Fraction of the capsule under water right now (0 on dry land or when no water is known here). */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	float GetImmersion() const { return Immersion; }

	/** The water surface height under the character, if there is open water there. */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	bool GetWaterSurfaceZ(float& OutSurfaceZ) const
	{
		OutSurfaceZ = WaterSurfaceZ;
		return bWaterKnown;
	}

	/** True when the eyes are below the surface (the head is under: the airway is closed, the underwater look applies). */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	bool IsHeadUnderwater() const;

	/** Depth (cm) of the water over the character's feet; 0 when dry. */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	float GetWaterDepthCm() const;

	/** Depth (cm) of the water column where the character is: surface down to the bottom it could stand on. Huge when no bottom is within the probe (deep water); 0 when dry. */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	float GetWaterColumnDepthCm() const { return WaterColumnDepthCm; }

	/** Effort scale on swimming speed (the character lowers it when exhausted, raises it for a hustle). 1 = the nominal MaxSwimSpeed. */
	UPROPERTY(BlueprintReadWrite, Category="MO|Water")
	float SwimSpeedScale = 1.0f;

	// ---- tuning ----

	/** Water at least this deep where the character is (surface to the bottom) and it starts to swim. About 1 m (Wes, Oct 16 2026). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="30"))
	float SwimEnterDepthCm = 100.0f;

	/** Swimming ends once the water here is shallower than this (lower than the enter depth = hysteresis, so the mode does not flicker at the line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="20"))
	float SwimExitDepthCm = 90.0f;

	/** Walking speed lost to the drag of the water at full immersion (0.55 = about half speed when chest deep). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0", ClampMax="0.9"))
	float WadeDragAtFullImmersion = 0.55f;

	/**
	 * Buoyancy of an unloaded body (stock CMC: 1 = neutral at full immersion; a floating body settles where Buoyancy x Immersion = 1). 1.27 settles at 0.79 immersion: the eyes about 10 cm above the water,
	 * the pose of a person treading water (the capsule is 192 cm with the eyes at 0.83 of it). A heavier load lowers it until the swimmer sinks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0", ClampMax="2"))
	float UnloadedBuoyancy = 1.27f;

	/**
	 * The generator's ocean plane fills everything under sea level, UNDERGROUND too (that is what lets a dug pit below sea level show water). So a pawn that ended up INSIDE the terrain below sea level
	 * (the new-game spawn on a stale coarse collision, a fall through unbuilt terrain) was classed as swimming, floated at the plane, and the fall-through rescue (which only handles FALLING pawns) never
	 * ran. A pawn whose feet are this far below the generator's ground is inside solid rock, not in open water: it does not swim, it falls, and the rescue lifts it. (Player-dug pits are above this line
	 * in practice and stay swimmable until deeper than it: a pit that deep walks its floor in the water instead of swimming, which is harmless.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="50"))
	float BuriedMarginCm = 300.0f;

	/** Pure decision: the feet are more than MarginCm below the ground, i.e. inside the terrain, not in open water. */
	static bool IsBuriedInTerrain(float FeetZ, float GroundZ, float MarginCm) { return FeetZ < GroundZ - MarginCm; }

	// ---- the movement the animation set was made for (Wes, Oct 28 2026: "match the animation project settings"; Docs/Animation_GASP.md) ----
	// GASP's character (SandboxCharacter_CMC: CalculateMaxAcceleration / CalculateGroundFriction / CalculateBrakingDeceleration,
	// UpdateRotation_PreCMC) sets these every frame from its gait; here they follow from the gait each move carries and the velocity, so the
	// server simulates a joiner exactly as the joiner predicts. The gait speeds are the character's (the survivor's: walk 200 / jog 500 / sprint
	// 700, crouched 225: GASP's walk / run / sprint / crouch). Swimming, climbing and the glider keep their own (MaxAcceleration, RotationRate).

	/** This pawn moves as the animation set expects (the survivor, AMOMetaHumanCharacter: it runs the GASP driver). Off, the engine's own
	 *  acceleration, braking, friction and turn rate: the creatures (AMOCreature) share this component and have no GASP animation -- a wolf
	 *  turning at once and reaching speed at 800 cm/s/s was the survivor's model leaking into them (Oct 28). Set on both machines by the class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Movement|Locomotion")
	bool bAnimationSetMovement = false;

	/** Acceleration walking or jogging, on the ground and in the air (air control scales it): GASP's 800. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="1.0"))
	float GaitAcceleration = 800.0f;

	/** Sprinting, the acceleration falls from GaitAcceleration to this as the speed rises through SprintAccelerationSpeedRange (GASP: 300 over
	 *  300-700 cm/s): a sprint builds up. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="1.0"))
	float SprintTopAcceleration = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion")
	FVector2D SprintAccelerationSpeedRange = FVector2D(300.0, 700.0);

	/** Ground friction walking or jogging (GASP 5); sprinting it falls to SprintTopGroundFriction by SprintFrictionTopSpeed (GASP 3 at 500 cm/s):
	 *  wider turns at a sprint. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="0.0"))
	float GaitGroundFriction = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="0.0"))
	float SprintTopGroundFriction = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="1.0"))
	float SprintFrictionTopSpeed = 500.0f;

	/** Braking on the ground while the input is held -- slowing to a lower gait (GASP 500); with no input, BrakingDecelerationWalking (2000). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="0.0"))
	float BrakingDecelerationWithInput = 500.0f;

	/** Turn rate in the air (GASP 200 deg/s). On the ground the capsule turns at once and the animation's root offset turns the body (GASP). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Movement|Locomotion", meta=(ClampMin="0.0"))
	float FallingYawRate = 200.0f;

	/** Pure: GASP's acceleration for a gait at a ground speed. */
	static float GaitAccelerationFor(bool bSprint, float Speed2D, float Gait, float SprintTop, const FVector2D& SprintSpeedRange)
	{
		return bSprint ? FMath::GetMappedRangeValueClamped(SprintSpeedRange, FVector2D(Gait, SprintTop), Speed2D) : Gait;
	}

	/** Pure: GASP's ground friction for a gait at a ground speed. */
	static float GaitFrictionFor(bool bSprint, float Speed2D, float Gait, float SprintTop, float SprintTopSpeed)
	{
		return bSprint ? FMath::GetMappedRangeValueClamped(FVector2D(0.0, SprintTopSpeed), FVector2D(Gait, SprintTop), Speed2D) : Gait;
	}

	//~ Begin UCharacterMovementComponent interface
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual float ImmersionDepth() const override { return Immersion; }
	/** The ENGINE reads this as "swimmable water here": SetPostLandedPhysics, OnTeleported and SetDefaultMovementMode switch to swimming whenever it is true. So it is
	 *  the swim decision (deep enough, see SwimEnterDepthCm), never "any water over the feet" -- that made every landing in the shallows swim again, so a swimmer
	 *  could not stand up until the capsule touched dry land (Wes, Oct 16 2026). Wading code reads GetImmersion(). */
	virtual bool IsInWater() const override { return bSwimmableWater; }
	virtual float GetMaxSpeed() const override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual bool ShouldRemainVertical() const override { return true; }
	virtual APhysicsVolume* GetPhysicsVolume() const override;
	/** The dev glider (see GlideVelocity); otherwise the stock fall. */
	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
	/** No air braking while gliding: hands off, a glider keeps its airspeed (the stock fall brakes a pawn with no input). */
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;
	/** Jump at an obstacle or a climbable face: a vault / hop up / pull-up, or a grab (FMOTraversal); while climbing: pull up or push off. Else the stock jump. */
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	/** A climber can always "jump" (to pull up or push off). */
	virtual bool CanAttemptJump() const override;
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;
	/** A climber faces the face and a traversal its path: no turning to the input meanwhile. */
	virtual void PhysicsRotation(float DeltaTime) override;

	// Co-op smoothness counters (MONetDiagnostics.h): the server's verdict on each of a joiner's moves, and on the joiner the corrections it got
	// and how far its pawn visibly snapped once it replayed its moves. Each calls the stock implementation first; nothing about movement changes.
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		FMovementBaseInterfaceData* ClientMovementBaseInterfaceData, FName ClientBaseBoneName, uint8 ClientMovementMode) override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	//~ End UCharacterMovementComponent interface

private:
	void RefreshWater();

	/** Where the pawn was drawn when the first correction since the last replay arrived (see ClientUpdatePositionAfterServerUpdate). */
	FVector PreCorrectionLocation = FVector::ZeroVector;
	/** What it stood on then (DescribeFloor): with the host's floor in the host's line, the two say which floor the machines disagree about. */
	FString PreCorrectionFloor;
	bool bCorrectionPending = false;

	float Immersion = 0.0f;
	/** See GetWaterColumnDepthCm (set by RefreshWater). */
	float WaterColumnDepthCm = 0.0f;
	float WaterSurfaceZ = 0.0f;
	bool bWaterKnown = false;
	/** Set by RefreshWater: the feet are buried (see BuriedMarginCm). */
	bool bBuried = false;
	/** The swim decision of the last movement update (see IsInWater). */
	bool bSwimmableWater = false;

	// Gait (see SetWantedGait). Read / written by the saved move (FSavedMove_MO) for prediction and replay.
	friend class FSavedMove_MO;
	uint8 bWantsToJog : 1;
	uint8 bWantsToSprint : 1;
	/** The dev glider (see SetWantsToGlide); in the saved moves as FLAG_Custom_2. */
	uint8 bWantsToGlide : 1;
	float GaitJogSpeed = 550.0f;
	float GaitSprintSpeed = 700.0f;

	// Traversal / climbing (see IsTraversing / IsClimbing). Saved and restored by FSavedMove_MO.
	void StartTraversal(const FTraversalPlan& Plan);
	void StartClimbing(const FHitResult& Wall, EMOClimbSurface Surface);
	/** SetMovementMode(MOVE_Custom, Mode) from a jump, keeping the jump press for the move the server replays (see the .cpp). */
	void EnterCustomModeFromJump(uint8 Mode);
	void PhysTraverse(float DeltaTime, int32 Iterations);
	void PhysClimb(float DeltaTime, int32 Iterations);
	/** The body's facing for traversal probes: the face held while climbing, else where the body looks (flat). */
	FVector TraversalFacing() const;
	/** A climbable face along Direction (flattened) within the climbing reach, felt at six holds (chest, head, waist, thighs, the chest's sides):
	 *  OutHit is the nearest climbable hold, its normals replaced by the holds' mean; Distance - capsule radius is the gap to the body. */
	bool TraceClimbFace(const FVector& Direction, FHitResult& OutHit, EMOClimbSurface& OutSurface) const;

	struct FTraverseState
	{
		FMOTraversal::FPath Path;
		FVector Facing = FVector::ForwardVector;
		float Duration = 0.0f;
		float Elapsed = 0.0f;
	};
	FTraverseState Traverse;
	FVector ClimbNormal = FVector::ZeroVector;
	EMOClimbSurface ClimbSurface = EMOClimbSurface::None;
};
