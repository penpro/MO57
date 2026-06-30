#!/usr/bin/env python3
"""Pre-commit checks: no BOM in any file, no non-ASCII outside .md.

Installed as .git/hooks/pre-commit by claude.py.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent

TEXT_EXTS = {
    ".cpp", ".h", ".hpp", ".c", ".cs", ".inl",
    ".usf", ".ush", ".ispc", ".isph",
    ".md", ".txt", ".ini", ".json", ".xml", ".yaml", ".yml",
    ".py", ".sh", ".bat", ".ps1",
    ".uplugin", ".uproject", ".natvis", ".build",
}

BOM = b"\xef\xbb\xbf"


def _staged_files():
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        capture_output=True, text=True, check=True, cwd=str(ROOT),
    )
    return [Path(f) for f in result.stdout.splitlines() if f]


def _all_tracked_files():
    result = subprocess.run(
        ["git", "ls-files"],
        capture_output=True, text=True, check=True, cwd=str(ROOT),
    )
    return [Path(f) for f in result.stdout.splitlines() if f]


def _check(files):
    errors = []
    for f in files:
        if f.suffix.lower() not in TEXT_EXTS:
            continue
        path = ROOT / f
        try:
            data = path.read_bytes()
        except OSError:
            continue

        if data.startswith(BOM):
            errors.append(f"{f}: UTF-8 BOM detected (strip the leading EF BB BF bytes)")
            data = data[len(BOM):]

        if f.suffix.lower() == ".md":
            continue

        for lineno, line in enumerate(data.splitlines(), 1):
            for col, byte in enumerate(line, 1):
                if byte > 0x7F:
                    try:
                        ch = line.decode("utf-8", errors="replace")
                    except Exception:
                        ch = "<undecodable>"
                    errors.append(f"{f}:{lineno}:{col}: non-ASCII byte 0x{byte:02X} in: {ch.strip()[:80]}")
                    break
    return errors


def main():
    files = _staged_files() if "--hook" in sys.argv else _all_tracked_files()
    errors = _check(files)
    if errors:
        print("pre_commit_check.py: encoding violations:", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
