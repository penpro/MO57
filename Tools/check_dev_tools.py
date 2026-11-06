"""Wes's filming tools in a real new game (Nov 6 2026): the early access notice, the console's Menu button, the spawn menu's AI switches, first person, flying, cinema and the floating island POI.

    python Tools/check_dev_tools.py [--seed 777] [--keep] [--shots DIR] [--only notice,menu,ai,firstperson,fly,cinema,island]

Each has a control that can fail: AI on again runs the same brain; the camera is far from the head out of first person; a double jump does not fly before it is allowed;
a stopped take says stopped, a finished one finished. How it all LOOKS goes to Wes (the shots). Prints each step and "DEV TOOLS PASS" / "FAIL". The editor must be closed.
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "devtools"
SHOTS = None
FAILS = []
ME = "p = unreal.GameplayStatics.get_player_pawn(world, 0); pc = unreal.GameplayStatics.get_player_controller(world, 0); "
started = time.time()


def check(what, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + what + (("  -- " + str(detail)[:210]) if detail != "" else ""), flush=True)
    if not ok:
        FAILS.append(what)
    return ok


def run(command, pattern, timeout=20):
    off = ui.log_size(NAME)
    ui.console(NAME, command)
    return ui.wait_log(NAME, pattern, since=off, timeout=timeout)


def q(code, tag):
    found = re.findall(tag + r" ([^\r\n]*)", ui._probe(NAME, code))
    return found[-1].strip() if found else ""


def numbers(text):
    return [float(x) for x in re.findall(r"-?\d+(?:\.\d+)?", text)]


def shot(label):
    ui.console(NAME, "shot showui")
    time.sleep(2.5)
    if not SHOTS:
        return
    fresh = [s for s in glob.glob(r"D:\UEProjects\MO57\Saved\Screenshots\**\*.png", recursive=True) if os.path.getmtime(s) > started]
    if fresh:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(max(fresh, key=os.path.getmtime), os.path.join(SHOTS, label + ".png"))


def menu_open():
    return q("ui2 = unreal.GameplayStatics.get_player_controller(world, 0).get_component_by_class(unreal.MOUIManagerComponent); out('MENU %s' % ui2.is_any_menu_open())", "MENU") == "True"


def hit(name):
    m = run(f"MO.Test.HitTest {name}", r"\[MOQUERY\] HITTEST[^\r\n]*", timeout=8)
    return m.group(0) if m else "no hit test line"


def esc():
    """Close the open menu the way a player does. (MO.UI.Close does not reach a modal popup.)"""
    ui.console(NAME, "MO.Test.PressKey Escape")
    time.sleep(0.7)


def hidden():
    return "p.get_editor_property('hidden')"


def mode():
    return q(ME + "out('MODE %s' % p.get_component_by_class(unreal.CharacterMovementComponent).movement_mode)", "MODE")


def where():
    return numbers(q(ME + "o = p.get_actor_location(); out('POS %f %f %f' % (o.x, o.y, o.z))", "POS"))


def brain_state(pawn):
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.AI.DumpFreezeState")
    time.sleep(1.5)
    found = re.findall(r"\[MO\.AI\]\s+" + re.escape(pawn) + r"[^\r\n]*?Brain=(\S+)", ui.read_log_from(NAME, off))
    return found[-1] if found else "not listed"


def pawn_at(name):
    return numbers(q(f"a = [x for x in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if x.get_name() == '{name}'][0]; o = a.get_actor_location(); out('AT %f %f %f' % (o.x, o.y, o.z))", "AT"))


def jump_twice():
    """Two real presses 0.17 s apart, timed inside the game: ui.console waits half a second after each command, and MO.Delay rounds to whole seconds (13 ms apart, one held press)."""
    q("""
import time
press = 'MO.Test.InjectInput IA_MOJump 1 0 0.05'
t0 = time.time()
handle = [None]
def second(dt):
    if time.time() - t0 >= 0.17:
        unreal.unregister_slate_post_tick_callback(handle[0])
        unreal.SystemLibrary.execute_console_command(world, press)
handle[0] = unreal.register_slate_post_tick_callback(second)
unreal.SystemLibrary.execute_console_command(world, press)
out('PRESSED 1')
""", "PRESSED")
    time.sleep(1.2)


def notice():
    off = ui.log_size(NAME)
    ui.console(NAME, "MO.Test.ClickWidget BeginButton")
    m = ui.wait_log(NAME, r"\[MOUI\] early-access notice shown: [^\r\n]*", since=off, timeout=60)
    check("the early access notice comes up", bool(m), m.group(0)[:150] if m else "")
    time.sleep(40)   # the world finishes loading behind it
    shot("1_early_access_notice")
    check("Got it is really clickable", "REACHES" in hit("ConfirmButton"), hit("ConfirmButton"))
    ui.console(NAME, "MO.Test.ClickWidget ConfirmButton")
    time.sleep(1.5)
    check("Got it closes it", not menu_open())


def menu():
    ui.console(NAME, "MO.Test.PressKey Tilde")
    time.sleep(1.5)
    shot("2_console")
    check("the console's Menu button is really clickable", "REACHES" in hit("ExtraButton"), hit("ExtraButton"))
    m = run("MO.Test.ClickWidget ExtraButton", r"\[MOConsole\] picker opened: Menu \((\d+) options\)")
    check("Menu opens the menu of menus", bool(m), m.group(0)[-50:] if m else "no picker line")
    time.sleep(1)
    shot("3_menu")
    for label, title in (("Time of day...", None), ("Weather...", "Weather"), ("Go to a biome...", "Biome"), ("Go to a POI...", "Point of interest"), ("Cinema...", "Cinema"), ("Spawn...", "Spawn")):
        if title is None:
            continue
        m = run(f"MO.Test.ClickWidget text={label}", r"\[MOConsole\] picker opened: " + re.escape(title), timeout=8)
        check(f"Menu -> {label} opens {title}", bool(m))
        time.sleep(0.5)
        esc()
        if title != "Spawn":
            ui.console(NAME, "MO.Test.PressKey Tilde")
            time.sleep(1)
            run("MO.Test.ClickWidget ExtraButton", r"picker opened: Menu", timeout=8)
            time.sleep(0.5)


def ai():
    m = run("spawn", r"\[MOConsole\] picker opened: Spawn \((\d+) options\)", timeout=8)
    check("the spawn menu opens", bool(m), m.group(0)[-40:] if m else "")
    time.sleep(0.7)
    m = run("MO.Test.ClickWidget text=Predator AI: on", r"\[MOConsole\] predator AI off", timeout=8)
    check("a click on Predator AI turns it off", bool(m), m.group(0) if m else "")
    time.sleep(1)
    shot("4_spawn_menu_ai_off")
    m = run("MO.Test.ClickWidget text=Wolf", r"\[MOConsole\] spawn -> Wolf: (\S+) at", timeout=10)
    check("a click on Wolf spawns one", bool(m), m.group(0)[:90] if m else "")
    if not m:
        return
    wolf = m.group(1)
    time.sleep(1)
    a = pawn_at(wolf)
    time.sleep(12)
    b = pawn_at(wolf)
    moved = sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5 if a and b else -1
    check("a wolf with its AI off stays put", 0 <= moved < 20, f"moved {moved:.0f} cm in 12 s")
    off_state = brain_state(wolf)
    check("... its brain is stopped", off_state.lower() not in ("running", "true"), off_state)
    shot("5_wolf_ai_off")
    run("spawn predatorai 1", r"\[MOConsole\] predator AI on", timeout=8)
    time.sleep(1.5)
    on_state = brain_state(wolf)
    check("CONTROL: AI on again runs the same wolf's brain", on_state != off_state, f"{off_state} -> {on_state}")
    run("spawn preyai 0", r"\[MOConsole\] prey AI off", timeout=8)
    m = run("spawn deer", r"\[MOConsole\] spawn -> \w+: (\S+) at", timeout=10)
    if m:
        deer = m.group(1)
        time.sleep(1.5)
        state = brain_state(deer)
        check("a deer spawned with Prey AI off starts with its brain stopped", state == off_state, state)
    run("spawn preyai 1", r"\[MOConsole\] prey AI on", timeout=8)


def firstperson():
    away = ME + "c = pc.player_camera_manager.get_camera_location(); h = p.mesh.get_socket_location('head'); out('FROM %f' % c.distance(h))"
    before = numbers(q(away, "FROM"))
    check("CONTROL: over the shoulder the camera is far from the head", bool(before) and before[0] > 100, before)
    run("firstperson 1", r"\[MOConsole\] first person on", timeout=8)
    time.sleep(1.5)
    at = numbers(q(away, "FROM"))
    check("first person puts the camera at the head", bool(at) and at[0] < 40, at)
    shot("6_first_person")
    ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 1 1.5")
    time.sleep(1.8)
    shot("7_first_person_walking")
    q(ME + "r = pc.get_control_rotation(); r.pitch = -60.0; pc.set_control_rotation(r); out('LOOKED 1')", "LOOKED")
    time.sleep(0.6)
    shot("8_first_person_looking_down")
    q(ME + "r = pc.get_control_rotation(); r.pitch = 0.0; pc.set_control_rotation(r); out('LOOKED 1')", "LOOKED")
    run("firstperson 0", r"\[MOConsole\] first person off", timeout=8)
    time.sleep(1.5)
    after = numbers(q(away, "FROM"))
    check("and back: the camera goes back over the shoulder", bool(after) and after[0] > 100, after)


def fly():
    jump_twice()
    check("CONTROL: in a survival world with nothing typed, a double jump does not fly", "FLYING" not in mode().upper(), mode())
    time.sleep(2.5)
    ui.console(NAME, "MO.World.Creative 1")
    time.sleep(1)
    jump_twice()
    check("in a creative world a double jump flies, nothing typed", "FLYING" in mode().upper(), mode())
    jump_twice()
    check("... and another stops it", "FLYING" not in mode().upper(), mode())
    time.sleep(3)
    ui.console(NAME, "MO.World.Creative 0")
    time.sleep(1)
    run("fly 1", r"\[MOConsole\] fly on", timeout=8)
    time.sleep(0.5)
    check("fly flies", "FLYING" in mode().upper(), mode())
    a = where()
    ui.console(NAME, "MO.Test.InjectInput IA_MOJump 1 0 1.0")
    time.sleep(1.4)
    b = where()
    check("Space rises", bool(a and b) and b[2] - a[2] > 300, f"{b[2] - a[2]:.0f} cm in about 1 s" if a and b else "")
    q(ME + "r = pc.get_control_rotation(); r.pitch = 20.0; pc.set_control_rotation(r); out('LOOKED 1')", "LOOKED")
    c = where()
    ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 1 2.0")
    time.sleep(2.2)
    d = where()
    run_cm = sum((x - y) ** 2 for x, y in zip(c, d)) ** 0.5 if c and d else 0
    check("W flies along the view, climbing when you look up", run_cm > 2000 and d[2] > c[2], f"{run_cm:.0f} cm, {d[2] - c[2]:.0f} up")
    time.sleep(1.2)
    e = where()
    time.sleep(0.8)
    f = where()
    stopped = sum((x - y) ** 2 for x, y in zip(e, f)) ** 0.5 if e and f else -1
    check("let go and it stops", 0 <= stopped < 40, f"{stopped:.0f} cm in 0.8 s")
    shot("9_flying")
    q(ME + "r = pc.get_control_rotation(); r.pitch = 0.0; pc.set_control_rotation(r); out('LOOKED 1')", "LOOKED")
    jump_twice()
    check("a double jump now stops flying", "FLYING" not in mode().upper(), mode())
    time.sleep(0.2)
    jump_twice()
    check("... and starts it again", "FLYING" in mode().upper(), mode())


def cinema():
    run("cinema start", r"\[MOCinema\] captured start", timeout=8)
    start = where()
    ui.console(NAME, "MO.Test.InjectInput IA_MOMove 0 1 2.0")
    time.sleep(2.4)
    end = where()
    m = run("cinema end", r"\[MOCinema\] captured end", timeout=8)
    check("both ends are captured", bool(m), m.group(0)[:90] if m else "")
    ui.console(NAME, "cinema")
    time.sleep(1)
    shot("10_cinema_controls")
    esc()
    off = ui.log_size(NAME)
    ui.console(NAME, "cinema play 8")
    m = ui.wait_log(NAME, r"\[MOCinema\] play: [^\r\n]*", since=off, timeout=8)
    check("Play starts", bool(m), m.group(0) if m else "")
    time.sleep(1)
    view = q(ME + "c = pc.player_camera_manager.get_camera_location(); out('VIEW %s %f %f %f %s' % (pc.get_view_target().get_name(), c.x, c.y, c.z, " + hidden() + "))", "VIEW")
    check("the view is the cinema camera and the body is hidden", "CameraActor" in view and "True" in view, view)
    a = numbers(view.split(" ", 1)[1])[:3] if " " in view else []
    time.sleep(3)
    shot("11_cinema_mid_play")
    view2 = q(ME + "c = pc.player_camera_manager.get_camera_location(); out('VIEW %s %f %f %f' % (pc.get_view_target().get_name(), c.x, c.y, c.z))", "VIEW")
    b = numbers(view2.split(" ", 1)[1])[:3] if " " in view2 else []
    moved = sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5 if a and b else 0
    check("the camera moves on its own while the player stands still", moved > 100, f"{moved:.0f} cm in 3 s; start {start}, end {end}")
    m = ui.wait_log(NAME, r"\[MOCinema\] finished after [\d.]+ s", since=off, timeout=15)
    check("it plays to the end", bool(m), m.group(0) if m else "")
    time.sleep(1)
    view3 = q(ME + "out('VIEW %s %s' % (pc.get_view_target().get_name(), " + hidden() + "))", "VIEW")
    check("then the view and the body are back", "CameraActor" not in view3 and "False" in view3, view3)
    esc()   # the controls come back for the next take
    off = ui.log_size(NAME)
    ui.console(NAME, "cinema play 30")
    time.sleep(1.5)
    ui.console(NAME, "MO.Test.PressKey Escape")
    m = ui.wait_log(NAME, r"\[MOCinema\] stopped after [\d.]+ s", since=off, timeout=8)
    check("Esc stops a take part way (the control for 'finished')", bool(m), m.group(0) if m else "")
    time.sleep(1)
    esc()


def island():
    run("fly 0", r"\[MOConsole\] fly off", timeout=8)
    off = ui.log_size(NAME)
    ui.console(NAME, "poi FloatingIsland")
    m = ui.wait_log(NAME, r"\[MOConsole\] poi FloatingIsland: [^\r\n]*", since=off, timeout=20)
    check("the POI finds a floating island", bool(m) and "top at" in m.group(0), m.group(0)[:170] if m else "")
    if not m or "top at" not in m.group(0):
        return
    top = float(re.search(r"z=(-?\d+)", m.group(0)).group(1))
    land = ui.wait_log(NAME, r"\[MOConsole\] biome teleport landed after ([\d.]+) s on the (\S+)", since=off, timeout=45)
    check("it lands on the island's voxels, not the generator's guess", bool(land) and land.group(2) == "voxel", land.group(0) if land else "no landing line")
    time.sleep(8)
    a = where()
    check("and stands there, on top of the island", bool(a) and abs(a[2] - top) < 350, f"pawn z {a[2] if a else '?'}, island top {top}")
    shot("12_on_a_floating_island")
    q(ME + "r = pc.get_control_rotation(); r.pitch = -25.0; pc.set_control_rotation(r); out('LOOKED 1')", "LOOKED")
    time.sleep(1)
    shot("13_on_a_floating_island_looking_down")


def main():
    global SHOTS
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--shots", default=None)
    ap.add_argument("--only", default="notice,menu,ai,firstperson,fly,cinema,island")
    a = ap.parse_args()
    SHOTS = a.shots
    steps = a.only.split(",")
    try:
        ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, notice=True, res=(1920, 1080), nosteam=True)
        if not ui.wait_ready(NAME, 300):
            print("FAIL: the instance did not boot")
            return 1
        ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        off = ui.log_size(NAME)
        ui.console(NAME, "MO.NewGame %d" % a.seed)
        if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=420):
            print("FAIL: no character creator")
            return 1
        time.sleep(5)
        table = {"notice": notice, "menu": menu, "ai": ai, "firstperson": firstperson, "fly": fly, "cinema": cinema, "island": island}
        if "notice" not in steps:
            notice()
        for step in steps:
            print("---", step, flush=True)
            try:
                table[step]()
            except Exception as e:  # one tool's broken check must not hide the others
                check(f"{step} ran to the end", False, repr(e))
    finally:
        if not a.keep:
            ui.stop(NAME)
    print("DEV TOOLS " + ("FAIL: " + "; ".join(FAILS) if FAILS else "PASS"), flush=True)
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
