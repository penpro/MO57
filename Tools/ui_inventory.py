"""The UI screen inventory (BACKLOG N-120), read from the project -- read-only, no editor needed.

For every widget Blueprint (WBP_*): its folder, its native parent class (the /Script/MOFramework class named in the uasset), the kit spec
that builds it (Content/Python/ui_specs/kit_*.py, kit_recipes.py), the input mode and mouse capture it asks for and whether it picks its
own focus target (read up its C++ class chain: GetDesiredInputConfig / NativeGetDesiredFocusTarget), and who uses it (C++, other assets,
config / Python). Writes Docs/UI_Screen_Inventory.md.

    python Tools/ui_inventory.py            # writes the doc
    python Tools/ui_inventory.py --print    # prints it instead
"""
import argparse
import glob
import io
import os
import re
from datetime import date

MO = os.path.dirname(os.path.dirname(os.path.abspath(__file__))).replace("\\", "/")
CONTENT_ROOTS = (f"{MO}/Plugins/MOFramework/Content", f"{MO}/Content")
VALIDATION_PREFIXES = ("validate_", "inspect_", "fix_", "audit_")


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--print", action="store_true")
    args = ap.parse_args()

    wbps = {}
    for root in CONTENT_ROOTS:
        for p in glob.glob(f"{root}/**/WBP_*.uasset", recursive=True):
            wbps[os.path.splitext(os.path.basename(p))[0]] = p.replace("\\", "/")

    kit = {}
    for p in glob.glob(f"{MO}/Content/Python/ui_specs/kit_*.py") + [f"{MO}/Content/Python/kit_recipes.py"]:
        for name in set(re.findall(r"WBP_[A-Za-z0-9_]+", read(p))):
            kit.setdefault(name, set()).add(os.path.basename(p)[:-3])

    src = {p.replace("\\", "/"): read(p) for p in glob.glob(f"{MO}/Plugins/MOFramework/Source/**/*.*", recursive=True) if p.endswith((".h", ".cpp"))}

    # Class chain: UMyWidget -> its base, from every UCLASS declaration in the plugin.
    base_of = {}
    for t in src.values():
        for m in re.finditer(r"class\s+(?:\w+_API\s+)?U(\w+)\s*:\s*public\s+U(\w+)", t):
            base_of.setdefault(m.group(1), m.group(2))
    input_cfg = {}
    focus = set()
    for p, t in src.items():
        if not p.endswith(".cpp"):
            continue
        for m in re.finditer(r"TOptional<FUIInputConfig>\s+U(\w+)::GetDesiredInputConfig\(\)\s*const\s*\{(.{0,800}?)\n\}", t, re.S):
            modes = sorted(set(re.findall(r"ECommonInputMode::(\w+)", m.group(2))))
            caps = sorted(set(re.findall(r"EMouseCaptureMode::(\w+)", m.group(2))))
            input_cfg[m.group(1)] = ("/".join(modes) or "computed") + (f", {'/'.join(caps)}" if caps else "")
        for m in re.finditer(r"UWidget\*\s+U(\w+)::NativeGetDesiredFocusTarget\(\)", t):
            focus.add(m.group(1))

    def chain(cls):
        seen = []
        while cls and cls not in seen:
            seen.append(cls)
            cls = base_of.get(cls)
        return seen

    def effective(cls, table):
        for c in chain(cls):
            if c in table:
                return table[c], c
        return None, None

    def parent_of(path):
        data = io.open(path, "rb").read()
        if b"ObjectRedirector" in data:
            # a rename's leftover: it points at the asset that took its place
            own = "/" + os.path.splitext(os.path.basename(path))[0]
            targets = sorted({m.decode("latin-1") for m in re.findall(rb"/(?:MOFramework|Game)/[A-Za-z0-9_/]+", data)
                              if not m.decode("latin-1").endswith(own)})
            return "redirector -> " + (", ".join(t.rsplit("/", 1)[-1] for t in targets) or "?")
        names = set(m.decode("latin-1") for m in re.findall(rb"\x00(MO[A-Za-z0-9_]+)\x00", data))
        mine = sorted(n for n in names if n in base_of)
        if mine:
            # the most derived of the MO classes named in the asset
            mine.sort(key=lambda n: -len(chain(n)))
            return mine[0]
        for engine in ("CommonActivatableWidget", "CommonButtonBase", "CommonUserWidget", "UserWidget"):
            if engine.encode() in data:
                return engine
        return ""

    other_text = {p.replace("\\", "/"): read(p) for p in glob.glob(f"{MO}/Config/*.ini") + glob.glob(f"{MO}/Content/Python/**/*.py", recursive=True)
                  + glob.glob(f"{MO}/Tools/*.py") if "ui_specs" not in p and not os.path.basename(p).startswith(("kit_", "ui_inventory"))}
    asset_bytes = {}
    for root in CONTENT_ROOTS:
        for p in glob.glob(f"{root}/**/*.uasset", recursive=True) + glob.glob(f"{root}/**/*.umap", recursive=True):
            try:
                if os.path.getsize(p) < 8_000_000:
                    asset_bytes[p.replace("\\", "/")] = io.open(p, "rb").read()
            except OSError:
                pass

    rows, legacy, redirectors = [], [], []
    for name, path in sorted(wbps.items()):
        parent = parent_of(path)
        cfg, cfg_from = effective(parent, input_cfg)
        has_focus = any(c in focus for c in chain(parent))
        cpp = sorted({os.path.basename(p) for p, t in src.items() if name in t})
        scripts = sorted({os.path.basename(p) for p, t in other_text.items() if name in t})
        users = sorted({os.path.splitext(os.path.basename(p))[0] for p, b in asset_bytes.items()
                        if name.encode() in b and os.path.splitext(os.path.basename(p))[0] != name})
        folder = os.path.dirname(path).replace(f"{MO}/Plugins/MOFramework/Content", "/MOFramework").replace(f"{MO}/Content", "/Game")
        live = bool(cpp or users or name in kit or any(not s.startswith(VALIDATION_PREFIXES) for s in scripts))
        if parent.startswith("redirector"):
            redirectors.append(f"{name} ({parent[len('redirector -> '):]})")
        elif not live:
            legacy.append(name)
        rows.append([name, folder, ("U" + parent) if parent.startswith("MO") else (parent or "?"),
                     ", ".join(sorted(kit.get(name, []))) or "-",
                     (f"{cfg} (U{cfg_from})" if cfg and cfg_from != parent else (cfg or "-")),
                     "yes" if has_focus else "-",
                     ", ".join(cpp + users + scripts)[:160] or "-"])

    out = io.StringIO()
    w = out.write
    w("# UI screen inventory\n\n")
    w(f"> **Generated** by `python Tools/ui_inventory.py` on {date.today().isoformat()} (read from the project: widget Blueprints, their native\n")
    w("> parents, the kit specs, the C++ input configs). Regenerate after UI work; do not edit by hand. BACKLOG N-120; the UI architecture is\n")
    w("> `Docs/UI.md`, the kit `Docs/UI_Kit_Implementation.md`.\n")
    w("> **Columns:** *Kit spec* = the `Content/Python/ui_specs` script that builds it (\"-\" = not in the Field Ledger kit). *Input* = the\n")
    w("> `GetDesiredInputConfig` of its class or the nearest base that has one (Menu = UI input with the cursor; Game = the pawn keeps it;\n")
    w("> computed = decided at run time), and the mouse capture. *Own focus* = its class chain overrides `NativeGetDesiredFocusTarget`.\n")
    w("> *Used by* = C++ files, other assets and scripts that name it.\n\n")
    in_kit = sum(1 for r in rows if r[3] != "-")
    w(f"**{len(rows)} widget Blueprints; {in_kit} built by a kit spec, {len(rows) - in_kit} not.** ")
    w(f"Redirectors left by renames (fix up in the editor: right-click the folder > Fix Up Redirectors): {len(redirectors)} -- "
      f"{', '.join(redirectors) or 'none'}. ")
    w(f"No live reference (only validation / fix scripts name them, if anything): {len(legacy)} -- {', '.join(legacy) or 'none'}. ")
    w("Several are on Codex's UI retirement list (`Tools/validate_ui_retirement.py`, BACKLOG N-126): retire them through that, not by hand.\n\n")
    w("| WBP | Folder | Native parent | Kit spec | Input | Own focus | Used by |\n|---|---|---|---|---|---|---|\n")
    for r in rows:
        w("| " + " | ".join(c.replace("|", "/") for c in r) + " |\n")
    text = out.getvalue()
    if args.print:
        print(text)
    else:
        io.open(f"{MO}/Docs/UI_Screen_Inventory.md", "w", encoding="utf-8", newline="\n").write(text)
        print(f"wrote Docs/UI_Screen_Inventory.md: {len(rows)} WBPs, {in_kit} in the kit, {len(legacy)} with no live reference")


if __name__ == "__main__":
    main()
