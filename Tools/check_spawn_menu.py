"""The dev console's `spawn` command in a real game, on a host (N-353).

    python Tools/check_spawn_menu.py [--seed 670711536] [--keep] [--shots DIR]

  Every species on the menu spawns on the ground in front of the player; `spawn wolf 3` makes three side by side; a survivor of one base wears that
  base; the menu opens with every entry. CONTROL: a name that is not on the menu spawns nothing, so the "spawned" lines can fail.

Prints each step and "SPAWN MENU PASS" / "FAIL". The editor must be closed.
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_save_roundtrip as base  # noqa: E402
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "spawnmenu"
base.NAME = NAME   # the helpers there read the instance's name from their own module

SPECIES = ["Bear", "Boar", "Chicken", "Cow", "Deer", "Fox", "Goat", "Pig", "Rabbit", "Sheep", "Wolf"]
FAILS = []


def check(ok, what):
    print(("PASS " if ok else "FAIL ") + what, flush=True)
    if not ok:
        FAILS.append(what)


def run(command, pattern, timeout=25):
    off = ui.log_size(NAME)
    ui.console(NAME, command)
    m = ui.wait_log(NAME, pattern, since=off, timeout=timeout)
    return m, ui.read_log_from(NAME, off)


def spawn(what, count=1):
    m, log = run(f"spawn {what} {count}", rf"(?i)\[MOConsole\] spawn -> {re.escape(what)}: (\d+) of (\d+) spawned[^\r\n]*")
    made = int(m.group(1)) if m else 0
    distances = [float(d) for d in re.findall(r"(?i)\[MOConsole\] spawn -> " + re.escape(what) + r": \S+ at [^\r\n]*?, (\d+) cm from", log)]
    return made, distances, (m.group(0) if m else "no result line")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=670711536)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--shots", default=None, help="copy the menu's screenshot here")
    a = ap.parse_args()

    rep = ui.Report()
    start = time.time()
    ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, nosteam=True, res=(1920, 1080))
    try:
        ui.wait_ready(NAME, 300)
        base.host_fresh_world(rep, a.seed)
        time.sleep(30)

        for species in SPECIES:
            made, distances, line = spawn(species)
            check(made == 1 and bool(distances) and 400 <= distances[0] <= 1200, f"{species}: {line[-60:]}, {distances} cm away")

        made, distances, line = spawn("wolf", 3)
        check(made == 3 and len(distances) == 3, f"three wolves in one command: {line[-60:]}, {distances} cm away")

        m, _ = run("spawn NotAnAnimal", r"\[MOConsole\] spawn -> NotAnAnimal: not on the menu")
        check(bool(m), "CONTROL: a name not on the menu spawns nothing")

        made, _, line = spawn("Survivor_Zuri")
        check(made == 1, f"a survivor of one base: {line[-60:]}")
        time.sleep(4)
        _, log = run("MO.Test.BodyShape", r"\[MOQUERY\] BODYSHAPE [^\r\n]*", timeout=10)
        time.sleep(1)
        check("base 'Zuri'" in log, "the survivor wears the Zuri base")

        m, _ = run("spawn", r"\[MOConsole\] picker opened: Spawn \((\d+) options\)", timeout=15)
        options = int(m.group(1)) if m else 0
        check(options >= len(SPECIES) + 1, f"the menu opens with {options} entries")
        time.sleep(1.5)
        ui.console(NAME, "shot showui")
        time.sleep(3)
        shots = sorted(glob.glob(r"D:\UEProjects\MO57\Saved\Screenshots\**\*.png", recursive=True), key=os.path.getmtime)
        fresh = [s for s in shots if os.path.getmtime(s) > start]
        print("menu shot:", fresh[-1] if fresh else "none", flush=True)
        if fresh and a.shots:
            os.makedirs(a.shots, exist_ok=True)
            shutil.copy(fresh[-1], os.path.join(a.shots, "spawn_menu.png"))
        ui.console(NAME, "MO.UI.Close")
    finally:
        if not a.keep:
            ui.stop(NAME)
    print("SPAWN MENU " + ("FAIL: " + "; ".join(FAILS) if FAILS else "PASS"), flush=True)
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()
