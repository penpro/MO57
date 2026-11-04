# Held items: how a hand holds what it holds (the standard)

> Nov 2 2026, Wes: "take what you learned here and make sure it works on the other metahumans and make sure we have a standard for how
> they grip other items etc". This replaces hand-tuned held transforms (N-207: `RightHandTransform` nudged per item, which Wes had kept for
> himself on Oct 24; BR31-12: the hammerstone floating beside Trey's hand). Code: `FMOHandGrip` (`MOHandGrip.h/.cpp`), `AMOCharacter::SeatHandGrip`,
> `UMOBodyRetargetAnimInstance` (the fingers, the second hand, the short draw), `AMOCharacter::AimHandGrip` (the item between two hands).
> Python mirror (the seat and the fingers) and item measuring: `Content/Python/held_grip.py`.

## 1. The rule

**An item says where its handle is and what kind of grip it takes; each body's own hand does the rest.** No per-body numbers and no
hand-tuned transforms: the five bases' hands differ a lot (knuckle span 5.6 cm on Femme, 7.3 on Harvey, palm 7.9 to 9.0 cm), and a seat
measured on one hand puts the handle into or off another. Each body works out the seat from its own bones when the item is equipped and
again whenever the body changes (another base), and its fingers close on the handle in every animation. A new base and a new item both
work with no extra numbers.

The item's part, `FMOItemHeldVisual`:

| Field | Meaning |
|---|---|
| `GripStyle` | `Power` (a handle across the palm, the fingers round it, the thumb over) or `Sphere` (something round in the palm). `None`: the old way, `RightHandTransform` / `LeftHandTransform` and the animation's fingers |
| `GripTransform` | The handle in the held mesh's space, cm as held (at the item's world scale): its origin is the middle of the part the hand closes on, +Z runs along it toward the item's head (the fist's index and thumb side), and +X is the way the item faces out of the front of the fist (an axe's edge, a hammer's face, a sword's edge, a bow's back). A `Sphere`'s axes: Z its longest, Y its thinnest (against the palm) |
| `GripHandOffset` | Where the centre of the hand sits on the handle, measured from that origin along +Z. 0 for most items; a bow's is +1.61 (the hand high on the grip, below the arrow pass, its thumb past the grip's top) |
| `GripHalfSize` | The handle's size in `GripTransform`'s axes, half each way: a `Power` handle's elliptic cylinder (X half-width, Y half-depth, Z half-length), a `Sphere`'s ellipsoid |
| `GripPose`, `GripPoseTime` | The fist: `/MOFramework/Characters/Bow/RMB_Loop_MH` at 0.33 (Sparrow's bow hand at full draw, a power grip) for every item. Its LEFT hand's local rotations serve either hand, because the MetaHuman's hands are mirrored |

The held mesh shows at the item's world scale (`WorldVisual.RelativeTransform`): a held item is the same size as on the ground.

## 2. The seat (`FMOHandGrip::Seat`; `AMOCharacter::SeatHandGrip` on equip and when the body changes)

1. **The hand in the fist.** The fist's local rotations go on this body's own bone lengths, giving the hand-bone-space positions of each
   finger's knuckle (01), last joint (03) and metacarpal, and the thumb's metacarpal.
2. **The knuckles' frame.** e1 runs from the little finger to the index, e2 points toward the curled fingertips (square to e1), and e3 is
   the same side in either hand (e1 x e2 on the left, e2 x e1 on the right). Span is the knuckle span.
3. **Power.** The handle's centre, axis and face are fixed in that frame, the centre per cm of span. They were measured from Sparrow's bow
   in her fist as retargeted onto Femme (`held_grip.derive_standard`, Nov 2):

   | Constant | Value |
   |---|---|
   | Centre | (0, 0.5619, 0.2897) |
   | Axis | (0.9998, 0.0037, -0.019): along the knuckles |
   | Face | (-0.0191, 0.374, -0.9272) |

   The handle then slides square to its axis, toward the palm, until it touches it. The palm is a capsule along each metacarpal to its
   knuckle, radius 1.2 cm times the hand's scale, plus the ball of the thumb: the thumb's metacarpal with its 1.0 cm capsule. A big stone or a
   thick handle rests on the ball of the thumb, not in it.
4. **Sphere.** The object moves out of the palm's hollow, from the middle of the metacarpals, until it touches the palm.
5. **The mesh.** `GripTransform⁻¹ × seat`, at the item's world scale. A hand's scale is its knuckle span over Femme's 5.64 cm (Jalena 1.03,
   Zuri 1.11, Trey 1.17, Harvey 1.29). The fingers' and palm's thickness scale with it.

## 3. The fingers (`FMOHandGrip::Wrap`; `UMOBodyRetargetAnimInstance`, every evaluation, after the slot and the climbing IK)

Each finger closes the way a tendon closes it. All three joints bend together from the open hand (the body's reference pose) toward full
flexion. When a phalanx touches the handle, it and the joints before it stop, and the rest go on. The animation's own fingers are replaced,
so the hand holds the item in every clip, a walk's loose hand too. The grip blends in or out over 0.15 s.

- **Which way a joint bends:** the fist's turn from open, about the same axis.
- **How far it can bend:** a human hand's range, or further if the fist itself goes further.

  | Joint | Full flexion |
  |---|---|
  | Finger knuckle | 90° |
  | Finger middle joint | 110° |
  | Finger end joint | 80° |
  | Thumb base | 50° |
  | Thumb knuckle | 60° |
  | Thumb end joint | 80° |

  Sparrow's fist bends the fingers only 22-73°. A hand bigger than the one the fist was made for therefore closes on past it, onto a thin
  handle, instead of stopping in the air. Before Nov 2, Harvey's fingers ended 0.5-1.5 cm off a bow's grip.
- **An open finger already in the handle's way:**
  - **A thumb** goes over the handle, along its closing, and closes from the far side.
  - **A thumb on a handle too thick to go over** swings out at its base, the root joint's abduction, until the whole thumb is clear, up to
    60°. It then closes onto the handle from outside. Without this, the thumb lay 0.9-2.8 cm inside the club, maul, sickle and wooden spear.
  - **A finger** straightens off the handle, by up to a quarter of its range. Its base then lies on the handle, and the joints past the
    phalanx touching it close round it. That is a hand on a thick handle; before, a straightened finger stopped there and pointed down the
    sickle.
  - **If nothing clears,** the finger rests wherever along its range it is least in the handle, and the joints past its deepest phalanx
    still close.
- **Capsules:** the radii are `FMOHandGrip::FingerRadii` times the hand's scale. The fingertip lies 0.85 of the middle phalanx past the
  last joint.

### Two hands (`FMOHandGrip::Support`; Nov 2, N-261)

The hand that carries nothing holds a long handle where the animation puts it: a spear's front hand, a maul or hoe swung two-handed. A
rigid shaft in two hands lies on the line through both grips, so the item turns to that line. Neither hand moves.

1. **The other hand's seat.** On equip and when the body changes, `AMOCharacter::SeatHandGrip` also seats the handle in the other hand
   (`SupportInHand`), the same way (§2). It also works out how far the item runs along the handle's axis, from its mesh's bounds as held
   (`Run`). A bow has neither: its other hand draws the string.
2. **The aim.** Every evaluation the handle turns about the carrying hand's grip until it runs through the other hand's seat. The turned
   shaft would cross a cupped palm at one edge, so the line then slides square to itself out of each palm until it only touches. That is
   the seat's own rule: never into a palm, at most 3 cm.
3. **The fingers.** The other hand closes on the turned handle, with the item's fist (§3). The carrying hand closes on it as turned.
4. **The item.** The anim instance hands the turned handle to `AMOCharacter::AimHandGrip` in `NativePostEvaluateAnimation`, before the
   attachments update, so the mesh shows in the same frame the fingers closed on.

**When the other hand holds it** (each gate ramps from full to none):

| Gate | Full | None |
|---|---|---|
| The line between the two seats, off the handle's axis | 20° | 30° |
| The other hand's own axis, off that line (either way round) | 25° | 45° |
| The two seats apart | 10 cm | 6 cm |
| The other hand's centre in from the item's end | 4 cm | 0 |
| The other hand's seat off the turned axis (its fingers) | 2 cm | 5 cm |

**Why the item turns.** Closing the fingers alone was the first plan. Measured Nov 2 (Femme and Harvey, FlintSpear01) before the change,
the clips put the front hand too far off for that:

- **Two-hand idle, walk, run and crouch:** the front hand is 8-18 cm off the shaft as the rear hand's seat aims it, 54-75 cm up it. The
  seat aims the shaft 8.5-14° off the line through the clip's two hands, the line the source's spear lay on. The front hand's own axis
  lies 5-16° off that line.
- **One-hand idle and walk:** the free hand is 39-46° off, turned 54-87°, toward the butt. It never grips.

**Where it holds (Nov 3).** The stances hold (§6): idle, walk, run, crouch, block and the two hit reactions. The six attacks do not: the
three thrusts, the jab and the two swings. The cause is how the Mega Spear pack is built, not the gates.

- **The spear is its own track.** The pack's spear (`SK_Spear`) rides the character's `ik_hand_gun` bone, not either hand. Its own clips
  (`A_SpearIK_*`) move only a `HandL` / `HandR` locator along it, never the spear. The pack's `ABP_SpearAnimations_IK` pulls the hands onto
  the locators (two FABRIK nodes, positions only). In the thrust a `DtachLeft` notify (0.14 s) lets the front hand go and `AttachLeft`
  (0.77 s) takes it back.
- **The spear pivots in the rear hand and slides through the front one** (measured Nov 3 on the pack's own clips).
  - **The rear hand** stays at one place on the shaft while the shaft turns in it: 24° in the jab, 43° in the horizontal swing, 34° in the
    vertical swing, 66° in the thrust.
  - **The front hand** keeps its turn on the shaft (0-4° in the jab and horizontal swing, 11-13° in the vertical) and slides along it (30 cm
    in the jab, 34 in the swing). In the middle of the thrust it is off the spear altogether.
  - **Changing to one hand,** the rear hand slides 89 cm up the shaft.
  - **The pack's IK barely works.** Wherever a hand holds the spear, the character clip already has it within 2 cm of its locator.
- **Our clips lose that track.** The retarget carries the arms but not `ik_hand_gun`: it stays at the origin in every `*_MH` clip.
- **So no shaft in the rear hand fits.** Measured on Femme in our clips, the line from the rear hand's seat to the front hand's runs
  22-54° off the shaft where the jab and the horizontal swing let go (the hands 22-45 cm apart, 55 at rest), up to 31° in the vertical
  swing, and 56-88° through most of the thrust (the front hand's seat 94-115 cm from the rear one, toward the butt). A shaft turned that
  far in the rear fist is not a grip.
- **The fix is the pack's own design** (N-269): the spear's track, next. Widening the gates would not do it: the one-hand clips' free
  hand starts at 39°.

### The spear's track (`FMOHandGrip::Track`; Nov 4, N-269)

In a spear clip the spear goes where the pack's spear goes, and the hands hold it where the pack's hands hold it. The arms stay ours.

1. **The track, baked into each clip** (`Content/Python/spear_track.py`, run by `retarget_weapon_sets.py` after its pelvis repair; local
   only, like the clips). From the pack's source clip, frame by frame:
   - `MO_SpearDirX / Y / Z`: the spear toward its head (`ik_hand_gun`, turned as `BP_SpearFighter` attaches it: -90° in roll), in
     component space.
   - `MO_SpearHoldL / R`: whether that hand holds it (1 with the hand bone within 15 cm of the shaft's axis, 0 past 21 cm or off the spear).
     The pack's holding hands sit 4-14 cm off it; a free one is 22 cm or more.
   - `MO_SpearSlideL / R`: how far that hand is up the shaft from where it holds it in `A_2H_Idle_01` (left 12.0, right -55.3 cm from the
     spear's middle).
   - `MO_SpearTrack`: 1 throughout. The other curves blend with the clip; this says by how much.

   Carrying `ik_hand_gun` through the retarget instead would scale the spear off our hands, and the spear clips' locators are not needed:
   the character clips already put the hands on the spear. Our hands match the pack's closely: the line between them within 1-12° of the
   pack's (mostly 1-5°), each hand 3-17 cm from the pack's scaled position (the most in the thrust's shallower lunge).
2. **The carrying hand's wrist** (`WristTurn`). A fist takes a shaft up to 30° off its seat. Past that the shaft runs through the fingers:
   the thrusts put the index 1.5-2.5 cm into it. So the wrist turns toward the clip's spear by the rest, at most 40°.
3. **The spear** (`Track`). The handle turns about the carrying hand's grip onto the clip's direction, slides out of that palm (at most
   3 cm, as two hands' does) and slides under the hand by `MO_SpearSlide`. The hand closes on the shaft where it now is.
4. **The free hand** (`HoldOn`), wherever the clip's hand holds the spear:
   - Its seat goes to the nearest point of the shaft's axis on the item, turned onto the axis the nearer way round. Two-bone IK moves the
     arm there, the elbow bending the way the clip has it.
   - Its palm then lies on the shaft, never in it: the hand moves off square to the axis (at most 3 cm). The clip's flat palm took the seat
     0.5-0.7 cm into it.
   - Its fingers close on the shaft round it.
   - **Gates:** full within 12 cm of the axis, none past 25 (an arm is never yanked across). Full within 45° of turn, none past 70 (a wrist
     is never wrung round). None in the carrying hand's place.
   - Where the clip's hand lets go (the middle of the thrust, the changes of grip), the free hand is the animation's.
5. **Blending.** The curves come in and out with the clip, so the track takes over from `Support` (above) as a spear clip blends in.
   Outside the spear set (no curves) `Support` alone holds the other hand, as before.

## 4. A bow

- **The grip.** The Blender bows (`Docs/Blender_Items.md`) have the grip's centre at the root, the handle along +Z and the draw along +Y.
  - `GripTransform` turns +X onto the bow's back (-Y).
  - `GripHalfSize` is the build's `grip_cm` with X and Y swapped, because the grip's X runs along the draw.
  - `GripHandOffset` is 1.61 for all three bows. That sets the hand along the grip so that at Sparrow's full draw the nocking point is level
    with the drawing fingers. There the string has 63.6-63.7 cm to go of the bows' 64.1, and lies 0.65-0.75 cm to the side.
  - `held_grip.bow_grip` works these out, and `Tools/blender_items.py` writes them.
- **The draw follows the arm.** Draw length grows with the arm: Sparrow's full draw needs 63.9 cm on Femme, 70.6 on Jalena, 76.0 on Zuri,
  79.3 on Trey and 85.4 on Harvey. The bows were built for Femme's 64.1 (`FULL_DRAW`).
- **The short draw.** A drawn bow (nocked) stops the drawing hand at the bow's full draw: `FMOBodyRetargetAnimInstanceProxy::LimitDraw`,
  two-bone IK along the draw, the hand's turn kept. This is what an archer does with a bow too short for his draw. The string stays in his
  fingers, and the bow is never drawn past what it was built for.
- **The nock.** Sparrow's draw brings the drawing fingers 4.6 cm from the string on Femme, 10.2 on Trey and 16.0 on Harvey (longer
  forearms). The string now takes them within 20 cm (`UMOBowMeshComponent::NockReachCm`, was 9, which never nocked the men's bows). On a
  bigger body the string goes over to the fingers in the 0.15 s nock blend.
- **Open decision.** Whether a bow should instead be built to its archer's draw (a longer bow for Harvey) is F57 (`Docs/DECISIONS.md`).

## 5. The items (Nov 2: 35 rows, `Docs/RowPatches/2026-11-02-held-grip-DT_Items.json`)

`held_grip.measure_all` measures each item off its mesh at its world scale:

1. **The long axis.** A principal-component axis.
2. **Cross-sections.** Taken every 0.5 cm along that axis.
3. **Head and butt.** The thicker end is the head. For a spear, the end that tapers to a point. For a sword, the blade is the long flat part.
4. **Where the hand goes.** Depends on the item's mode (table below).
5. **The handle's own line.** Fitted through those sections, then cut again square to it, so a curved handle (the sickle) or a cord beside the
   handle (the bow drill) does not widen it. Anything more than 4 cm off the line is left out.
6. **The face.** The head's main direction across the handle, toward where its mass is.

| Mode | Where the hand goes | Items |
|---|---|---|
| end | at the butt: the little finger 1 cm in, the hand's centre 5.25 cm in (an 8.5 cm hand); the hand fills a handle shorter than that | the axes, adze, hoe, sickle, smithing hammer, maul, club, torch, atlatl, drop spindle, bow drill |
| sword | on the grip, the narrow round part between the pommel and the guard; on a long grip, just under the guard | the three swords |
| shaft | a third of the way up from the butt (a spear's rear hand) | the six spears |
| middle | at the middle | the fire drill |
| tongs | both handles in one fist, squeezed together across the palm | the smithing tongs |
| shield | across the shield, 2.5 cm off the back of the rim at the middle; the face out of the fist | the three shields |
| sphere | in the palm, thinnest axis against it | hammerstone, chopper, scraper, apple, cooked meat |

**A new held item:**

1. Add its row name and mode to `held_grip.ITEMS`.
2. In the editor, run
   `python Tools/ue.py py --timeout 1800 -c "import held_grip as g; out(g.measure_all(only=['NewItem01']))"`.
3. Run `MSYS_NO_PATHCONV=1 python Tools/ue.py rows set /MOFramework/Data/DT_Items.DT_Items --file Saved/HeldGrip/rows.json`.
4. Run `python Tools/check_grip.py --items NewItem01`.

For a bow: `held_grip.bow_rows()`, or `Tools/blender_items.py`, which writes it.

Measured sizes worth knowing:

- **Wooden spear and fire-hardened spear:** they shared `/Game/Kitbash/WoodenSpear/SM_WoodenSpear` at world scale 1.5, a kitbashed
  Megascans trench stick 7-8 cm thick as held. That is a log, not a spear, and no hand closed on it (the two failures in §6). Rebuilt Nov 2
  in Blender at real size (`Docs/Blender_Items.md`): 200 cm long, a 3.3-3.5 cm handle.
- **Stone sickle:** its handle is 5.7 cm thick.
- **Club:** its grip is 7 cm thick.

Those are the meshes' real sizes: the hand opens round them.

**The hammerstone** was 27 cm across at world scale 1 (its held row scaled it to 0.4). Its world scale is 0.4 now: an 11 x 11 x 7 cm stone,
on the ground and in the hand.

## 6. Checking

- **`MOFramework.Animation.HandGrip.*`**
  - `Clearance`: both shapes.
  - `Wrap`: touching, past the fist's own bend, the thumb going over, a finger straightening, least-in.
  - `EveryBase`: every base in `UMOSurvivorBaseSettings`, both hands, a bow's grip, an axe's haft and a stone. The four fingers on it, the
    thumb not in it, the two hands alike. It needs the local Sparrow clip; without it the test warns and skips.
  - `Support`: the other hand on a shaft. The shaft turns through its seat either way round, toward the head or the butt, half way at half
    the blend. It stays as it was where no hand holds it: a one-hand clip's angles, past the head, beside the carrying hand, no run, a stone.
  - `TwoHands`: every base, either hand as the other hand, its seat 55 cm up a shaft held 12° off the line, its own axis 0° or 12° off.
    The four fingers on the shaft, the thumb not in it, the palm on it and not in it. Before the palm slide, the 12° case put the palm
    0.56-0.78 cm into the shaft on every base and the index finger 0.04-0.06.
  - `Track` (N-269): the spear's track. The carrying hand's shaft turns onto the clip's spear about its grip, slides 89 cm under the hand,
    is cut at the head, and goes half way at half the weight; it stays seated with no weight, no direction, no run or a stone. The free
    hand's seat lands on the shaft, turned only as far as it was off (either way round), half held at 18.5 cm off. It is not held 26 cm off,
    75° turned, in the carrying hand's place or past the head. A palm 1.0 cm into the shaft moves off until it touches; out of reach the
    hand slides up the shaft to where the arm reaches; with no point in reach it lets go. The wrist takes a 24° pivot whole, 45° of the
    thrust's 66, half at half the weight.
- **`held_grip.hold_all()`:** the same in Python, offline in the editor.
- **`python Tools/check_grip.py [--bases ..] [--items ..] [--shots all|first|none]`:** the game itself. Each base holds each item, idle.
  - Measured: each finger's and the palm's nearest approach to the handle as the game holds it (`AMOCharacter::GetHeldGripShape`).
  - Fails: a finger more than 0.3 cm into the handle, the palm more than 0.5 (its soft side gives, and an animation's palm is not quite the
    fist's), or one of the four fingers more than 0.5 off it.
  - Close-ups: `Saved/Screenshots/grip/<base>_<item>_{out,front}.png`.
  - **Two hands:** `--clip A_2H_Idle_01_MH --support [--samples 8]`. It plays a clip on the body (a name under
    `/MOFramework/Characters/Spear/` or a full path), measures through it, and measures the other hand on the same shaft too. It fails
    unless the game holds it fully (`AMOCharacter::GetHeldGripSupport` 1) with the same finger and palm limits. Close-ups of that hand:
    `<base>_<item>_<clip>_support_{out,front}.png`. `--name` runs a second instance beside another session's.
  - **The spear's track** (any spear clip, N-269): the spear must lie within 1° of the clip's spear wherever it follows it fully
    (`AMOCharacter::GetHeldGripTrack`); the carrying hand is measured on the whole shaft (it may slide up it); the other hand must hold it in
    the samples where the clip's hand holds it and let go where the clip's lets go. `--verbose` prints every sample, with the arm's
    straightness (1: straight) and the shaft's turn in the fist off the knuckle line and out of the palm's plane. `--freeze 0.35` takes
    the close-ups with the clip held still at 35 % of its length (its play rate 0, not a pause): a moving attack carries the hand out of
    the camera's view between aiming and the shot. Those are named `..._at35_...`.
- **`python Tools/check_bow.py --item CompositeBow01 --base Harvey`:** the bow drawn on any base; the string stays on the fingers.

**Measured Nov 2** (`check_grip.py --shots all`, seed 777, the idle hand). 163 of 165 base x item pairs hold:

- Every finger is on the handle, 0.00 cm.
- Palms are within 0.5 cm.
- Thumbs are on the handle, or off it on thin handles where the thumb closes over the fingers.
- The two failures were the wooden spear mesh above, the index finger 2.7 cm into the log on Femme and Jalena. Rebuilt at real size, both
  spears pass on all five bases (10 / 10): the four fingers at 0.00 cm, palms within 0.02 cm, thumbs 0.00-0.41 cm.

Close-ups: `Saved/Screenshots/grip/`.

**Two hands, measured Nov 2** (`check_grip.py --bases Femme,Harvey --clip A_2H_Idle_01_MH | A_2H_walk_01_MH --support`, the six spears;
6 samples through the idle, 8 through the walk): GRIP PASS 12 / 12 in each.

- **The front hand:** held fully in every sample (support 1.00). Its four fingers are at 0.00 cm on the shaft and its palm is on it
  (0.00). It holds the shaft 55-57 cm up from the rear hand on Femme and 74-76 cm on Harvey.
- **The thumbs:** 0.00-0.32 cm off on Femme, 0.47-0.95 on Harvey (it closes over the fingers on a thin shaft).
- **The rear hand,** holding the turned shaft: its fingers at 0.00 cm and its palm 0.04-0.07 cm clear.
- **The one-hand idle and walk** (FlintSpear01, WoodenSpear01): the free hand is left alone (support 0.00, 24-64 cm off the shaft), and
  the carrying hand still holds it.

Close-ups of the front hand on Femme: `Saved/Screenshots/grip/Femme_<spear>_support_{out,front}.png`.

**Every other two-hand clip, Nov 3** (the same check, Femme and Harvey, FlintSpear01 and WoodenSpear01, 12 samples each):

| Clip | Result |
|---|---|
| `A_2H_run_01_MH`, `A_2H_Crouch_01_MH`, `A_2H_Block_01_MH`, `A_2H_HitHigh_01_MH`, `A_2H_HitLow_01_MH` | GRIP PASS 4 / 4 each: support 1.00 in every sample, the front fingers at 0.00 cm (the block's front hand 59 cm up the shaft on Femme, 81 on Harvey) |
| `A_2H_01_Thrust_Mid / High / Low_MH` | FAIL 4 / 4 each: the front hand holds only near the start and end; for most of the clip its fingers are 13 cm to over a metre off the shaft |
| `A_2H_01_Jab_Mid_MH`, `A_2H_01_Swing_HorizontalMid_MH`, `A_2H_01_Swing_VerticalDown_MH` | FAIL 4 / 4 each: it holds at the start and end and lets go mid-attack, the four fingers at worst 2.6-7.3 cm off in the jab and 5-25 cm in the swings |

Why the attacks failed: §3, two hands, "Where it holds". The fix: "The spear's track".

**The spear's track, Nov 4** (the same check on all 19 spear clips: the two-hand idle, walk, run, crouch, block, hits, the three thrusts,
the jab, both swings, the one-hand idle, walk, jab and thrust, both changes of grip; Femme and Harvey, FlintSpear01 and WoodenSpear01,
12 samples each): **GRIP PASS 76 / 76.**

- **The spear** lies on the pack's spear in every sample (0.00°).
- **The carrying hand's** four fingers are on the shaft (0.00 cm) and its palm on it or clear (0.00-0.25 cm).
- **The front hand** holds wherever the pack's does: every sample of the stances, the jab and the swings. Its four fingers are on the
  shaft and its palm on it (0.00). It slides along the shaft with the pack's (28-57 cm up from the rear hand in Femme's jab). It lets go
  where the pack's does: the middle of each thrust, the changes of grip, every one-hand clip.
- **The arm** never quite straightens (0.44-0.995).
- **On the way** (the same clips, Nov 4):
  - **Pivot in the fist.** With the pivot left in the fist, each thrust put the carrying index 1.5-2.5 cm into the shaft. Left at 30° in
    the fist, the one-hand jab's index flipped between 2.2 cm in and clear. With the wrist taking it, both are clear.
  - **Palm.** Without the palm slide the front palm went 0.5-0.7 cm in (the walk, the jab, the vertical swing).
  - **Reach.** Without the reach every sample with the palm still in had the arm straight (1.000), the hand short of the pack's shaft.

Close-ups of Femme's hands, each attack held still (`--freeze`): `Saved/Screenshots/grip/Femme_FlintSpear01_<clip>_at<NN>_{out,front}.png`
and `..._support_{out,front}.png` (the by-eye list in `Docs/BACKLOG.md` names them).

**The bow on three bodies** (`check_bow.py --item CompositeBow01 --base Femme / Trey / Harvey`): each passes.

- 36 samples nocked, with the string 0.0 cm from the fingers through the draw and the hold.
- After the loose, the string settles at brace (11.5 cm).
- The bow hand is at most 0.14 cm into the grip, and every finger is on it.
- Harvey's drawing hand stops at the bow's 64.1 cm full draw. Before the short draw it reached 85.6 cm, and before the 20 cm nock reach his
  string never caught (0 samples nocked).

## 7. Not done

- **The spear's track** (N-269, done Nov 4: §3, "The spear's track"). Left over:
  - **Coverage.** Measured on Femme and Harvey with two of the six spears; Jalena, Zuri and Trey and the other spears are not.
  - **The clips' own animation.** Only `check_grip.py` plays the spear clips: the attacks are not in the game yet (F53, F54).
  - **The throw.** The track lets go of the spear with the clip's hand at the release (`A_1H_Throw_MH`, `MO_SpearHoldR` 0), so the item
    goes back to the seat until throwing takes it (F53).
  - **The look.** Wes's by-eye check (`Docs/BACKLOG.md`): the wrist angles the hand up to 45° with the spear, and the front hand slides.
  - **No Python mirror.** `held_grip.py` has no copy of `Track` / `HoldOn`.
- **Other two-hand items.** No clip swings a maul, hoe or axe two-handed yet, so hands side by side on a haft are covered by the tests
  only.
  - **The 1H to 2H changes:** measured with the track (above); the spear set's own changes pass.
  - **The Python mirror:** `held_grip.py` has no copy of `Support`.
- **The drawing hand to the string.** Sparrow's draw keeps the hand 7-11 cm off the bow's plane and, on a bigger body, further back at
  the start; the string goes to the fingers. An IK bringing the fingers onto the string's line from the nock on would make every body draw
  in the bow's plane, as an archer does.
- **A precision grip.** Beads, a needle, a flint flake between finger and thumb. No held item needs one yet.
- **The look is Wes's.** Whether each item sits as it should (the shields' handle above all, and the club, sickle and wooden spear's thick
  handles) is on the by-eye list in `Docs/BACKLOG.md`.
