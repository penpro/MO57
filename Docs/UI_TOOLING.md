# MO57 UI Tooling — building and verifying Widget Blueprints without the Designer

**Short version:** UMG is fully scriptable. You do not need screen control, accessibility trees, or
click-by-click Designer work. A widget tree is *data*: describe it in a spec file, run
`python Tools/ue.py ui build <spec>`, and the editor creates/updates the Widget Blueprint, forces
`Is Variable` on every `BindWidget` name, compiles, saves, and re-checks the C++ contract. Verification
is the same loop: run the menu in PIE, click by widget name, read widget state back, take one screenshot.

Everything below was proven on the Host/Join panels (2026-10-05). Entry point: `python Tools/ue.py ui`.
Implementation: `Content/Python/mo_ui.py` (editor side), `Content/Python/mo_ui_contract.py` (header
parser, pure Python), `Tools/ue.py` (host side). Example specs: `Content/Python/ui_specs/coop_menu.py`.

## Why not screen control?

| Approach | Verdict |
|----------|---------|
| Screenshot → analyze → click | Works, slowest possible. Only used now for one final look. |
| Accessibility tree / UIA | Slate draws its own pixels; the editor exposes almost nothing useful to UIA. Not needed. |
| Injected key presses | Fine for typing, but unnecessary — editing happens through the Python API, runtime clicks through `MO.Test.ClickWidget`. |
| **Editor Python + in-editor MCP** | Reads/writes the exact widget tree, properties and slots. Deterministic, ~seconds per build. **This is the toolset.** |

## The loop

```bash
# 1. What does the C++ parent demand?  (parsed from the headers, never hand-kept)
python Tools/ue.py ui contract MOJoinGamePanel

# 2. Start a spec from that contract (or copy an existing one)
python Tools/ue.py ui scaffold MOJoinGamePanel /MOFramework/UI/MainMenu_UI/WBP_JoinGamePanel --out Content/Python/ui_specs/my_panel.py

# 3. Edit the spec, then build (idempotent: run it as often as you like)
python Tools/ue.py ui build Content/Python/ui_specs/coop_menu.py

# 4. Verify structurally
python Tools/ue.py ui check /MOFramework/UI/MainMenu_UI/WBP_JoinGamePanel

# 5. Verify at runtime: PIE at the main menu, click by name, look once
python Tools/ue.py ui menu
python Tools/ue.py ui click JoinGameButton
python Tools/ue.py ui find JoinStatusText          # live text + on-screen rect
python Tools/ue.py ui shot out.png                 # whole editor window via MCP
python Tools/ue.py ui stop
```

## Verbs

| Verb | Does |
|------|------|
| `contract <Class>` | BindWidget members (required/optional, type, declaring class) through the base chain |
| `scaffold <Class> <asset> [--out f]` | starter spec from the contract; unknown native widget types become `TODO:` placeholders the build refuses |
| `build <spec.py>` | create/update every asset in `SPECS`; compile; save; re-check |
| `check <asset>` | names + types + `Is Variable` vs the parent contract (`RESULT: contract satisfied`) |
| `dump <asset> [--no-props] [--out f]` | the live tree as a spec-shaped dict, non-default properties only — the way to learn property names |
| `list [folder] [parent]` | Widget Blueprints and their native parents |
| `compile <asset>` | compile + save |
| `remove <asset> <widget>` | delete a widget and its subtree (`MOWidgetEditorUtils.RemoveWidget`), compile, save. Use it to replace a node, then re-run `build` |
| `open <asset>` | open the Designer tab (MCP) |
| `shot [--asset A] [out.png]` | PNG of the editor window (MCP `CaptureEditorImage`, ~1280 px wide) |
| `menu` | close asset tabs, begin PIE, skip the intro, wait until the main menu is painted |
| `click <name>` / `find <name>` | `MO.Test.ClickWidget` / `MO.Test.FindWidget` with the output attributed to this call |
| `preview <asset>` / `preview --clear` | add a widget to the running PIE viewport / remove it |
| `close-tabs` | close every asset editor (PIE cannot start painting the level viewport while a tab covers it) |
| `stop` | end PIE |

## Spec format

```python
SPECS = [{
    "asset":  "/MOFramework/UI/MainMenu_UI/WBP_JoinGamePanel",      # package path
    "parent": "/Script/MOFramework.MOJoinGamePanel",                # C++ parent (only used on create)
    "root":   NODE,                                                 # optional
    "attach": [{"parent": "FocusWindowSwitcher", "node": NODE}],    # optional: nodes under EXISTING widgets
    "class_defaults": {"entry_widget_class": "/Game/.../WBP_X"},    # optional: CDO properties
}]
# NODE = {"type": "VerticalBox" | "MOButton" | "/Game/Path/WBP_X" | "/Script/Mod.Class",
#         "name": "RefreshButton",            # EXACT name: BindWidget names are contracts
#         "var": True,                        # Is Variable (forced on for every contract name)
#         "props": {"button_label": "Refresh"},   # any editor property of the widget
#         "slot": {"padding": 8, "size": {"size_rule": "Fill", "value": 1}},
#         "children": [NODE, ...],
#         # sugar: text, label, font_size, color, width, height }
```

Value coercion: `Margin` from `8` / `[h, v]` / `[l, t, r, b]`; `LinearColor` from `[r,g,b(,a)]` or `"#RRGGBB(AA)"`;
enums by friendly name (`"Fill"`, `"HitTestInvisible"`); structs as dicts (only the keys given change);
class and asset references as package paths. Aliases: `MOButton`, `MOListEntry`, `MOScrollList`.

## Rules that keep it fast

1. **The C++ header is the source of truth.** Add/rename a `BindWidget` member in C++, then re-run
   `ui contract` / `ui check`; the contract is parsed, not remembered.
2. **Specs are idempotent and additive.** `build` creates what is missing and updates what exists.
3. **Look once, at the end.** `check` + `find` prove structure and state; one `shot` proves it looks right.
4. **Use `dump` to discover property names** instead of guessing — then paste them into the spec.

## Limits (found by use)

- **Reparent / reorder.** `ui remove` deletes a node, but nothing moves a widget between parents or reorders
  siblings. `build` appends new children last, so to put a node *before* an existing sibling, remove the sibling
  too and let `build` re-create both in spec order. (z-order inside an Overlay = child order, last is topmost.)
- **Never use a native UserWidget class as a node** (`MOCommonButton`, any C++ `UCommonButtonBase`/`UUserWidget`
  subclass). It has no widget tree: it renders as an empty, hit-test-invisible spacer, so real mouse clicks pass
  through it, while `SimulateClick` and bound delegates still "work". Use a Widget Blueprint (`MOButton` for
  buttons). `build` refuses it, `check` flags it, and `Tools/tests/test_ui_tooling.py` fails a spec that has one.
- **Simulated clicks are not real clicks.** `ui click` (`MO.Test.ClickWidget`) calls `SimulateClick` on MO buttons,
  skipping Slate hit-testing, and its pointer path does not click CommonUI buttons at all. Anything that must work
  with a mouse needs one real click, e.g. through computer-use on a `ue.py inst` window (see
  `Docs/AUTONOMOUS_TOOLING.md`).
- **PIE must be painted for runtime queries.** `FindWidget` reads painted geometry. With an asset-editor tab in
  front of the level viewport, PIE draws nothing, every widget reports zero size and `find` matches nothing.
  `ui menu` closes the tabs first; if PIE was already running behind a tab: `ui stop` → `ui menu`.
- **`ui shot` uses the MCP capture** (`CaptureEditorImage`, ~1280 px wide, whole editor). `HighResShot` is black
  when the viewport is hidden; do not use it for UI.
- **Git Bash path mangling.** `/MOFramework/...` becomes `C:/Program Files/Git/MOFramework/...`. The tool repairs
  it; `MSYS_NO_PATHCONV=1` avoids it entirely.
- **Asset editors cannot close during PIE** ("The Editor is currently in a play mode") — end PIE first.
- **The in-editor MCP has no UMG toolset** and `BlueprintTools.compile_blueprint` returns null; errors are only
  in the log. `build` compiles via Python and reports problems.
- **Editor must be OPEN** (bridge + MCP). C++ changes still need the editor closed to build.
- **SizeBox overrides need their setter (fixed 2026-10-09).** `width`/`height` (and min/max desired, aspect) were written with `set_editor_property`, which stores the VALUE
  but leaves the `bOverride_*` enable flag off -- the dump showed `height_override: 100.0` while the packaged game laid the box out at its content size (the bug report
  form's multi-line boxes were one line high and its preview ran off the screen). `set_props` now also calls the SizeBox setter (`set_height_override` ...), which turns the
  flag on (verified: the same form, repackaged, has 100 px / 70 px multi-line boxes, a 110 px scrolling preview and 64 px buttons). **Specs built before this fix probably have inert
  SizeBox sizes too** (not checked): `coop_menu.py`'s 800x800 panel footprints and 72 px button rows, `load_host.py`, `save_slot_layout.py` and `bug_report_button.py` -- re-running
  `build` would apply the sizes that were written down, which can change how those panels look; check each with a screenshot before keeping it.
- **`ui preview` cannot create the widget in 5.8** (`WidgetBlueprintLibrary::Create` is not exposed to Python, so `ui find` on a previewed panel finds nothing). Look at a new panel in a
  packaged build or a `ue.py inst` window, with a real click to open it, until a console verb exists for it.
- **A bare native class is fine as the PARENT of a new Widget Blueprint** (`parent` in a spec); only a native class used as a NODE in a tree is the empty-spacer trap
  (`resolve_class(..., as_parent=True)`).

## Verified end to end (2026-10-05)

`build` created `WBP_SessionListEntry`, `WBP_SessionList`, `WBP_HostGamePanel`, built out `WBP_JoinGamePanel`,
and attached both panels to `WBP_MOInGameMenu1` in one run; `check` reports `contract satisfied` for all five.
In PIE: Join opens, searches (`No sessions found.`), Host opens, **Host creates the session and travels as a
listen server with a spawned pawn** (Null OSS); populated session rows render (injected via
`MOSessionListWidget::SetSessionResults`).

## Load panel "Host" button (2026-10-07) -- a second end-to-end example, and what it taught

`Content/Python/ui_specs/load_host.py` adds `HostButton` (an `MOButton`) to the existing `WBP_MOSaveSlot` next to Rename/Delete
(attach to `VerticalBox_454`) and a collapsed `StatusText` to `WBP_LoadPanel` (attach to `VerticalBox_0`). `ui dump <asset>` first
to find the container names. Verified with a REAL mouse click (computer-use) on the Host button of a test save in a `ue.py inst`
window: the click reached `UMOMainMenuWidget`, `StartHosting` resumed the save, a session was created, the listen server loaded it.

Pitfalls this run hit:
- **Hard-coded switcher indices rot.** `UMOMainMenuWidget` had Load=2/Options=3 while the asset's `FocusWindowSwitcher` order was
  Options=2/Load=3, so the Load button opened the Options panel. Every panel is now resolved from the widget
  (`ResolvePanelIndex`). Rule: never address a child of a designer-ordered container by number.
- **`MO.Test.ClickWidget <substring>` clicks the first visible MO button that matches, and hidden WidgetSwitcher children count
  as visible.** `HostButton` also matched the Host Game panel's own button and hosted a *fresh* world; the later "session
  created" PASS lines were from that wrong click. Use a real click at coordinates from `MO.Test.FindWidget`, and find the row by
  its text first (the Load list shows the player's real saves: never click a row you did not create).
- A screenshot beats a log line: `FindWidget 'Back'` returning 0 was the clue; one screenshot showed the Options panel.
