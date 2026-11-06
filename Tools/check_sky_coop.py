"""Floating islands in co-op (generator v13 on, fork F62): a joiner stamps the same islands, grows the same forests and digs the same rock as the host.

    python Tools/check_sky_coop.py [--seed 777] [--keep]

Two real -game processes, host and joiner, in one world from a known seed (Docs/Coop_And_Networking.md section 6). Stamps and plants are made per machine from the seed, never replicated, so
every machine must come to the same island by itself; only the digs travel (the volume sculpt):
1. the same seed and MO.WorldGen.Checksum match=1 on both;
2. both pawns hover over the nearest island's top (the server moves them): both machines log the island stamped, both find rock at the same height across it;
3. MO.WorldGen.IslandPlants on both: the top is planted on each, the same plants within 25 % (each machine's tiers build at their own pace), none floating, no rock hanging;
4. both land on the rock; the joiner digs five times through the real RPC (MO.Test.TerraformRPC, a dig on the stamp's rock: the volume sculpt), then the host: each dig lowers the rock on BOTH machines;
5. a dig under the grass top yields the island's stone, not earth.

Prints "SKY COOP PASS" or "SKY COOP FAIL".
"""
import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

CHECKSUM_RE = r"\[MOQUERY\] WORLDGEN Checksum height=(0x[0-9A-F]+) base=(0x[0-9A-F]+) cave=(0x[0-9A-F]+) version=(\d+) params=(0x[0-9A-F]+) match=(\d)"
ISLAND_RE = r"Island #\d+ dist=[\d.]+ x=(-?[\d.]+) y=(-?[\d.]+) top=(-?[\d.]+) ground=(-?[\d.]+) radius=([\d.]+)"
PLANTS_RE = r"ISLANDPLANTS at \((-?\d+), (-?\d+)\) radius=\d+ top=-?\d+ ground=-?\d+: (\d+) instances, (\d+) meshes, (\d+) rocks hanging"
NEAR_RE = r"ISLANDPLANTS near the pawn[^\r\n]*floating over 60 cm (\d+)"
MOVE = "unreal.CharacterMovementComponent"
OWN_MODE = "p = unreal.GameplayStatics.get_player_pawn(world, 0); p.get_component_by_class(" + MOVE + ").set_movement_mode(unreal.MovementMode.%s); out('MODE ok')"
SERVER_HOVER = ("for i, dx in ((0, -600.0), (1, 600.0)):\n"
                "    p = unreal.GameplayStatics.get_player_pawn(world, i)\n"
                "    p.get_component_by_class(" + MOVE + ").set_movement_mode(unreal.MovementMode.MOVE_FLYING)\n"
                "    p.set_actor_location(unreal.Vector(%f + dx, %f, %f), False, True)\n"
                "out('HOVER ok')\n")
SERVER_LAND = ("for i in (0, 1):\n"
               "    p = unreal.GameplayStatics.get_player_pawn(world, i)\n"
               "    p.get_component_by_class(" + MOVE + ").set_movement_mode(unreal.MovementMode.MOVE_FALLING)\n"
               "out('LAND ok')\n")
OWN_SPOT = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); l = p.get_actor_location(); f = p.get_actor_forward_vector(); "
            "m = p.get_component_by_class(" + MOVE + "); out('SPOT %.1f %.1f %.1f %.4f %.4f %s' % (l.x, l.y, l.z, f.x, f.y, m.movement_mode))")


def checksum(name):
    off = ui.log_size(name)
    ui.console(name, "MO.WorldGen.Checksum")
    return ui.wait_log(name, CHECKSUM_RE, since=off, timeout=120)


def plants(name):
    off = ui.log_size(name)
    ui.console(name, "MO.WorldGen.IslandPlants 2000000")
    ui.wait_log(name, r"ISLANDPLANTS done", since=off, timeout=180)
    return ui.read_log_from(name, off)


def spot(name):
    m = re.search(r"SPOT (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (\S+)", ui._probe(name, OWN_SPOT))
    return tuple(float(v) for v in m.groups()[:5]) + (m.group(6),) if m else None


def dig_and_compare(rep, who, digger, machines):
    """Five digs through the real RPC, 3 m ahead of `digger`: the rock there must go down on every machine."""
    at = spot(digger)
    if not rep.step(f"{who}: stands somewhere", bool(at), str(at)):
        return
    x, y, z, fx, fy = at[:5]
    tx, ty = x + fx * 300.0, y + fy * 300.0
    before = {n: ui._surface_z(n, tx, ty, z + 1500.0, z - 2000.0) for n in machines}
    if not rep.step(f"{who}: both machines find the rock 3 m ahead of it, at one height", all(b and b[0] == 1 for b in before.values())
                    and abs(before[machines[0]][1] - before[machines[1]][1]) < 5.0, str({n: b[1] if b else None for n, b in before.items()})):
        return
    off = ui.log_size(digger)
    for _ in range(5):
        ui.console(digger, "MO.Test.TerraformRPC Dig 300")
        time.sleep(3.0)
    sent = re.findall(r"\[MOTEST\] TerraformRPC[^\r\n]*Z=(-?[\d.]+)", ui.read_log_from(digger, off))
    # The verb aims 90 cm under the pawn: on a slope that is over the rock, and the 75 cm sphere reaches only so deep (its bottom comes about 9 cm short: the sculpt's falloff).
    target_z = float(sent[-1]) if sent else z - 90.0
    want = max(5.0, 75.0 - (target_z - before[machines[0]][1]) - 15.0)
    print(f"   aimed at z {target_z:.1f}, the rock at {before[machines[0]][1]:.1f}: the sphere should take at least {want:.1f} cm", flush=True)
    after, waited = {}, 0.0
    while waited < 60.0:   # a joiner fetches the sculpt's new data from the host (request / reply over the voxel channel)
        time.sleep(5.0)
        waited += 5.0
        after = {n: ui._surface_z(n, tx, ty, z + 1500.0, z - 2000.0) for n in machines}
        if all(a and a[0] == 1 and before[n][1] - a[1] > want for n, a in after.items()) and abs(after[machines[0]][1] - after[machines[1]][1]) < 5.0:
            break
    dug = {n: round(before[n][1] - after[n][1], 1) if after.get(n) and after[n][0] == 1 else None for n in machines}
    rep.step(f"{who}'s five digs lower the island's rock on both machines (> {want:.0f} cm, the two within 5 cm), after {waited:.0f} s", all(v is not None and v > want for v in dug.values())
             and abs(after[machines[0]][1] - after[machines[1]][1]) < 5.0, f"dug {dug}, rock {[after[n][1] for n in machines]}")


def dig_the_rock(rep, host):
    """A dig under the island's grass top (the top's 2.5 m is soil): what comes out is the rock, not the earth of the ground far below."""
    at = spot(host)
    x, y, z, fx, fy = at[:5]
    tx, ty = x + fx * 300.0, y + fy * 300.0
    rock = ui._surface_z(host, tx, ty, z + 1500.0, z - 2000.0)
    if not rep.step("the host: rock to dig into", bool(rock) and rock[0] == 1, str(rock)):
        return
    off = ui.log_size(host)
    # The real RPC aims 90 cm under the pawn, so the pawn goes down for one tick (flying: the yield line is the server's, before anything frees it from the rock).
    ui._probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); p.get_component_by_class(" + MOVE + ").set_movement_mode(unreal.MovementMode.MOVE_FLYING); "
                    f"p.set_actor_location(unreal.Vector({x:.1f}, {y:.1f}, {rock[1] - 360.0:.1f}), False, True); out('DOWN ok')")
    ui.console(host, "MO.Test.TerraformRPC Dig 300")
    m = ui.wait_log(host, r"dug [\d.]+m3 at [^\r\n]*?: ([^\r\n]*)", since=off, timeout=30)
    rep.step("a dig 4.5 m into the island yields its stone, no earth", bool(m) and "Dirt01" not in m.group(1), m.group(0) if m else "no dug line")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--seed", default="777")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    rep = ui.Report()
    host, client = "skyhost", "skyjoin"
    try:
        ui.start(host, ue.UPROJECT, ue.EDITOR_EXE, pos=(0, 0), nosteam=True)
        ui.start(client, ue.UPROJECT, ue.EDITOR_EXE, pos=(660, 0), nosteam=True)
        if not ui._host_and_join(rep, host, client, "SkyCoopTest", boot_timeout=300, seed=a.seed):
            return 1
        time.sleep(30)

        # 1. One world.
        seeds = {n: ui._seed_query(n) for n in (host, client)}
        rep.step("both machines run one seed", all(seeds.values()) and seeds[host][1] == seeds[client][1], str(seeds))
        sums = {n: checksum(n) for n in (host, client)}
        rep.step("MO.WorldGen.Checksum match=1 on both, one generator version, v13 or later (islands)", all(m and m.group(6) == "1" and int(m.group(4)) >= 13 for m in sums.values()) and len({m.group(4) for m in sums.values() if m}) == 1,
                 " / ".join(m.group(0)[:110] if m else "no line" for m in sums.values()))

        # 2. The nearest island, both pawns over it.
        off = ui.log_size(host)
        ui.console(host, "MO.WorldGen.FindIsland 1500000")
        ui.wait_log(host, r"Island #\d+ dist", since=off, timeout=60)
        time.sleep(3)
        islands = [tuple(float(v) for v in m) for m in re.findall(ISLAND_RE, ui.read_log_from(host, off))]
        if not rep.step("host: an island near the start", bool(islands), f"{len(islands)} found"):
            return 1
        x, y, top, ground, radius = islands[0]
        print(f"   island at ({x:.0f}, {y:.0f}): top {top:.0f}, ground {ground:.0f}, radius {radius:.0f}", flush=True)
        ui._probe(client, OWN_MODE % "MOVE_FLYING")
        ui._probe(host, SERVER_HOVER % (x, y, top + 1500.0))
        ui._probe(client, OWN_MODE % "MOVE_FLYING")
        time.sleep(90)
        at = {n: spot(n) for n in (host, client)}
        rep.step("both pawns hover over the island on their own machines", all(s and abs(s[0] - x) < 2000.0 and abs(s[1] - y) < 2000.0 and abs(s[2] - top) < 5000.0 for s in at.values()), str(at))
        stamped = {n: re.findall(r"\[MOFloatingIsland\] stamped the island at \((-?\d+), (-?\d+)\)", ui.read_log_from(n, 0)) for n in (host, client)}
        here = (str(int(round(x))), str(int(round(y))))
        rep.step("both machines stamped this island by themselves", all(here in s for s in stamped.values()), f"host {len(stamped[host])} stamped in all, joiner {len(stamped[client])}")
        # The rocks under the islands and in the ravines' walls come from the seed alone (N-330, N-331): where both machines have one, the same count.
        for what, pattern in (("island", r"\[MOFloatingIsland\] (\d+) rocks under the island at \((-?\d+), (-?\d+)\)"), ("ravine", r"\[MORavineRocks\] (\d+) rocks in the ravine of cell \((-?\d+), (-?\d+)\)")):
            placed = {n: {(m[1], m[2]): m[0] for m in re.findall(pattern, ui.read_log_from(n, 0))} for n in (host, client)}
            both = set(placed[host]) & set(placed[client])
            rep.step(f"{what} rocks: the same on both machines", all(placed[host][k] == placed[client][k] for k in both) and (what == "ravine" or bool(both)),
                     f"{len(both)} on both, host {len(placed[host])}, joiner {len(placed[client])}")
        rock = {}
        bad = []
        for dx, dy in ((0.0, 0.0), (800.0, 0.0), (-800.0, 300.0), (0.0, 800.0), (400.0, -800.0)):
            h, c = ui._surface_z(host, x + dx, y + dy, top + 3000.0, top - 5000.0), ui._surface_z(client, x + dx, y + dy, top + 3000.0, top - 5000.0)
            rock[(dx, dy)] = (h[1] if h and h[0] == 1 else None, c[1] if c and c[0] == 1 else None)
            if not (h and c and h[0] == 1 and c[0] == 1 and abs(h[1] - c[1]) < 5.0):
                bad.append((dx, dy))
        rep.step("rock at one height on both machines across the top (5 spots, within 5 cm)", not bad, str(rock))

        # 3. The forest, on each machine.
        text = {n: plants(n) for n in (host, client)}
        counts = {}
        for n in (host, client):
            for m in re.findall(PLANTS_RE, text[n]):
                if abs(float(m[0]) - x) < 2.0 and abs(float(m[1]) - y) < 2.0:
                    counts[n] = (int(m[2]), int(m[3]), int(m[4]))
        rep.step("the top is planted on both machines (>= 1000), within 25 % of each other, no rock hanging", len(counts) == 2 and min(counts[host][0], counts[client][0]) >= 1000
                 and abs(counts[host][0] - counts[client][0]) <= 0.25 * max(counts[host][0], counts[client][0]) and counts[host][2] == 0 and counts[client][2] == 0, f"(instances, meshes, rocks hanging): {counts}")
        floating = {n: re.search(NEAR_RE, text[n]) for n in (host, client)}
        rep.step("nothing floats over 60 cm round either pawn", all(m and m.group(1) == "0" for m in floating.values()), str({n: m.group(0)[:150] if m else None for n, m in floating.items()}))

        # 4. On the rock: the digs travel.
        ui._probe(client, OWN_MODE % "MOVE_WALKING")
        ui._probe(host, SERVER_LAND)
        time.sleep(15)
        at = {n: spot(n) for n in (host, client)}
        rep.step("both landed on the island (still up there, walking)", all(s and abs(s[2] - top) < 5000.0 and "WALKING" in s[5] for s in at.values()), str(at))
        dig_and_compare(rep, "the joiner", client, (host, client))
        dig_and_compare(rep, "the host", host, (host, client))
        dig_the_rock(rep, host)
        errors = {n: re.findall(r"[^\r\n]*(?:GENERATOR CHANGED|DIFFERENT world)[^\r\n]*", ui.read_log_from(n, 0)) for n in (host, client)}
        rep.step("no generator complaint on either machine", not any(errors.values()), str({n: e[:1] for n, e in errors.items()}))
        return 0 if rep.ok else 1
    finally:
        print("SKY COOP %s" % ("PASS" if rep.rows and rep.ok else "FAIL"), flush=True)
        if not a.keep:
            ui.stop(client)
            ui.stop(host)


if __name__ == "__main__":
    sys.exit(main())
