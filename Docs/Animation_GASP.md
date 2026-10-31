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
- **The ankles keep a human range** (`FMOBodyRetargetAnimInstanceProxy::LimitAnkles`, last in the body's evaluation, after the slot and the
  climbing IK; BR31-6, Oct 31 2026). GASP's foot placement turns the foot to the ground with no limit, and the retarget onto the MetaHuman
  adds to it: on Wes's boulder the driver's foot rolled 17 deg while the body's rolled 56-65 ("my ankle is bending like a twig"). In the
  calf's frame, from the foot's reference pose (standing on flat ground the feet sit 5-20 deg from it), flexion is the twist about Z
  (a flat-ground crouch, -42 deg; negative = toes toward the shin): kept to -60 (dorsiflexion: a loaded deep squat; crouching round that
  rock takes 42-58) .. 50 (plantarflexion); the rest of the turn (rolling, turning about the shin) one 30 deg cone. Ordinary motion stays
  inside (walk / sprint / crouch-walk / swim stroke -56..+20 and at most 27 of roll); at the twig spot the body's foot now sits on the
  limits (-60, 30).
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

- **Traversal**: done in phase 2 (section 5) with GASP's sequences on the driver, played by our own moves (its character Blueprint's
  traversal logic and `Traversable` channel are still not taken). Montages on the driver have no root motion effect on our capsule (only
  `GetMesh()` feeds the CMC; `UpdateTraversalAnimation` also drops the driver's extracted root motion each tick).
- **Turned without moving**: an actor turned by anything other than movement or an action (a teleport, `set_actor_rotation` in a check)
  keeps the body's facing until she moves (§3; TRAPS §5).
- **The driver's graph reads the movement component on the animation worker threads** (PoseSearch's trajectory: `GetMaxSpeed`,
  `GetMaxBrakingDeceleration`, `GetGravityZ` -> `GetPhysicsVolume`): whatever those reach must be safe there and never spawn or load. The
  swim volume was spawned lazily from `GetPhysicsVolume`; two swimming survivors spawned it at once and the joiner crashed (Oct 28, TRAPS §11).
- **Cost**: the driver poses even off screen (it must, to feed the body). Fine for a handful of survivors; a colony may want the driver's
  tick tied to the body being rendered.
- **Swim**: the strokes play in the body's slot over the retargeted pose (blended now; before, the override swapped blueprints).
- **ABP_GenericRetarget's per-chain curve blending** (its `ChainCurveMap`, blend-to-source by curve) is not in the native retarget.

## 5. Phase 2 (Oct 29): the moves onto things, and climbing

Wes: "can we use the hanging/grabbing animation for climbing instead of them just walking up the cliff", then of Titan Climbing (free on
Fab, Team Rewind, Fab Standard License, installed to the 5.8 engine): "yeah i just want the animations, we can probably reuse the hand/foot
IK rigging to follow the cliff". Before this the driver's motion matching, which counts our custom modes as on the ground, walked her up
the face.

- **Hop up / pull-up / vault** (`AMOCharacter::UpdateTraversalAnimation`): GASP's own sequences -- `M_Neutral_Traversal_Mantle_1_0_stand`,
  `..._Climb_Start_2_5_stand` (the hands catch the edge, the feet walk the wall), `..._Vault_1_0_stand` -- as dynamic montages on the
  DRIVER (its slot; the retarget carries them to the body; the driver's hold yields to them). Each one's span (leaving the ground -> up on
  top, by the root's whole travel; a vault, -> landed) is fitted to our move's duration: measured from the root curve and set on the survivor
  Blueprint by `Content/Python/setup_gasp_traversal.py` (`FMOTraversalAnim`: mantle 0.48-1.17 s, climb-start 0.56-1.87 s, vault 0.29-0.87 s:
  rates ~1.0 / 0.8-1.2 / ~1.05). Imported by `Tools/import_gasp.py` (10 sequences added to its list Oct 29; the `AM_` montages are not:
  they reference GASP's traversal pose database). Other machines learn the move from the server (`TraversalCue`, replicated skip-owner: kind
  + duration; `[MOTraversal] ...: the server's cue for move N (D s) -> playing`): they see only the movement mode.
- **On a face** (`AMOCharacter::UpdateClimbAnimation`): Titan Climbing's climbing set (`AS_Climb_Idle / Up / Down / Left / Right`, UE5
  mannequin, in place) retargeted onto the MetaHuman (`/MOFramework/Characters/Climbing/*_MH`, the crouch set's `RTG_MoverMannyToMetaHuman`,
  plugin content mounted not enabled: `MOEditorContentUtils.mount_content_path`), its pelvis repaired (the same retargeter put it on the
  floor again: `Content/Python/repair_crouch_pelvis.py` with `REPAIR_CLIPS`, pelvis -5 -> 61-71 cm), looped in the BODY's slot like the
  swim strokes: idle, or up / down / left / right by the climber's motion along the face (debounced 0.2 s), at the rate that matches the climb
  (`ClimbClipVerticalSpeed` 70 cm/s, measured from a planted hand; side 35, an estimate): at 30 cm/s up rock the up clip plays at ~0.42.
- **Hands and feet on the rock** (`UMOBodyRetargetAnimInstance`, `FMOClimbLimbIK`): Titan's approach, our code. The clips carry contact
  curves (`c_lefthand / c_righthand / c_leftfoot / c_rightfoot`; idle and up all four, down the hands, the side clips none); past 0.6 a limb
  sweeps for the rock from where the clip puts it, past 0.95 it stays on that hold (kept in the touched component's space); after the slot
  each chain (upperarm-lowerarm-hand, thigh-calf-foot) bends to its hold by two-bone IK, keeping the clip's elbow / knee direction. Per
  machine and presentation only. Measured on SM_Cliff_09: hands on holds sit 4 cm off the rock, feet 8 (the offsets).
- **Other machines' copies** read the server's replicated movement mode and velocity (`IsClimbingAsSeen` / `IsTraversingAsSeen`, the
  replicated `LinearVelocity`), and never run the climb themselves (`PhysCustom` returns on a simulated proxy) -- until then a copy ran
  `PhysClimb` with a zero face normal, "lost" the face and let go every tick: a host resting on a wall dropped to the ground on the joiner's
  screen (TRAPS).
- No free-climbing cycle in GASP; Titan's jumps, cling, mantle (another skeleton asset) and exit are not taken.

## 6. AnimGen (5.8, experimental): not a source of new motion (checked Oct 29)

Wes: "check out the new animgen example to see if we can build some animations for us to use for stuff like the bows and spears and sling
and all the things we're currently missing". The AnimGen Example (Fab, installed Oct 29 into `Engine/Plugins/Marketplace/AnimGenExample`,
1.9 GB) runs the experimental `AnimGen` plugin (`Engine/Plugins/Experimental/Animation/AnimGen`): a neural controller (an autoencoder plus a
12-layer denoiser, `train_autoencoder.py` / `train_controller.py`, PyTorch through PythonMLPackages, trained in the editor) LEARNED from an
animation database. Its controls are numbers -- a trajectory or a velocity to follow, a target to move to, a bone reaching a point
(Trajectory Interaction), style tags over frame ranges -- with no text or prompt input. It recombines motion it was trained on; it cannot
make a movement nobody recorded. The example's training data is four long mocap takes on the UEFN mannequin: walk and strafe, neutral and
holding a pistol (`Civ_Loc_*`, 0.4-0.56 GB each) and one pistol shot; its trained networks (`AGC_` / `AGAE_` AnimGenCharacter / NPC) know
exactly that. So it makes no bow draw, spear throw or sling swing. Where it could help later: a learned locomotion controller in place of
motion matching (40-50 MB of network instead of the GASP pose database), or carry styles ("walking with a spear") once we HAVE clips of
them. Where the missing action animations should come from is F53's sibling, F54 (DECISIONS).

## 7. Weapon sets: the bow and the spear (Oct 29, F54)

Wes: "let's start with sparrow"; "i added sparrow and 'mega spear animation pack' to mo tools for you to go through". Sources (Fab, local
only, gitignored, copied from MO_Tools at the same /Game paths): **Paragon: Sparrow** (Epic, free; `/Game/ParagonSparrow`, 3 GB: her bow
is bones under `hand_l` -- `bow_base`, the shaft, the string --, the nock under `hand_r`: bow in the left hand, drawn with the right) and
the **Mega Spear Animation Pack** (`/Game/SpearAnimationsV2`, 76 MB, UE4 mannequin: one- and two-hand spear, four 1H and six 2H stances,
jabs / thrusts / swings high, mid and low, walk / run / idle / crouch per stance, blocks, hit reactions, every stance-to-stance
transition, a one-hand throw, and the spear prop's own clips). No sling in either.

Made (tracked, `/MOFramework/Characters/Bow|Spear/`, 34 clips `<name>_MH`): **bow** -- `idle`, `idle_relaxed`, `Travel_mode_Idle`,
`Travel_Mode_Idle_BowDown`, `RMB_Drawback` (the draw), `RMB_Loop` (held at full draw), `RMB_Fire` (the release), `Primary_Fire_Slow /
Med / Fast` (draw and loose in one), `idle_AO_CU / CC / CD` (aim up / level / down at full draw); **spear** -- 2H `Idle`, `walk`, `run`,
`Crouch`, `Thrust_High / Mid / Low`, `Jab_Mid`, `Swing_HorizontalMid`, `Swing_VerticalDown`, `Block`, `HitHigh`, `HitLow`, 1H `Idle`,
`walk`, `run`, `Thrust_Mid`, `Jab_Mid`, the 1H<->2H changes and `A_1H_Throw` (the spear throw; a thrown stone can use it too).
`Content/Python/retarget_weapon_sets.py` remakes all of it in one run: IK rigs (`IK_BowSource` / `IK_SpearSource`: GASP's UE4 mannequin
rig on each pack's mesh, every bone it names present), retargeters (`RTG_BowToMetaHuman` / `RTG_SpearToMetaHuman`: the crouch set's
`RTG_MoverMannyToMetaHuman` with the source swapped, chains re-mapped), the batch retarget, the source's curves removed (Paragon's crashed
a -game process on load: TRAPS), the pelvis repair (the retargeter put every pelvis on the floor again; the repair's thigh check widened
for these: TRAPS), then each clip measured against its source, scaled by the bodies' head heights (her 0.84-0.86 of theirs).

Measured (Oct 29): pelvis, thighs and feet within 4-11 cm of the source in 31 clips; the three 2H thrusts 15 cm (a shallower lunge: pelvis
64 vs 49 cm at the deepest). The draw brings the right hand to 7-15 cm of the head (the anchor at the face), the bow arm out straight;
the 2H grip scales with her (hands 54 cm apart, the source's 69 x 0.84 = 58). Every bow and spear clip loads and plays in a -game instance
(held poses photographed on the survivor: `Saved/Screenshots/weapons/`, side and front, eleven poses).

**The hands (fixed Oct 29, Wes: "the hands are completely fucked up in all of them"):** the first retarget mapped each source finger chain to
the MetaHuman's metacarpal chain as well as its finger chain (the sources have no metacarpals), so every palm bone bent like a knuckle --
140-177 deg off rest, in all 34 clips. The metacarpal chains now have no source and keep the rest pose (0 deg in every clip, as in the
crouch and climbing sets); the script checks it (a palm > 12 deg off rest fails the clip). Close-ups: `Saved/Screenshots/weapons_hands/`.

Not done (F53's work, when the attacks are built): **no weapon in her hands** (the hands close on nothing; the bow item, the spear and
their grip sockets, the bow string following the nock); **the lunges step forward in the pelvis**, not the root -- the throw 110 cm,
the 1H thrust 75, the 2H thrusts 97 (their source moved its root 42 cm, folded into the pelvis by the retargeter) -- so an attack must
carry the capsule with it (root motion from the pelvis, as `UpdateTraversalAnimation` fits the GASP moves) or the body leaves it and
snaps back; the 2H stance is the pack's (back heel up), Sparrow's is Paragon's (wide, stylized); the rest of the spear pack (the other
stances, high / low variants, the spear prop's clips) and the bow's locomotion (jog / sprint with the bow) are there to retarget when wanted
(add them to `SETS` in the script).

## 8. Verifying

`python Tools/check_legs.py` (the femme and every base: standing, walking, crouched; feet and pelvis heights, screenshots in
`Saved/Screenshots/legs/`); `python Tools/check_cliff_climb.py --side 3 [--shots DIR] [--base Harvey]` (each climbing sample names the clip on the body and
the move on the driver with their rates, and each hand's / foot's distance to the rock; `--shots` saves behind + side pairs; `--base` climbs as
another base: Harvey Oct 30, `AS_Climb_Up_MH` on his body, hands 4-6 cm off the rock); `python Tools/check_gasp_movement.py` (the capsule, the gait speeds, the jump, the facing hold, the body's sex). The instance log shows `[MOCharacter] ...: GASP drives the locomotion (SKM_UEFN_Mannequin with
SandboxCharacter_CMC_ABP_C, retargeted by RTG_UEFN_to_Metahuman_nrw)` per survivor.
