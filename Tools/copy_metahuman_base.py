"""Copy a built MetaHuman base from MO_Tools into MO57 (step 4 of the base pipeline, MO_Tools CLAUDE.md "A base"; Docs/TOOLING.md).

Both projects' Content/MetaHumans are local and gitignored. Copies the base's folder and every package it depends on outside it, as listed by
MO_Tools' Tools/body_shape/base_deps.py (Saved/base_deps_<name>.json). A file MO57 already has that differs is backed up first to
D:/UEProjects/MO57_Backups/<date>_<name>/Content/<same path>, then replaced; a manifest sits next to the backup.

    python Tools/copy_metahuman_base.py MH_Jalena_Base [--dry-run] [--replace-common]

Close MO57's editor first. A file MO57 already has in /Game/MetaHumans/Common is kept by default even when the lab's differs: MO57's carries
its own edits (the foot IK's virtual bones on the skeleton), and a base refresh -- a rebaked face, Oct 28 -- needs none of the lab's. With
--replace-common those are replaced too; then run Content/Python/ensure_foot_ik_virtual_bones.py in MO57's editor: the Creator's skeleton
lacks the foot IK's virtual bones (TRAPS). The dry run lists every file it would replace.
"""
import argparse
import datetime
import hashlib
import json
import os
import shutil
import sys

LAB = "D:/UEProjects/MO_Tools/Content/"
MO = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Content").replace("\\", "/") + "/"


def digest(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--replace-common", action="store_true", help="also replace MO57's existing /Game/MetaHumans/Common files that differ")
    a = ap.parse_args()
    deps = "D:/UEProjects/MO_Tools/Saved/base_deps_%s.json" % a.name
    if not os.path.exists(deps):
        sys.exit("no %s: run MO_Tools' Tools/body_shape/base_deps.py with BASE_FOLDER=/Game/MetaHumans/%s first" % (deps, a.name))
    rows = json.load(open(deps))
    backup = "D:/UEProjects/MO57_Backups/%s_%s/Content/" % (datetime.date.today().isoformat(), a.name)
    counts = {"new": 0, "replaced (backed up)": 0, "identical": 0, "no file": 0, "Common kept (MO57's own)": 0}
    common_replaced = 0
    manifest = []
    for row in rows:
        if not row["file"]:
            counts["no file"] += 1
            continue
        rel = row["file"].replace("\\", "/").split("/Content/", 1)[1]
        src, dst = LAB + rel, MO + rel
        if os.path.exists(dst):
            if digest(src) == digest(dst):
                counts["identical"] += 1
                continue
            if rel.startswith("MetaHumans/Common/") and not a.replace_common:
                counts["Common kept (MO57's own)"] += 1
                print("kept MO57's " + rel)
                continue
            counts["replaced (backed up)"] += 1
            common_replaced += rel.startswith("MetaHumans/Common/")
            manifest.append("replaced " + rel)
            if not a.dry_run:
                os.makedirs(os.path.dirname(backup + rel), exist_ok=True)
                shutil.copy2(dst, backup + rel)
        else:
            counts["new"] += 1
            manifest.append("new " + rel)
        if not a.dry_run:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
    if manifest and not a.dry_run:
        root = backup.rstrip("/").rsplit("/", 1)[0]
        os.makedirs(root, exist_ok=True)
        with open(root + "/MANIFEST.txt", "w") as f:
            f.write("Copied %s from MO_Tools into MO57 (%s). Restore a replaced file from Content/ here.\n" % (a.name, datetime.datetime.now().isoformat(" ", "minutes")))
            f.write("\n".join(manifest) + "\n")
    if a.dry_run:
        for line in manifest:
            print("would: " + line)
    print(("would copy " if a.dry_run else "copied ") + a.name, counts, "backup:", backup if counts["replaced (backed up)"] else "-")
    if common_replaced:
        print("%d file(s) in /Game/MetaHumans/Common replaced: run Content/Python/ensure_foot_ik_virtual_bones.py in MO57's editor" % common_replaced)
    if not a.dry_run:
        print("then run Content/Python/ensure_body_jiggle.py in MO57's editor: the lab's body mesh runs the Creator's post-process, without the springs")


if __name__ == "__main__":
    main()
