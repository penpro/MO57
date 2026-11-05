"""Write rel-path, size, mtime for a project tree (read only). usage: list_tree.py <root> <out.tsv>"""
import os
import sys

root, out = sys.argv[1], sys.argv[2]
SKIP = {".git", "Binaries", "Intermediate", "Saved", "DerivedDataCache", ".vs", ".claude", "graphify-out", "Packaged", "__pycache__",
        "Art", "SteamArt", "ExportForPS", ".idea", ".agents", ".codex", ".cursor", ".gemini", ".vscode", "DesignKit"}
n = 0
with open(out, "w", encoding="utf-8", newline="\n") as f:
    for d, subs, files in os.walk(root):
        subs[:] = [s for s in subs if s not in SKIP]
        rel = os.path.relpath(d, root).replace("\\", "/")
        for name in files:
            p = os.path.join(d, name)
            try:
                st = os.stat(p)
            except OSError:
                continue
            r = name if rel == "." else rel + "/" + name
            f.write(f"{r}\t{st.st_size}\t{int(st.st_mtime)}\n")
            n += 1
print(n, "files")
