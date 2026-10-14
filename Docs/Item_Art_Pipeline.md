# Item art pipeline: from a Fab scan to a wired item with an icon

Built 2026-10-14 while giving 119 items real meshes and icons (135 of 216 now; the rest are on the gray
`/Game/Penumbra/Graybox/Items/SM_GBI_*` placeholders). Everything runs with the editor open (bridge + MCP) and
**PIE not running** (saves are refused in PIE). The buy list for what free assets cannot cover:
https://claude.ai/artifact/43sVJcEssqp59LzohRgtt3

## 1. Find candidates (Fab JSON API, built-in browser)

```js
fetch('https://www.fab.com/i/listings/search?seller=Quixel%20Megascans&q=antler&count=24').then(r => r.json())
fetch('https://www.fab.com/i/listings/<uid>').then(r => r.json())        // isFree, startingPrice, publishedAt
```

* `seller=Quixel%20Megascans` restricts to Megascans, but the search is fuzzy and pads every page with unrelated scans:
  filter results by title.
* **Price tells you nothing about ownership.** Megascans published by Oct 2024 that Wes claimed during the free period
  show "Saved in My Library" in the Fab panel whatever the listed price ($0, $1.99, $2.99). 2025–26 scans
  ("Historic Medieval …", "Animal Bones …") are not owned. Listings at $0 are free for anyone.
* Thumbnails: `media.fab.com` URLs download fine with `curl -A Mozilla/5.0` (the API itself 403s to curl). Build a
  contact sheet and look before importing.

## 2. Import (editor Fab panel, computer use)

Window > Fab, search, click the card, "Add to Project" (Medium quality). Traps:

* Each import opens a **maximized Content Browser** in front: close it (X, top right) and **check which window is in
  front before typing anything**, because keystrokes go to the browser (Return opens the selected asset).
* Cards in the **right-hand columns often ignore clicks**: narrow the search until the card is in column 1.
* After "back" the cards stop navigating: always run a fresh search.
* A title that wraps to two lines moves "Add to Project" down: zoom on the panel before clicking.
* The panel happily **re-imports an asset that is already in the project** (same folder id). Check the folder name it
  lands in against `git ls-files` and the rows that use it before wiring.

Then always:

```bash
python Tools/ue.py py --file Content/Python/fab_finalize_import.py      # reparent onto /Game/Materials/FabOverrides, save
python Tools/ue.py py -c "import item_mesh_prepare as p; out(p.prepare(['<mesh>', ...]))"   # Nanite, convex hull, project MI
```

`item_mesh_prepare` also handles Fab masters that have no FabOverrides twin (the translucent `M_MS_Base_Trm` on fruit and
gem scans: Nanite cannot draw it, so the slot gets an opaque MI on `M_MS_Base`). Before it adds a hull to a **plant**
mesh, check that the mesh is not scattered in the world (asset registry referencers): a hull on world grass blocks the
player.

## 3. Wire the row

```bash
python Tools/ue.py rows set /MOFramework/Data/DT_Items.DT_Items --file patch.json
# {"Sand01": {"worldVisual": {"StaticMesh": "<path.Name>", "MaterialOverride": "None",
#             "RelativeTransform": {"Scale3D": {"X": 0.06, "Y": 0.06, "Z": 0.06}}}}}
```

Scale to the real object size; non-uniform scale is fine (a flatbread is a rye loaf at Z 0.4).

## 4. One scan, several items

* **Recolour** (`Content/Python/item_tint.py`): `make(path, parent_mi, tint=(r,g,b), Brightness=…, Saturation=…,
  Normal_Intensity=…, Max_Roughness=…)` creates a child MI, then set the row's `WorldVisual.MaterialOverride`
  (AMOWorldItem and the held mesh apply it to slot 0). Examples: RawFish01 (smoked fish desaturated), IronBloom01, the
  seven soil and powder piles from one sand pile, six leathers from one canvas. `make` refuses a parameter the master
  lacks: the foliage master names its tint "Base Color Tint" and has no Brightness.
  It cannot make anything metallic (metallic comes from the ORM texture), so a copper-tinted brick still reads as a brick.
* **Kitbash** (`Tools/kitbash.py`, Blender headless):
  * `taper` sharpens one end (WoodenSpear01, PlantingStick01, BoneAwl01).
  * `cut` keeps one end and caps it. It is axis-aligned: a cut square to the local direction failed on a forked
    antler.
  * `scale x,y,z` bakes a non-uniform scale into a mesh, so it can serve as a head (the whorl: a clay cup at Z 0.3).
  * `attach` puts a head on a shaft:
    * `tip`: FlintSpear01, the arrows, the dart, FishSpear01
    * `axe` / `adze`: the hafted axes, Adze01, the hoes
    * `stand`: FireDrill01, with the spindle upright in the fireboard
    * `ring`: DropSpindle01, with the whorl threaded on the shaft
  * Shaft proportions are baked in, so wire with a uniform row scale. Materials are matched by slot name.
  * The re-import uses the legacy FBX path, so no stray materials are created (assets cannot be deleted safely from
    editor Python).
  * For FBX round trips Blender must export `FBX_SCALE_NONE`: with unit scaling the legacy importer brought a
    131 cm stick in at 1.3 cm.

## 5. Icon

```bash
python Tools/icon_studio.py --items A,B            # export -> Blender Cycles -> import <Id>Icon + wire UI.IconSmall/Large
python Tools/icon_studio.py --items A --render-only   # look first (Saved/IconStudio/<Id>/icon.png); --replace overwrites
```

The studio applies the row's scale and material override, plus the MI's tint, brightness, contrast and saturation,
so the icon matches the world. A long item stood on end or lying on an edge with a flat side is laid down with its
thinnest axis up: a blade shows its flat, an axe head shows its face. The slender vase (ratio 3.9) still stands. The
icon import turns `Interchange.FeatureFlags.Import.SyncToBrowser` off for its duration, because otherwise every
import pops a Content Browser over the editor.

## 6. Check it

Look at the render; then in the game: `python Tools/ue.py pie begin`, `boot --seed 777`, give and drop the items
(`MOInventoryComponent.add_item_by_guid` + `drop_item_by_guid` from editor Python, as `MO.Test.DropPickup` does),
`MO.Cam.LookAt …`, `ue.py asset shot pie out.png`. Expect textured meshes resting on the ground; gray means a material
on a master without Nanite usage (step 2 was skipped).
