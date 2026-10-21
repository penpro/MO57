"""Stage PART of a file's working-tree changes (a non-interactive `git add -p` that keeps insertions where they belong).

    python Tools/git_stage_part.py <path> --text <regex>   [--base REV] [--dry]   changes whose NEW text matches the regex
    python Tools/git_stage_part.py <path> --func a,b,...   [--base REV] [--dry]   changes inside these Python functions (by `def`)

The index gets <base>'s version of the file (default HEAD) plus only the chosen changes, found with difflib on lines anchored to the base:
an insertion lands where it belongs whatever else is left out. (`git diff -U0` + `git apply --cached --unidiff-zero` of some hunks places a
pure insertion by its new-side line number, which counts the hunks left out: Docs/TRAPS.md section 1.) Prints TAKE / skip per change;
read the result with `git show :<path>` before committing. LF in the index (the repo's form).
"""
import argparse, bisect, difflib, re, subprocess, sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--text", help="regex matched against each change's new text")
    group.add_argument("--func", help="comma-separated Python function names")
    ap.add_argument("--base", default="HEAD")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()

    base = subprocess.run(["git", "show", f"{a.base}:{a.path}"], capture_output=True, check=True).stdout.decode("utf-8").splitlines(keepends=True)
    work = open(a.path, encoding="utf-8", newline="").read().replace("\r\n", "\n").splitlines(keepends=True)
    defs = [(i, m.group(1)) for i, l in enumerate(work) for m in [re.match(r"def (\w+)\(", l)] if m]
    def_lines = [i for i, _ in defs]
    funcs = set(a.func.split(",")) if a.func else set()
    pattern = re.compile(a.text) if a.text else None

    def owner(j1, j2):
        new_def = next((l for l in work[j1:j2] if l.startswith("def ")), None)   # a block that adds a function belongs to it
        if new_def:
            return re.match(r"def (\w+)\(", new_def).group(1)
        k = bisect.bisect_right(def_lines, j1 if j2 > j1 else max(j1 - 1, 0)) - 1
        return defs[k][1] if k >= 0 else "<module>"

    out, taken = [], 0
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, base, work, autojunk=False).get_opcodes():
        if tag == "equal":
            out += base[i1:i2]
            continue
        new = "".join(work[j1:j2])
        where = owner(j1, j2) if funcs else ""
        take = (where in funcs) if funcs else bool(pattern.search(new))
        out += work[j1:j2] if take else base[i1:i2]
        taken += take
        print("TAKE" if take else "skip", tag, f"base line {i1 + 1}", f"work line {j1 + 1}", f"+{j2 - j1}/-{i2 - i1}", where, repr(new[:80]))
    if not taken:
        sys.exit("nothing matched: nothing staged")
    if a.dry:
        return
    sha = subprocess.run(["git", "hash-object", "-w", "--stdin", "--path", a.path], input="".join(out).encode("utf-8"),
                         capture_output=True, check=True).stdout.decode().strip()
    subprocess.run(["git", "update-index", "--cacheinfo", f"100644,{sha},{a.path}"], check=True)
    print(f"staged {taken} change(s) of {a.path} ({sha[:10]})")


if __name__ == "__main__":
    main()
