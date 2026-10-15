"""Biome stocking model: what the biome spawner REALLY plants per hectare, per tier, from DT_Biomes.

    python Tools/pcg_stocking.py <DT_Biomes.json>                    # realised density per biome + tier vs the tier caps
    python Tools/pcg_stocking.py <DT_Biomes.json> --plan plan.json   # the same for a proposed change (see apply_plan)
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
import random
import sys

LARGE_TAGS = {"MOResource_Tree", "MOResource_Rock", "MOResource_Bush", "MOBiomeCover_Shrub", "MOBiomeCover_Rock"}
CAPS = {"ground": 6000.0, "large": 1200.0, "trees": 800.0, "far_grass": 288.0, "far_tree": 300.0, "far_canopy": 40.0}   # Oct 14 eve: ground 2000 -> 6000, far tree 112.5 -> 300, far canopy 18 -> 40
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


def realised(species, cap, rng):
    """Expected instances/ha for one tier: E[min(1, sum p)] * cap, plus each species' share."""
    groups = sorted({sp.get("ClusterGroup", -1) for sp in species if sp.get("ClusterGroup", -1) >= 0})
    total, per = 0.0, [0.0] * len(species)
    for _ in range(SAMPLES):
        field = {g: rng.random() for g in groups}
        acc = []
        for sp in species:
            p = sp.get("DensityPerHectare", 0.0) / cap
            g = sp.get("ClusterGroup", -1)
            if g >= 0:
                core = sp.get("ClusterCore", 0.0)
                p = 0.0 if field[g] < core else p * min(1.0 / max(1.0 - core, 0.25), 4.0)
            acc.append(p)
        s = sum(acc)
        filled = min(s, 1.0)
        total += filled
        if s > 0:
            for i, p in enumerate(acc):
                per[i] += filled * p / s
    return total / SAMPLES * cap, [x / SAMPLES * cap for x in per]


def report(rows, title):
    rng = random.Random(7)
    print(title)
    print("%-15s " % "biome" + "  ".join("%-22s" % t for t in CAPS))
    out = {}
    for r in rows:
        cells, out[r["Name"]] = [], {}
        for t, cap in CAPS.items():
            sp = [s for s in r.get("Species", []) if tier(s) == t]
            authored = sum(s.get("DensityPerHectare", 0.0) for s in sp)
            got, _ = realised(sp, cap, rng) if sp else (0.0, [])
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
    report(rows, "== current ==  authored -> realised /ha (% of tier cap)")
    if "--plan" in sys.argv:
        plan = json.load(open(sys.argv[sys.argv.index("--plan") + 1], encoding="utf-8"))
        if "_caps" in plan:
            CAPS.update(plan["_caps"])
        report(apply_plan(rows, plan), "\n== with plan ==")


if __name__ == "__main__":
    main()
