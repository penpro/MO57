# terrain_lab -- a numpy replica of the world generator (statistics-faithful, own hash)

`proto_world.py` ports `FMOWorldGen::SampleBaseHeightCm` (v5.1 + the v6 hooks) and **reproduces the real automation-test numbers** (land slope p50/p90/p99, highest ground), which is what makes it a
trustworthy lab: iterate a terrain shape here in seconds, then port it to C++ and confirm with `MOFramework.WorldGen.*`. `proto_shape.py` is the v6 layered mountain (massif / peaks / gullies / crags),
`proto_view.py` a tiny heightfield ray-marcher for perspective checks, `proto_seam.py` the LOD-to-LOD surface mismatch, `proto_v6.py` the mask / coverage experiments.
Needs numpy + Pillow (system Python). Run from this folder, e.g. `python proto_world.py` (validation line), `python proto_shape.py 7 a` (stats + renders for seed 7).
NOT bit-exact with the C++ generator (different hash): never use it to pin a golden. Remember: every wavelength in the real tuning is in CENTIMETRES.
