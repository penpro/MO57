# Items built in Blender (Nov 2 2026)

Wes: "I saw a video about Claude being able to use blender really well now. Take a few items that need done and see if you can create some
things, especially things like the crude bow that also needs animation."

Seven items that were on gray placeholders now come out of one Blender script, built from measurements rather than scans or images:
the three bows (rigged, with their draw), two arrows and two kits. Everything is procedural and re-runnable: change a number, run it again.

| Item | What it is | Size, triangles |
|---|---|---|
| SimpleBow01 | a crude self bow: one scraped stave, bark left in patches on the back, D section, stiff tips, nettle-fibre string, leather grip strip | 158 cm along the stave (153 braced), 10.2k |
| SinewBackedBow01 | wide flat limbs with a cream sinew layer on the back, a little reflex in the tips, sinew string, rawhide grip | 142 cm (137 braced), 8.9k |
| CompositeBow01 | horn belly, wood core, sinew back under birch bark; rigid siyahs set 34 deg forward (a static recurve); cord-wrapped grip | 128 cm (121 braced), 23.5k |
| FletchedArrow01 | a shoot shaft (nodes, heat-straightening scorch), sawn self nock, three barred feathers on a 5 deg helix bound with sinew, a side-notched flint point lashed into a split and sealed with pitch | 76 cm + 3.4 cm head, 6.6k |
| IronArrow01 | a planed shaft with red-ochre / black cresting, grey goose fletching, a socketed leaf-bladed iron head (rust in patches) | 78 cm + 5.6 cm head, 6.7k |
| BowDrill01 | the kit as it lies: the bow (a bent branch, bark on) with its cord turned once round the spindle, a fireboard with three burnt sockets and their notches, the bearing block | 62 x 29 cm, 3.8k |
| Snare01 | an anchor stake (sharpened, bark on) with the cord tied round its head, run out to an 11 cm noose and its slip knot | 36 x 19 cm, 1.8k |

## 1. Running it

```bash
python Tools/blender_items.py SimpleBow01 SinewBackedBow01 CompositeBow01 --rows    # Blender, import, prepare, fit, rows
python Tools/blender_items.py FletchedArrow01 --blender-only                       # Blender only: look at Saved/BlenderItems/<Id>/<Id>_preview.png
python Tools/check_bow.py --item SimpleBow01                                       # the bow drawn and loosed in a -game instance
```

- `Tools/blender_items_blender.py` (Blender 5.2, headless) builds one item into `Saved/BlenderItems/<Id>/`: the FBX (a bow: the skeletal mesh
  and its draw; plus `<Id>_static.fbx`, the braced bow), three baked textures (`T_<Id>_BaseColor`, `_Normal` -- DirectX, green down --, `_ORM`:
  occlusion, roughness, metallic, the glTF order `M_MS_Base` reads), `<Id>.json` (sizes, triangles, a bow's draw) and `<Id>_preview.png` (Eevee
  renders on the baked textures: a bow braced from three sides, then its draw from the side, then close-ups). About 3 minutes for a bow
  (2048 px bake on the CPU), 1 for the rest (1024 px). `--no-bake` gives the shape and the preview in seconds.
- `Content/Python/import_blender_items.py` (editor) imports it into `/Game/BlenderItems/<Id>/`: the textures, `MI_<Id>` on
  `/Game/Materials/FabOverrides/M_MS_Base` (its material AO on), `SM_<Id>` (the world item and the icon) and for a bow `SK_<Id>`, its
  skeleton and `SK_<Id>_Anim` (the draw). The legacy FBX importer, as kitbash.py: it makes exactly these assets and no materials from the file.
  It gave `M_MS_Base` "used with skeletal meshes" (a held bow is skinned).
- `Content/Python/bow_hand_fit.py` seats a bow in her left hand (below); `Tools/blender_items.py --rows` writes the row patch
  (`Saved/BlenderItems/rows.json`: the world mesh; a bow's `HeldVisual.SkeletalMesh`, `DrawAnimation` and hand transform) and applies it with
  `ue.py rows set`. Icons: `Tools/icon_studio.py --items ...` as for any item.

How each part is made: everything is a `Part` (vertices in cm, faces, a natural "proc" UV per island and per-vertex attributes); lofts
(`loft`, `tube`: the stave, shafts, strings, wraps, cords), blades (`lens`: the flint point, the iron blade) and boxes (`rounded_box`). One
object per item, one material slot: the islands are shelf-packed into one atlas at one texel density (42 texels / cm), each part keeps its own
procedural material for the bake (wood grain that follows the bent stave from the `along` / `depth` / `side` attributes, twisted cord from the
proc UV, barred feathers, knapped flint scars, rusted iron), and Cycles bakes colour, normal, roughness, occlusion and metallic into the
single texture set. The arrows' nock is a boolean cut.

## 2. The bow: solved, not keyed

The handle is rigid; each limb bends with curvature `rest(s) + lam * m(s)` along its length (`m` is where the limb works: zero in the handle,
rising through the fade, easing off in the tips; a composite bow's siyahs are rigid levers set forward). The string is a fixed length. At brace
the limbs bend until the string stands **16 cm** from the grip centre; for any other draw `D` the solver finds the `lam` that keeps the string's
length from both nocks to the nocking point `(0, -D, 2 cm)`. Measured on the survivor (the retargeted Sparrow clips): her drawing fingers are
16.3 cm from the bow hand's grip at the start of `RMB_Drawback` and **64.6 cm** at full draw (`RMB_Loop`), so the bows draw to exactly that.

| Bow | String | Tips at brace | Tips at full draw |
|---|---|---|---|
| Simple | 147.9 cm | 25.5 deg | 53.9 deg |
| Sinew-backed | 132.0 cm | 21.8 deg | 54.5 deg |
| Composite | 118.0 cm | 6.1 deg | 36.4 deg |

After a release the string overshoots forward of brace and the limbs bend back IN (the string pulls the tips together): the solver finds that
too (the simple bow's tips at 28 deg with the string 7 cm ahead of brace).

**The rig:** `root` at the grip centre; `limb_t_01..08` and `limb_b_01..08` along each working limb (plus `siyah_t` / `siyah_b`); `string_top`,
`string_nock`, `string_bott` under the root. The stave is skinned by arc length (hat weights between bone centres); the string's vertices
blend linearly between its end bones and `string_nock`, so it stays two straight runs whatever the nocking point does. **The draw clip**
(`SK_<Id>_Anim`, 2 s, 120 frames) runs LINEARLY from the string's forward limit (15 % of the travel ahead of brace: the release overshoot) at
frame 0 to full draw at the last frame; the reference pose is brace. In Unreal the nocking point travels from (0, 8.7, 2) to (0, 64.6, 2) cm,
brace at (0, 16, 2): +Y is toward the archer.

## 3. In her hand (`UMOBowMeshComponent`, `UMOBowAnimInstance`)

- **The data:** `FMOItemHeldVisual::SkeletalMesh` (a held mesh that moves: held in place of `StaticMesh`; the world item keeps
  `WorldVisual.StaticMesh`) and `DrawAnimation` (the clip above). `AMOCharacter::LeftHandBow` / `RightHandBow` hold it on `hand_l` / `hand_r`
  (`UpdateHeldItemMesh`; `GetHeldBow()`). Without a draw clip the bow is held braced and a Warning says why.
- **The seat** (`bow_hand_fit.py`): from her full draw, G = the centre of her curled left fingers (the knuckle and middle joints of the four),
  F = the string between `index_02_r` / `middle_02_r`, U = the fist's axis (little finger to index), square to the draw. The bow's root goes at
  F - 64.6 D - 2 U (D = G->F), so at full draw its nocking point is exactly at her fingers (0.000 cm) and its grip is in the fist.
  Relative to `hand_l`: (6.23, -1.28, 0.84) cm, quaternion (-0.5010, 0.7666, -0.2458, 0.3177). The right hand gets the same seat until a
  left-handed draw exists (her clips draw with the right).
- **Nock / Release** (Blueprint-callable): nocked, the string waits for her fingers (within 9 cm), then the limbs bend for the fingers'
  distance along the draw and the nocking point goes to the fingers themselves, blended over 0.15 s. Sparrow's draw brings the hand in 7-10 cm
  off the bow's plane (her own rig moves the string sideways too), so following only along the draw left the string 7-10 cm from the fingers
  mid-draw. Released, the string snaps to brace in 0.03 s, swings past it at 11 Hz and settles over ~0.5 s, the swing as deep as the draw.
- **Built every tick** on each machine from its own copy of the body (cosmetic: the body's pose replicates, the bow follows it). The component
  ticks after the body (attached), picks the time in the clip before its own pose evaluates, and `UMOBowAnimInstance`'s proxy samples the clip
  on a worker and moves `string_nock`.

**Measured** (`Tools/check_bow.py`, seed 777, Nov 2): SimpleBow01 -- the string is taken 0.2 s into the draw (6.2 cm off) and is on her
fingers from 0.4 s (0.0 cm) through the draw to 64.6 cm and the 1 s hold; loosed, it is at 15.4-16.0 cm within 0.1 s and settles at brace
(16.0). CompositeBow01 the same (0.0 cm nocked, 16.0 after), at slomo 0.06. Screenshots: `Saved/Screenshots/bow/`.

**A crash found on the way** (Nov 2): `MO.Test.BowShot` kept the three Sparrow clips in raw pointers inside its timer lambdas; nothing
else held them, and at slomo 0.06 the hold timer fired ~40 s of real time later, after a garbage collection had freed them: the -game
instance died in the hold's `PlaySlotAnimationAsDynamicMontage` (access violation at 0, `RunBowShotTest` lambda_1 on the stack). The clips
are `TStrongObjectPtr`s now; the same run passes. Faster runs (slomo 0.25) had beaten the collector, which is why the first three passed.

## 4. Not done (and where it goes)

- **Firing** is F53 (ranged attacks: the aim, the closing circle, the projectile): it calls `Nock` / `Release` and plays the clips;
  `MO.Test.BowShot` is the test stand-in (Sparrow's draw / hold / loose on the body, nothing flies). F53 also owns: the arrow on the string
  (an arrow mesh at `string_nock`, its point on the bow hand), equipping a bow into the LEFT hand, and the bow on other machines (Nock / Release
  through a replicated cue, as the creatures' `PlayAnimationEverywhere`).
- **The carry:** in her relaxed idle the bow hangs almost level across her body (the full-draw seat in an idle hand). Sparrow's
  `Travel_Mode_Idle_BowDown` is retargeted: a held-bow idle belongs with F53's stances.
- **The assets are local:** `/Game/BlenderItems/`, the `DT_Items` rows (patch: `Docs/RowPatches/2026-11-02-blender-items-DT_Items.json`) and
  `M_MS_Base`'s skeletal usage are binary and wait on the GitHub LFS budget; the scripts rebuild all of it on any clone (`blender_items.py --rows`).

## 5. Traps

- **Iterating an `unreal.Array` of structs hands out copies** (`for m in sk.get_editor_property("materials"): m.set_editor_property(...)`
  changed nothing: the first bow was grey in game, its slot empty). Build new structs and set the whole list; read it back.
- **An `AssetImportTask` with `save = False` saves nothing**: the skeletal mesh was saved by hand, its skeleton and draw clip were not, and the
  game loaded a bow with no skeleton ("has no skeleton", "was not available"). Save every asset in the folder after the import.
- Blender's 1D noise has no `Vector` input (use 3D); a render or image path must be absolute in background mode (a relative one was written
  nowhere).
- Blender's -Y (the draw, toward the archer) arrives as +Y in Unreal: read the axis from the clip (the component does), never assume it.
- **A UObject a timer lambda will use later must be held** (`TStrongObjectPtr`, a UPROPERTY, or loaded again inside the lambda): a raw
  pointer to something only `LoadObject` touched dies at the next garbage collection (the BowShot crash above).
- A boolean's cutter brings its material slots: give the cutter the material its cut faces should have (`material_mode = "TRANSFER"`),
  or an empty slot stops the bake.
