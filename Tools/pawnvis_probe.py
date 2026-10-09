"""Diagnostic: does each machine see the OTHER player's pawn? (Wes: "the pawn on the client can't see the pawn controlled by the host, but the host can see the one
controlled by the client".)

Starts host + client (editor-binary -game, real gameplay map), joins them, then asks BOTH machines the same questions about every MOCharacter they have: does it exist
here, where, how far from the local pawn, hidden flags, every skeletal mesh component's visibility + asset, roles, controller. Leaves both instances RUNNING so a window can be
looked at (computer-use on UnrealEditor.exe); stop them with `python Tools/ue.py inst stop pvhost` / `pvclient`.

    python Tools/pawnvis_probe.py [--stop]
"""
import re
import sys
import time

sys.path.insert(0, __file__.rsplit("\\", 1)[0] if "\\" in __file__ else __file__.rsplit("/", 1)[0])
import ue_inst as ui  # noqa: E402

UPROJECT = r"D:\UEProjects\MO57\MO57.uproject"
EDITOR = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
HOST, CLIENT = "pvhost", "pvclient"

PROBE = r'''
import unreal
chars = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter)
lp = unreal.GameplayStatics.get_player_pawn(world, 0)
lpn = lp.get_name() if lp else "none"
lploc = lp.get_actor_location() if lp else None
def g(o, p, d="?"):
    try:
        return o.get_editor_property(p)
    except Exception:
        return d
out("PV local=%s nchars=%d" % (lpn, len(chars)))
for c in chars:
    try:
        loc = c.get_actor_location()
        dist = (((loc.x - lploc.x) ** 2 + (loc.y - lploc.y) ** 2 + (loc.z - lploc.z) ** 2) ** 0.5) if lploc else -1.0
        ctrl = c.get_controller()
        ctl = ctrl.get_name() if ctrl else "none"
        hum = (ctrl is not None and isinstance(ctrl, unreal.PlayerController))
        try: lc = c.is_locally_controlled()
        except Exception: lc = "?"
        out("PV  char=%s class=%s loc=(%.0f,%.0f,%.0f) dist=%.0f hidden=%s controller=%s playerctl=%s localctl=%s localrole=%s remoterole=%s netcull=%s" % (
            c.get_name(), c.get_class().get_name(), loc.x, loc.y, loc.z, dist, g(c, "hidden"), ctl, hum, lc, g(c, "local_role"), g(c, "remote_role"), g(c, "net_cull_distance_squared")))
        if hum or dist < 5000:
            for m in c.get_components_by_class(unreal.SkeletalMeshComponent):
                sm = g(m, "skeletal_mesh_asset", None) or g(m, "skeletal_mesh", None)
                out("PV     mesh=%s asset=%s visible=%s hidden_in_game=%s owner_no_see=%s only_owner_see=%s render_main=%s" % (
                    m.get_name(), sm.get_name() if sm else "NONE", g(m, "visible"), g(m, "hidden_in_game"), g(m, "owner_no_see"), g(m, "only_owner_see"),
                    g(m, "render_in_main_pass")))
    except Exception as e:
        out("PV  ERR %s: %s" % (c.get_name(), str(e)[:200]))
'''


def look(name):
    code, out = ui.call(name, ["py", "-c", PROBE], timeout=60)
    lines = [ln for ln in out.splitlines() if re.search(r"\bPV\b", ln) and "import unreal" not in ln]
    print(f"===== {name}", flush=True)
    for ln in lines:
        print(re.sub(r"^.*?(PV)", r"\1", ln)[:330], flush=True)
    if not lines:
        print(out[-600:], flush=True)


def main():
    stop_after = "--stop" in sys.argv
    if "--look" in sys.argv:  # instances already running: just ask again
        look(HOST)
        look(CLIENT)
        return 0
    rep = ui.Report()
    for n, pos in ((HOST, (0, 0)), (CLIENT, (680, 0))):
        ui.start(n, UPROJECT, EDITOR, pos=pos, nosteam=True)
    try:
        if not ui._host_and_join(rep, HOST, CLIENT, "PawnVis", 300):
            print("[pawnvis] could not set up the pair", flush=True)
            return 1
        time.sleep(12)  # let the world settle
        look(HOST)
        look(CLIENT)
        return 0
    finally:
        if stop_after:
            for n in (HOST, CLIENT):
                ui.stop(n)
        else:
            print(f"[pawnvis] instances left running: python Tools/ue.py inst stop {HOST} / {CLIENT}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
