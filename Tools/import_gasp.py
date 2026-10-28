"""Epic's Game Animation Sample into MO57 (Oct 27 2026; Wes: "start phase 1: plugins, migrate the CMC character and its animation set (keep our
MetaHumans/Common), and drive our survivor with it").

Copies the packages listed in Tools/data/gasp_packages.json -- the dependency set of the sample's CMC animation Blueprint
(SandboxCharacter_CMC_ABP: motion matching, its UEFN mannequin animation set, pose search databases, choosers, foley) and of the MetaHuman
retarget (ABP_GenericRetarget, RTG_UEFN_to_Metahuman_nrw / _ovw) -- from a Game Animation Sample project into MO57's Content, at the same
/Game paths (so every reference holds). NEVER overwrites a file MO57 already has: MO57's /Game/MetaHumans/Common stays ours (its
metahuman_base_skel carries the foot IK's virtual bones). The copied folders are gitignored like the Animals Full Pack (1.4 GB; a fresh
clone or DARKTOWER re-runs this with the sample from Fab). The list was made by a survey in the sample's editor; to remake it, see
Docs/Animation_GASP.md.

    python Tools/import_gasp.py [--from D:/UEProjects/GameAnimationSample] [--dry-run]

Close MO57's editor first (it must not hold the packages being added).
"""
import argparse
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EXTS = (".uasset", ".umap", ".ubulk", ".uexp", ".uptnl")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src", default=r"D:\UEProjects\GameAnimationSample")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    src_content = os.path.join(a.src, "Content")
    if not os.path.isdir(src_content):
        sys.exit(f"no Game Animation Sample at {a.src} (its Content folder is missing)")
    packages = json.load(open(os.path.join(HERE, "data", "gasp_packages.json"), encoding="utf-8"))["packages"]
    copied = kept = absent = size = 0
    for p in packages:
        rel = p[len("/Game/"):].replace("/", os.sep)
        found = False
        for ext in EXTS:
            s = os.path.join(src_content, rel + ext)
            if not os.path.exists(s):
                continue
            found = True
            t = os.path.join(ROOT, "Content", rel + ext)
            if os.path.exists(t):
                kept += 1   # MO57's own (metahuman_base_skel): never overwritten
                continue
            size += os.path.getsize(s)
            copied += 1
            if not a.dry_run:
                os.makedirs(os.path.dirname(t), exist_ok=True)
                shutil.copy2(s, t)
        absent += 0 if found else 1
    verb = "would copy" if a.dry_run else "copied"
    print(f"GASP import: {verb} {copied} file(s), {size / 2**30:.2f} GB; kept {kept} MO57 already has; {absent} package(s) not in the sample")
    return 0 if absent == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
