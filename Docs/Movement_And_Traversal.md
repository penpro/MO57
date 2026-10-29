# Movement and traversal (gaits, vaulting, pulling up, climbing)

> **Covers:** how a pawn gets over and up things on foot: the vault, the hop up, the pull-up and climbing (cliffs, rocks, tree trunks); how
> they ride co-op prediction; what they cost the body. Swimming: `Docs/Water.md`. Gaits (walk / jog / sprint) and the dev glider are in
> `UMOCharacterMovementComponent` beside these (its header); since Oct 28 the gaits, acceleration, braking, turning and the plain jump are the
> animation set's (walk 200 / jog 500 / sprint 700, jump 500 cm/s: `Docs/Animation_GASP.md` §3).
> **Current state (2026-10-29):** built and verified in a running game (Wes, Oct 26: "we need to implement climbing of cliffs / trees and we
> need a vault mechanic to hop / pull up on things"), and since Oct 29 on the world's own rocks (Wes: "i can't vault or climb rocks and
> such"): section 5; since Oct 29 up the Electric Dreams cliffs (a 19 m top-out; roofs and undercuts stop a line, as on real rock).
> No climbing / vaulting animations yet: the body moves on the right path in a stand-in pose (BACKLOG). Stamina does not limit a climb
> today (it recovers faster than climbing costs: F52, then F51, section 3).
> **Entry points:** `MOTraversal.h` (the pure decisions), `UMOCharacterMovementComponent` (custom modes `CMOVE_Traverse`, `CMOVE_Climb`;
> `DoJump`, `FindTraversal`, `FindClimbableWall`, `PhysTraverse`, `PhysClimb`), `AMOCharacter` (`ClimbingMET` / `ClimbingExertion` /
> `ClimbingStaminaCostPerSecond`). **Dev verbs:** `MO.Test.SpawnLedge <HeightCm> [DepthCm] [Climbable] [DistanceCm]`, `MO.Test.ClearLedges`,
> `MO.Test.Traversal` (what a jump would do here, and why not). **Tests:** `MOFramework.Traversal.*` (ClassifyLedges, PathShape, Climbable,
> PickTop); `python Tools/check_traversal_world.py [--execute]` (the world's rocks, cliffs and trees round a new world's start);
> `python Tools/check_cliff_climb.py [--all]` (each side of an Electric Dreams cliff climbed for real).

## 1. Jump is the one verb

| In front of the body | Jump does | Time |
|---|---|---|
| under 50 cm | a step or a plain jump (the survivor's movement component steps up 75 cm and walks 60 degree slopes: below, section 5) | |
| 50-120 cm, thin, ground just behind (a fence, a log: at most 70 cm deep) | **vault** over it to the ground behind | 0.55 s |
| 50-120 cm with room on top | **hop up** onto it | 0.6-0.8 s |
| 120-220 cm (arm's reach with a jump) with room on top | **pull up** onto it | 1.1-1.6 s |
| higher, and climbable, while pushing at it | **grab** it and climb | |
| while climbing | **pull up** if the edge is in reach, else **push off** the face | |

Crouch while climbing lets go -- and is used up by it (crouch is a toggle here: a climber who let go lands standing and can grab again).
A grab from the ground needs the input pushing at the face (a jump beside a tree trunk stays a jump); in the air it does not (catch a face
mid-fall). Heights are a 1.75 m adult's.

## 2. What is climbable (`FMOTraversal::ClassifyClimbSurface`)

- Rock: a component with a rock tag (`FMOTraversal::IsRockTag`: `MOLandmark` the crags, cliffs, xlarge boulders of the landmark tier;
  `MOResource_Rock` the harvestable rocks; `MOBiomeCover_Rock` the biomes' rock cover -- Oct 29: the PCG rocks named by an id, ukopfbks_tier_2,
  wgosajs_tier_2, were not climbable before), `MOClimbable` (component or actor tag: anything a designer marks), a mesh whose path says Rock /
  Cliff / Boulder / Crag / Stone / Shelf (the Electric Dreams cliffs: SM_Cliff_*, the sandstone and vine cliffs), or the voxel terrain where
  its face is steeper than 55 degrees (a cliff; gentler ground is walked).
- A tree: a component tagged `MOTreeTrunk` (the PCG tree tier) or a mesh whose path says Tree / Trunk.
- Never a built thing (`AMOBuildableActor`): its walls are smooth.
- Speed (hands and feet, a hold at a time): rock 30 cm/s, a trunk 22 cm/s; `ClimbSpeedScale` on the movement component (the character can
  lower it when tired). W climbs up the face, S down (reaching the ground stands), A / D across it; pushing up at the top pulls over the edge.

## 3. What it costs

Climbing is 8 METs (rock scrambling and tree climbing run 7-9), exertion 75, and 3 stamina a second moving or hanging (a jog is 2, a sprint
8). Out of stamina the hands give out and the climber falls -- decided on the server. On paper a full bar (100) lasts 33 s of climbing,
about 10 m at 30 cm/s, where the Electric Dreams cliffs are 18-45 m. **In play it never runs out** (Oct 29): stamina recovers 10/s at all
times, faster than every cost, so with the real cost on, SM_Cliff_09 was climbed to its 23 m top with stamina never below 97 (and a 15 s
sprint never got below 92). Open forks (DECISIONS): F52, whether recovery pauses while the body works hard (recommended), then F51, the
cost following the face's steepness (a slab mostly feet and restful, an overhang dear).

## 4. How it works (co-op)

- Two custom movement modes on `UMOCharacterMovementComponent`. **Traverse** follows a path worked out at the jump
  (`FMOTraversal::FPath`: rise straight up beside the face to clear the edge, over it at that height, settle) for the move's duration;
  **Climb** holds the capsule `ClimbWallGapCm` from the face and moves it along the face with the input.
- They start in `DoJump`, which the movement component runs from the jump input on the joiner AND, from the same input in the moves, on the
  server: both machines trace the same world and start the same move. The traversal's state (path, duration, elapsed) and the face held
  ride in the saved moves (`FSavedMove_MO`), so a replay after a correction runs them the same way. **The jump press must ride the move
  that starts it** (`EnterCustomModeFromJump`): changing to any mode but falling clears the press (`ACharacter::OnMovementModeChanged` ->
  `ResetJumpState`), and the joiner's machine changes mode inside `DoJump`, BEFORE it records the move for the server. Until Oct 26 the
  server therefore never saw the press, never started the move and pulled the joiner back (a plain jump goes to falling, which keeps
  it, so plain jumps always worked). Found by the first two-process run. A root-motion source was considered
  and dropped: its path curve is compared by pointer between machines (`FRootMotionSource_MoveToForce::Matches`), and a delayed second leg
  is not handled under corrections (engine comment).
- `FindTraversal` probes: rays at knee to reach height for a face (reach = capsule radius + 60 cm; the survivor's radius is 30.7 cm since
  the Oct 28 capsule); small spheres dropped onto the top at 8, 25, 45, 70 and 100 cm past the face, and `FMOTraversal::RankTops` orders the
  walkable ones best first (the highest; the nearest of equals within 2 cm, so a block's top is its edge) up to where the obstacle rises out
  of reach or ends -- a rock is a dome, and one probe just past the face read its steep shoulder as its top (Oct 29: a 135 cm boulder as a
  77 cm ledge with no room on top); for each, best first, a capsule swept straight up for room to rise (from 5 cm up: the body's floor
  contact is not a ceiling) and a capsule overlap for room on top, standing 2 cm above the top or, on uneven rock, 20 cm (it lands at the
  move's end); for a vault the ground just behind. `MO.Test.Traversal` prints the answer and, when there is none, why.
- Climbing, the face lost at the hands while pushing up (the top come level with the chest: a rounded edge, a boulder's crest) pulls over
  the top if one is in reach, before letting go (Oct 29: a rock grabbed and let go 0.24 s later).
- **The face is felt at six holds** (`TraceClimbFace`: the chest, above the head, the waist, the thighs, either side of the chest), for the
  grab and for every climbing step: the climber holds the gap to the nearest and steers by their mean normal, which the body turns to over
  ~0.1 s. Until Oct 29 one ray at the chest did both, and on an Electric Dreams cliff its normal swings up to 70 degrees between facets 25 cm
  apart: the climber turned off the face and let go 1-3 m up every side. The face is lost only when it leaves every hold; the log then
  says what was there and why there was no pull-over (`[MOTraversal] ... the face ended at the hands (...) ...; no pull-over: <why>`).
- The stuck-in-the-ground rescue (`AMOCharacter::CheckEmbeddedSafety`) skips a climbing or traversing body: it read a climber against a
  cliff's bumps as embedded and lifted it off the face 5 s into every climb (Oct 29).
- W is camera-relative, like every move: a face that turns 50 degrees off the camera splits W into up and across, so a player keeps the
  camera on the face (as on foot). A face-relative mapping (W always up) is possible: Wes's call.

## 5. Verified (Oct 26, one game instance, real input actions through `MO.Test.InjectInput`)

Hop up onto a 90 cm block (standing on it after); vault a 90 x 30 cm fence (1.58 m on, same ground height); pull up onto a 170 cm ledge; a
4 m plain block: no move (control); a 4 m climbable block: grabbed while pushing at it, 120 cm climbed in 4 s, crouch let go and the body
landed standing; a 3.2 m climbable block with W held: climbed and pulled over the top (standing on it).

**A joiner (Oct 26, `python Tools/ue.py nettest traversal`: host + joiner processes, real input actions on the joiner):** hops up onto a
90 cm block and is on it on both machines (0.0 cm apart); jumping at a 4 m climbable face while pushing at it grabs it on both; W climbs
120 cm in 4 s on both (0.0 cm apart); crouch lets go on both. The blocks are `AMOTestLedge`s (replicated with their size and climbable
flag: an `AStaticMeshActor`'s scale and tags do not replicate, so a joiner saw a 1 m plain cube where the host had a 4 m climbable face;
since Oct 28 the climb uses a 5 m face: the grab climbs ~70 cm while the first push is held, so 4 s of W more brought a 4 m edge within arm's
reach and the climber pulled over the top inside the window);
`MO.Test.SpawnLedge ... [PlayerIndex]` on the host puts one in front of the joiner's pawn. The host logs why it refused a joiner's jump
(`[MOTraversal] server: ... has no traversal: <why>`).

**The world's own rocks (Oct 29, `python Tools/check_traversal_world.py --execute`, seed 777, the PCG rocks round the start):** before the
rock tags and the ranked tops, 1 of ~25 rocks offered a move (the rest a plain jump: "a 77 cm ledge with no room on top", "nothing in
front"); after, of 22 rocks 60 cm or more above the ground 16 offer a vault, hop up or pull-up and 2 taller ones a climb. Done for real
(W held, a jump): pull-ups onto 126-209 cm and hop-ups onto 73-115 cm, each standing on top after; a grab on wc2hbfr_tier_2 climbed and
pulled over. Rocks whose sides are walkable are walked over (the survivor steps up 75 cm and walks 60 degree slopes: the Blueprint's
movement values, which no code or script sets -- a person steps ~50 cm unaided; Wes's call, BACKLOG N-232). Left: one 149 cm beach-rock
cluster with no room on its lumpy top; two big boulders that read "nothing in front" (the check's placement on their slopes is suspect); a
jump that found nothing 0.15 s after a check that offered a pull-up. Trees: the trunks are grabbed ("climbable=tree"). The Electric Dreams
cliffs (600 m from that start) read as rock by their names; a full climb to a top was not run: F51. How it looks: no animations yet.

**An Electric Dreams cliff (Oct 29, `python Tools/check_cliff_climb.py --all`, seed 777, SM_Cliff_09 with its neighbours, climbing stamina 0;
Wes: "i want to be able to climb on top of the electric dreams cliffs"):** five of its eight sides have a face at the foot (the others: an
undercut, or no steep face). How high the feet got of the ~11 m to its top:

| Run | Side 0 | Side 2 | Side 3 | Side 4 | Side 6 |
|---|---|---|---|---|---|
| before (W held, one jump) | 1.4 m, then moved off the face by the stuck-in-the-ground rescue | | | | |
| rescue fixed, one ray at the chest | 0.9 m | 2.2 m, let go | 3.2 m, let go | 1.5 m, let go | 0.8 m |
| six holds, mean normal | 3.2 m | 5.7 m | 5.7 m | 3.8 m | 0.8 m |
| + steering across when stuck, as a player would | 4.8 m | 5.7 m | **topped out: 19 m** (two ledges, then 8.6 m of face in 30 s and over the crest) | 3.8 m | 0.8 m |

Where it stops now is the rock, not the climb: side 4 sits in a roof pocket (a second cliff mesh's flat underside over the head, 6-30 cm
of room to rise for 3.5 m to one side, rock jutting in on the other); side 0 under another such roof; side 6 is an undercut (the foot
overhangs, nothing in reach). A player goes round a roof or down and across to another line; the check's steering tries 3.6 m at most.
With the real stamina cost on (`--real-stamina`), side 3 topped out at 23 m (the whole stack) with stamina never below 97: it recovers faster than climbing costs (F52). On paper the 8.6 m pitch's 30 s would be about a full bar.
