"""DARKTOWER lane: find git-lfs pointer files among the lane's assets and replace each with the real file from DARKSIDE's tree, through the share. Writes only under the lane."""
import os
import shutil
import sys

SRC = next((p for p in (r"Z:\UEProjects\MO57", r"\\DARKSIDE\DarksideProduction\UEProjects\MO57") if os.path.exists(os.path.join(p, "MO57.uproject"))), None)
DST = r"C:\UEProjects\MO57_Lane"
roots = [os.path.join(DST, "Content")] + [os.path.join(DST, "Plugins", n, "Content") for n in ("MOFramework", "MOBodyShape")]
EXT = (".uasset", ".umap", ".ubulk", ".uexp", ".uptnl")
ptr = []
total = 0
for r in roots:
    for d, subs, files in os.walk(r):
        for n in files:
            if not n.lower().endswith(EXT):
                continue
            total += 1
            p = os.path.join(d, n)
            try:
                if os.path.getsize(p) > 400:
                    continue
                with open(p, "rb") as f:
                    if f.read(20).startswith(b"version https://git"):
                        ptr.append(p)
            except OSError as e:
                print("error", p, e)
print(f"scanned {total} assets, {len(ptr)} are LFS pointers", flush=True)
fixed = 0
missing = []
for p in ptr:
    rel = os.path.relpath(p, DST)
    s = os.path.join(SRC, rel) if SRC else None
    if s and os.path.exists(s) and os.path.getsize(s) > 400:
        shutil.copy2(s, p)
        fixed += 1
    else:
        missing.append(rel)
print(f"replaced {fixed}; no real copy for {len(missing)}", flush=True)
for m in missing[:50]:
    print("  still a pointer:", m)
print("POINTERS DONE")
