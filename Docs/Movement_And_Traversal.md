# Movement and traversal (gaits, vaulting, pulling up, climbing)

> **Covers:** how a pawn gets over and up things on foot: the vault, the hop up, the pull-up and climbing (cliffs, rocks, tree trunks); how
> they ride co-op prediction; what they cost the body. Swimming: `Docs/Water.md`. Gaits (walk / jog / sprint) and the dev glider are in
> `UMOCharacterMovementComponent` beside these (its header); since Oct 28 the gaits, acceleration, braking, turning and the plain jump are the
> animation set's (walk 200 / jog 500 / sprint 700, jump 500 cm/s: `Docs/Animation_GASP.md` §3).
> **Current state (2026-10-29):** built and verified in a running game (Wes, Oct 26: "we need to implement climbing of cliffs / trees and we
> need a vault mechanic to hop / pull up on things"), and since Oct 29 on the world's own rocks (Wes: "i can't vault or climb rocks and
> such"): section 5; since Oct 29 up the Electric Dreams cliffs (a 28.7 m top-out on one bar; roofs and undercuts stop a line, as on real
> rock). Animated since Oct 29: GASP's mantle / climb-start / vault for the moves, Titan Climbing's climbing set with hand / foot IK on a
> face (`Docs/Animation_GASP.md` section 5). Since Oct 29 stamina limits a
> climb, by the face's angle, the climber's Climbing skill and muscle share (F51, F52 B; section 3): a slab rests, a sheer face ~100 s a bar.
> **Entry points:** `MOTraversal.h` (the pure decisions), `UMOCharacterMovementComponent` (custom modes `CMOVE_Traverse`, `CMOVE_Climb`;
> `DoJump`, `FindTraversal`, `FindClimbableWall`, `PhysTraverse`, `PhysClimb`), `AMOCharacter` (`GetClimbEffort`, `ClimbingMET`,
> `ClimbingStaminaCostPerSecond`, `ClimbingCostByFaceAngle`, `ClimbingExertionByFaceAngle`, `ClimbingSkillId`). **Dev verbs:** `MO.Test.SpawnLedge <HeightCm> [DepthCm] [Climbable] [DistanceCm]`, `MO.Test.ClearLedges`,
> `MO.Test.Traversal` (what a jump would do here, and why not). **Tests:** `MOFramework.Traversal.*` (ClassifyLedges, PathShape, Climbable,
> PickTop); `python Tools/check_traversal_world.py [--execute]` (the world's rocks, cliffs and trees round a new world's start);
> `python Tools/check_cliff_climb.py [--all]` (each side of an Electric Dreams cliff climbed for real). Both take `--base <Id>` (Oct 30, Wes:
> "i tried harvey and he just walks up the tree ... it also doesn't seem like he can vault"): as Harvey on the current build the climb plays
> the climbing set on his body, hands on the rock, and he vaults, hops up and pulls up -- his Steam build (`2121b981`, Oct 28) predates the
> climbing animation (Oct 29), when every survivor walked up a face. The base only swaps the body; the moves and the driver are the same.

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
At a lip -- the hands find no more rock above and there is no room to pull over (a top under an overhang, a plain wall above) -- the climber
**holds on** (N-236, Wes Oct 29: "Yes, fix N-236"): W does nothing, A / D move along the lip to a place to pull over, S climbs down; only a
crouch, exhaustion or a lost grip lets go. Until then it let go there and fell.
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
- Speed (hands and feet, a hold at a time): rock 30 cm/s, a trunk 22 cm/s, times the climber's own `ClimbSpeedScale` (0.85-1.55: the
  Climbing skill and the muscle share, section 3; the character sets it on the server and the owning client alike). W climbs up the face, S down (reaching the ground stands), A / D across it; pushing up at the top pulls over the edge.

## 3. What it costs

Climbing is 8 METs (rock scrambling and tree climbing run 7-9). What a climb costs follows three things (F51, Wes Oct 29: "make it angle
dependent, but also tie some skill or physical attribute to it ... the more muscle mass percentage of the body the better at climbing and
the more skill the better"), worked out in one place, `AMOCharacter::GetClimbEffort` (`FMOClimbEffort`, Blueprint-readable):

- **The face's angle** from level ground (`FMOTraversal::FaceAngleDeg` of the smoothed normal the six holds give): a slab leaning back is
  mostly feet, an overhang hangs off the arms. Share of a sheer face's cost (`ClimbingCostByFaceAngle`) and exertion
  (`ClimbingExertionByFaceAngle`):

  | Face | 45 deg | 60 deg | 75 deg | 90 deg (sheer) | 100 deg | 110 deg | 125 deg | 140 deg on |
  |---|---|---|---|---|---|---|---|---|
  | Cost share | 0.2 | 0.3 | 0.6 | 1 | 1.3 | 2 | 3 | 4 |
  | Exertion | 30 | 40 | 55 | 70 | ~78 | 85 | 90 | 90 |

  Stamina recovers by exertion (F52 B: none from 55 up), so a slab under ~70 deg recovers more than it costs -- it rests as it climbs --
  and steeper rock only spends.
- **The climber** (`FMOTraversal::ClimbAbility`): strength against weight -- the cost scales with 0.5 / (muscle mass / total weight),
  clamped 0.6-1.8 (the femme base 0.48 -> x1.03; 35 % fat with 25 kg of muscle 0.37 -> x1.34; 10 % fat with 40 kg 0.62 -> x0.81) and the
  speed gently (0.85-1.15); and the **Climbing** skill (DT_Skills; 0.5 XP a second on a sheer face, times the face's cost share: steeper
  rock teaches more) -- 1 - e^(-level/10) of its effect (level 5 ~40 %, 10 ~63 %, 20 ~86 %): up to half the cost off, up to 35 % speed.
- **The base**: `ClimbingStaminaCostPerSecond` 1 a second on a sheer face for an untrained climber of average build (was 3 on every face
  until Oct 29): a full bar is ~100 s, ~26 m at ~26 cm/s.

Out of stamina the hands give out and the climber falls -- decided on the server (the log line names the face and both scales). Each
climb logs what it costs (`[MOTraversal] ... climbs a 87 deg face: 0.94 stamina/s (face x0.91, climber x1.03: Climbing 0, muscle 48 % of
62 kg), speed x0.98`). Standing on a ledge the bar comes back at 10/s. Measured Oct 29 (`check_cliff_climb.py --side 3 --real-stamina`,
SM_Cliff_09, the femme base, Climbing 0): the 75 deg first pitch at 0.62/s to a ledge, full again on it, then 21 m of 83-104 deg face at
0.84-1.63/s from 100 to 13 stamina, and over the crest at 28.7 m. (Before F51, at 3/s everywhere: out of strength 7.9 m up the second face,
a fall from 15 m.) Other climbers on the same line, fresh runs (`--body`, `--climbing-level`): 35 % fat with 25 kg of muscle (x1.34, speed
x0.87) 24 cm/s up the second face, 100 -> 24 by 18 m above the foot; the femme at Climbing 10 (x0.71, speed x1.20) 33 cm/s, 100 -> 71 by
18 m -- both then reached a lip with no room to stand at ~27.5 m and let go (N-236), with a quarter and two thirds of a bar left. Since the
climber holds on at a lip (N-236, same day, fresh runs): at Climbing 10 it held at that lip (hands 27.5 m; the holds there feel a 24 deg
slab, so the bar came back 72 -> 100), the check's steering moved it along, and it topped out at 33.5 m; the heavy body held at the same lip
(24 -> 42), then stuck higher after four steers and ended 25 m up, still on the face -- no fall either way.

## 4. How it works (co-op)

- Two custom movement modes on `UMOCharacterMovementComponent`. **Traverse** follows a path worked out at the jump
  (`FMOTraversal::FPath`: rise straight up beside the face to clear the edge, over it at that height, settle) for the move's duration;
  **Climb** holds the capsule `ClimbWallGapCm` from the face and moves it along the face with the input.
- Only the owner and the server run them. Another machine's copy (a simulated proxy) is moved by the server's replicated position: its
  `PhysCustom` returns for these modes (Oct 29: a copy ran the climb with a face normal it never had, let go every tick and dropped to the
  ground), and what it shows reads the replicated mode (`IsClimbingAsSeen` / `IsTraversingAsSeen`). Held at a lip (N-236), the undone move
  also goes back to the tick's starting face normal (Oct 29: kept, the next tick traced in the new direction from the old spot and let go).
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
  the top if one is in reach (Oct 29: a rock grabbed and let go 0.24 s later). **A climbing step that would leave every hold is not made**
  (`PhysClimb`, N-236): after each step the holds are felt again; none, and no pull-over from there, and the step is undone -- the body keeps
  the holds it had (`[MOTraversal] ... holds on at the lip at ..., no pull-over: <why>`, once per lip). Decided by the position, the face
  normal (which rides the saved moves) and the rock, so a joiner's replay holds where the server does. The face lost at the start of a
  step (no step took it there: right after a grab, a correction) still lets go.
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
reach and the climber pulled over the top inside the window); since Oct 29 (N-236) a lip -- a 4 m climbable wall 10 cm deep with a 7 m
plain wall flush behind it: the joiner climbs to it and holds on on both machines (the host and the joiner logged the hold at the same
spot, 0.0 cm apart; 3 of 3 runs that reached it). The test turns the joiner to each block only once the joiner's machine has it and
prints what it faced. Some runs fail from the first step (N-238): the joiner's jump never reaches the server's traversal;
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
With the real stamina cost on (`--real-stamina`), side 3 topped out at 23 m (the whole stack) with stamina never below 97 while recovery ran whatever the body did; since F52 B (Oct 29) at 3/s the same run was out of strength 7.9 m up the second pitch and fell from 15 m; since F51 (Oct 29) the face sets the cost and it tops out at 28.7 m on one bar (section 3). After a top-out W is still held: the climber walks on across the crest and the feet's last reading can be lower than the pull-over.
