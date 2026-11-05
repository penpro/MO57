"""DARKTOWER lane: copy DARKSIDE's binary assets listed in overlay.txt onto the lane tree, throttled, through the share.
Run in Wes's session (session_task.ps1): an SSH login cannot see the share. Only ever writes under C:\\UEProjects\\MO57_Lane.
Reads files one after another at no more than MAX_MB_S, so DARKSIDE's disk barely notices."""
import os
import sys
import time

SRC = next((p for p in (r"Z:\UEProjects\MO57", r"\\DARKSIDE\DarksideProduction\UEProjects\MO57") if os.path.exists(os.path.join(p, "MO57.uproject"))), None)
DST = r"C:\UEProjects\MO57_Lane"
MAX_MB_S = 30.0
CHUNK = 1 << 20
if not SRC:
    print("FAILED: the share is not reachable from this session")
    sys.exit(3)
here = os.path.dirname(os.path.abspath(__file__))
lines = [l.strip() for l in open(os.path.join(here, "overlay.txt"), encoding="utf-8") if l.strip()]
print("source", SRC, "files", len(lines), flush=True)
t0 = time.time()
total = 0
copied = 0
missing = []
for i, rel in enumerate(lines):
    s = os.path.join(SRC, rel.replace("/", "\\"))
    d = os.path.join(DST, rel.replace("/", "\\"))
    if not d.lower().startswith(DST.lower() + "\\"):
        print("REFUSED", d)
        continue
    try:
        st = os.stat(s)
    except OSError:
        missing.append(rel)
        continue
    os.makedirs(os.path.dirname(d), exist_ok=True)
    tmp = d + ".lanetmp"
    with open(s, "rb") as fi, open(tmp, "wb") as fo:
        while True:
            b = fi.read(CHUNK)
            if not b:
                break
            fo.write(b)
            total += len(b)
            ahead = total / (MAX_MB_S * 1e6) - (time.time() - t0)
            if ahead > 0:
                time.sleep(ahead)
    os.replace(tmp, d)
    os.utime(d, (st.st_atime, st.st_mtime))
    copied += 1
    if i % 500 == 0:
        print(f"{i}/{len(lines)} {total / 1e9:.2f} GB {time.time() - t0:.0f} s", flush=True)
print(f"OVERLAY DONE: {copied} files, {total / 1e9:.2f} GB in {(time.time() - t0) / 60:.1f} min; missing {len(missing)}", flush=True)
for m in missing[:30]:
    print("  missing", m)
