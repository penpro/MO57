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

	//~ Begin UCharacterMovementComponent interface
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
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
};
