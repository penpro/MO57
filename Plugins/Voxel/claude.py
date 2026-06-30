#!/usr/bin/env python3
import difflib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent

CLAUDE_TOOLS = "Bash,Edit,Glob,Grep,Read,Write"


def _find_uproject():
    current = ROOT
    for _ in range(10):
        for f in current.glob("*.uproject"):
            return f
        if current.parent == current:
            break
        current = current.parent
    return None


def _resolve_engine_path():
    uproject = _find_uproject()
    if not uproject:
        return None, None

    project_name = uproject.stem
    try:
        engine_association = json.loads(uproject.read_text()).get("EngineAssociation", "")
    except (json.JSONDecodeError, OSError):
        return None, project_name

    # EngineAssociation is either a version string ("5.7") or a guid for source builds
    if not re.fullmatch(r"\d+\.\d+", engine_association):
        return None, project_name

    if sys.platform == "win32":
        engine_path = Path("C:/Program Files/Epic Games") / f"UE_{engine_association}"
    elif sys.platform == "darwin":
        engine_path = Path("/Users/Shared/Epic Games") / f"UE_{engine_association}"
    else:
        engine_path = Path.home() / "Epic Games" / f"UE_{engine_association}"

    return engine_path, project_name


ENGINE_PATH, PROJECT_NAME = _resolve_engine_path()


def _get_git_files():
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
        capture_output=True, text=True, check=True, cwd=str(ROOT)
    )
    return [Path(f) for f in result.stdout.strip().split("\n") if f]


def _format_tree(files):
    tree = {}
    for filepath in files:
        parts = filepath.parts
        current = tree
        for part in parts[:-1]:
            current = current.setdefault(part, {})
        current.setdefault("_files", []).append(filepath)

    lines = []

    def _walk(node, indent=""):
        for dirname in sorted(k for k in node if k != "_files"):
            lines.append(f"{indent}{dirname}/")
            _walk(node[dirname], indent + "\t")
        by_stem = {}
        for fp in node.get("_files", []):
            by_stem.setdefault(fp.stem, []).append(fp.suffix)
        for stem in sorted(by_stem):
            exts = sorted(by_stem[stem])
            lines.append(f"{indent}{stem}{' '.join(exts)}" if len(exts) > 1 else f"{indent}{stem}{exts[0]}")

    _walk(tree)
    return "\n".join(lines)


def generate_overview():
    files = _get_git_files()
    tree = _format_tree(files) if files else ""
    overview = f"Engine: {ENGINE_PATH}\nProject: {PROJECT_NAME}\n\n{tree}\n"
    new_content = f"## Project Overview\n\n```\n{overview}```\n"
    output = ROOT / "CLAUDE.overview.generated.md"
    old_content = output.read_text() if output.exists() else ""
    if old_content == new_content:
        return
    output.write_text(new_content)
    print(f"Generated CLAUDE.overview.generated.md ({output.stat().st_size} bytes)")
    if old_content:
        diff_lines = list(difflib.unified_diff(
            old_content.splitlines(), new_content.splitlines(),
            lineterm="", n=1
        ))
        changes = [line for line in diff_lines[2:] if line.startswith("+") or line.startswith("-")]
        for line in changes:
            print(f"  {line}")


def has_pending_changes():
    result = subprocess.run(
        ["git", "status", "--porcelain"],
        capture_output=True, text=True, cwd=str(ROOT)
    )
    return result.returncode != 0 or bool(result.stdout.strip())


def git_sync():
    if has_pending_changes():
        print("Skipping git sync: pending changes detected")
        return

    result = subprocess.run(
        ["git", "pull", "--no-rebase"],
        capture_output=True, text=True, cwd=str(ROOT)
    )
    if result.returncode != 0:
        stderr = result.stderr.strip()
        if "CONFLICT" in stderr or "CONFLICT" in result.stdout:
            print(f"ERROR: Merge conflicts detected. Resolve manually.")
            sys.exit(1)
        print(f"WARNING: git pull failed: {stderr}")
        return

    pull_output = result.stdout.strip()
    if pull_output and pull_output != "Already up to date.":
        print(f"git pull: {pull_output}")

    result = subprocess.run(
        ["git", "push"],
        capture_output=True, text=True, cwd=str(ROOT)
    )
    if result.returncode == 0:
        push_output = result.stderr.strip()
        if push_output and "Everything up-to-date" not in push_output:
            print(f"git push: {push_output}")
    else:
        print(f"WARNING: git push failed: {result.stderr.strip()}")


def install_git_hooks():
    hooks_dir = ROOT / ".git" / "hooks"

    post_commit = hooks_dir / "post-commit"
    post_commit_content = "#!/bin/sh\ngit push 2>/dev/null &\n"
    if not post_commit.exists() or post_commit.read_text() != post_commit_content:
        post_commit.write_text(post_commit_content)
        post_commit.chmod(0o755)
        print("Installed post-commit hook (git push)")

    pre_commit = hooks_dir / "pre-commit"
    pre_commit_content = f"#!/bin/sh\nexec python \"{ROOT / 'pre_commit_check.py'}\" --hook\n"
    if not pre_commit.exists() or pre_commit.read_text() != pre_commit_content:
        pre_commit.write_text(pre_commit_content)
        pre_commit.chmod(0o755)
        print("Installed pre-commit hook (pre_commit_check.py)")


def main():
    install_git_hooks()
    git_sync()
    generate_overview()
    env = os.environ.copy()
    env["CLAUDE_CODE_DISABLE_AUTO_MEMORY"] = "1"

    args = sys.argv[1:]

    cmd = ["claude"]
    cmd += args
    cmd += ["--tools", CLAUDE_TOOLS]
    cmd += ["--dangerously-skip-permissions"]
    sys.exit(subprocess.call(cmd, env=env))


if __name__ == "__main__":
    main()
