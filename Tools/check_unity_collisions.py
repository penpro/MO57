"""Find file-local (anonymous-namespace) function / struct names defined in more than one .cpp of the SAME module.

    python Tools/check_unity_collisions.py        (exit code 1 when a collision exists)

Why: UBT's adaptive unity build merges .cpp files into blobs and ONLY isolates files that are modified in the working set. A file-local helper named like one in a sibling .cpp
compiles fine while either file is being edited, then breaks (error C2668 / C2084 "already has a body") the day the files are committed and land in the same blob. That happened on 2026-10-11:
`Unit16` in two world-generator files and `MakeWaterParams` in two test files, found only after a commit. Fix a hit by moving the helper to ONE shared header, or by giving it a unique name.
"""
import collections
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Plugins", "MOFramework", "Source")
MODULES = ["MOFramework", "MOFrameworkCore", "MOFrameworkMedical"]


def local_names(path):
    s = open(path, encoding="utf-8", errors="ignore").read().replace("\r\n", "\n")
    out = set()
    for m in re.finditer(r"^namespace\s*\n?\{\n(.*?)^\}", s, re.S | re.M):   # anonymous namespaces only (a named one would be `namespace Foo`)
        body = m.group(1)
        for fn in re.finditer(r"^\t(?:static |FORCEINLINE |inline |constexpr )*[\w:<>,\*&\s]+?[\s\*&](\w+)\s*\([^;{]*\)\s*(?:const\s*)?\n?\t?\{", body, re.M):
            if fn.group(1) not in ("if", "for", "while", "switch"):
                out.add(fn.group(1))
        for st in re.finditer(r"^\t(?:struct|class)\s+(\w+)", body, re.M):
            out.add(st.group(1))
    return out


def main():
    bad = 0
    for mod in MODULES:
        names = collections.defaultdict(set)
        for dp, _, fs in os.walk(os.path.join(ROOT, mod)):
            for f in fs:
                if f.endswith(".cpp"):
                    p = os.path.join(dp, f)
                    for n in local_names(p):
                        names[n].add(os.path.relpath(p, os.path.join(ROOT, mod)))
        for n, files in sorted(names.items()):
            if len(files) > 1:
                bad += 1
                print("COLLISION in %s: '%s' is file-local in %s" % (mod, n, ", ".join(sorted(files))))
    print("unity collisions: %d" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
