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

# Uncomment to verify Python is working on editor startup:
# log("Python scripting initialized")
