"""Migrate a SELECTED slice of Epic's Electric Dreams Environment sample (D:\\UEProjects\\ElectricDreamsEnv) into MO57's Content, preserving package paths (what the editor's Migrate does).

    python Tools/ede_migrate.py scan            headless UnrealEditor-Cmd on the sample: hard-reference closure of every group below -> Saved/ede_selection.json
    python Tools/ede_migrate.py copy [--dry]    copy the closure's files (.uasset/.uexp/.ubulk/.uptnl) into Content/, never overwriting a different file; writes Docs/ElectricDreams_Migrated.md
                                                and .gitignore lines for the NEW folders (third-party content stays out of git/LFS, like UltraDynamicSky)

The sample is Epic content licensed for use in Unreal Engine projects. Nothing here downloads anything. Needs the ElectricDreamsEnv project on disk; the editor on MO57 may stay closed.
Add a group by listing /Game folders / assets in GROUPS (a group's closure is hard references inside /Game only; engine assets are always available).
"""
import argparse
import filecmp
import json
import os
import shutil
import subprocess
import sys

SRC_PROJECT = r"D:\UEProjects\ElectricDreamsEnv\ElectricDreamsEnv.uproject"
SRC_CONTENT = r"D:\UEProjects\ElectricDreamsEnv\Content"
DST_CONTENT = r"D:\UEProjects\MO57\Content"
SELECTION = r"D:\UEProjects\MO57\Saved\ede_selection.json"
EDITOR_CMD = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
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
    ("fx: fog cards, water drips, falling leaves, puddle", ["/Game/FX", "/Game/Effects"]),
    ("shore assemblies (packed level blueprints)", [A + n for n in ("BPP_SmallEmbankment_01", "BPP_SmallEmbankment_02", "BPP_SmallRiverbedEmbankment_01", "BPP_SmallRiverbedEmbankment_02",
                                                                      "BPP_FernAssembly_01", "BPP_MediumEmbankment_Mud_01", "BPP_MediumGround_Mud_01")]),
]

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
def roots_of(entry):
    found = sorted({str(a.package_name) for a in ar.get_assets_by_path(entry, recursive=True)})
    return found if found else [entry]
have, out = set(), {}
for name, entries in GROUPS:
    roots = []
    for e in entries:
        roots.extend(roots_of(e))
    c = closure(roots)
    out[name] = sorted(c - have)
    have |= c
json.dump(out, open(OUT, "w"))
unreal.log_warning("EDEMIGRATE scan done: %%d packages" %% len(have))
'''


def scan():
    os.makedirs(os.path.dirname(SELECTION), exist_ok=True)
    script = os.path.join(os.path.dirname(SELECTION), "ede_scan_script.py")
    with open(script, "w") as f:
        f.write(SCAN_SCRIPT % (json.dumps(GROUPS), SELECTION, SRC_CONTENT))
    cmd = [EDITOR_CMD, SRC_PROJECT, "-ExecutePythonScript=" + script, "-RenderOffScreen", "-unattended", "-nosplash", "-stdout", "-FullStdOutLogOutput"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    lines = [l for l in (r.stdout or "").splitlines() if "EDEMIGRATE" in l]
    print("\n".join(lines) or "no EDEMIGRATE line: scan failed (exit %d)" % r.returncode)
    return os.path.exists(SELECTION)


def files_of(pkg, root):
    base = os.path.join(root, pkg[len("/Game/"):].replace("/", os.sep))
    return [base + e for e in EXTS if os.path.exists(base + e)]


def copy(dry):
    sel = json.load(open(SELECTION))
    copied = skipped_same = 0
    conflicts, new_dirs, per_group = [], set(), {}
    nbytes = 0
    for name, pkgs in sel.items():
        gbytes = 0
        for pkg in pkgs:
            for src in files_of(pkg, SRC_CONTENT):
                rel = os.path.relpath(src, SRC_CONTENT)
                dst = os.path.join(DST_CONTENT, rel)
                if os.path.exists(dst):
                    if filecmp.cmp(src, dst, shallow=False):
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
                    shutil.copy2(src, dst)
                copied += 1
                sz = os.path.getsize(src)
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
    return folded, per_group, conflicts, copied, nbytes


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("action", choices=("scan", "copy"))
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.action == "scan":
        sys.exit(0 if scan() else 1)
    copy(a.dry)
