# Movement and traversal (gaits, vaulting, pulling up, climbing)

> **Covers:** how a pawn gets over and up things on foot: the vault, the hop up, the pull-up and climbing (cliffs, rocks, tree trunks); how
> they ride co-op prediction; what they cost the body. Swimming: `Docs/Water.md`. Gaits (walk / jog / sprint) and the dev glider are in
> `UMOCharacterMovementComponent` beside these (its header).
> **Current state (2026-10-26):** built and verified in a running game (Wes, Oct 26: "we need to implement climbing of cliffs / trees and we
> need a vault mechanic to hop / pull up on things"). No climbing / vaulting animations yet: the body moves on the right path in a stand-in
> pose (BACKLOG).
> **Entry points:** `MOTraversal.h` (the pure decisions), `UMOCharacterMovementComponent` (custom modes `CMOVE_Traverse`, `CMOVE_Climb`;
> `DoJump`, `FindTraversal`, `FindClimbableWall`, `PhysTraverse`, `PhysClimb`), `AMOCharacter` (`ClimbingMET` / `ClimbingExertion` /
> `ClimbingStaminaCostPerSecond`). **Dev verbs:** `MO.Test.SpawnLedge <HeightCm> [DepthCm] [Climbable] [DistanceCm]`, `MO.Test.ClearLedges`,
> `MO.Test.Traversal` (what a jump would do here, and why not). **Tests:** `MOFramework.Traversal.*` (ClassifyLedges, PathShape, Climbable).

## 1. Jump is the one verb

| In front of the body | Jump does | Time |
|---|---|---|
| under 50 cm | a step (the movement component walks up 45 cm) or a plain jump | |
| 50-120 cm, thin, ground just behind (a fence, a log: at most 70 cm deep) | **vault** over it to the ground behind | 0.55 s |
| 50-120 cm with room on top | **hop up** onto it | 0.6-0.8 s |
| 120-220 cm (arm's reach with a jump) with room on top | **pull up** onto it | 1.1-1.6 s |
| higher, and climbable, while pushing at it | **grab** it and climb | |
| while climbing | **pull up** if the edge is in reach, else **push off** the face | |

Crouch while climbing lets go -- and is used up by it (crouch is a toggle here: a climber who let go lands standing and can grab again).
A grab from the ground needs the input pushing at the face (a jump beside a tree trunk stays a jump); in the air it does not (catch a face
mid-fall). Heights are a 1.75 m adult's.

## 2. What is climbable (`FMOTraversal::ClassifyClimbSurface`)

- Rock: a component tagged `MOLandmark` (the crags, cliffs, xlarge boulders of the landmark tier), `MOClimbable` (component or actor tag:
  anything a designer marks), a mesh whose path says Rock / Cliff / Boulder / Crag / Stone / Shelf, or the voxel terrain where its face is
  steeper than 55 degrees (a cliff; gentler ground is walked).
- A tree: a component tagged `MOTreeTrunk` (the PCG tree tier) or a mesh whose path says Tree / Trunk.
- Never a built thing (`AMOBuildableActor`): its walls are smooth.
- Speed (hands and feet, a hold at a time): rock 30 cm/s, a trunk 22 cm/s; `ClimbSpeedScale` on the movement component (the character can
  lower it when tired). W climbs up the face, S down (reaching the ground stands), A / D across it; pushing up at the top pulls over the edge.

## 3. What it costs

Climbing is 8 METs (rock scrambling and tree climbing run 7-9), exertion 75, and 3 stamina a second moving or hanging (a jog is 2, a sprint
8: a full bar lasts about half a minute). Out of stamina the hands give out and the climber falls -- decided on the server.

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
- `FindTraversal` probes: rays at knee to reach height for a face (reach = capsule radius + 60 cm; the MetaHuman's radius is 28.9 cm);
  a sphere dropped onto the top just past the face (none below arm's reach = "taller than arm's reach"); a capsule swept straight up for
  room to rise (from 5 cm up: the body's floor contact is not a ceiling); a capsule overlap for room on top, and for a vault the ground just
  behind. `MO.Test.Traversal` prints the answer and, when there is none, why.

## 5. Verified (Oct 26, one game instance, real input actions through `MO.Test.InjectInput`)

Hop up onto a 90 cm block (standing on it after); vault a 90 x 30 cm fence (1.58 m on, same ground height); pull up onto a 170 cm ledge; a
4 m plain block: no move (control); a 4 m climbable block: grabbed while pushing at it, 120 cm climbed in 4 s, crouch let go and the body
landed standing; a 3.2 m climbable block with W held: climbed and pulled over the top (standing on it).

**A joiner (Oct 26, `python Tools/ue.py nettest traversal`: host + joiner processes, real input actions on the joiner):** hops up onto a
90 cm block and is on it on both machines (0.0 cm apart); jumping at a 4 m climbable face while pushing at it grabs it on both; W climbs
120 cm in 4 s on both (0.0 cm apart); crouch lets go on both. The blocks are `AMOTestLedge`s (replicated with their size and climbable
flag: an `AStaticMeshActor`'s scale and tags do not replicate, so a joiner saw a 1 m plain cube where the host had a 4 m climbable face);
`MO.Test.SpawnLedge ... [PlayerIndex]` on the host puts one in front of the joiner's pawn. The host logs why it refused a joiner's jump
(`[MOTraversal] server: ... has no traversal: <why>`). Not yet checked: a real cliff and tree in the world (the classification is
unit-tested), how it looks.
