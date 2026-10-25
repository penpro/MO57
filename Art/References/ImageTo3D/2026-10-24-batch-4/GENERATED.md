# Batch 4: generated image-to-3D references

Generated 2026-10-24 Pacific time with the built-in image_gen tool. All ten requested items are separate 1254 × 1254 RGB PNGs on light-grey backgrounds, using the complete prompts in the supplied README.

| File | Intended physical size |
|---|---|
| BirdEgg01.png | About 5 cm long |
| Feathers01.png | Bundle about 28 cm long |
| FlaxFiber01.png | Hank about 55 cm long |
| WoolFleece01.png | Roll about 40 cm long |
| Porridge01.png | Bowl about 15 cm across |
| BronzeArrowhead01.png | About 5 cm long |
| BronzeHelmet01.png | About 22 cm tall, 21 cm across |
| IronArrowhead01.png | About 7 cm long |
| IronNails01.png | Pile about 10 cm across; each nail 5–8 cm |
| PlowShare01.png | About 35 cm long |

## Review and corrections

The initial prompts were used verbatim. Visual inspection led to three targeted edits: Feathers01 and IronNails01 initially each showed seven pieces rather than eight, so one matching piece was added to each; IronArrowhead01 was turned to expose the socket's hollow opening for reconstruction. The final bundle shows eight feather tips and the final pile has eight nail heads. The bronze arrowhead has two barbs and a tang. The helmet is upright on its rim, with lining holes and no plume or lining. The plowshare has a visible socket opening.

Exact initial prompts, edit prompts and selected original paths are in prompts.json. Generated originals were copied without image post-processing. Intended centimetre dimensions remain instructions for mesh scaling; the PNGs have no dimensional calibration.

## Files and validation

- All ten files opened and passed Pillow PNG verification: 1254 × 1254, RGB.
- All ten have distinct SHA-256 hashes, recorded with byte sizes in manifest.json.
- Project PNGs match their selected generated originals byte for byte.
- Copies in D:/MOAssets/FromChtgpt/ were SHA-256 verified against the project copies. Existing files were not overwritten.
- Initial prompts were compared programmatically against all ten README text blocks and match. The supplied README's SHA-256 remains unchanged.
- Download archive: C:/Users/penum/.codex/visualizations/2026/10/13/01a134bd-d8ba-7d62-bec2-59138e3ee28a/MO57-2026-10-24-batch-4.zip. Contents: ten PNGs, the supplied README, this rundown, prompts.json and manifest.json.

## Handoff

These are reference images for the next pipeline stage. TRELLIS reconstruction, opacity/mesh review, physical scaling, Unreal imports and row/icon updates still follow the supplied README and Docs/Item_Art_Pipeline.md. Feathers and flax are intended to reconstruct as solid ground-item bundles; single hairs are texture detail. The small arrowheads use the brief's 1024 px texture preparation in the 3D step. The source README was left unchanged.
