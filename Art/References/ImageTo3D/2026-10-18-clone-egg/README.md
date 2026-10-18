# Clone incubation egg: body and hatch

Selected from concept B on 2026-10-18. Separate single-object image-to-3D references, generated with the built-in image_gen tool. Original generation prompts are in `prompts.json`; revision prompts are in `hatch-concave-prompt.txt` and `hatch-shallow-prompts.json`.

- `CloneEggPod01-body-v1.png`: horizontal open pale-granite egg, tapered nose at left, broad rear at right. Nose has cooled atmospheric-entry scorching. Smooth oval hatch opening and recessed sealing lip, subtle alien structures in the mineral lining, sparse clear nutritional jelly residue. No detached panel in this image.
- `CloneEggPod01-hatch-v1.png`: one shallow convex granite hatch with a smooth continuous fitted perimeter, matching mineral palette and restrained leading-end heat staining. Outer face shown; no main body in this image.
- `CloneEggPod01-hatch-concave-v2.png`: historical underside view; Wes found it too cupped. Superseded for modelling by v3.
- `CloneEggPod01-hatch-shallow-v3.png`: current primary hatch input for image-to-3D. Almost flat thin shell segment with only a slight bow, inner face visible at a low oblique angle, no raised bowl walls. Retains the mineral lining, subtle alien inlays, granite edge and left-tip heat stain. This supersedes the geometry of both earlier hatch views; they remain material/history references.

All are 1536 x 1024 PNGs with opaque white backgrounds. The body image was generated first; the hatch used that exact image as its design reference. The concave view edits the hatch with the body as an interior/material reference. These are separate reference images, not a mesh or engine import.

## Story and intended scale

An ancient civilization sent trillions of rocklike pods across the universe, each carrying a human clone. On a habitable planet, the egg grows its occupant to early adulthood, consuming most of the clear nutritional growth medium, then releases the hatch. These references show the empty hatched state. Most of the exterior should read as stone; technology is embedded and understated.

Suggested scale in the prompts: closed egg approximately 2.2 m long and 1.3 m broad; hatch approximately 1.4 m long and 0.75 m wide. Both long axes run left to right, nose at left. During 3D assembly, keep a shared scale and fit the panel's mating edge to the body's aperture. Single-image generation supplies the concept and material reference; the two meshes' final seam is established during modelling.

Wes's final hatch shape correction: almost flat, only slight shell curvature. The selected v3 prompt requests approximately 1 cm uniform thickness and 1.5 cm total bow across the width; those are modelling guides rather than measurements of a generated mesh. Use v3's shallow geometry when fitting the hatch, not v2's deep concavity.
