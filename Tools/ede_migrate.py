"""Migrate a SELECTED slice of a sample project into MO57's Content, preserving package paths (what the editor's Migrate does). Two sources (SOURCES):
  ede        Epic's Electric Dreams Environment sample (D:\\UEProjects\\ElectricDreamsEnv): GROUPS, `Docs/ElectricDreams_Migrated.md`
  darkruins  the Dark Ruins Megascans sample (D:\\UEProjects\\DarkRuinsMegascansSample; Wes, Oct 24 2026): DARKRUINS_GROUPS, `Docs/DarkRuins_Assets.md`

    python Tools/ede_migrate.py scan [--source S]          headless UnrealEditor-Cmd on the sample: reference closure of every group -> Saved/<S>_selection.json
    python Tools/ede_migrate.py copy [--source S] [--dry]  copy the closure's files (.uasset/.uexp/.ubulk/.uptnl) into Content/, never overwriting a different file,
                                                           and append .gitignore lines for the NEW folders (third-party content stays out of git/LFS, like UltraDynamicSky)

The samples are Epic / Quixel content licensed for use in Unreal Engine projects. Nothing here downloads anything. Needs the sample project on disk; the editor on
MO57 may stay closed. Add a group by listing /Game folders / assets in the source's groups (a group's closure is hard references inside /Game only, plus soft ones
when the group's third element is True; engine assets are always available).
"""
import argparse
import filecmp
import json
import os
import shutil
import subprocess
import sys

DST_CONTENT = r"D:\UEProjects\MO57\Content"
from ue import EDITOR_CMD  # noqa: E402
EXTS = (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl", ".upayload")

M = "/Game/Megascans/"
A = "/Game/Asmbly/Assemblies/"
# group -> list of /Game folders (every asset beneath) or single asset packages. Order matters only for the report (incremental cost).
GROUPS = [
    ("surfaces: mud bed, damp bank, moss ground", [M + "Surfaces/" + n for n in ("SwampWater", "DriedGrassOnDampSoil", "MossyRockyGround", "NordicMoss", "MossyGrass")]),
    ("plants: shore / understory / ground cover", [M + "3D_Plants/" + n for n in ("Taro", "JungleGinger", "Periwinkle", "KikuyuGrass", "Fern", "BostonFern", "SilverLadyFern", "CloverVarieties",
                                                                                "CustomMoss", "Lichen", "GroundCover", "DeadLeaves", "YellowArchangel", "WhiteWindflower")]),
    ("decals: leakage, moss patches, debris", [M + "Decals/" + n for n in ("Leakage", "TileableLeakage", "TileableMossPatches", "ScatteredSticksAndTwigs", "JungleDebris")]),
    ("rocks, embankments, roots, branches", [M + "3D_Assets/" + n for n in ("GrassyEmbankment", "MossyEmbankment", "NordicBeachRocks", "NordicBeachRock", "SmoothRockShelf", "SmallStonesPack",
                                                                          "DryRiverBed", "ForestRoots", "DryBranches")]),
    ("forest: hornbeam trees (SimpleWind set), dead trees, rock formations, forest path", [M + "3D_Plants/EuropeanHornbeam/Geometry/SimpleWind"] + [M + "3D_Plants/Elderberry"] + [M + "3D_Assets/" + n for n in ("DeadTree", "ForestRockFormation", "RockyForestPath", "ForestGround", "MossyForestBoulder", "MossyRocks", "MossyForestRock")]),
    ("nordic: boulders, tundra moss boulders, rocky ground plates, coastal cliff, pebble beach", [M + "3D_Assets/" + n for n in ("NordicBoulder", "NordicBeachRockyGround", "MassiveNordicCoastalCliff",
                                                                                                                   "TundraMossyBoulder", "TundraRockyGround", "BeachSandWithPebbles")]),
    ("fx: fog cards, water drips, falling leaves, puddle", ["/Game/FX", "/Game/Effects"]),
    ("shore assemblies (packed level blueprints)", [A + n for n in ("BPP_SmallEmbankment_01", "BPP_SmallEmbankment_02", "BPP_SmallRiverbedEmbankment_01", "BPP_SmallRiverbedEmbankment_02",
                                                                      "BPP_FernAssembly_01", "BPP_MediumEmbankment_Mud_01", "BPP_MediumGround_Mud_01")]),
    # The LIVE assembly system (Wes, Oct 14 2026: "in the video demo they move the cliffs around at runtime and it re-builds on the fly"): the assembly actors whose own
    # PCG component builds the cliff + its moss / vines / debris, their graphs and custom nodes, the assembly levels / PCG data they read, the hanging vines, cliff vines.
    # SOFT references too (third element): PCG graphs and data assets name their meshes by soft path, so a hard-only closure would arrive hollow.
    ("electric dreams live assemblies (PCG): cliffs, boulders, vines", ["/Game/PCG/Assets/BP_PCG_LargeAssembly", "/Game/PCG/Assets/BP_PCG_SmallAssembly", "/Game/PCG/Assets/BP_PCG_LocationMarker",
                                                                         "/Game/PCG/Assets/PCGCustomNodes", "/Game/PCG/Assets/Blueprints", "/Game/PCG/Utilities", "/Game/PCG/Script",
                                                                         "/Game/PCG/Graphs/LargeAssembly", "/Game/PCG/Graphs/SmallAssembly", "/Game/PCG/Graphs/Forest/PCGDemo_ForestCliffs",
                                                                         "/Game/PCG/Assets/PCGAssemblies/Forest", "/Game/PCG/Assets/PCGAssemblies/LargeAssembly", "/Game/PCG/Assets/PCGAssemblies/SmallAssembly",
                                                                         "/Game/Asmbly/BPP_Asmbl_HangingVines_00", "/Game/Asmbly/BPP_Asmbl_HangingVines_01", "/Game/Asmbly/BPP_Asmbl_HangingVines_02",
                                                                         "/Game/Asmbly/BPP_Asmbl_HangingVines_03", "/Game/Asmbly/BPP_Asmbl_HangingVines_04", "/Game/Asmbly/BPP_Asmbl_HangingVines_05",
                                                                         "/Game/Custom/IvyTest"], True),
]

# The Dark Ruins Megascans sample (Wes, Oct 24 2026: "there's a bunch of very large rocky assets in the dark ruins megascans project ... the big western cliffs
# are ENORMOUS and could help really get some good looking cliffs without a shit ton of cpu calls"). Its scans use the Smart master material (/Game/Smart) and
# its detail textures (/Game/Textures/Detail, /Game/Materials/Custom). Only what the world uses is listed; the candidates are in Docs/DarkRuins_Assets.md.
MS3D = "/Game/MS/3D/"
DARKRUINS_GROUPS = [
    ("cliffs: Huge Nordic Coastal Cliff (Smart; the crags and NordicStrand's coast)", [MS3D + "Huge_Nordic_Coastal_Cliff_venrdcgga/Smart"]),
    ("cliffs: Desert Western Cliff Sheer XL 04 (Smart; Wes's own migration)", [MS3D + "Desert_Western_Cliff_Sheer_Xl_04_xgknben/Smart"]),
]

SOURCES = {
    "ede": {"project": r"D:\UEProjects\ElectricDreamsEnv\ElectricDreamsEnv.uproject", "groups": GROUPS, "label": "Electric Dreams slice", "plugins": ""},
    # the sample has no Python plugin: the scan enables it for that run only (nothing in the sample changes)
    "darkruins": {"project": r"D:\UEProjects\DarkRuinsMegascansSample\DarkRuinsMegascansSample.uproject", "groups": DARKRUINS_GROUPS,
                  "label": "Dark Ruins Megascans sample", "plugins": "PythonScriptPlugin,EditorScriptingUtilities"},
}


def source(name):
    src = dict(SOURCES[name])
    src["name"] = name
    src["content"] = os.path.join(os.path.dirname(src["project"]), "Content")
    src["selection"] = os.path.join(os.path.dirname(DST_CONTENT), "Saved", "%s_selection.json" % name)
    return src


SCAN_SCRIPT = r'''
import json, os
import unreal
GROUPS = json.loads(r"""%s""")
OUT = r"%s"
CONTENT = r"%s"
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.search_all_assets(True)
opts = unreal.AssetRegistryDependencyOptions()
for k, v in (("include_hard_package_references", True), ("include_soft_package_references", False), ("include_searchable_names", False),
             ("include_hard_management_references", False), ("include_soft_management_references", False)):
    opts.set_editor_property(k, v)
dc = {}
def deps(p):
    if p not in dc:
        dc[p] = [str(x) for x in ar.get_dependencies(unreal.Name(p), opts)]
    return dc[p]
def closure(roots):
    seen, st = set(), list(roots)
    while st:
        p = st.pop()
        if p in seen or not p.startswith("/Game/"):
            continue
        seen.add(p); st.extend(deps(p))
    return seen
soft_opts = unreal.AssetRegistryDependencyOptions()
for k, v in (("include_hard_package_references", True), ("include_soft_package_references", True), ("include_searchable_names", False),
             ("include_hard_management_references", False), ("include_soft_management_references", False)):
    soft_opts.set_editor_property(k, v)
sdc = {}
def soft_closure(roots):
    seen, st = set(), list(roots)
    while st:
        p = st.pop()
        if p in seen or not p.startswith("/Game/"):
            continue
        seen.add(p)
        if p not in sdc:
            sdc[p] = [str(x) for x in ar.get_dependencies(unreal.Name(p), soft_opts)]
        st.extend(sdc[p])
    return seen
def roots_of(entry):
    found = sorted({str(a.package_name) for a in ar.get_assets_by_path(entry, recursive=True)})
    return found if found else [entry]
have, out = set(), {}
for group in GROUPS:
    name, entries = group[0], group[1]
    roots = []
    for e in entries:
        roots.extend(roots_of(e))
    c = soft_closure(roots) if len(group) > 2 and group[2] else closure(roots)
    out[name] = sorted(c - have)
    have |= c
json.dump(out, open(OUT, "w"))
unreal.log_warning("EDEMIGRATE scan done: %%d packages" %% len(have))
'''


def scan(src):
    os.makedirs(os.path.dirname(src["selection"]), exist_ok=True)
    script = os.path.join(os.path.dirname(src["selection"]), "%s_scan_script.py" % src["name"])
    with open(script, "w") as f:
        f.write(SCAN_SCRIPT % (json.dumps(src["groups"]), src["selection"], src["content"]))
    cmd = [EDITOR_CMD, src["project"], "-ExecutePythonScript=" + script, "-RenderOffScreen", "-unattended", "-nosplash", "-stdout", "-FullStdOutLogOutput"]
    if src["plugins"]:
        cmd.append("-EnablePlugins=" + src["plugins"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    lines = [l for l in (r.stdout or "").splitlines() if "EDEMIGRATE" in l]
    print("\n".join(lines) or "no EDEMIGRATE line: scan failed (exit %d)" % r.returncode)
    return os.path.exists(src["selection"])


def files_of(pkg, root):
    base = os.path.join(root, pkg[len("/Game/"):].replace("/", os.sep))
    return [base + e for e in EXTS if os.path.exists(base + e)]


def copy(src, dry):
    sel = json.load(open(src["selection"]))
    copied = skipped_same = 0
    conflicts, new_dirs, per_group = [], set(), {}
    nbytes = 0
    for name, pkgs in sel.items():
        gbytes = 0
        for pkg in pkgs:
            for f in files_of(pkg, src["content"]):
                rel = os.path.relpath(f, src["content"])
                dst = os.path.join(DST_CONTENT, rel)
                if os.path.exists(dst):
                    if filecmp.cmp(f, dst, shallow=False):
                        skipped_same += 1
                    else:
                        conflicts.append(rel)
                    continue
                d = os.path.dirname(dst)
                # the top-most directory of this path that does not exist yet = a "new folder" for .gitignore
                probe = d
                top_new = None
                while probe and not os.path.exists(probe):
                    top_new = probe
                    probe = os.path.dirname(probe)
                if top_new:
                    new_dirs.add(os.path.relpath(top_new, DST_CONTENT).replace(os.sep, "/"))
                if not dry:
                    os.makedirs(d, exist_ok=True)
                    shutil.copy2(f, dst)
                copied += 1
                sz = os.path.getsize(f)
                nbytes += sz
                gbytes += sz
        per_group[name] = gbytes
    print("%s: %d files (%.2f GB), %d identical already present, %d CONFLICTS (different content, left alone)" % ("DRY RUN" if dry else "copied", copied, nbytes / 1e9, skipped_same, len(conflicts)))
    for k, v in per_group.items():
        print("  %-55s %7.1f MB" % (k, v / 1048576.0))
    for c in conflicts[:20]:
        print("  CONFLICT", c)
    # fold nested new dirs under their parents
    folded = sorted(d for d in new_dirs if not any(d != o and d.startswith(o + "/") for o in new_dirs))
    print("new folders (for .gitignore):")
    for d in folded:
        print("  Content/" + d + "/")
    if not dry and folded:
        # Third-party content stays out of git: append the new folders (the doc said this happened; it only printed them -- Oct 14 2026 12 GB showed as untracked).
        gitignore = os.path.join(os.path.dirname(DST_CONTENT), ".gitignore")
        existing = open(gitignore, encoding="utf-8").read().splitlines() if os.path.exists(gitignore) else []
        add = ["Content/" + d + "/" for d in folded if "Content/" + d + "/" not in existing]
        if add:
            with open(gitignore, "a", encoding="utf-8") as f:
                f.write("\n# %s (Tools/ede_migrate.py): third-party, re-created by `ede_migrate.py copy --source %s`\n" % (src["label"], src["name"]) + "\n".join(add) + "\n")
            print("  -> appended %d line(s) to .gitignore" % len(add))
    if not dry and copied:
        # The copied meshes come with Electric Dreams' collision (many cliffs have none, or a few hulls a 4x outcrop falls through). The fix lives in
        # these gitignored assets, so every machine re-applies it after a copy.
        print("NEXT (editor open, PIE stopped): python Tools/ue.py py --file Content/Python/fix_mesh_collision.py   -- gives the world-feature rocks their real surface as collision")
    return folded, per_group, conflicts, copied, nbytes


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("action", choices=("scan", "copy"))
    ap.add_argument("--source", choices=sorted(SOURCES), default="ede")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.action == "scan":
        sys.exit(0 if scan(source(a.source)) else 1)
    copy(source(a.source), a.dry)
