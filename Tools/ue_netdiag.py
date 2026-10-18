"""ue_netdiag - `ue.py nettest netdiag`: how SMOOTH co-op is, not whether it works.

WHY: Wes, Oct 17 2026: "very bad rubber banding and animation latency" host-to-client at ~9 ms ping. The other nettest modes check
correctness (terrain agrees, actions reach the server); none measured smoothness. Ping can be low while the host's channel to a joiner is
saturated, while the server corrects every move, while a frame takes 60 ms, or while a remote pawn's position arrives five times a second.

HOW: host + joiner (two -game processes, Null OSS) both walk / jog / sprint MO.Test.NetPath while MO.Net.Stats logs one [MONet] line a
second on each machine and MO.Net.Trace logs every player pawn's position 20 times a second ([MONetPos]). Both processes run on this
machine, so their clocks (FPlatformTime::Seconds = QueryPerformanceCounter) agree, and one machine's picture of a pawn can be lined up with
the pawn's owner's (visual_lag: how far behind, how far off).

PHASES: clean; NetEmulation.PktLag 50 on both (+100 ms round trip); NetEmulation.PktLoss 1 on both; then two NEGATIVE CONTROLS that must
make the checks fail: a 4 KB/s rate cap on the host (MO.Net.TestRateCap: saturation must show) and a joiner whose own copy walks 1.6x what
the server allows (corrections must show). A control that does not fail means the measurement cannot see what it looks for.

    python Tools/ue.py nettest netdiag [--keep] [--seconds 60] [--phases clean,lag50]

The pure parts (parse_monet, parse_monetpos, visual_lag, analyse_netdiag_phase, netdiag_verdicts) are unit-tested offline
(Tools/tests/test_ue_netdiag.py).
"""
import json
import math
import os
import re
import sys
import time

ui = None   # Tools/ue_inst: imported by the live run only (_load_ui); the log reader runs isolated (python -I) and needs nothing else


def _load_ui():
    global ui
    if ui is None:
        import ue_inst
        ui = ue_inst
    return ui


MONET_RE = re.compile(r"\[MONet\] role=(host|client) (.*)")
MONETPOS_RE = re.compile(r"\[MONetPos\] (t=.*)")
MONETEVENT_RE = re.compile(r"\[MONetEvent\] st=(-?[\d.]+)(?: utc=([\d.]+))? (.*)")
MONETDRIVER_RE = re.compile(r"\[MONet\] net driver (\S+) \((host|client)\): actor channel (.+?), MaxClientRate (\d+), "
                            r"MaxInternetClientRate (\d+), NetServerMaxTickRate (\d+)")
MONETCORR_RE = re.compile(r"\[MONetCorr\] (server|client): (.*)")
READINESS_RE = r"\[MOReadiness\] READY after"

# Thresholds for the CLEAN phase (both processes on one machine: no real network in the way). One per layer of the task's hypotheses.
THRESHOLDS = dict(
    saturated_seconds=0,        # the host's channel to the joiner never runs out of budget
    correction_ratio=0.02,      # at most 2 % of the joiner's moves corrected by the server
    snap_max_cm=50.0,           # no visible rubber band longer than half a metre
    remote_updates_min=15.0,    # every second, each machine gets >= 15 positions a second of the other player's pawn
    remote_gap_ms=250.0,        # ... and never waits longer than a quarter second for one
    visual_p95_cm=60.0,         # each machine's picture of the other's pawn is within 60 cm of where its owner has it (after the delay)
    visual_delay_ms=250.0,      # ... and at most a quarter second behind
)
NAN = float("nan")


def _isnan(x):
    return x != x


# ---------------------------------------------------------------------------------------------------------------------------------
# parsing (pure)
# ---------------------------------------------------------------------------------------------------------------------------------

def parse_monet(text):
    """[MONet] lines -> dicts. 'frame=17.2/41.0ms' -> frame 17.2, frame2 41.0; units (ms, cm, %) dropped; topOut / topIn -> {class: bytes}."""
    out = []
    for m in MONET_RE.finditer(text):
        d = {"role": m.group(1)}
        for tok in m.group(2).split():
            if "=" not in tok:
                continue
            k, v = tok.split("=", 1)
            if k in ("topOut", "topIn"):
                d[k] = {} if v == "-" else {c: int(b) for c, b in (p.rsplit(":", 1) for p in v.split(",") if ":" in p)}
                continue
            parts = re.sub(r"(ms|cm|%)$", "", v).split("/")
            try:
                d[k] = float(parts[0])
                if len(parts) > 1:
                    d[k + "2"] = float(parts[1])
            except ValueError:
                d[k] = v
        out.append(d)
    return out


def parse_monetpos(text):
    """[MONetPos] lines -> dicts (t = this machine's clock, st = the server clock when the line has one, else -1)."""
    out = []
    for m in MONETPOS_RE.finditer(text):
        d = {}
        for tok in m.group(1).split():
            if "=" not in tok:
                continue
            k, v = tok.split("=", 1)
            if k in ("gait", "mgait"):
                d[k] = v
                continue
            try:
                d[k] = float(v)
            except ValueError:
                d[k] = v
        if not all(k in d for k in ("t", "id", "mx", "my")):
            continue
        for k in ("id", "local", "role"):
            d[k] = int(d.get(k, 0))
        d.setdefault("st", -1.0)
        d.setdefault("utc", -1.0)
        out.append(d)
    return out


def parse_events(text, who, clock="st"):
    """[MONetEvent] lines -> [(time, who, text)], time on `clock` ("utc" falls back to st on lines from before utc was logged)."""
    out = []
    for m in MONETEVENT_RE.finditer(text):
        t = float(m.group(2)) if clock == "utc" and m.group(2) else float(m.group(1))
        out.append((t, who, m.group(3).strip()))
    return out


# ---------------------------------------------------------------------------------------------------------------------------------
# analysis (pure)
# ---------------------------------------------------------------------------------------------------------------------------------

def _interp_xy(samples, times, t, clock="t"):
    """Mesh position of a time-sorted trajectory at time t (linear), or None outside it or across a gap of more than 0.5 s."""
    import bisect
    i = bisect.bisect_left(times, t)
    if i <= 0 or i >= len(samples):
        return None
    a, b = samples[i - 1], samples[i]
    if b[clock] - a[clock] > 0.5:
        return None
    f = (t - a[clock]) / (b[clock] - a[clock]) if b[clock] > a[clock] else 0.0
    return a["mx"] + f * (b["mx"] - a["mx"]), a["my"] + f * (b["my"] - a["my"])


def _moving_at(samples, times, t, clock="t", min_speed=50.0):
    """True if the owner's sample nearest to t says it was moving (v >= min_speed cm/s)."""
    import bisect
    if not samples:
        return False
    i = min(max(bisect.bisect_left(times, t), 0), len(samples) - 1)
    if i > 0 and abs(samples[i - 1][clock] - t) < abs(samples[i][clock] - t):
        i -= 1
    return samples[i].get("v", 0.0) >= min_speed


def visual_lag(owner, viewer, max_delay=0.6, step=0.01, clock="t"):
    """How far behind (delay_ms) and how far off (cm) a viewer's picture of a pawn is from its owner's, both sampled on one clock.

    The delay is the one that makes the two agree best (lowest median distance); the distances at that delay are the jitter and the
    snaps. Mesh positions on both sides: what each player SEES (the viewer's mesh carries the network smoothing).
    clock: "t" when both ran on one PC (one QueryPerformanceCounter), "st" (the replicated server clock) for two PCs -- that clock is only good to about
    half a round trip on a joiner, so a two-PC delay carries that much uncertainty."""
    owner = sorted((s for s in owner if s.get(clock, -1.0) >= 0.0), key=lambda s: s[clock])
    viewer = sorted((s for s in viewer if s.get(clock, -1.0) >= 0.0), key=lambda s: s[clock])
    times = [s[clock] for s in owner]
    best = None
    for k in range(int(round(max_delay / step)) + 1):
        d = k * step
        res = []
        for v in viewer:
            if not _moving_at(owner, times, v[clock], clock):
                continue   # a pawn standing still is trivially "close": only moving stretches say anything
            o = _interp_xy(owner, times, v[clock] - d, clock)
            if o is not None:
                res.append(math.hypot(v["mx"] - o[0], v["my"] - o[1]))
        if len(res) < 10:
            continue
        res.sort()
        med = res[len(res) // 2]
        if best is None or med < best[1] - 1e-9:
            best = (d, med, res)
    if best is None:
        return dict(n=0, delay_ms=NAN, p50=NAN, p95=NAN, max=NAN)
    d, med, res = best
    return dict(n=len(res), delay_ms=d * 1000.0, p50=med, p95=res[min(len(res) - 1, int(0.95 * len(res)))], max=res[-1])


def _avg(xs):
    xs = [x for x in xs if x is not None and not _isnan(x)]
    return sum(xs) / len(xs) if xs else NAN


def _sum_top(stats, key, n=6):
    """Bytes per second by actor class over the phase (summed lines / seconds)."""
    tot = {}
    for s in stats:
        for c, b in s.get(key, {}).items():
            tot[c] = tot.get(c, 0) + b
    secs = max(1, len(stats))
    return sorted(((c, b / secs) for c, b in tot.items()), key=lambda cb: -cb[1])[:n]


def analyse_phase(host_stats, client_stats, host_pos, client_pos, clock="t"):
    """One phase's numbers from both machines' [MONet] and [MONetPos] lines (clock: see visual_lag)."""
    # seconds of play only: a world building behind the loading screen (play=0) has second-long frames and the join's first replication burst
    h = [s for s in host_stats if s["role"] == "host" and s.get("play", 1) == 1]
    c = [s for s in client_stats if s["role"] == "client" and s.get("play", 1) == 1]
    r = dict(seconds_host=len(h), seconds_client=len(c))
    r["host_fps"] = _avg([s.get("fps") for s in h])
    r["host_fps_min"] = min((s.get("fps", 0) for s in h), default=NAN)
    r["host_frame_max"] = max((s.get("frame2", 0) for s in h), default=NAN)
    r["client_fps"] = _avg([s.get("fps") for s in c])
    r["client_fps_min"] = min((s.get("fps", 0) for s in c), default=NAN)
    r["client_frame_max"] = max((s.get("frame2", 0) for s in c), default=NAN)
    r["cap"] = max((s.get("cap", 0) for s in h), default=0)
    r["host_out_avg"] = _avg([s.get("out") for s in h])
    r["host_out_max"] = max((s.get("out", 0) for s in h), default=0)
    r["use_max_pct"] = max((s.get("use", 0) for s in h), default=0)
    r["saturated_seconds"] = sum(1 for s in h if s.get("satRepl", 0) > 0 or s.get("satFrames", 0) > 0)
    r["sat_repl_max_pct"] = max((s.get("satRepl", 0) for s in h), default=0)
    r["host_reps_avg"] = _avg([s.get("reps") for s in h])
    r["host_objs_avg"] = _avg([s.get("objs") for s in h])
    r["client_chans_avg"] = _avg([s.get("chans") for s in c])
    r["client_in_avg"] = _avg([s.get("in") for s in c])
    r["loss_max_pct"] = max([s.get("loss", 0) for s in h + c] + [s.get("loss2", 0) for s in h + c] + [0])
    r["rtt_avg"] = _avg([s.get("rtt") for s in c])
    r["ping_avg"] = _avg([s.get("ping") for s in c])
    moves = sum(s.get("moves", 0) for s in h)
    corr = sum(s.get("corr", 0) for s in h)
    r["moves"], r["corrections"] = int(moves), int(corr)
    r["correction_ratio"] = corr / moves if moves else NAN
    r["server_err_max"] = max((s.get("err2", 0) for s in h), default=0)
    r["server_err_xy_avg"] = _avg([s.get("xy") for s in h if s.get("corr", 0) > 0])
    r["server_err_z_avg"] = _avg([s.get("z") for s in h if s.get("corr", 0) > 0])
    r["client_corrections"] = int(sum(s.get("recv", 0) for s in c))
    r["snap_max"] = max((s.get("snap2", 0) for s in c), default=0)
    with_remote_h = [s for s in h if s.get("remote", 0) > 0]
    with_remote_c = [s for s in c if s.get("remote", 0) > 0]
    r["host_sees_joiner_upd_min"] = min((s["upd"] for s in with_remote_h), default=NAN)
    r["host_sees_joiner_upd_avg"] = _avg([s["upd"] for s in with_remote_h])
    r["host_sees_joiner_gap_max"] = max((s["gap"] for s in with_remote_h), default=NAN)
    r["client_sees_host_upd_min"] = min((s["upd"] for s in with_remote_c), default=NAN)
    r["client_sees_host_upd_avg"] = _avg([s["upd"] for s in with_remote_c])
    r["client_sees_host_gap_max"] = max((s["gap"] for s in with_remote_c), default=NAN)
    r["top_out_host"] = _sum_top(h, "topOut")
    r["top_in_client"] = _sum_top(c, "topIn")
    # each machine's picture of the OTHER player's pawn, against that pawn's owner
    hid = next(iter({p["id"] for p in host_pos if p["local"] == 1}), None)
    cid = next(iter({p["id"] for p in client_pos if p["local"] == 1}), None)
    r["client_view_of_host"] = visual_lag([p for p in host_pos if p["id"] == hid and p["local"] == 1],
                                          [p for p in client_pos if p["id"] == hid and p["local"] == 0], clock=clock)
    r["host_view_of_joiner"] = visual_lag([p for p in client_pos if p["id"] == cid and p["local"] == 1],
                                          [p for p in host_pos if p["id"] == cid and p["local"] == 0], clock=clock)
    r["joiner_speed_avg"] = _avg([p["v"] for p in client_pos if p["local"] == 1])
    r["host_speed_avg"] = _avg([p["v"] for p in host_pos if p["local"] == 1])
    return r


def verdicts(r, t=THRESHOLDS):
    """The clean phase against the thresholds: [(label, ok, evidence)]."""
    cv, hv = r["client_view_of_host"], r["host_view_of_joiner"]
    return [
        ("the host's channel to the joiner never saturates", r["saturated_seconds"] <= t["saturated_seconds"],
         f"{r['saturated_seconds']} of {r['seconds_host']} s saturated (max {r['sat_repl_max_pct']:.0f}% of passes); out avg {r['host_out_avg']:.0f} "
         f"max {r['host_out_max']:.0f} B/s of a {r['cap']:.0f} cap ({r['use_max_pct']:.0f}%)"),
        (f"the server corrects <= {t['correction_ratio']:.0%} of the joiner's moves",
         not _isnan(r["correction_ratio"]) and r["correction_ratio"] <= t["correction_ratio"],
         f"{r['corrections']} of {r['moves']} moves ({r['correction_ratio']:.1%}); max error {r['server_err_max']:.0f} cm "
         f"(avg xy {r['server_err_xy_avg']:.1f} z {r['server_err_z_avg']:.1f}); the joiner got {r['client_corrections']}"),
        (f"no visible rubber band on the joiner (snap <= {t['snap_max_cm']:.0f} cm)", r["snap_max"] <= t["snap_max_cm"],
         f"max snap {r['snap_max']:.1f} cm"),
        (f"the joiner gets the host's pawn >= {t['remote_updates_min']:.0f}/s with gaps <= {t['remote_gap_ms']:.0f} ms",
         not _isnan(r["client_sees_host_upd_min"]) and r["client_sees_host_upd_min"] >= t["remote_updates_min"]
         and r["client_sees_host_gap_max"] <= t["remote_gap_ms"],
         f"slowest second {r['client_sees_host_upd_min']:.1f}/s (avg {r['client_sees_host_upd_avg']:.1f}), longest gap {r['client_sees_host_gap_max']:.0f} ms"),
        (f"the host gets the joiner's moves >= {t['remote_updates_min']:.0f}/s with gaps <= {t['remote_gap_ms']:.0f} ms",
         not _isnan(r["host_sees_joiner_upd_min"]) and r["host_sees_joiner_upd_min"] >= t["remote_updates_min"]
         and r["host_sees_joiner_gap_max"] <= t["remote_gap_ms"],
         f"slowest second {r['host_sees_joiner_upd_min']:.1f}/s (avg {r['host_sees_joiner_upd_avg']:.1f}), longest gap {r['host_sees_joiner_gap_max']:.0f} ms"),
        ("the joiner's picture of the host's pawn is close and current",
         cv["n"] >= 50 and cv["p95"] <= t["visual_p95_cm"] and cv["delay_ms"] <= t["visual_delay_ms"],
         f"{cv['n']} samples: {cv['delay_ms']:.0f} ms behind, off by p50 {cv['p50']:.0f} p95 {cv['p95']:.0f} max {cv['max']:.0f} cm"),
        ("the host's picture of the joiner's pawn is close and current",
         hv["n"] >= 50 and hv["p95"] <= t["visual_p95_cm"] and hv["delay_ms"] <= t["visual_delay_ms"],
         f"{hv['n']} samples: {hv['delay_ms']:.0f} ms behind, off by p50 {hv['p50']:.0f} p95 {hv['p95']:.0f} max {hv['max']:.0f} cm"),
    ]


def phase_line(name, r):
    cv, hv = r["client_view_of_host"], r["host_view_of_joiner"]
    top = ", ".join(f"{c} {b / 1000:.1f}k" for c, b in r["top_out_host"][:4]) or "-"
    return (f"[netdiag] {name:13} fps host {r['host_fps']:.0f} (min {r['host_fps_min']:.0f}) joiner {r['client_fps']:.0f} (min {r['client_fps_min']:.0f}) | "
            f"out {r['host_out_avg']:.0f}/{r['host_out_max']:.0f} B/s cap {r['cap']:.0f} sat {r['saturated_seconds']}s | rtt {r['rtt_avg']:.0f} ms "
            f"loss<={r['loss_max_pct']:.1f}% | corr {r['corrections']}/{r['moves']} err<={r['server_err_max']:.0f} snap<={r['snap_max']:.0f} cm | "
            f"host pawn on joiner {r['client_sees_host_upd_min']:.0f}/s gap {r['client_sees_host_gap_max']:.0f} ms, {cv['delay_ms']:.0f} ms behind "
            f"p95 {cv['p95']:.0f} cm | joiner on host {r['host_sees_joiner_upd_min']:.0f}/s gap {r['host_sees_joiner_gap_max']:.0f} ms, "
            f"{hv['delay_ms']:.0f} ms behind p95 {hv['p95']:.0f} cm | top out: {top}")


# ---------------------------------------------------------------------------------------------------------------------------------
# the run (real processes)
# ---------------------------------------------------------------------------------------------------------------------------------

PROBE_WALK_SPEED = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); "
                    "cm = p.get_component_by_class(unreal.CharacterMovementComponent) if p else None; "
                    "out('WALKSPEED=%.0f' % (cm.get_editor_property('max_walk_speed') if cm else -1))")


def _set_local_walk_speed(name, speed):
    """A LOCAL property write on the instance's own copy of its pawn (no RPC): the speed-hack control."""
    return ui._probe(name, "p = unreal.GameplayStatics.get_player_pawn(world, 0); "
                           "cm = p.get_component_by_class(unreal.CharacterMovementComponent) if p else None; "
                           f"cm.set_editor_property('max_walk_speed', {float(speed)}) if cm else None; "
                           "out('WALKSPEED=%.0f' % (cm.get_editor_property('max_walk_speed') if cm else -1))")


def wait_ready_count(name, count, timeout=240):
    """True once the instance's log holds `count` [MOReadiness] READY lines (or False at the timeout / when the process died)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if len(re.findall(READINESS_RE, ui.read_log_from(name, 0))) >= count:
            return True
        if not ui.is_alive(ui.read_pid(name)):
            return False
        time.sleep(2)
    return False


def _apply(steps):
    for who, cmd in steps:
        if callable(cmd):
            cmd()
        else:
            ui.console(who, cmd)


def run_phase(host, client, name, seconds, setup=(), teardown=()):
    """Both pawns run MO.Test.NetPath for `seconds` (the host turned 180 degrees, so the two walk apart); returns the phase's numbers."""
    _apply(setup)
    time.sleep(2)
    off_h, off_c = ui.log_size(host), ui.log_size(client)
    ui.console(host, f"MO.Test.NetPath {seconds} 180")
    ui.console(client, f"MO.Test.NetPath {seconds}")
    time.sleep(seconds + 2)
    th, tc = ui.read_log_from(host, off_h), ui.read_log_from(client, off_c)
    _apply(teardown)
    hs, cs = parse_monet(th), parse_monet(tc)
    hp, cp = parse_monetpos(th), parse_monetpos(tc)
    t0 = min([p["t"] for p in hp + cp] or [0.0]) + 3.0   # skip the path's first frames (and the setup's own second)
    hp, cp = [p for p in hp if p["t"] >= t0], [p for p in cp if p["t"] >= t0]
    r = analyse_phase(hs[3:], cs[3:], hp, cp)
    r["corr_lines"] = [m.group(0) for m in MONETCORR_RE.finditer(th + tc)][:12]
    r["path_blocked"] = th.count("[NETPATH] blocked") + tc.count("[NETPATH] blocked")
    print(phase_line(name, r), flush=True)
    for line in r["corr_lines"][:4]:
        print(f"[netdiag]   {line}", flush=True)
    return r


def netdiag_test(uproject, editor_exe, keep=False, boot_timeout=300, seconds=60, control_seconds=30, only=None):
    """See the module docstring. Writes every phase's numbers to %TEMP%/claude/netdiag_<time>.json."""
    _load_ui()
    rep = ui.Report()
    host, client = names = ("diaghost", "diagclient")
    results = {}
    try:
        print("[nettest] netdiag: host + joiner on the REAL gameplay map (Null OSS, -NoSteam); both walk / jog / sprint ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            ui.start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not ui._host_and_join(rep, host, client, "NetDiag", boot_timeout):
            return False
        # a joiner logs its menu world's READY first (Docs/TRAPS.md section 5), so the gameplay world's is its second; the host, travelling
        # straight from the menu to its hosted world, logs one (seen Oct 17)
        ready = {n: wait_ready_count(n, 2 if n == client else 1, timeout=240) for n in names}
        rep.step("both worlds finished building (the second READY on each) before measuring", all(ready.values()),
                 ", ".join(f"{n}: {'READY' if ok else 'not ready'}" for n, ok in ready.items()))
        for n in names:
            ui.console(n, "MO.Net.Stats 1")
            ui.console(n, "MO.Net.Trace 1")
        time.sleep(3)
        drivers = {m.group(2): m.groups() for m in MONETDRIVER_RE.finditer(ui.read_log_from(host, 0) + ui.read_log_from(client, 0))}
        for role, g in sorted(drivers.items()):
            print(f"[netdiag] {role}: net driver {g[0]}, actor channel {g[2]}, MaxClientRate {g[3]}, MaxInternetClientRate {g[4]}, "
                  f"NetServerMaxTickRate {g[5]}", flush=True)
        rep.step("both machines count actor bytes (the actor channel is MONetStatsActorChannel)",
                 len(drivers) == 2 and all("MONetStatsActorChannel" in g[2] for g in drivers.values()), str({k: v[2] for k, v in drivers.items()}))

        m = re.search(r"WALKSPEED=(-?\d+)", ui._probe(client, PROBE_WALK_SPEED))
        walk = float(m.group(1)) if m and float(m.group(1)) > 0 else 400.0
        phases = [
            ("clean", seconds, (), ()),
            ("lag50", seconds, ((host, "NetEmulation.PktLag 50"), (client, "NetEmulation.PktLag 50")),
             ((host, "NetEmulation.PktLag 0"), (client, "NetEmulation.PktLag 0"))),
            ("loss1", seconds, ((host, "NetEmulation.PktLoss 1"), (client, "NetEmulation.PktLoss 1")),
             ((host, "NetEmulation.PktLoss 0"), (client, "NetEmulation.PktLoss 0"))),
            ("ctl_ratecap", control_seconds, ((host, "MO.Net.TestRateCap 4000"),), ((host, "MO.Net.TestRateCap 0"),)),
            ("ctl_speedhack", control_seconds, ((client, lambda: _set_local_walk_speed(client, walk * 1.6)),),
             ((client, lambda: _set_local_walk_speed(client, walk)),)),
        ]
        for name, secs, setup, teardown in phases:
            if only and name not in only:
                continue
            results[name] = run_phase(host, client, name, secs, setup, teardown)

        if "clean" in results:
            c = results["clean"]
            for label, ok, ev in verdicts(c):
                rep.step(f"clean: {label}", ok, ev)
            print(f"[netdiag]   clean, for the record: host fps {c['host_fps']:.0f} (min {c['host_fps_min']:.0f}, worst frame {c['host_frame_max']:.0f} ms), "
                  f"joiner fps {c['client_fps']:.0f} (min {c['client_fps_min']:.0f}, worst {c['client_frame_max']:.0f} ms) -- both processes share this "
                  f"machine's CPU and GPU; joiner speed avg {c['joiner_speed_avg']:.0f} cm/s, host {c['host_speed_avg']:.0f}", flush=True)
        if "clean" in results and "lag50" in results:
            c, lag = results["clean"], results["lag50"]
            rep.step("CONTROL lag50: the round trip rises by >= 80 ms (the emulation is on)", lag["rtt_avg"] - c["rtt_avg"] >= 80.0,
                     f"rtt {c['rtt_avg']:.0f} -> {lag['rtt_avg']:.0f} ms")
            rep.step("CONTROL lag50: the joiner's picture of the host's pawn falls >= 30 ms further behind (the delay measure sees latency)",
                     lag["client_view_of_host"]["delay_ms"] - c["client_view_of_host"]["delay_ms"] >= 30.0,
                     f"{c['client_view_of_host']['delay_ms']:.0f} -> {lag['client_view_of_host']['delay_ms']:.0f} ms")
            rep.step("lag50: prediction absorbs +100 ms (corrections <= 5% of moves)",
                     not _isnan(lag["correction_ratio"]) and lag["correction_ratio"] <= 0.05, f"{lag['corrections']} of {lag['moves']}")
        if "ctl_ratecap" in results:
            r = results["ctl_ratecap"]
            rep.step("CONTROL ratecap 4 KB/s: saturation shows (the saturation check can fail)", r["saturated_seconds"] > 0,
                     f"{r['saturated_seconds']} of {r['seconds_host']} s saturated, cap {r['cap']:.0f}")
        if "ctl_speedhack" in results:
            r = results["ctl_speedhack"]
            rep.step("CONTROL speedhack (the joiner walks 1.6x what the server allows): corrections show (the correction check can fail)",
                     not _isnan(r["correction_ratio"]) and r["correction_ratio"] > 0.10,
                     f"{r['corrections']} of {r['moves']} moves corrected, max snap {r['snap_max']:.0f} cm")
        return rep.ok
    finally:
        if results:
            path = os.path.join(ui.TMP, f"netdiag_{time.strftime('%Y%m%d_%H%M%S')}.json")
            try:
                with open(path, "w", encoding="utf-8") as f:
                    json.dump(results, f, indent=1, default=str)
                print(f"[netdiag] numbers: {path}", flush=True)
            except OSError:
                pass
        print(f"[nettest] netdiag: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not keep:
            for n in names:
                ui.stop(n)
        else:
            print("[nettest] --keep: instances left running (ue.py inst stop diaghost / diagclient)")


# ---------------------------------------------------------------------------------------------------------------------------------
# reading two machines' logs after a real session (Steam): python -I Tools/ue_netdiag.py logs <host log> [<joiner log>]
# ---------------------------------------------------------------------------------------------------------------------------------

def correlate_events(events, before_s=1.5):
    """For every visible correction / rubber band, what came just before it on the server clock: a joiner frame >= 250 ms, a host frame, a
    saturated channel, a silent remote pawn -- or nothing. Pure (unit-tested). Returns (rows, counts)."""
    events = sorted(events)
    big = [e for e in events if "corrected joiner pawn" in e[2] or "rubber-banded" in e[2]]
    rows, counts = [], {}
    seen_st = []
    for st, who, text in big:
        if any(abs(st - s0) < 0.5 for s0 in seen_st):   # the host's correction and the joiner's snap are one event seen twice
            continue
        seen_st.append(st)
        causes = [e for e in events if st - before_s <= e[0] <= st and e is not None and "corrected joiner pawn" not in e[2] and "rubber-banded" not in e[2]]
        label = ("joiner hitch" if any("joiner frame took" in c[2] for c in causes) else
                 "host hitch" if any("host frame took" in c[2] for c in causes) else
                 "saturated channel" if any("saturated" in c[2] for c in causes) else
                 "position stall" if any("waited" in c[2] for c in causes) else "no hitch, saturation or stall just before")
        counts[label] = counts.get(label, 0) + 1
        rows.append((st, text, label, [c[2] for c in causes]))
    return rows, counts


def analyse_logs(host_text, joiner_text="", minute=60.0):
    """The whole session and each minute (by the server clock) from the host's and the joiner's logs. Pure (unit-tested)."""
    hs = [s for s in parse_monet(host_text) if s["role"] == "host"]
    cs = [s for s in parse_monet(joiner_text) if s["role"] == "client"]
    hp, cp = parse_monetpos(host_text), parse_monetpos(joiner_text)
    # Two PCs line up by the wall clock (utc=, both synced by Windows time). The server clock (st=) is world time: it stops at 0.4 s a frame, so it
    # drifted 3-6 s apart between a host and a joiner that hitched (measured Oct 17); it is the fallback for logs from before utc was written.
    clock = "utc" if any(s.get("utc", 0) > 0 for s in hs + cs) else "st"
    events = parse_events(host_text, "host", clock) + parse_events(joiner_text, "joiner", clock)
    r = dict(clock=clock, whole=analyse_phase(hs, cs, hp, cp, clock=clock), minutes=[], events=sorted(events))
    keys = sorted({int(s.get(clock, -1) // minute) for s in hs + cs if s.get(clock, -1) >= 0})
    for k in keys:
        lo, hi = k * minute, (k + 1) * minute
        r["minutes"].append((lo, analyse_phase([s for s in hs if lo <= s.get(clock, -1) < hi], [s for s in cs if lo <= s.get(clock, -1) < hi], [], [])))
    r["correlation"], r["causes"] = correlate_events(events)
    r["drivers"] = sorted({m.group(0) for m in MONETDRIVER_RE.finditer(host_text + joiner_text)})
    r["has_host"], r["has_joiner"] = bool(hs), bool(cs)
    r["has_trace"] = any(p[clock] >= 0 for p in hp) and any(p[clock] >= 0 for p in cp)   # MO.Net.Trace 1 on both, with the clock in use
    return r


def report_lines(r):
    w = r["whole"]
    out = [f"[netdiag] {line}" for line in r["drivers"]]
    if not r["has_host"]:
        out.append("[netdiag] no [MONet] role=host lines: the host's log is missing (or MO.Net.AutoStats was 0 there)")
    if not r["has_joiner"]:
        out.append("[netdiag] no [MONet] role=client lines: the joiner's log is missing (or MO.Net.AutoStats was 0 there)")
    out.append(f"[netdiag] session: {w['seconds_host']} s on the host, {w['seconds_client']} s on the joiner")
    out.append(phase_line("whole session", w))
    for label, ok, ev in verdicts(w):
        if ("picture" in label) and not r["has_trace"]:
            continue   # needs MO.Net.Trace 1 on both machines
        out.append(f"[{'PASS' if ok else 'FAIL'}] {label}  -- {ev}")
    out.append(f"[netdiag] per minute ({'UTC' if r['clock'] == 'utc' else 'server clock'}):")
    for lo, m in r["minutes"]:
        if m["seconds_host"] == 0 and m["seconds_client"] == 0:
            continue   # nothing but world building that minute
        when = time.strftime("%H:%M UTC", time.gmtime(lo)) if r["clock"] == "utc" else f"{lo:7.0f}s"
        out.append(f"[netdiag]   {when}  host fps {m['host_fps']:.0f} (min {m['host_fps_min']:.0f}) joiner {m['client_fps']:.0f} (min {m['client_fps_min']:.0f}, "
                   f"worst frame {m['client_frame_max']:.0f} ms) | out {m['host_out_avg']:.0f} B/s sat {m['saturated_seconds']}s | rtt {m['rtt_avg']:.0f} ms "
                   f"loss<={m['loss_max_pct']:.1f}% | corr {m['corrections']} max {m['server_err_max']:.0f} cm, snap<={m['snap_max']:.0f} cm | "
                   f"host pawn on joiner gap<={m['client_sees_host_gap_max']:.0f} ms")
    total = sum(r["causes"].values())
    out.append(f"[netdiag] visible corrections (>= 30 cm): {total}" + (": " + ", ".join(f"{n} after a {k}" if k.endswith("hitch") or k.endswith("channel") or k.endswith("stall") else f"{n} with {k}"
                                                                         for k, n in sorted(r["causes"].items(), key=lambda kv: -kv[1])) if total else ""))
    stamp = (lambda t: time.strftime("%H:%M:%S", time.gmtime(t)) + f".{int((t % 1) * 1000):03d}") if r["clock"] == "utc" else (lambda t: f"st {t:9.2f}")
    for t, text, label, causes in r["correlation"][:20]:
        out.append(f"[netdiag]   {stamp(t)}  {text}  <- {label}" + (f" ({'; '.join(causes[:2])})" if causes else ""))
    out.append(f"[netdiag] events ({len(r['events'])}, newest last):")
    for t, who, text in r["events"][-30:]:
        out.append(f"[netdiag]   {stamp(t)}  {who:6}  {text}")
    return out


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(prog="ue_netdiag.py", description="Read the co-op diagnostics in a host's and a joiner's logs (Saved/Logs/MO57.log of each "
                                 "machine after a session): the whole session, each minute, and what came just before each visible rubber band.")
    sub = ap.add_subparsers(dest="cmd", required=True)
    lp = sub.add_parser("logs", help="read one or two logs (the roles are found in the lines)")
    lp.add_argument("logs", nargs="+")
    lp.add_argument("--json", help="also write the numbers here")
    a = ap.parse_args(argv)
    texts = {"host": "", "joiner": ""}
    for path in a.logs[:2]:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        role = "host" if "[MONet] role=host" in text else "joiner" if "[MONet] role=client" in text else None
        if role is None:
            print(f"[netdiag] {path}: no [MONet] lines (MO.Net.AutoStats 0, a Shipping build, or no co-op session in this log)")
            continue
        texts[role] += text
    r = analyse_logs(texts["host"], texts["joiner"])
    for line in report_lines(r):
        print(line)
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(r, f, indent=1, default=str)
    return 0


if __name__ == "__main__":
    sys.exit(main())
