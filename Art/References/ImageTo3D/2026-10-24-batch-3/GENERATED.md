# Batch 3: generated image-to-3D references

Generated 2026-10-24 with the built-in image_gen tool. All ten requested items are separate 1254 × 1254 RGB PNGs on light-grey backgrounds, using the complete prompts in the supplied README.

| File | Intended physical size |
|---|---|
| Acorns01.png | Heap about 8 cm; each acorn about 2.5 cm |
| BoneHook01.png | About 4.5 cm long |
| MicrolithSickle01.png | About 32 cm long |
| OilLamp01.png | About 11 cm across, 4 cm tall |
| BronzeChisel01.png | About 18 cm long |
| BronzeSaw01.png | About 32 cm overall; blade about 22 cm |
| IronKnife01.png | About 24 cm overall; blade about 14 cm |
| IronSpearhead01.png | About 28 cm long; socket about 2.5 cm across |
| IronSword01.png | About 85 cm overall |
| IronShield01.png | About 70 cm across, 4 cm thick |

## Review and corrections

The initial prompts were used verbatim. Visual inspection led to two targeted edits: Acorns01 initially showed six acorns, so one was added; IronSword01 initially had an acute tip, so it was rounded. The final acorn reference shows seven acorns and two loose cups. The sickle shows five inserts, the saw has two handle rivets, the spearhead has a visibly open socket and nail hole, and the shield boss has four rivets. The lamp is unlit.

Exact initial prompts, edit prompts and selected original paths are in prompts.json. The generated originals were copied without image post-processing. Intended centimetre dimensions remain instructions for mesh scaling; the PNGs have no dimensional calibration.

## Files and validation

- All ten files opened and passed Pillow PNG verification: 1254 × 1254, RGB.
- All ten have distinct SHA-256 hashes, recorded with byte sizes in manifest.json.
- Project PNGs match their selected generated originals byte for byte.
- Copies in D:/MOAssets/FromChtgpt/ were SHA-256 verified against the project copies. Existing files were not overwritten.
- Download archive: C:/Users/penum/.codex/visualizations/2026/10/13/01a134bd-d8ba-7d62-bec2-59138e3ee28a/MO57-2026-10-24-batch-3.zip. Contents: ten PNGs, the supplied README, this rundown, prompts.json and manifest.json.

## Handoff

These are reference images for the next pipeline stage. TRELLIS reconstruction, opacity/mesh review, physical scaling, Unreal imports and row/icon updates still follow the supplied README and Docs/Item_Art_Pipeline.md. The source README was left unchanged.
