"""Claude sequence runner: tick-driven multi-frame test sequences for the bridge.

WHY THIS EXISTS
---------------
The core bridge (claude_bridge.py) executes each submitted command synchronously
inside one slate tick. That is fine for one-shot calls, but UI tests need work
spread across FRAMES — CommonUI defers widget construction/activation a tick, so
"push a modal, then focus its text box, then inject a key, then assert" must
happen on successive frames. Before this, that meant orchestrating from outside
with `Add-Content; Start-Sleep; Add-Content; ...` — slow, brittle, and the state
between steps lived in module globals.

This module lets a whole sequence live in ONE python file and advance one step
per frame, in-process. You write a generator; each `yield` = "let a frame pass."

SAFETY
------
Registers its OWN slate post-tick callback, independent of the core bridge. If
this file has a bug it does not touch command polling — the core bridge keeps
working and you can fix and reload this. Idempotent: re-running replaces the
prior callback. All stepping is wrapped in try/except; a failing sequence logs
[seq] FAILED and is dropped, never crashing the tick.

USAGE
-----
Write a test file that defines `sequence(ctx)` as a GENERATOR:

    # mytest.py
    def sequence(ctx):
        ctx.out("opening dialog")
        dlg = ctx.push_modal(ctx.find_class("MOConfirmationDialog"))
        yield 1                      # let CommonUI construct/activate it
        dlg.set_keyboard_focus()
        yield                        # 1 tick (bare yield == yield 1)
        ctx.assert_true("focused", dlg.has_keyboard_focus())
        ctx.simulate_escape()
        yield 1
        ctx.assert_true("closed", not dlg.is_activated())

Then run it through the core bridge:

    py:import claude_seq; claude_seq.run_file(r"C:\\path\\mytest.py")

Results stream to ue_out.txt with a [seq:<name>] prefix, exactly like the bridge,
so you read them the same way (Get-Content ue_out.txt -Tail N). The final line is
[seq:<name>] DONE pass=<n> fail=<n>  (or FAILED <traceback>).

ctx API
-------
  ctx.out(msg)                      timestamped log line, [seq:<name>] prefixed
  ctx.world / ctx.game / ctx.editor live-resolved each access (fresh post-travel)
  ctx.pc / ctx.pawn                 player controller / pawn (or None)
  ctx.atl                           the agent_test_lib module
  ctx.find_class("MOFoo")           unreal.MOFoo, or load by /Script path fallback
  ctx.all_widgets(cls)              get_all_widgets_of_class(world, cls, False)
  ctx.push_modal(cls)               UMOGameUIManagerSubsystem.push_modal_widget
  ctx.test_subsystem()              MOUITestSubsystem (world or game-instance)
  ctx.simulate_escape() / _tab() / _key(FKey)   via the test subsystem
  ctx.assert_true(label, cond)      record + log PASS/FAIL, returns cond
  ctx.assert_eq(label, a, b)        record + log PASS/FAIL
  ctx.guard(label, cond)            like assert_true but ABORTS the sequence if
                                    false — use before injecting Escape so a
                                    failed focus claim can't end PIE

NOTE: dev-machine tooling only, never ship. Executes arbitrary local files.
"""
import os
import tempfile
import time
import traceback

import unreal

BRIDGE_DIR = os.path.abspath(os.environ.get(
    "MO57_BRIDGE_DIR", os.path.join(tempfile.gettempdir(), "claude")))
OUT_PATH = os.path.join(BRIDGE_DIR, "ue_out.txt")


def _append(msg):
    try:
        os.makedirs(BRIDGE_DIR, exist_ok=True)
        with open(OUT_PATH, "a", encoding="utf-8") as f:
            f.write(f"[{time.strftime('%H:%M:%S')}] {msg}\n")
    except Exception:
        pass


def _worlds():
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = ues.get_game_world()
    editor = ues.get_editor_world()
    return game, editor


class _Ctx:
    """Live handle passed to a sequence generator. Most accessors re-resolve
    on every use so they stay valid across level travel / PIE start."""

    def __init__(self, name):
        self.name = name
        self.passed = 0
        self.failed = 0
        self.aborted = False
        try:
            import agent_test_lib as _atl
            self.atl = _atl
        except Exception:
            self.atl = None

    # --- world / actors -----------------------------------------------------
    @property
    def game(self):
        return _worlds()[0]

    @property
    def editor(self):
        return _worlds()[1]

    @property
    def world(self):
        g, e = _worlds()
        return g if g else e

    @property
    def pc(self):
        return unreal.GameplayStatics.get_player_controller(self.world, 0)

    @property
    def pawn(self):
        pc = self.pc
        return pc.get_controlled_pawn() if pc else None

    # --- lookups ------------------------------------------------------------
    def find_class(self, name):
        cls = getattr(unreal, name, None)
        if cls is not None:
            return cls
        # fallback: load by script path
        return unreal.load_class(None, "/Script/MOFramework." + name)

    def all_widgets(self, cls):
        if isinstance(cls, str):
            cls = self.find_class(cls)
        return unreal.WidgetLibrary.get_all_widgets_of_class(self.world, cls, False)

    def test_subsystem(self):
        ws = None
        if self.atl:
            try:
                ws = self.atl.world_subsystem(self.world, "MOUITestSubsystem")
            except Exception:
                ws = None
            if ws:
                return ws
            try:
                return self.atl.gi_subsystem(self.world, "MOUITestSubsystem")
            except Exception:
                return None
        return None

    # --- UI helpers ---------------------------------------------------------
    def push_modal(self, cls):
        if isinstance(cls, str):
            cls = self.find_class(cls)
        return unreal.MOGameUIManagerSubsystem.push_modal_widget(self.world, cls)

    def simulate_escape(self):
        ts = self.test_subsystem()
        if ts:
            ts.simulate_escape()

    def simulate_tab(self):
        ts = self.test_subsystem()
        if ts:
            ts.simulate_tab()

    def simulate_key(self, key):
        ts = self.test_subsystem()
        if ts:
            ts.simulate_key_press(key)

    # --- assertions ---------------------------------------------------------
    def out(self, msg):
        _append(f"[seq:{self.name}] {msg}")

    def assert_true(self, label, cond):
        if cond:
            self.passed += 1
            self.out(f"PASS {label}")
        else:
            self.failed += 1
            self.out(f"FAIL {label}")
        return bool(cond)

    def assert_eq(self, label, a, b):
        return self.assert_true(f"{label} ({a!r} == {b!r})", a == b)

    def guard(self, label, cond):
        if not cond:
            self.aborted = True
            self.out(f"GUARD-ABORT {label} (precondition false — sequence aborted)")
        return bool(cond)


# active sequences: list of dicts {gen, ctx, wait}
_seqs = []


def run_file(path, name=None):
    """Load a python file defining sequence(ctx) and start driving it tick-by-tick."""
    if name is None:
        name = os.path.splitext(os.path.basename(path))[0]
    try:
        with open(path, "r", encoding="utf-8-sig") as f:
            src = f.read()
        ns = {"unreal": unreal}
        exec(compile(src, path, "exec"), ns)
        fn = ns.get("sequence")
        if fn is None:
            _append(f"[seq:{name}] ERROR: file defines no sequence(ctx) function")
            return
        ctx = _Ctx(name)
        gen = fn(ctx)
        if not hasattr(gen, "__next__"):
            # not a generator — ran to completion synchronously
            _append(f"[seq:{name}] DONE (non-generator) pass={ctx.passed} fail={ctx.failed}")
            return
        # drop any prior run of the same name
        global _seqs
        _seqs = [s for s in _seqs if s["ctx"].name != name]
        _seqs.append({"gen": gen, "ctx": ctx, "wait": 0})
        _append(f"[seq:{name}] STARTED")
    except Exception:
        _append(f"[seq:{name}] FAILED to load\n{traceback.format_exc()}")


def _finalize(entry, failed_exc=None):
    ctx = entry["ctx"]
    if failed_exc:
        ctx.out(f"FAILED\n{failed_exc}")
    elif ctx.aborted:
        ctx.out(f"ABORTED pass={ctx.passed} fail={ctx.failed}")
    else:
        ctx.out(f"DONE pass={ctx.passed} fail={ctx.failed}")


def _tick(dt):
    if not _seqs:
        return
    still = []
    for entry in _seqs:
        ctx = entry["ctx"]
        if ctx.aborted:
            _finalize(entry)
            continue
        if entry["wait"] > 0:
            entry["wait"] -= 1
            still.append(entry)
            continue
        try:
            requested = next(entry["gen"])
            # a yielded int N means "wait N more ticks"
            entry["wait"] = int(requested) - 1 if isinstance(requested, int) and requested > 0 else 0
            still.append(entry)
        except StopIteration:
            _finalize(entry)
        except Exception:
            _finalize(entry, traceback.format_exc())
    _seqs[:] = still


# Idempotent registration on its own handle — independent of the core bridge.
if hasattr(unreal, "_claude_seq_handle"):
    try:
        unreal.unregister_slate_post_tick_callback(unreal._claude_seq_handle)
    except Exception:
        pass

unreal._claude_seq_handle = unreal.register_slate_post_tick_callback(_tick)
_append("[seq] runner registered")
unreal.log("[claude_seq] sequence runner registered")
