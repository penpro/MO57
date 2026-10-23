"""End-to-end check of a world's creation settings (Docs/World_Creation.md) on a host + joiner pair: the shape reaches the joiner's terrain,
the pace scales a timed action, the start lands in the chosen biome, and a save / load keeps all of it without GENERATOR CHANGED.

    python Tools/world_creation_check.py [--seed 3014] [--keep]

Editor closed (TRAPS 5). Instances wch (host) and wcc (joiner). PASS / FAIL lines like the nettest; exit code 0 when all pass.
"""
import argparse
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue_inst as ui  # noqa: E402

UE = [sys.executable, os.path.join(HERE, "ue.py")]
CREATION = "BiomeSize=2 Mountains=0.5 Inspecting=0.5 StartingBiome=Rainforest"
CHECKSUM_RE = r"\[MOQUERY\] WORLDGEN Checksum height=(0x[0-9A-F]+) base=(0x[0-9A-F]+) cave=(0x[0-9A-F]+) version=(\d+) params=(0x[0-9A-F]+)"


def checksum(name):
    off = ui.log_size(name)
    ui.console(name, "MO.WorldGen.Checksum")
    return ui.wait_log(name, CHECKSUM_RE, since=off, timeout=60)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", default="3014")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    rep = ui.Report()
    host, client = "wch", "wcc"
    try:
        for n, pos in ((host, "0,0"), (client, "680,0")):
            subprocess.run(UE + ["inst", "start", n, "--nosteam", "--pos", pos], capture_output=True, text=True, timeout=400)
        hoff0 = ui.log_size(host)
        ui.console(host, "MO.World.Creation reset " + CREATION)
        pending = ui.wait_log(host, r"\[MOQUERY\] WORLDCREATION pending[^\r\n]*", since=hoff0, timeout=30)
        rep.step("host: the next new game's creation settings are set", bool(pending) and "start=Rainforest" in pending.group(0),
                 pending.group(0)[-200:] if pending else "no WORLDCREATION line")
        if not ui._host_and_join(rep, host, client, "WCTest", seed=a.seed):
            return 1
        if not ui._worlds_built(rep, host, client):
            return 1
        hlog = ui.read_log_from(host, hoff0)
        this = re.search(r"\[MOWorldCreation\] this world: [^\r\n]*", hlog)
        rep.step("host: the world was created with the settings (consumed once by the new game)", bool(this) and "BiomeSize=2" in this.group(0)
                 and "Inspecting=0.5" in this.group(0) and "start=Rainforest" in this.group(0), this.group(0)[-220:] if this else "no line")
        start = re.search(r"\[MOGameMode\] Starting biome Rainforest: \((-?\d+), (-?\d+)\)[^\r\n]*", hlog)
        rep.step("host: the first survivor starts deep inside the Rainforest", bool(start), start.group(0)[:200] if start else "no starting-biome line")
        if start:
            at = ui._probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); l = p.get_actor_location(); out('HAT %.0f %.0f' % (l.x, l.y))")
            m = re.search(r"HAT (-?\d+) (-?\d+)", at)
            d = ((int(m.group(1)) - int(start.group(1))) ** 2 + (int(m.group(2)) - int(start.group(2))) ** 2) ** 0.5 if m else -1
            rep.step("... and the host's pawn stands there (within 30 m)", m is not None and 0 <= d < 3000, f"{d:.0f} cm from the spot")

        hc, cc = checksum(host), checksum(client)
        rep.step("the joiner generates the host's shaped land (same params hash and height checksum)", bool(hc) and bool(cc)
                 and hc.group(5) == cc.group(5) and hc.group(1) == cc.group(1),
                 f"host params {hc.group(5) if hc else '?'} height {hc.group(1) if hc else '?'} | joiner params {cc.group(5) if cc else '?'} height {cc.group(1) if cc else '?'}")
        rep.step("... and it is not the default world (params hash differs from the authored one)", bool(hc) and hc.group(5).lower() != "0xecac384c",
                 hc.group(5) if hc else "?")

        coff = ui.log_size(client)
        ui.console(client, "MO.World.Creation")
        act = ui.wait_log(client, r"\[MOQUERY\] WORLDCREATION active \(this world\): [^\r\n]*", since=coff, timeout=30)
        rep.step("the joiner's copy of the world's settings (replicated) has the pace", bool(act) and "Inspecting=0.5" in act.group(0),
                 act.group(0)[-200:] if act else "no line")

        hoff = ui.log_size(host)
        ui._probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); "
                        "p.get_component_by_class(unreal.MOInventoryComponent).add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Stick01', 1); out('GAVE')")
        ui.console(host, "MO.Test.Inspect Stick01")
        st = ui.wait_log(host, r"\[MOInspection\] \S+: studying 'Stick01' \([^,]*, ([\d.]+) s, server-timed\)", since=hoff, timeout=20)
        rep.step("host: an inspection takes half its real time (Inspecting 0.5: 0.5 s of 1.0)", bool(st) and abs(float(st.group(1)) - 0.5) < 0.05,
                 st.group(0)[:160] if st else "no studying line")

        slot = "WorldCreationCheck_Claude"
        hoff = ui.log_size(host)
        ui.console(host, f"MO.Save.SaveAs {slot}")
        saved = ui.wait_log(host, r"\[MOPersist\][^\r\n]*(Save|save)[^\r\n]*" + slot, since=hoff, timeout=60)
        rep.step("host: the world saves", bool(saved), saved.group(0)[:160] if saved else "no save line")
        time.sleep(3)
        hoff = ui.log_size(host)
        ui.console(host, f"MO.Save.LoadFrom {slot}")
        again = ui.wait_log(host, r"\[MOWorldCreation\] this world: [^\r\n]*", since=hoff, timeout=180)
        time.sleep(10)
        llog = ui.read_log_from(host, hoff)
        rep.step("host: the loaded world has its creation settings back", bool(again) and "BiomeSize=2" in again.group(0) and "start=Rainforest" in again.group(0),
                 again.group(0)[-200:] if again else "no line after the load")
        rep.step("... with no GENERATOR CHANGED (the save's shape is applied before the hash is compared)", "GENERATOR CHANGED" not in llog and "GENERATOR UNKNOWN" not in llog,
                 (re.search(r"GENERATOR [A-Z]+[^\r\n]*", llog).group(0)[:200]) if re.search(r"GENERATOR [A-Z]+", llog) else "none")
        return 0 if rep.ok else 1
    finally:
        print(f"[worldcreation] {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not a.keep:
            for n in (host, client):
                subprocess.run(UE + ["inst", "stop", n], capture_output=True, text=True, timeout=120)


if __name__ == "__main__":
    sys.exit(main())
