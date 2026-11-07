"""Close open rock scans in Blender and bring them back as closed copies (Wes, Nov 6: "there's still a bunch of open geometry assets being used, like the circular gravel assets around the caves ... put them all into blender and
curve the edges down so they sit better and close them off so worst case they're not see through from one side, it's mostly rocks").

    python Tools/close_rocks.py census                         which rocks the world places and how open they are (Content/Python/open_mesh_census.py -> Saved/open_mesh_census.json)
    python Tools/close_rocks.py one <mesh path> [--in-place] [blender args]  one mesh: export, Tools/close_rocks_blender.py, import as /Game/ClosedRocks/<Name>/<Name>_Closed (or over itself)
    python Tools/close_rocks.py all [--limit N] [--match TEXT]               every open rock or ground mesh of the census not closed yet (SKIP says what is not)
    python Tools/close_rocks.py swap [--dry]                   point DT_CaveScatter and DT_Biomes at the closed copies (rows get -> patch -> rows set, readback-verified)

In Git Bash run it with MSYS_NO_PATHCONV=1, or the /Game paths come out as C:/Program Files/Git/Game/... (TRAPS). Needs the editor running (bridge; PIE not running) and Blender 5.2. Work files in Saved/CloseRocks/<Name>/. The originals are never touched: the vendor packs are gitignored and per machine, and a mesh's closed copy is what is in git.
Where a closed mesh goes: only the data tables place it -> a copy under /Game/ClosedRocks, in git, the tables pointed at it (`swap`). A composition places it -> over itself: a composition's meshes are point attributes inside PCG data no table can
swap. The packs those live in are gitignored and per machine like Tools/ede_migrate.py's, so this is per machine too: after a migration or a re-download run `all` again (Saved/CloseRocks/<Name>/original.uasset keeps the file it overwrote).
Saved/CloseRocks/closed.json records source -> closed with the Blender's last line. A mesh whose Blender step does not end WATERTIGHT is reported and not imported.
"""
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
UE = [sys.executable, os.path.join(ROOT, "Tools", "ue.py")]
WORK = os.path.join(ROOT, "Saved", "CloseRocks")
RECORD = os.path.join(WORK, "closed.json")
CENSUS = os.path.join(ROOT, "Saved", "open_mesh_census.json")
MARK = "CLOSEROCKS="
OPEN_AT = 0.2
# Not rock or ground: climbing plants, leaf sheets, an editor icon, a built arch, a bark item. /Game/Custom/IvyTest's embankments are ivy shells.
SKIP = re.compile(r"Vine|Hornbeam|flower_|LandSlideIcon|SM_Circle_|Arch_Flat|wjhiecy|IvyTest|/ClosedRocks/")


class EditorStuck(Exception):
    """The editor did not answer: a prompt, a crash, or a call that outlasted the timeout. Every later call would queue behind it, so a batch stops here."""


def ue_py(code, timeout=1800):
    r = subprocess.run(UE + ["py", "--timeout", str(timeout), "-c", code], capture_output=True, text=True, cwd=ROOT)
    for line in r.stdout.splitlines():   # the echo of the code holds the mark too: the answer is the line whose mark is followed by JSON
        if MARK in line and line.split(MARK, 1)[1].lstrip().startswith("{"):
            return line.split(MARK, 1)[1]
    raise EditorStuck("editor gave no answer:\n" + r.stdout[-1500:] + r.stderr[-800:])


def load_record():
    try:
        return json.load(open(RECORD, encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def content_file(mesh):
    root, rest = ("Plugins/MOFramework/Content", mesh[len("/MOFramework/"):]) if mesh.startswith("/MOFramework/") else ("Content", mesh[len("/Game/"):])
    return os.path.join(ROOT, root, rest + ".uasset")


def close_one(mesh, extra=(), in_place=False):
    name = mesh.rsplit("/", 1)[1].split(".")[0]
    work = os.path.join(WORK, name).replace("\\", "/")
    os.makedirs(work, exist_ok=True)
    src_fbx, out_fbx = work + "/src.fbx", work + "/closed.fbx"
    dest = mesh if in_place else f"/Game/ClosedRocks/{name}/{name}_Closed"
    if in_place and not os.path.isfile(work + "/original.uasset"):
        shutil.copy2(content_file(mesh), work + "/original.uasset")
    if os.path.isfile(src_fbx) and os.path.getmtime(src_fbx) >= os.path.getmtime(content_file(mesh)):
        print(f"{name}: the export from {os.path.getmtime(src_fbx):.0f} is newer than the mesh, used as it is", flush=True)
    else:
        exported = json.loads(ue_py(f"import importlib, kitbash, close_rocks as c; importlib.reload(c); out({MARK!r} + c.export({mesh!r}, {src_fbx!r}))"))
        print(f"{name}: exported {exported}", flush=True)
        if not exported.get("ok"):
            return None
    if os.path.exists(out_fbx):
        os.remove(out_fbx)
    b = subprocess.run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(ROOT, "Tools", "close_rocks_blender.py"), "--", src_fbx, out_fbx] + list(extra), capture_output=True, text=True)
    lines = [l for l in b.stdout.splitlines() if l.startswith("CLOSE")]
    print("\n".join("  " + l for l in lines) or b.stdout[-2000:] + b.stderr[-1000:], flush=True)
    final = next((l for l in reversed(lines) if l.startswith("CLOSE final")), "")
    if "already closed" in " ".join(lines):
        print(f"{name}: closed already, nothing to import", flush=True)
        return None
    if not final and os.path.exists(src_fbx):
        os.remove(src_fbx)   # Blender could not read it (an export cut short?): the next run exports it again
    if "-- WATERTIGHT" not in final or not os.path.isfile(out_fbx):
        print(f"{name}: NOT imported: {final or 'blender wrote nothing'}", flush=True)
        return None
    result = json.loads(ue_py(f"import importlib, kitbash, close_rocks as c; importlib.reload(c); out({MARK!r} + c.import_closed({out_fbx!r}, {mesh!r}, {dest!r}))"))
    print(f"{name}: imported {result}", flush=True)
    if "error" in result:
        return None
    record = load_record()
    record[mesh] = {"closed": dest, "blender": final, "import": result}
    os.makedirs(WORK, exist_ok=True)
    json.dump(record, open(RECORD, "w", encoding="utf-8"), indent=1)
    return dest


def swap(dry):
    record = load_record()
    table_of = {"CaveScatter": "/MOFramework/Data/DT_CaveScatter.DT_CaveScatter", "Biomes": "/MOFramework/Data/DT_Biomes.DT_Biomes"}
    def closed_of(path):
        base = (path or "").split(".")[0]
        hit = record.get(base)
        if not hit or hit["closed"] == base:   # closed in place: the tables already point at it
            return None
        name = hit["closed"].rsplit("/", 1)[1]
        return f"{hit['closed']}.{name}"
    for label, table in table_of.items():
        listed = subprocess.run(UE + ["rows", "list", table], capture_output=True, text=True, cwd=ROOT).stdout
        names = [l.strip() for l in listed.splitlines() if l.strip()]
        got = subprocess.run(UE + ["rows", "get", table] + names, capture_output=True, text=True, cwd=ROOT)
        rows = json.loads(got.stdout)
        patch, swaps = {}, 0
        for name, row in rows.items():
            changed = {}
            if label == "CaveScatter":
                key = next((k for k in row if k.lower() == "mesh"), None)
                new = closed_of(row.get(key)) if key else None
                if new:
                    changed[key] = new
                    swaps += 1
            else:
                for field in ("species", "waterEdgeSpecies", "Species", "WaterEdgeSpecies"):
                    entries = row.get(field)
                    if not entries:
                        continue
                    touched = False
                    for e in entries:
                        key = next((k for k in e if k.lower() == "mesh"), None)
                        new = closed_of(e.get(key)) if key else None
                        if new:
                            e[key] = new
                            touched = True
                            swaps += 1
                    if touched:
                        changed[field] = entries
            if changed:
                patch[name] = changed
        print(f"{label}: {swaps} entries in {len(patch)} rows point at closed copies", flush=True)
        path = os.path.join(WORK, f"swap_{label}.json")
        json.dump(patch, open(path, "w", encoding="utf-8"), indent=1)
        if patch and not dry:
            r = subprocess.run(UE + ["rows", "set", table, "--file", path], capture_output=True, text=True, cwd=ROOT)
            print((r.stdout + r.stderr)[-600:], flush=True)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "census"
    if cmd == "census":
        r = subprocess.run(UE + ["py", "--timeout", "590", "--file", "Content/Python/open_mesh_census.py"], capture_output=True, text=True, cwd=ROOT)
        print(r.stdout[-400:], flush=True)
    elif cmd == "one":
        args = sys.argv[3:]
        in_place = "--in-place" in args
        close_one(sys.argv[2], [a for a in args if a != "--in-place"], in_place)
    elif cmd == "all":
        limit = int(sys.argv[sys.argv.index("--limit") + 1]) if "--limit" in sys.argv else 10000
        match = sys.argv[sys.argv.index("--match") + 1] if "--match" in sys.argv else ""
        record = load_record()
        todo = [m for m in json.load(open(CENSUS, encoding="utf-8")) if (m["openness"] >= OPEN_AT or m["boundaryEdges"] > 0) and m["mesh"] not in record and not SKIP.search(m["mesh"]) and match in m["mesh"]]
        todo.sort(key=lambda m: m["triangles_lod0"])
        print(f"{len(todo)} to close", flush=True)
        done, failed, stuck = 0, [], None
        for m in todo[:limit]:
            in_place = any(w in ("Composition", "Feature") for w in m["where"])
            print(f"--- {m['mesh']} ({'in place' if in_place else 'a copy'}), {done} done, {len(failed)} failed", flush=True)
            try:
                result = close_one(m["mesh"], (), in_place)
            except EditorStuck as e:
                stuck = f"{m['mesh']}: {e}"
                break
            if result:
                done += 1
            else:
                failed.append(m["mesh"])
        print(f"CLOSED {done}, failed {len(failed)}: {failed}", flush=True)
        if stuck:
            raise SystemExit("STOPPED, the editor is stuck: " + stuck[:1500])
    elif cmd == "swap":
        swap("--dry" in sys.argv)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
