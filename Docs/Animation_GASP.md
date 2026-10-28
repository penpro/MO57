# The survivor's locomotion: Epic's Game Animation Sample (GASP)

> **Decided Oct 27 2026** (DECISIONS F50 = A; Wes: "the game animation sample looks great", then "start phase 1: plugins, migrate the CMC
> character and its animation set (keep our MetaHumans/Common), and drive our survivor with it"; "i like everything in the animation level").
> Phase 1 (this doc): walking, running, sprinting, crouching, jumping and landing come from GASP's motion-matching character. Later phases:
> GASP's traversal (vault / mantle / hurdle) for `Docs/Movement_And_Traversal.md`'s moves, which today slide the body in a stand-in pose.

## 1. How it is put together

```
capsule (AMOCharacter, our CMC: UMOCharacterMovementComponent)
 └─ body: GetMesh(), the MetaHuman              anim: UMOBodyRetargetAnimInstance (native)
     ├─ face, grooms (leader pose on the body)        retarget(driver -> body) -> "DefaultSlot" -> pose
     └─ LocomotionDriver: SKM_UEFN_Mannequin     anim: GASP's SandboxCharacter_CMC_ABP (motion matching)
        hidden, attached at identity                  asks the pawn: BPI_SandboxCharacter_Pawn::Get_PropertiesForAnimation
```

- **The driver** (`AMOCharacter::SetUpLocomotionDriver`, BeginPlay, not on a dedicated server) is GASP's UEFN mannequin running GASP's CMC
  animation blueprint, unchanged. It is hidden but always poses (`AlwaysTickPoseAndRefreshBones`, forced LOD 0 so the fingers the retarget
  reads stay in). Attached to the body at identity, it stands exactly where the body stands and follows every offset ACharacter gives the body
  (the crouch's capsule adjustment, network smoothing, the swimmer's lift), so GASP's foot placement traces from the right place. Tick order:
  movement -> driver -> body (tick prerequisites; the attach-made "child after parent" prerequisite is removed, or the body would copy last
  frame's pose -- the engine refuses the body's prerequisite as a cycle and logs it).
- **The body** runs `UMOBodyRetargetAnimInstance`: the engine's `FAnimNode_RetargetPoseFromMesh` (source = the driver,
  `RTG_UEFN_to_Metahuman_nrw`) run natively as the graph's root, then a `DefaultSlot` kept by hand in the proxy (as `UMOCreatureAnimInstance`
  keeps its cue slot). The slot is why it is not GASP's `ABP_GenericRetarget`: our montages are on the MetaHuman skeleton (combat swings
  `UMOCombatComponent`, the swim strokes `AMOCharacter::UpdateSwimAnimation`) and play over the retargeted walk. The retarget is the pattern
  of the engine's retarget preview instance (IKRigEditor `UIKRetargetAnimInstance`). Not `FAnimNode_Slot`: in 5.8 it calls the Sequencer
  mixer (MovieSceneAnimMixer, enabled for GASP), which asserts that the instance has a blueprint's anim subsystems (TRAPS).
- **The pawn interface.** GASP's ABP calls `Get_PropertiesForAnimation` on its owning actor. `BP_MOMetaHuman_Female` implements
  `BPI_SandboxCharacter_Pawn` and builds the `S_CharacterPropertiesForAnimation` struct the way GASP's own character does, except where the
  data is ours (C++ getters, category `MO|Animation`):

  | GASP field | From |
  |---|---|
  | Gait (walk / run / sprint) and `WantsToSprint` / `WantsToWalk` | `IsAnimGaitSprint` / `IsAnimGaitWalk`: our walk / jog / sprint. The owner and the server read the gait the moves carry (`UMOCharacterMovementComponent::WantsToJog / WantsToSprint`); another machine reads it from the replicated speed (halfway between the gait speeds) |
  | Stance, `WantsToCrouch` | the CMC's `IsCrouching` (our crouch is ACharacter's) |
  | Rotation mode, `WantsToStrafe` | the CMC's `bOrientRotationToMovement` (ours: orient to movement) |
  | Movement mode | falling or swimming = InAir, else OnGround (GASP's mapping; our traversal / climbing custom modes count as OnGround) |
  | `JustLanded`, `LandVelocity` | `HasJustLanded` (0.3 s after a fall ends, GASP's window), `GetLandVelocity`: watched from the movement mode every tick on every machine |
  | Ground normal / location | `GetAnimGroundNormal` / `GetAnimGroundLocation`: the CMC's current floor hit |
  | Velocity, acceleration, max acceleration, braking, actor transform / rotation | the pawn and its CMC, as GASP reads them |
  | Aiming rotation | `GetBaseAimRotation` (GASP picks control rotation when locally controlled: same value for a player) |
  | `WantsToAim` | false for now (no aim stance yet) |

  The graph is written by `Content/Python/setup_gasp_survivor.py` (re-runnable; it also sets the three asset properties below).
- **The switch** is data: `AMOCharacter::LocomotionDriverMesh` / `LocomotionDriverAnimClass` / `BodyRetargeter` (soft, set on the survivor
  Blueprint). A pawn without them -- or a clone without the imported GASP content -- keeps the body's own blueprint (`ABP_MH_F`) and logs a
  Warning naming the missing asset.
- **Crouch.** Under GASP the motion matching crouches (Stance); the old crouch clips in the slot (`UpdateCrouchAnimation`) are skipped while a
  driver exists.
- **Config** the sample needs, copied into MO57: its `DDCvar.*` console variables (`DefaultEngine.ini` `[/Script/Engine.DataDrivenConsoleVariableSettings]`:
  foot placement on, the thread-safe update on, ...), `p.EnableCharacterAccelerationReplication=1` (a remote pawn's trajectory reads its
  acceleration), `PoseSearchSettings AvailabilitiesBufferSize=230`, and its `Foley.*` / `MotionMatching.*` gameplay tags
  (`DefaultGameplayTags.ini`). Not copied: its trace channels (`Traversable` / `Mouse` / `Obstacle` collide with our GameTraceChannel1/2,
  Interactable / WaterTrace; traversal phase), its input, cameras, levels and the Mover setup.

## 2. The content (imported, not committed)

`python Tools/import_gasp.py [--from <GameAnimationSample project>]` copies the 1,323 packages in `Tools/data/gasp_packages.json` (1.4 GB:
the CMC ABP, the UEFN animation set, pose-search databases, choosers, foley, the MetaHuman retargets) to the same `/Game` paths, never
overwriting: MO57's `/Game/MetaHumans/Common` stays ours (its `metahuman_base_skel` carries the foot IK virtual bones). The folders are
gitignored (`Content/Blueprints/`, `Content/Characters/UE4_Mannequin|UE5_Mannequins|UEFN_Mannequin/`, `Content/Misc/`, `Content/Audio/Foley|Mix/`)
like the Animals Full Pack: a fresh clone or DARKTOWER gets the sample from Fab (free) and runs the script. The list is the dependency
closure of `SandboxCharacter_CMC_ABP`, `ABP_GenericRetarget` and `RTG_UEFN_to_Metahuman_nrw` / `_ovw`, cut at the Paragon / Echo / Kellan
characters and the levels; to remake it, run the closure survey (`closure()` in the Oct 27 session's `gasp_survey2.py`: the asset registry's
hard + soft package dependencies) in the sample's editor and write `packages` to the JSON.

Plugins enabled for it (`MO57.uproject`): PoseSearch, Chooser, AnimationWarping, MotionWarping, BlendStack, CurveExpression, DrawDebugLibrary,
Mover, NetworkPrediction, MovieSceneAnimMixer, AnimationLocomotionLibrary; MOFramework depends on IKRig (+ AnimGraphRuntime).

## 3. The movement the set was made for (Oct 28)

Wes: "match the animation project settings". GASP's SandboxCharacter_CMC sets its movement every frame from its gait; ours follows from the gait
each move carries and the velocity (`UMOCharacterMovementComponent`: `GetMaxAcceleration`, `GetMaxBrakingDeceleration`, `CalcVelocity`,
`GetDeltaRotation`), so co-op prediction stays exact. Read from the sample's graphs (test `MOFramework.Movement.AnimationSetMovement`):

| | GASP / ours now | before |
|---|---|---|
| walk / jog (GASP's run) / sprint | 200 / 500 / 700 cm/s | 250 / 450 / 700 |
| crouched walk | 225 | 200 |
| acceleration (ground and air) | 800; sprinting it falls to 300 as the speed rises 300 -> 700 | 2048 |
| braking on the ground | 500 with the input held (slowing to a lower gait), 2000 with none | 2000 |
| ground friction | 5; sprinting it falls to 3 by 500 cm/s | 8 |
| turning | at once on the ground (the animation's root offset turns the body), 200 deg/s in the air | 500 deg/s |
| jump / air control / air braking | 500 cm/s (a 1.27 m rise) / 0.25 / 0 | 700 (2.5 m) / 0.35 / 1500 |

Swimming, climbing and the glider keep MaxAcceleration and RotationRate. Only the survivor moves this way: `AMOMetaHumanCharacter`'s
constructor sets the speeds and the component's `bAnimationSetMovement`; the creatures share `UMOCharacterMovementComponent` and
`AMOCharacter`'s defaults and keep the engine's acceleration and turning (the first build of this put GASP's model on every wolf and bear
too: instant turns, 800 cm/s/s; test `AnimationSetMovement` has the creature CONTROL). The survivor Blueprint's values are set by
`Content/Python/setup_survivor_body.py` (the capsule and the body at scale 1: they carried 0.9425 / 1.05). Footstep loudness follows the
gait speeds (test `FootfallLoudness`).

**A standing body keeps its facing.** GASP's root offset accumulates a capsule turn while she stands and lets go only while a montage plays in
its own slot (`Get_OffsetRootRotationMode`: Release then, else Accumulate), or turns her on the spot when she stops with the body more than 50
degrees off. So when the GAME turns a standing survivor (an attack facing its target, `UMOCombatComponent::BeginFacingSwing`), the body would
keep pointing the old way. Two mirrors of GASP's rule (`AMOCharacter::UpdateDriverHold`): whenever a montage plays on the body (our attacks,
swim strokes) the driver plays a hold idle (`LocomotionDriverHoldAnim`, GASP's M_Neutral_Stand_Idle_Loop) in its slot, and an attack's turn
asks for the same while she stands (`HoldBodyToCapsule`) -- the body lines up with the capsule. Measured (`python Tools/check_gasp_movement.py`):
the capsule turned 90 degrees with no action, the body stays; an attack, it follows.

## 4. Known gaps (phase 1)

- **Traversal**: GASP's vault / mantle / hurdle live in its character Blueprint (not taken) and need its `Traversable` trace channel; ours
  still slide in a stand-in pose. Montages GASP plays on the driver have no root motion effect on our capsule (only `GetMesh()` feeds the CMC).
- **Turned without moving**: an actor turned by anything other than movement or an action (a teleport, `set_actor_rotation` in a check)
  keeps the body's facing until she moves (§3; TRAPS §5).
- **The driver's graph reads the movement component on the animation worker threads** (PoseSearch's trajectory: `GetMaxSpeed`,
  `GetMaxBrakingDeceleration`, `GetGravityZ` -> `GetPhysicsVolume`): whatever those reach must be safe there and never spawn or load. The
  swim volume was spawned lazily from `GetPhysicsVolume`; two swimming survivors spawned it at once and the joiner crashed (Oct 28, TRAPS §11).
- **Cost**: the driver poses even off screen (it must, to feed the body). Fine for a handful of survivors; a colony may want the driver's
  tick tied to the body being rendered.
- **Swim**: the strokes play in the body's slot over the retargeted pose (blended now; before, the override swapped blueprints).
- **ABP_GenericRetarget's per-chain curve blending** (its `ChainCurveMap`, blend-to-source by curve) is not in the native retarget.

## 5. Verifying

`python Tools/check_legs.py` (the femme and every base: standing, walking, crouched; feet and pelvis heights, screenshots in
`Saved/Screenshots/legs/`); `python Tools/check_gasp_movement.py` (the capsule, the gait speeds, the jump, the facing hold, the body's sex). The instance log shows `[MOCharacter] ...: GASP drives the locomotion (SKM_UEFN_Mannequin with
SandboxCharacter_CMC_ABP_C, retargeted by RTG_UEFN_to_Metahuman_nrw)` per survivor.
