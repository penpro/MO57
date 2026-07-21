"""Claude command bridge: file-driven console/python execution for autonomous testing.

Watches  %TEMP%/claude/ue_cmd.txt   (one command per line, appended)
Writes   %TEMP%/claude/ue_out.txt   (timestamped results)

Line formats:
  <console command>      e.g.  MO.Clock.SetTime 12 0      (runs on PIE world if active, else editor world)
  py:<python>            e.g.  py:out(str(world))          (exec with: unreal, world, out(msg))

Load once per editor session:  py "D:/UEProjects/MO57/Content/Python/claude_bridge.py"
(auto-loaded by init_unreal.py). Re-running this file safely replaces the old callback.

NOTE: this executes arbitrary local commands — dev-machine tooling only, never ship.
"""
import os
import tempfile
import time
import traceback

import unreal

BRIDGE_DIR = os.path.abspath(os.environ.get(
    "MO57_BRIDGE_DIR", os.path.join(tempfile.gettempdir(), "claude")))
CMD_PATH = os.path.join(BRIDGE_DIR, "ue_cmd.txt")
OUT_PATH = os.path.join(BRIDGE_DIR, "ue_out.txt")
POLL_INTERVAL = 0.25  # seconds


def _append(msg):
    try:
        os.makedirs(BRIDGE_DIR, exist_ok=True)
        with open(OUT_PATH, "a", encoding="utf-8") as f:
            f.write(f"[{time.strftime('%H:%M:%S')}] {msg}\n")
    except Exception:
        pass


def _get_worlds():
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = ues.get_game_world()
    editor = ues.get_editor_world()
    return game, editor


def _execute(line):
    game, editor = _get_worlds()
    world = game if game else editor
    if line.startswith("py:"):
        code = line[3:]
        env = {"unreal": unreal, "world": world, "game": game, "editor": editor, "out": _append}
        try:
            exec(code, env)
            _append(f"[py-ok] {code[:120]}")
        except Exception:
            _append(f"[py-err] {code[:120]}\n{traceback.format_exc()}")
    else:
        if world:
            unreal.SystemLibrary.execute_console_command(world, line)
            _append(f"[cmd-ok] {line}  (world={world.get_name()})")
        else:
            _append(f"[cmd-err] {line}  (no world)")


# Throttled slate tick
_state = {"pos": 0, "accum": 0.0}


def _tick(dt):
    _state["accum"] += dt
    if _state["accum"] < POLL_INTERVAL:
        return
    _state["accum"] = 0.0
    try:
        if not os.path.exists(CMD_PATH):
            return
        size = os.path.getsize(CMD_PATH)
        if size < _state["pos"]:
            _state["pos"] = 0  # file truncated/reset — reread from start
        if size == _state["pos"]:
            return
        with open(CMD_PATH, "r", encoding="utf-8") as f:
            f.seek(_state["pos"])
            chunk = f.read()
            _state["pos"] = f.tell()
        for raw in chunk.splitlines():
            line = raw.strip().lstrip("﻿").strip()  # PS5.1 writes BOMs on file-create
            if line and not line.startswith("#"):
                _execute(line)
    except Exception:
        _append(f"[bridge-error]\n{traceback.format_exc()}")


# Idempotent registration: stash the handle on the unreal module so re-running
# this file (py "claude_bridge.py") replaces the previous callback.
if hasattr(unreal, "_claude_bridge_handle"):
    try:
        unreal.unregister_slate_post_tick_callback(unreal._claude_bridge_handle)
    except Exception:
        pass

# Start reading at current EOF so stale commands from a previous session don't replay.
try:
    _state["pos"] = os.path.getsize(CMD_PATH) if os.path.exists(CMD_PATH) else 0
except Exception:
    _state["pos"] = 0

unreal._claude_bridge_handle = unreal.register_slate_post_tick_callback(_tick)
_append(f"[bridge] registered (cmd={CMD_PATH}, skip-to-pos={_state['pos']})")
unreal.log("[claude_bridge] registered")
