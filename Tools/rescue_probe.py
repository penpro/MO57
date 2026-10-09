"""Diagnostic: how does the fall-through rescue behave for a host pawn buried at different depths, with the OLD first-hit ground trace (MO.Rescue.FirstHitOnly 1) and the fixed one?

One editor-binary -game instance hosts a fresh world on the real map. For each depth and each rule: put the host's pawn `depth` cm under the voxel surface, log what the rescue's traces
see (MO.Test.RescueTrace), wait, and record where the pawn ended up and what the rescue logged. Prints a table; stops the instance when done.

    python Tools/rescue_probe.py [depth_cm ...]
    python Tools/rescue_probe.py --hunt N       # host N fresh worlds in a row; report every host pawn that ends up in a fall-through loop, with what the rescue's traces see
"""
import re
import sys
import time

sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
import ue_inst as ui  # noqa: E402

UPROJECT = r"D:\UEProjects\MO57\MO57.uproject"
EDITOR = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
NAME = "rescueprobe"


def pawn(idx=0):
    return ui._pawn_xyz(NAME, idx)


def hunt(rounds):
    """Host `rounds` fresh random-seed worlds with ONE process (menu -> host -> menu ...). For each: the spawn line, the host pawn's z against the real ground, how many fall-through
    rescues fired in the first 40 s, and (when it looped) what the sky-line traces hit."""
    ui.start(NAME, UPROJECT, EDITOR, pos=(0, 0), nosteam=True)
    try:
        if not ui.wait_ready(NAME, 300):
            print("[hunt] instance never became ready")
            return 1
        for r in range(1, rounds + 1):
            ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
            time.sleep(2)
            off = ui.log_size(NAME)
            ui.console(NAME, f"MO.Session.Host Hunt{r} 2")
            reached, st = ui._wait_state(NAME, "ListenServer", "", 300)
            time.sleep(40)
            log = ui.read_log_from(NAME, off)
            spawn = re.search(r"Spawning initial pawn at: (X=\S+ Y=\S+ Z=\S+)", log)
            seed = re.search(r"host published world seed (-?\d+)", log)
            falls = len(re.findall(r"Fall-through detected", log))
            p = pawn()
            ground = ui._surface_z(NAME, p[0], p[1], 30000, -30000) if p else None
            gz = ground[1] if ground else None
            settle = re.search(r"Spawn settle: (ground stable[^\n]*|no stable ground[^\n]*)", log)
            print(f"[hunt] round {r}: ready={reached} seed={seed.group(1) if seed else '?'} spawn={spawn.group(1) if spawn else '?'} pawn_z={p[2] if p else None} ground_z={gz} "
                  f"standing_height={(p[2] - gz) if (p and gz is not None) else None} rescues_in_40s={falls}", flush=True)
            print(f"[hunt]    settle: {settle.group(1)[:170] if settle else '(none logged)'}", flush=True)
            if falls or (p and gz is not None and p[2] < gz - 50):
                mark = ui.log_size(NAME)
                ui.console(NAME, "MO.Test.RescueTrace 0")
                time.sleep(1)
                tail = ui.read_log_from(NAME, mark)
                for ln in tail.splitlines():
                    if "RescueTrace" in ln:
                        print("    ", re.sub(r"^.*\[MOTEST\] ", "", ln)[:240], flush=True)
                for ln in log.splitlines():
                    if "rescue found no ground" in ln or "Fall-through detected" in ln:
                        print("    LOG", re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", ln)[:300], flush=True)
                        falls -= 1
                        if falls < -3:
                            break
            # back to the menu for the next fresh world
            ui.call(NAME, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{ui.MENU_MAP}")'], timeout=30)
            ui._wait_state(NAME, "Standalone", ui.MENU_MAP, 120)
        return 0
    finally:
        ui.stop(NAME)


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--hunt":
        return hunt(int(sys.argv[2]))
    depths = [float(a) for a in sys.argv[1:]] or [146.0, 600.0, 1500.0, 4000.0]
    ui.start(NAME, UPROJECT, EDITOR, pos=(0, 0), nosteam=True)
    try:
        if not ui.wait_ready(NAME, 300):
            print("[rescue] instance never became ready")
            return 1
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(2)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.Session.Host RescueProbe 2")
        reached, st = ui._wait_state(NAME, "ListenServer", "", 300)
        print("[rescue] hosted:", reached, st, flush=True)
        time.sleep(20)
        p = pawn()
        print("[rescue] host pawn:", p, flush=True)
        if not p:
            return 1
        x, y, z = p
        ground = ui._surface_z(NAME, x, y, 30000, -30000)
        print("[rescue] ground at the pawn:", ground, flush=True)
        gz = ground[1]
        print(f"{'rule':8} {'depth':>6} {'pawn z after':>13} {'rescue line':60}")
        for depth in depths:
            for rule, label in ((1, "OLD"), (0, "NEW")):
                ui.console(NAME, f"MO.Rescue.FirstHitOnly {rule}")
                ui._probe(NAME, f"p = unreal.GameplayStatics.get_player_pawn(world, 0); p.set_actor_location(unreal.Vector({x}, {y}, {gz - depth}), False, True); "
                                "p.get_character_movement().stop_movement_immediately(); out('BURIED')")
                mark = ui.log_size(NAME)
                time.sleep(1.0)
                ui.console(NAME, "MO.Test.RescueTrace 0")
                time.sleep(14)
                log = ui.read_log_from(NAME, mark)
                trace = [re.sub(r"^.*\[MOTEST\] RescueTrace ", "    trace: ", ln)[:200] for ln in log.splitlines() if "[MOTEST] RescueTrace" in ln]
                rescue = [re.sub(r"^.*Fall-through detected! ", "", ln)[:70] for ln in log.splitlines() if "Fall-through detected" in ln]
                after = pawn()
                print(f"{label:8} {depth:6.0f} {after[2] if after else None!s:>13} {(rescue[0] if rescue else '(no rescue line)'):60} x{len(rescue)}", flush=True)
                for t in trace:
                    print(t, flush=True)
                # put it back on the ground for the next case
                ui.console(NAME, "MO.Rescue.FirstHitOnly 0")
                ui._probe(NAME, f"p = unreal.GameplayStatics.get_player_pawn(world, 0); p.set_actor_location(unreal.Vector({x}, {y}, {gz + 200.0}), False, True); out('RESET')")
                time.sleep(4)
        return 0
    finally:
        ui.stop(NAME)


if __name__ == "__main__":
    sys.exit(main())
