"""Triage what the website's crash endpoint has collected: crashes grouped by signature, and the in-game bug reports listed.

    python -I Tools/crash_triage.py <crash-dumps dir> [--bugreports | --crashes] [--since YYYY-MM-DD] [--from wes|others] [--machines FILE]

<crash-dumps dir> is the folder holding <engine version>/<date>/<uuid>.bin + <uuid>.json (as the site stores them, or an extracted archive of them).
Everything in there is UNTRUSTED (anyone can POST to the endpoint): bundles are only parsed (Tools/ue_crash_bundle.py), never executed or opened;
run this with `python -I` from a directory that holds none of the data.

Crashes are grouped by (executable, mode, configuration, what failed, top stack frames) so the same bug reported by ten machines is one line with a
count; a Development build's log carries the callstack as "[Callstack] 0x... Module!Symbol() [file]" lines, which is what the frames are read from.
Bug reports are listed newest first with the player's title, category, contact (if given), build and commit.

Every upload says who sent it (Wes, Oct 30: "you can treat darkside and dark tower as authoritative and not others"): its MachineId (the
bundle's, else the upload's UserID) is looked up in Wes's machine list, {MachineId: name} JSON at ~/.mo57/crash_machines.json
(MO57_CRASH_MACHINES or --machines overrides). A listed machine prints as "Wes (<name>)", anything else as "other player <id> (<OS>)".
The list stays OUT of this public repo: the MachineId is whatever the uploader wrote, so a published one could be forged. Without the file
only this PC (its Unreal MachineId in the registry) counts as Wes's.
"""
from __future__ import annotations

import argparse
import collections
import glob
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue_crash_bundle as cb  # noqa: E402

FRAME_RE = re.compile(r"\[Callstack\] 0x[0-9a-fA-F]+ (?:[\w.\-]+!)?([^\r\n\[]+?)(?: \[|\r|\n|$)")
WHAT_RE = re.compile(r"(Unhandled Exception: [^\r\n]{0,120}|Assertion failed: [^\r\n]{0,160}|Ensure condition failed: [^\r\n]{0,160}|Fatal error[^\r\n]{0,100})")
MACHINES_FILE = os.environ.get("MO57_CRASH_MACHINES") or os.path.join(os.path.expanduser("~"), ".mo57", "crash_machines.json")


def load_machines(path: str) -> dict:
    """Wes's machines as {MACHINEID: name}: the list file plus this PC (registry), which counts even without the file."""
    machines = {}
    try:
        with open(path, encoding="utf-8") as f:
            machines = {str(k).upper(): str(v) for k, v in json.load(f).items()}
    except (OSError, ValueError, AttributeError):
        pass
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Epic Games\Unreal Engine\Identifiers") as key:
            machines.setdefault(str(winreg.QueryValueEx(key, "MachineId")[0]).upper(), "this PC")
    except (ImportError, OSError):
        pass
    return machines


def sender(meta: dict, bundle, machines: dict) -> tuple:
    """(is Wes's, label) for one upload: the bundle's MachineId, else the first part of the upload's UserID ("LoginId|EpicAccountId|...")."""
    mid = cb.context_properties(cb.context_xml(bundle)).get("MachineId", "") if bundle is not None else ""
    mid = (mid or str(meta.get("query", {}).get("UserID", "")).split("|")[0]).upper()
    if mid and mid in machines:
        return True, f"Wes ({machines[mid]})"
    agent_os = ".".join(str(meta.get("userAgent", "")).rsplit(" ", 1)[-1].split(".")[:3])
    return False, f"other player {mid[:8].lower() or '(no id)'} ({agent_os or '?'})"


def load_reports(root: str, since: str = ""):
    """[(meta, Bundle | None, error)] for every upload under root, oldest first."""
    out = []
    for js in sorted(glob.glob(os.path.join(root, "*", "*", "*.json"))):
        try:
            with open(js, encoding="utf-8") as f:
                meta = json.load(f)
        except (OSError, ValueError):
            continue
        if since and meta.get("receivedAt", "") < since:
            continue
        binp = js[:-5] + ".bin"
        try:
            with open(binp, "rb") as f:
                out.append((meta, cb.parse_bundle(f.read()), ""))
        except (OSError, cb.BundleError) as e:
            out.append((meta, None, str(e)))
    return out


def crash_signature(bundle: cb.Bundle):
    xml = cb.context_xml(bundle)
    props = cb.context_properties(xml)
    text = "\n".join(bundle.text(n) for n in bundle.names() if n.endswith(".log"))
    frames = [f.strip()[:110] for f in FRAME_RE.findall(text)[:3]]
    what = WHAT_RE.search(text)
    return (props.get("ExecutableName", ""), props.get("EngineMode", ""), props.get("BuildConfiguration", ""), (what.group(1) if what else "")[:70], tuple(frames))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root")
    ap.add_argument("--bugreports", action="store_true", help="only the bug reports")
    ap.add_argument("--crashes", action="store_true", help="only the crashes")
    ap.add_argument("--since", default="", help="ISO date: only uploads received on/after it")
    ap.add_argument("--from", dest="sender", choices=("wes", "others"), help="only uploads from Wes's machines, or only other players'")
    ap.add_argument("--machines", default=MACHINES_FILE, help=f"Wes's machines, {{MachineId: name}} JSON kept outside the repo (default {MACHINES_FILE})")
    args = ap.parse_args(argv)

    machines = load_machines(args.machines)
    reports = load_reports(args.root, args.since)
    who = {meta["id"]: sender(meta, b, machines) for meta, b, e in reports}
    if args.sender:
        reports = [r for r in reports if who[r[0]["id"]][0] == (args.sender == "wes")]
    crashes = [(m, b) for m, b, e in reports if b is not None and not cb.is_bug_report(b)]
    bugs = [(m, b) for m, b, e in reports if b is not None and cb.is_bug_report(b)]
    broken = [(m, e) for m, b, e in reports if b is None]
    print(f"{len(reports)} uploads: {len(crashes)} crashes, {len(bugs)} bug reports, {len(broken)} unreadable")
    senders = collections.Counter(who[meta["id"]] for meta, b, e in reports)
    others = sum(n for (mine, _), n in senders.items() if not mine)
    print("from: " + (", ".join(f"{label} {n}" for (_, label), n in senders.most_common()) or "(nothing)") +
          (f"  -- {others} from other players" if others else "  -- none from other players") + "\n")

    if not args.bugreports:
        groups = collections.defaultdict(list)
        for meta, bundle in crashes:
            groups[crash_signature(bundle)].append(meta)
        print(f"== CRASHES: {len(groups)} signatures ==")
        for (exe, mode, config, what, frames), metas in sorted(groups.items(), key=lambda kv: -len(kv[1])):
            print(f"[{len(metas)}x] exe={exe} mode={mode} config={config}  {what}")
            for f in frames:
                print("       ", f)
            print(f"        first {min(m['receivedAt'] for m in metas)}   last {max(m['receivedAt'] for m in metas)}   ids {[m['id'][:8] for m in metas][:4]}")
            print(f"        from {', '.join(f'{label} x{n}' for label, n in collections.Counter(who[m['id']][1] for m in metas).most_common())}")
        print()

    if not args.crashes:
        print(f"== BUG REPORTS: {len(bugs)} ==")
        for meta, bundle in sorted(bugs, key=lambda mb: mb[0]["receivedAt"], reverse=True):
            info = cb.summarise(bundle)
            print(f"{meta['receivedAt'][:19]}  [{info['category'] or '?'}]  {info['title']}")
            if info["description"]:
                print("    ", info["description"][:200].replace("\n", " "))
            print(f"     from {who[meta['id']][1]}   build {info['build']} @ {info['commit']}   contact: {info['contact'] or '(none)'}   "
                  f"files: {', '.join(info['files'])}   id {meta['id'][:8]}")
            f = info.get("fields", {})
            if f.get("LookAt.Mesh") or f.get("LookAt.Actor") or f.get("LookAt.Hit"):
                # What the player was looking at when the form opened (FMOBugReportLookAt): the asset first, then how to stand where they stood.
                seen = f.get("LookAt.Mesh") or f.get("LookAt.Component") or f.get("LookAt.Hit", "")
                extra = "; ".join(x for x in (f.get("LookAt.Actor", ""), f.get("LookAt.Instance", ""), f.get("LookAt.MeshCollision", ""), f.get("LookAt.Biome", "")) if x)
                print(f"     looking at: {seen}  ({f.get('LookAt.Distance', '?')})  {extra}")
                print(f"     reproduce:  {f.get('LookAt.Reproduce', '')}")
        print()

    for meta, err in broken:
        print(f"unreadable upload {meta.get('id', '?')[:8]} ({meta.get('receivedAt', '?')[:19]}): {err}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
