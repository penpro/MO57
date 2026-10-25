"""Biome stocking model: what the biome spawner REALLY plants per hectare, per tier, from DT_Biomes.

    python Tools/pcg_stocking.py <DT_Biomes.json>                    # realised density per biome + tier vs the tier caps
    python Tools/pcg_stocking.py <DT_Biomes.json> --plan plan.json   # the same for a proposed change (see apply_plan)
    python Tools/pcg_stocking.py <DT_Biomes.json> --old              # + what the pre-fix spawner and caps planted from that table
    python Tools/pcg_stocking.py apply plan.json                     # author the plan into the live DT_Biomes (editor up, PIE off)
    (export the table first: DataTableFunctionLibrary.export_data_table_to_json_string; tier caps in a plan's "_caps" are model
    inputs only -- the real caps are the voxel samplers / InputPointsPerHectare of the MO Biome Spawner nodes in MOPCG_StampScatter1)

The authored DensityPerHectare is not what you get. MOPCGBiomeSpawnerSettings plants at most ONE species per input point (the tier's
InputPointsPerHectare, set by its voxel sampler), with acceptance p = density * clump_compensation / input; a species in a
ClusterGroup only grows where that group's clump field >= its ClusterCore and is compensated by min(1/(1 - core), 4). Where the
summed acceptance passes 1 the slot is full and the rest is clipped, so a forest whose trees sit in high cores is saturated
inside the clumps and bare in the glades however high the densities go. This replays that rule (Monte Carlo over independent,
uniform clump fields; edge feather and slope bands ignored) so a change can be judged before it is authored.
Why (2026-10-14): Wes asked for more trees, bushes and ground cover; the palettes were 15-30 % of the tier caps and the tree
clumps already saturated, so "raise the densities" alone would have been clipped.
"""
import json
import os
import random
import sys

LARGE_TAGS = {"MOResource_Tree", "MOResource_Rock", "MOResource_Bush", "MOBiomeCover_Shrub", "MOBiomeCover_Rock"}
CAPS = {"ground": 6000.0, "large": 1200.0, "trees": 800.0, "far_grass": 288.0, "far_tree": 300.0, "far_canopy": 40.0}   # Oct 14 eve: ground 2000 -> 6000, far tree 112.5 -> 300, far canopy 18 -> 40
OLD_CAPS = {"ground": 2000.0, "far_tree": 112.5, "far_canopy": 18.0}   # the tier caps before Oct 14 eve
SAMPLES = 4000


def tier(sp):
    tag, mesh = sp.get("HISMTag", ""), sp.get("Mesh", "")
    if tag == "MOFar_Grass":
        return "far_grass"
    if tag == "MOFar_Tree":
        return "far_tree"
    if tag == "MOFar_Canopy":
        return "far_canopy"
    if tag == "MOResource_Tree" and "Sapling" not in mesh and "Seedling" not in mesh:
        return "trees"
    return "large" if tag in LARGE_TAGS else "ground"


# ---- The clump fields as the spawner really sees them -------------------------------------------------------------------------------------
# The spawner's clump field is FMath::PerlinNoise2D * 0.5 + 0.5, and that noise is NOT uniform (std ~0.26 of its -1..1 range): the field
# bunches around 0.5, so a core of 0.7 covers ~7 % of the ground, not 30 %. This samples the engine's own noise (ported below, permutation table
# read from the engine source) so the model sees the same distribution. Until Oct 14 2026 this model drew the field uniformly -- and so did the
# spawner's compensation, which is why forests realised a quarter of their authored trees (see UMOPCGBiomeSpawnerSettings::ClumpCoverage).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # loaded by file path from the editor too (biome_palettes.py): Tools is not on its path
import ue  # noqa: E402
UE_ROOT = os.environ.get("UE_ROOT") or ue.ENGINE_ROOT
MAX_CLUMP_CONCENTRATION = 8.0   # UMOPCGBiomeSpawnerSettings::MaxClumpConcentration
_CLUMP = None


def _perlin_samples(n=200000):
    import re
    import numpy as np
    src = open(os.path.join(UE_ROOT, "Engine/Source/Runtime/Core/Private/Math/UnrealMath.cpp"), encoding="utf-8", errors="replace").read()
    i = src.index("static const int32 Permutation[512]")
    perm = np.array([int(x) for x in re.findall(r"\d+", src[src.index("{", i) + 1:src.index("};", i)])], dtype=np.int64)
    r = np.random.default_rng(1)
    X, Y = r.uniform(0, 256, n), r.uniform(0, 256, n)
    xf, yf = np.floor(X), np.floor(Y)
    xi, yi = xf.astype(np.int64) & 255, yf.astype(np.int64) & 255
    x, y = X - xf, Y - yf

    def grad(h, gx, gy):
        h = h & 7
        return np.select([h == 0, h == 1, h == 2, h == 3, h == 4, h == 5, h == 6, h == 7], [gx, gx + gy, gy, -gx + gy, -gx, -gx - gy, -gy, gx - gy])

    smooth = lambda t: t * t * t * (t * (t * 6 - 15) + 10)
    lerp = lambda a, b, t: a + (b - a) * t
    u, v = smooth(x), smooth(y)
    aa, bb = perm[xi] + yi, perm[xi + 1] + yi
    return lerp(lerp(grad(perm[aa], x, y), grad(perm[bb], x - 1, y), u), lerp(grad(perm[aa + 1], x, y - 1), grad(perm[bb + 1], x - 1, y - 1), u), v)


def clump():
    """(field samples 0..1, coverage(core), legacy multiplier samples / their mean) -- measured once."""
    global _CLUMP
    if _CLUMP is None:
        import numpy as np
        noise = _perlin_samples()
        field = np.clip(noise * 0.5 + 0.5, 0.0, 1.0)
        legacy = np.clip(noise * 2.0 + 0.5, 0.0, 2.0)
        sorted_field = np.sort(field)
        coverage = lambda core: 1.0 - np.searchsorted(sorted_field, core, side="left") / len(sorted_field)
        _CLUMP = (field.tolist(), coverage, (legacy / legacy.mean()).tolist(), float(legacy.mean()))
    return _CLUMP


def compensation(core, old=False):
    """The spawner's in-clump multiplier: since Oct 14 2026 1 / measured coverage (capped); old = the pre-fix 1 / (1 - core), capped at 4."""
    if old:
        return min(1.0 / max(1.0 - core, 0.25), 4.0)
    return min(1.0 / max(clump()[1](core), 1e-6), MAX_CLUMP_CONCENTRATION)


def realised(species, cap, rng, old=False):
    """Expected instances/ha for one tier: E[min(1, sum p)] * cap, plus each species' share. old=True replays the pre-Oct-14-eve spawner
    (uniform-assumption clump compensation, un-normalised legacy clumping) -- what the world really planted before."""
    field_samples, _, legacy_samples, legacy_mean = clump()
    groups = sorted({sp.get("ClusterGroup", -1) for sp in species if sp.get("ClusterGroup", -1) >= 0})
    comp = [compensation(sp.get("ClusterCore", 0.0), old) for sp in species]
    total, per = 0.0, [0.0] * len(species)
    for _ in range(SAMPLES):
        field = {g: rng.choice(field_samples) for g in groups}
        acc = []
        for i, sp in enumerate(species):
            p = sp.get("DensityPerHectare", 0.0) / cap
            g = sp.get("ClusterGroup", -1)
            if g >= 0:
                p = 0.0 if field[g] < sp.get("ClusterCore", 0.0) else p * comp[i]
            elif sp.get("ClusterRadius", 0.0) > 0.0:
                m = rng.choice(legacy_samples)   # normalised: mean 1
                p *= m * legacy_mean if old else m
            acc.append(p)
        s = sum(acc)
        filled = min(s, 1.0)
        total += filled
        if s > 0:
            for i, p in enumerate(acc):
                per[i] += filled * p / s
    return total / SAMPLES * cap, [x / SAMPLES * cap for x in per]


def report(rows, title, old=False):
    rng = random.Random(7)
    print(title)
    print("%-15s " % "biome" + "  ".join("%-22s" % t for t in CAPS))
    out = {}
    for r in rows:
        cells, out[r["Name"]] = [], {}
        for t, cap in CAPS.items():
            sp = [s for s in r.get("Species", []) if tier(s) == t]
            authored = sum(s.get("DensityPerHectare", 0.0) for s in sp)
            got, _ = realised(sp, cap, rng, old) if sp else (0.0, [])
            out[r["Name"]][t] = (authored, got)
            cells.append("%5.0f->%5.0f (%3.0f%%)" % (authored, got, 100 * got / cap) if sp else "%-22s" % "-")
        print("%-15s " % r["Name"] + "  ".join("%-22s" % c for c in cells))
    return out


def apply_plan(rows, plan):
    """plan = {Biome: {tier: factor, "<tier>:<HISMTag>": factor, "<tier>_core_max": c}}: multiply densities per tier (a tier:tag
    factor wins over the tier's, e.g. "ground:MOBiomeCover_Grass": 12 grows the grass and not the pebbles), cap clustered species'
    cores at c."""
    for r in rows:
        p = plan.get(r["Name"], {})
        for s in r.get("Species", []):
            t = tier(s)
            factor = p.get("%s:%s" % (t, s.get("HISMTag", "")), p.get(t, 1.0))
            s["DensityPerHectare"] = round(s.get("DensityPerHectare", 0.0) * factor, 2)
            core_max = p.get(t + "_core_max")
            if core_max is not None and s.get("ClusterGroup", -1) >= 0:
                s["ClusterCore"] = min(s.get("ClusterCore", 0.0), core_max)
    return rows


def apply_live(plan_path):
    """Author a plan into the live DT_Biomes (editor running, PIE off): rows get -> apply_plan -> rows set (readback-verified)."""
    import os
    import subprocess
    import tempfile
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ue = [sys.executable, os.path.join(root, "Tools", "ue.py")]
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    table = "/MOFramework/Data/DT_Biomes.DT_Biomes"
    plan = json.load(open(plan_path, encoding="utf-8"))
    names = [n for n in plan if not n.startswith("_")]
    out = subprocess.run(ue + ["rows", "get", table] + names, capture_output=True, text=True, cwd=root, env=env).stdout
    live = json.loads(out[out.find("{"):])
    rows = [{"Name": n, "Species": live[n]["species"]} for n in names]
    apply_plan(rows, plan)
    patch = {r["Name"]: {"species": r["Species"]} for r in rows}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(patch, f)
    res = subprocess.run(ue + ["rows", "set", table, "--file", f.name], capture_output=True, text=True, cwd=root, env=env).stdout
    os.unlink(f.name)
    print("\n".join(l for l in res.splitlines() if "already exist" not in l)[-800:])


def main():
    if sys.argv[1] == "apply":
        apply_live(sys.argv[2])
        return
    rows = json.load(open(sys.argv[1], encoding="utf-8"))
    if "--old" in sys.argv:   # what the pre-Oct-14-eve world planted: old spawner compensation and old tier caps
        saved = dict(CAPS)
        CAPS.update(OLD_CAPS)
        report(rows, "== as planted by the pre-fix spawner (old caps) ==  authored -> realised /ha (% of tier cap)", old=True)
        CAPS.clear()
        CAPS.update(saved)
    report(rows, "== current ==  authored -> realised /ha (% of tier cap)")
    if "--plan" in sys.argv:
        plan = json.load(open(sys.argv[sys.argv.index("--plan") + 1], encoding="utf-8"))
        if "_caps" in plan:
            CAPS.update(plan["_caps"])
        report(apply_plan(rows, plan), "\n== with plan ==")


if __name__ == "__main__":
    main()
