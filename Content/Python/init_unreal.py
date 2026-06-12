# MO57 Unreal Python Scripts
# This file runs automatically when the editor starts (if named init_unreal.py)
#
# To run scripts manually in the editor:
#   1. Window -> Developer Tools -> Output Log
#   2. Type: py "Content/Python/your_script.py"
#
# Or use the Python console:
#   Window -> Developer Tools -> Python Console

import unreal

def log(msg):
    """Helper to log to UE output"""
    unreal.log(f"[MO57] {msg}")

# Auto-load the Claude command bridge (file-driven console/python execution for
# autonomous test sessions — see Docs/Agent_PIE_Testing.md). Safe no-op cost when
# unused: one file-stat per 0.25s.
try:
    import claude_bridge  # noqa: F401  (registration happens at import)
    log("claude_bridge loaded")
except Exception as bridge_err:
    log(f"claude_bridge failed to load: {bridge_err}")
