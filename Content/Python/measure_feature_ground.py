"""Measure a world feature composition's AUTHORED ground (FMOWorldFeatureGround) in a running game.

Run inside an MO57 instance (python Tools/ue.py inst do NAME py -c "exec(open(r'.../measure_feature_ground.py').read()); measure('ED_CliffAssembly', out_dir)").
The nearest composition actor carrying the feature's id tag must be a FRESH spawn (no grounding pass has moved anything: mo.WorldFeatures.Ground 0).

Where the composition's pieces say the authored landscape was, in the actor's UNSCALED local frame, per 1 m cell:
  * ground-lying pieces (leaves, pebbles, ground skins, plants, trees -- not climbers) that do NOT rest on one of the composition's tall rocks: their
    pivots are the landscape (a leaf on a landslide boulder says where the boulder is, not the ground);
  * under the tall rocks (cliffs, boulders, landslide stones): the rock's underside LOWERS a measured cell (a rock set into the ground; a log lying
    across the step bridges air, so an underside never makes a cell on its own) -- off by default;
  * wide flat ground cover on the LOWER ground (scree spreads, ground skins: GROUND_COVER) counts over its whole footprint at its underside, not
    only at its pivot (a scree spread reaches metres past its pivot: the apron hovered at its edges on a slope).
A cell's value is the 25th percentile of what it holds (robust to a plant on a ledge); interior holes are closed from their neighbours; high cells
give up their raise within inset_m of the rim; past the measured LOW ground the raise fades out over FADE_M taking the LOWEST neighbour.
Writes <out_dir>/<FeatureId><tag>.json in the row struct's field names (OriginCm, CellCm, NumX, NumY, HeightsCm, Weights), plus
<FeatureId><tag>.known.json (the measured cells) for merge(): the per-site PCG seed moves scree and plants, so the row's grid is merged from several
sites (merge() runs outside the engine).

Why: Electric Dreams' hillside sets carry their own landform (the cliff assembly: a 4 m plateau behind a cliff face). The terrain rises to it under
every site (FMOFeatureTerrain) so the composition stands on the ground it was made for -- Steam playtest Oct 16 2026, #15 / #19. The first version took
every small piece's pivot: leaves on the landslide boulders read as ground and the terrain rose over the boulders and the cliff face.
"""
import json
import math
import os

CELL = 100.0
FADE_M = 4.0
CLIMBERS = ("vine", "ivy", "moneyplant", "creeper", "lichen", "decal")
SKINS = ("grassyembankment", "forestterrain", "forestground", "groundpatch", "terrain", "soil", "dirt")   # the authored ground surface itself
TREES = ("tree", "hornbeam", "beech", "alder", "birch", "oak", "pine", "spruce", "maple", "willow")
TALL_ROCK_CM = 250.0
REST_BELOW_CM = 150.0
SUPPORT_CM = 60.0   # a piece at least this tall can carry others (rocks, logs, stumps, bushes): what rests on it is not the landscape
FLAT_MAX_CM = 120.0   # a ground-cover piece this low and ...
FLAT_MIN_SPAN_CM = 150.0   # ... at least this wide (a scree spread, a ground skin) lying on the LOWER ground counts over its whole footprint
GROUND_COVER = ("leaves", "pebble", "groundpatch", "terrain", "beach", "embankment", "path", "gravel", "scree", "soil", "dirt", "moss")   # not plants, not branches (a branch bridges air)
LOW_CM = 50.0   # the composition's lower ground, relative to its root (Electric Dreams authored the root there)
N8 = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]


def _kind(mesh):
    """(sample, support): is the piece's pivot a landscape sample candidate, can it carry other pieces."""
    name = mesh.get_name().lower()
    if any(w in name for w in CLIMBERS) or mesh.get_path_name().startswith("/Engine/BasicShapes/"):
        return False, False
    height = mesh.get_bounds().box_extent.z * 2.0
    skin = any(w in name for w in SKINS)
    tree = any(w in name for w in TREES) and "fallen" not in name
    return (tree or skin or height < TALL_ROCK_CM), (height >= SUPPORT_CM and not skin)


def _composition(feature_id):
    p = unreal.GameplayStatics.get_player_pawn(world, 0)
    best = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if feature_id in [str(t) for t in a.tags]:
            d = (a.get_actor_location() - p.get_actor_location()).length()
            if best is None or d < best[0]:
                best = (d, a)
    return best[1] if best else None


def _trace_rocks(rocks, start, end):
    """z of the nearest hit on the composition's tall rocks along start -> end (world), or None. rocks: [(component, (minx, miny, maxx, maxy))]."""
    best = None
    lo_x, hi_x = min(start.x, end.x), max(start.x, end.x)
    lo_y, hi_y = min(start.y, end.y), max(start.y, end.y)
    for comp, box in rocks:
        if hi_x < box[0] or lo_x > box[2] or hi_y < box[1] or lo_y > box[3]:
            continue
        r = comp.line_trace_component(start, end, True, False, False)
        if r is None:
            continue
        loc = r[0] if isinstance(r, tuple) else r
        d = (loc - start).length()
        if best is None or d < best[0]:
            best = (d, loc.z)
    return None if best is None else best[1]


def _grid(known, inset_m):
    """known: {(cx, cy): z} measured cells (unscaled local frame) -> the row struct's FMOWorldFeatureGround fields."""
    xs = [k[0] for k in known]; ys = [k[1] for k in known]
    margin = int(math.ceil(FADE_M * 100.0 / CELL)) + 1
    x0, x1, y0, y1 = min(xs) - margin, max(xs) + margin, min(ys) - margin, max(ys) + margin
    nx, ny = x1 - x0 + 1, y1 - y0 + 1
    h = [[None] * nx for _ in range(ny)]
    for (kx, ky), z in known.items():
        h[ky - y0][kx - x0] = z
    # Close interior holes: an unknown cell with >= 5 known neighbours takes their median (3 passes).
    for _ in range(3):
        fills = []
        for iy in range(ny):
            for ix in range(nx):
                if h[iy][ix] is None:
                    v = sorted(h[iy + dy][ix + dx] for dx, dy in N8 if 0 <= ix + dx < nx and 0 <= iy + dy < ny and h[iy + dy][ix + dx] is not None)
                    if len(v) >= 5:
                        fills.append((iy, ix, v[len(v) // 2]))
        for iy, ix, z in fills:
            h[iy][ix] = z
    # Inset from the rims: a HIGH cell (more than a metre above the composition's lower ground) within inset_m of a drop of more than a metre (to a
    # lower or unmeasured cell) gives up its raise -- the terrain's own step then stays behind the wall rocks. Low cells (the apron, the scree at the
    # foot) keep theirs right to the edge, and fade outward below.
    # "Low" is relative to the actor root, which Electric Dreams authored at the composition's lower ground: a percentile of the measured cells read
    # the cliff's plateau as low (its lower ground has few pieces: p10 was 318 cm) and the plateau faded outward into mounds again.
    z_low = LOW_CM
    inset = int(round(inset_m * 100.0 / CELL))
    if inset > 0:
        drop = []
        for iy in range(ny):
            for ix in range(nx):
                z = h[iy][ix]
                if z is None or z <= z_low + 100.0:
                    continue
                for dy in range(-inset, inset + 1):
                    for dx in range(-inset, inset + 1):
                        jx, jy = ix + dx, iy + dy
                        if dx * dx + dy * dy > inset * inset:
                            continue
                        o = h[jy][jx] if 0 <= jx < nx and 0 <= jy < ny else None
                        if o is None or z - o > 100.0:
                            drop.append((iy, ix))
                            break
                    else:
                        continue
                    break
        for iy, ix in drop:
            h[iy][ix] = None
    # Fade ring by ring outward from the LOW ground only (the lowest neighbour): the apron's edge gets a ramp; a rim inset above never re-rises.
    dist = [[0.0 if h[iy][ix] is not None else 1e9 for ix in range(nx)] for iy in range(ny)]
    rings = int(math.ceil(FADE_M * 100.0 / CELL))
    for ring in range(1, rings + 1):
        fills = []
        for iy in range(ny):
            for ix in range(nx):
                if h[iy][ix] is None:
                    v = [h[iy + dy][ix + dx] for dx, dy in N8 if 0 <= ix + dx < nx and 0 <= iy + dy < ny and h[iy + dy][ix + dx] is not None and dist[iy + dy][ix + dx] < ring
                         and h[iy + dy][ix + dx] <= z_low + 100.0]
                    if v:
                        fills.append((iy, ix, min(v)))
        for iy, ix, z in fills:
            h[iy][ix] = z
            dist[iy][ix] = float(ring)
    heights, weights = [], []
    for iy in range(ny):
        for ix in range(nx):
            d = dist[iy][ix]
            w = 0.0 if h[iy][ix] is None else (1.0 if d == 0.0 else max(0.0, 1.0 - d / (rings + 1)))
            w = w * w * (3.0 - 2.0 * w)   # smoothstep: no crease where the raise ends
            heights.append(round(h[iy][ix] if h[iy][ix] is not None else 0.0, 1))
            weights.append(round(w, 3))
    return {"OriginCm": {"X": x0 * CELL + CELL * 0.5, "Y": y0 * CELL + CELL * 0.5}, "CellCm": CELL, "NumX": nx, "NumY": ny, "HeightsCm": heights, "Weights": weights}


def measure(feature_id, out_dir, tag="", max_radius_cm=4000.0, fade_m=None, inset_m=2.0, exclude_supported=False, use_undersides=False):
    """inset_m: the raise stops this far short of any edge where the authored ground drops by more than a metre (a plateau's rim): the terrain's own
    step then stays behind the composition's wall rocks instead of sloping out in front of them (and between them) as mossy mounds."""
    global FADE_M
    if fade_m is not None:
        FADE_M = fade_m
    a = _composition(feature_id)
    if a is None:
        out("MEASURE no %s near the pawn" % feature_id)
        return
    tr = a.get_actor_transform()
    inv = tr.inverse()
    scale = tr.scale3d.z
    rocks, grounds = [], []
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.static_mesh
        if m is None or not c.is_visible() or c.get_instance_count() == 0:
            continue
        sample, support = _kind(m)
        if support:
            o, e, _ = unreal.SystemLibrary.get_component_bounds(c)
            rocks.append((c, (o.x - e.x, o.y - e.y, o.x + e.x, o.y + e.y)))
        if sample:
            grounds.append(c)

    cells = {}
    slab_cells = set()
    used = on_rock = flat = 0
    for c in grounds:
        mb = c.static_mesh.get_bounds()
        mo, me = mb.origin, mb.box_extent
        mname = c.static_mesh.get_name().lower()
        is_flat = me.z * 2.0 < FLAT_MAX_CM and max(me.x, me.y) * 2.0 >= FLAT_MIN_SPAN_CM and any(w in mname for w in GROUND_COVER)
        for i in range(c.get_instance_count()):
            t = c.get_instance_transform(i, True)
            t = t[1] if isinstance(t, tuple) else t
            w = t.translation
            l = inv.transform_location(w)   # unscaled local frame (the inverse removes the actor's scale)
            if math.hypot(l.x, l.y) > max_radius_cm:
                continue   # stray pieces far outside the composition (the small assembly buries a few 200 m away, 74 m down)
            if exclude_supported and _trace_rocks([r for r in rocks if r[0] != c], unreal.Vector(w.x, w.y, w.z - 2.0), unreal.Vector(w.x, w.y, w.z - REST_BELOW_CM * scale)) is not None:
                on_rock += 1
                continue
            cells.setdefault((int(math.floor(l.x / CELL)), int(math.floor(l.y / CELL))), []).append(l.z)
            used += 1
            if is_flat and l.z <= LOW_CM + 100.0:
                # The slab's whole footprint at its UNDERSIDE (the plane through its mesh box's bottom corners, in the actor's frame): a slab's edges
                # reach metres past its pivot, tilted with the authored slope, and an apron measured only at its pivots hovered at the edges on a
                # slope (Oct 16 2026 tour, embankment #2).
                c4 = [inv.transform_location(t.transform_location(unreal.Vector(mo.x + sx * me.x, mo.y + sy * me.y, mo.z - me.z)))
                      for sx, sy in ((-1.0, -1.0), (1.0, -1.0), (-1.0, 1.0))]
                ux, uy, uz = c4[1].x - c4[0].x, c4[1].y - c4[0].y, c4[1].z - c4[0].z
                vx, vy, vz = c4[2].x - c4[0].x, c4[2].y - c4[0].y, c4[2].z - c4[0].z
                nx_, ny_, nz_ = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
                if abs(nz_) > 1e-3:
                    ti = t.inverse()
                    xs = [c.x for c in c4] + [c4[1].x + vx]
                    ys = [c.y for c in c4] + [c4[1].y + vy]
                    for cx in range(int(math.floor(min(xs) / CELL)), int(math.floor(max(xs) / CELL)) + 1):
                        for cy in range(int(math.floor(min(ys) / CELL)), int(math.floor(max(ys) / CELL)) + 1):
                            px, py = (cx + 0.5) * CELL, (cy + 0.5) * CELL
                            pz = c4[0].z - (nx_ * (px - c4[0].x) + ny_ * (py - c4[0].y)) / nz_
                            m = ti.transform_location(tr.transform_location(unreal.Vector(px, py, pz)))
                            if abs(m.x - mo.x) <= me.x and abs(m.y - mo.y) <= me.y:
                                cells.setdefault((cx, cy), []).append(pz)
                                slab_cells.add((cx, cy))
                flat += 1

    # Under the tall rocks: their undersides, traced up from well below the root.
    under = {}
    if cells and use_undersides:
        xs = [k[0] for k in cells]; ys = [k[1] for k in cells]
        span = int(max_radius_cm / CELL)
        for cx in range(max(min(xs) - 6, -span), min(max(xs) + 6, span) + 1):
            for cy in range(max(min(ys) - 6, -span), min(max(ys) + 6, span) + 1):
                lx, ly = (cx + 0.5) * CELL, (cy + 0.5) * CELL
                wb = tr.transform_location(unreal.Vector(lx, ly, -3000.0))
                wt = tr.transform_location(unreal.Vector(lx, ly, 3000.0))
                z = _trace_rocks(rocks, wb, wt)
                if z is not None:
                    under[(cx, cy)] = inv.transform_location(unreal.Vector(wb.x, wb.y, z)).z

    def p25(v):
        v = sorted(v)
        return v[int(0.25 * (len(v) - 1))]

    known = {k: p25(v) for k, v in cells.items() if len(v) >= 2 or k in slab_cells}
    for k, z in under.items():
        if k in known:
            known[k] = min(z, known[k])   # a rock set into the ground can only LOWER a measured cell: a log across the step bridges air
    if not known:
        out("MEASURE %s: nothing measured" % feature_id)
        return

    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "%s%s.known.json" % (feature_id, tag)), "w") as f:
        json.dump({"%d,%d" % k: round(z, 1) for k, z in known.items()}, f)   # for merge(): several sites (per-site seeds) into one grid
    data = _grid(known, inset_m)
    nx, ny = data["NumX"], data["NumY"]
    zs = sorted(known.values())
    q = lambda f: zs[min(len(zs) - 1, int(f * len(zs)))]
    path = os.path.join(out_dir, "%s%s.json" % (feature_id, tag))
    with open(path, "w") as f:
        json.dump(data, f)
    out("MEASURE %s %s scale %.2f: %d landscape piece(s) (%d flat, rasterised; %d on rocks left out), %d rock-underside cell(s), %d known cell(s); grid %d x %d; z p10 %.0f / median %.0f / p90 %.0f -> %s"
        % (feature_id, a.get_name(), scale, used, flat, on_rock, len(under), len(known), nx, ny, q(0.1), q(0.5), q(0.9), path))
    return data


def merge(feature_id, known_paths, out_dir, tag="", fade_m=None, inset_m=1.5):
    """One grid from several sites' measured cells (<Id><tag>.known.json): the per-site PCG seed moves the scree and the plants, so one site's grid
    left another site's apron hovering. A cell measured anywhere counts; on the LOWER ground it takes an upper site (the 2/3 quantile: a slab a few cm
    deeper in the ground beats an edge in the air, while the highest site alone would bury every other site's leaves), above it the median (the
    block's filled interior stays under every site's plants).
    Runs outside the engine: python -c "exec(open('measure_feature_ground.py').read()); merge('ED_EmbankmentAssembly', [...], 'out')"."""
    global FADE_M
    if fade_m is not None:
        FADE_M = fade_m
    per = {}
    for p in known_paths:
        for k, z in json.load(open(p)).items():
            cx, cy = (int(v) for v in k.split(","))
            per.setdefault((cx, cy), []).append(z)
    known = {}
    for k, v in per.items():
        v = sorted(v)
        known[k] = v[min(len(v) - 1, (2 * len(v)) // 3)] if v[-1] <= LOW_CM + 100.0 else v[len(v) // 2]
    data = _grid(known, inset_m)
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "%s%s.json" % (feature_id, tag))
    with open(path, "w") as f:
        json.dump(data, f)
    (globals().get("out") or print)("MERGE %s: %d site(s), %d known cell(s); grid %d x %d, %d full -> %s"
                                     % (feature_id, len(known_paths), len(known), data["NumX"], data["NumY"], sum(1 for w in data["Weights"] if w >= 0.999), path))
    return data
