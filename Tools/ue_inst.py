"""ue_inst - extra `-game` instances of the project, each with its own bridge, plus the networking tests built on them.

WHY: multiplayer needs more than one process. The editor's PIE runs every "client" inside ONE process, so it can
never exercise the online-subsystem path (session create / find / join). A `UnrealEditor.exe <proj> -game` process
is a real standalone game; this module launches them, gives each its own command bridge and log, and drives them
with the same `ue.py` verbs used for the editor:

    ue.py inst start host --nosteam          launch an instance (bridge dir + log under %TEMP%/claude/inst_host)
    ue.py inst do host run "MO.Session.Status"   run any ue.py command against it
    ue.py inst stop host
    ue.py nettest lan                         host + client on the Null OSS (LAN beacon), no Steam
    ue.py nettest steam-host                  one instance on real Steam: init, lobby create, listen-server travel

Isolation is two env vars, both honoured by ue.py and Content/Python/claude_bridge.py:
    MO57_BRIDGE_DIR  where ue_cmd.txt / ue_out.txt live        MO57_GAMELOG  the log to read deltas from

SAFETY: only processes this module launched (pid recorded in inst_<name>/pid.txt and image-checked) are ever
stopped, gracefully first. It never matches processes by name.
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
UE_PY = os.path.join(HERE, "ue.py")
TMP = os.path.abspath(os.environ.get("MO57_INST_DIR", os.path.join(tempfile.gettempdir(), "claude")))

# Host map for the network tests. NOT the real gameplay map: a `-game` process of the EDITOR binary crashes when
# it loads a voxel world (Voxel's WITH_EDITOR EnsureViewportIsUpToDate() calls GEditor, which is null there).
# A packaged game is unaffected. TestMap has no voxel content and uses the real BP_MOGameMode.
DEFAULT_MAP = "/Game/Penumbra/Maps/TestMap"

STATE_RE = re.compile(r"\[MOQUERY\] STATE netmode=(\w+) level=(\S+) inGame=(\S+) pawn=(\S+)")
FOUND_RE = re.compile(r"FindSessions complete: (\d+) result")


def inst_dir(name):
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
        raise ValueError(f"bad instance name {name!r} (letters, digits, - and _ only)")
    return os.path.join(TMP, f"inst_{name}")


def game_log(name):
    return os.path.join(inst_dir(name), "game.log")


def env_for(name):
    env = dict(os.environ)
    env["MO57_BRIDGE_DIR"] = inst_dir(name)
    env["MO57_GAMELOG"] = game_log(name)
    return env


def build_launch_args(uproject, log_path, pos=(0, 0), res=(640, 360), nosteam=False, extra=()):
    """Command line (after the exe) of a standalone game process. Pure: unit-tested offline.

    -forcelogflush so the log can be polled line by line; no -log (no console window to clutter the desktop);
    -NoSteam makes the Steam OSS refuse to start, so the Null OSS (LAN beacon) and the plain IP net driver
    take over -- exactly what the editor itself runs on.
    """
    args = [uproject, "-game", "-windowed", f"-ResX={res[0]}", f"-ResY={res[1]}", f"-WinX={pos[0]}",
            f"-WinY={pos[1]}", "-nosplash", "-nosound", "-forcelogflush", f"-abslog={log_path}"]
    if nosteam:
        args.append("-NoSteam")
    return args + list(extra)


def _pid_file(name):
    return os.path.join(inst_dir(name), "pid.txt")


def read_pid(name):
    try:
        with open(_pid_file(name), encoding="ascii") as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return None


def is_alive(pid):
    """True if `pid` is a running UnrealEditor.exe (the image check keeps a recycled pid from matching)."""
    if not pid:
        return False
    out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH", "/FO", "CSV"],
                         capture_output=True, text=True).stdout
    return "UnrealEditor.exe" in out


def start(name, uproject, editor_exe, **kw):
    pid = read_pid(name)
    if is_alive(pid):
        raise RuntimeError(f"instance {name!r} is already running (pid {pid}); `ue.py inst stop {name}` first")
    d = inst_dir(name)
    os.makedirs(d, exist_ok=True)
    for f in ("ue_cmd.txt", "ue_out.txt", "game.log"):
        try:
            os.remove(os.path.join(d, f))
        except OSError:
            pass
    cmd = [editor_exe] + build_launch_args(uproject, game_log(name), **kw)
    flags = getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    p = subprocess.Popen(cmd, env=env_for(name), creationflags=flags, stdin=subprocess.DEVNULL,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(_pid_file(name), "w", encoding="ascii") as f:
        f.write(str(p.pid))
    return p.pid


def call(name, ue_args, timeout=90):
    """Run `ue.py <ue_args>` against instance `name`; returns (exit_code, stdout+stderr)."""
    r = subprocess.run([sys.executable, UE_PY] + list(ue_args), env=env_for(name), capture_output=True,
                       text=True, encoding="utf-8", errors="replace", timeout=timeout)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


def log_size(name):
    try:
        return os.path.getsize(game_log(name))
    except OSError:
        return 0


def read_log_from(name, offset):
    try:
        with open(game_log(name), "rb") as f:
            f.seek(offset)
            return f.read().decode("utf-8", errors="replace")
    except OSError:
        return ""


def wait_log(name, regex, since=0, timeout=60, step=1.0):
    """Poll the instance log from byte offset `since` for `regex`; returns the match or None. Fails fast if the
    process died (a crashed instance would otherwise burn the whole timeout)."""
    pat = re.compile(regex)
    deadline = time.time() + timeout
    while time.time() < deadline:
        m = pat.search(read_log_from(name, since))
        if m:
            return m
        if not is_alive(read_pid(name)):
            return pat.search(read_log_from(name, since))
        time.sleep(step)
    return None


def wait_ready(name, timeout=240):
    """Bridge answers AND the game world exists."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if not is_alive(read_pid(name)):
            return False
        try:
            code, out = call(name, ["py", "-c", 'out("READY" if world is not None else "NOWORLD")'], timeout=20)
        except subprocess.TimeoutExpired:
            code, out = 1, ""
        # match whole lines only: the bridge also echoes the submitted code, which contains both words
        if code == 0 and re.search(r"^READY\s*$", out, re.M):
            return True
        time.sleep(3)
    return False


def console(name, cmd):
    """Run a console command in the instance; returns the log text it produced ('' if none yet)."""
    off = log_size(name)
    call(name, ["run", cmd, "--grep", "MOQUERY|MOSession", "--logwait", "1"], timeout=30)
    time.sleep(0.5)
    return read_log_from(name, off)


def state(name):
    """(netmode, level, inGame, pawn) from MO.Test.State, or None."""
    off = log_size(name)
    call(name, ["run", "MO.Test.State", "--grep", "MOQUERY", "--logwait", "1"], timeout=30)
    m = wait_log(name, STATE_RE.pattern, since=off, timeout=8, step=0.5)
    return m.groups() if m else None


def host_session(name, display, map_path, max_players=4):
    """Start hosting. With `map_path` calls UMOSessionSubsystem::HostSession(display, max, map) directly -- the very call
    the Host button's controller makes (AMOMainMenuPlayerController::HostSession just supplies its GameplayLevelPath,
    an EditDefaultsOnly property Python cannot reach). Without it, uses the console verb (the real gameplay map).
    Returns (request_accepted, evidence)."""
    if not map_path:
        console(name, f"MO.Session.Host {display} {max_players}")
        return True, "console verb (real gameplay map)"
    code, out = call(name, ["py", "-c",
                            's = unreal.MOSessionSubsystem.get(world); '
                            f'out("HOSTCALL=" + str(s.host_session("{display}", {max_players}, "{map_path}")))'], timeout=30)
    m = re.search(r"^HOSTCALL=(\w+)", out, re.M)
    return bool(m and m.group(1) == "True"), (m.group(0) if m else out.strip()[-300:])


def in_world(st, netmode, map_path):
    """Is `st` (from state()) a `netmode` world on the expected map? On the real gameplay map (no override) the
    possessed MO pawn is required too; TestMap spawns none, so there only the net mode and map are checked."""
    if not st or st[0] != netmode:
        return False
    return st[1] == map_path.rsplit("/", 1)[-1] if map_path else st[2] == "YES"


def net_driver(name, since=0):
    """Class of the GameNetDriver the instance created ('IpNetDriver', 'SteamNetDriver', ...), or None."""
    m = re.search(r"Name:GameNetDriver Def:GameNetDriver (\w+?)_\d+ ", read_log_from(name, since))
    return m.group(1) if m else None


def player_count(name):
    code, out = call(name, ["py", "-c",
                            'gs = unreal.GameplayStatics.get_game_state(world); '
                            'out("PLAYERS=%d" % (len(gs.get_editor_property("player_array")) if gs else -1))'],
                     timeout=30)
    m = re.search(r"PLAYERS=(-?\d+)", out)
    return int(m.group(1)) if m else None


def stop(name, grace=20):
    """Graceful quit via the bridge, then terminate by recorded pid. Returns 'stopped' | 'not running'."""
    pid = read_pid(name)
    if not is_alive(pid):
        return "not running"
    try:
        call(name, ["run", "quit"], timeout=15)
    except Exception:
        pass
    deadline = time.time() + grace
    while time.time() < deadline and is_alive(pid):
        time.sleep(1)
    if is_alive(pid):
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True)
        time.sleep(1)
    return "stopped"


class Report:
    def __init__(self):
        self.rows = []

    def step(self, label, ok, evidence=""):
        self.rows.append((label, ok, evidence))
        print(f"[{'PASS' if ok else 'FAIL'}] {label}" + (f"  -- {evidence}" if evidence else ""), flush=True)
        return ok

    @property
    def ok(self):
        return all(r[1] for r in self.rows)


def lan_test(uproject, editor_exe, keep=False, boot_timeout=240, map_path=DEFAULT_MAP):
    """Host + client as two standalone processes on the Null OSS (LAN beacon). Returns True on success."""
    rep = Report()
    names = ("lanhost", "lanclient")
    try:
        print("[nettest] launching host + client (Null OSS, -NoSteam) ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        host, client = names
        if not rep.step("both instances boot and expose a world",
                        all(wait_ready(n, boot_timeout) for n in names)):
            return False

        for n in names:
            call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(2)

        off = log_size(host)
        ok, ev = host_session(host, "LanTest", map_path)
        if not rep.step(f"host: HostSession accepted ({map_path or 'real gameplay map'})", ok, ev):
            return False
        created = wait_log(host, r"CreateSession succeeded", since=off, timeout=45)
        rep.step("host: CreateSession succeeded (Null OSS)", bool(created),
                 "" if created else read_log_from(host, off)[-400:])
        if not created:
            return False

        deadline, st = time.time() + 150, None
        while time.time() < deadline:
            st = state(host)
            if in_world(st, "ListenServer", map_path):
                break
            time.sleep(4)
        if not rep.step("host: world is a ListenServer on the expected map",
                        in_world(st, "ListenServer", map_path), str(st)):
            return False

        found = None
        for attempt in range(1, 5):
            off = log_size(client)
            console(client, "MO.Session.Find")
            m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
            found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
            if found:
                break
            time.sleep(3)
        if not rep.step("client: FindSessions discovers the host over LAN", bool(found),
                        f"{found} result(s)" if found is not None else "search never completed"):
            return False

        console(client, "MO.Session.Join 0")
        deadline, cst = time.time() + 180, None
        while time.time() < deadline:
            cst = state(client)
            if in_world(cst, "Client", map_path):
                break
            time.sleep(4)
        if not rep.step("client: joined and is a Client on the host's map",
                        in_world(cst, "Client", map_path), str(cst)):
            return False

        time.sleep(3)
        hp, cp = player_count(host), player_count(client)
        if not rep.step("replication: host and client both see 2 players", hp == 2 and cp == 2,
                        f"host sees {hp}, client sees {cp}"):
            return False

        return _rehost_after_menu(rep, host, client, map_path)
    finally:
        print(f"[nettest] lan: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not keep:
            for n in names:
                stop(n)
        else:
            print("[nettest] --keep: instances left running (ue.py inst stop lanhost / lanclient)")


MENU_MAP = "/Game/Penumbra/Maps/LoadingLevel"


def _possession_menu_rpcs(rep, host, client):
    """What the possession menu does for a REMOTE client: ask the server for the pawn list, then ask it to put the client
    into a spare colonist. Driven by MO.Possess.List / MO.Possess.Take, which defer a tick -- NEVER by calling the
    component from editor-Python: under the editor script guard every RPC maps to LOCAL callspace, so the "server"
    request would just run on the client and prove nothing (it did, and cost a detour)."""
    # the list round trip: the SERVER must log that it built the list for the client's controller
    host_off, client_off = log_size(host), log_size(client)
    console(client, "MO.Possess.List")
    built = wait_log(host, r"\[MOPossession\] list for \S+: (\d+) entr", since=host_off, timeout=40)
    got = wait_log(client, r"received (\d+) possession list entr", since=client_off, timeout=40)
    rep.step("possession menu: the server builds the pawn list for the remote client and it arrives",
             bool(built) and bool(got) and built.group(1) == got.group(1) and int(got.group(1)) >= 1,
             f"server built {built.group(1) if built else '-'}, client received {got.group(1) if got else '-'}")

    # a spare, recruited colonist for the client to take
    off = log_size(host)
    console(host, "MO.Colony.SpawnSurvivor 500")
    spawned = wait_log(host, r"COLONY SpawnSurvivor (\S+)", since=off, timeout=20)
    name = spawned.group(1).strip() if spawned else None
    if not rep.step("host: spawned a spare survivor to take", bool(name) and name != "FAILED", str(name)):
        return
    console(host, f"MO.Colony.Recruit {name}")
    time.sleep(2)
    out = _probe(host, "ps = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) "
                       f"if p.get_name() == '{name}']; "
                       "out('SPAREGUID=' + (unreal.GuidLibrary.conv_guid_to_string(ps[0].get_component_by_class("
                       "unreal.MOIdentityComponent).get_or_create_guid()) if ps else 'none'))")
    m = re.search(r"SPAREGUID=([0-9A-Fa-f]{32})", out)
    if not rep.step("host: spare survivor has a GUID", bool(m), m.group(1) if m else out.strip()[-200:]):
        return

    # the client asks the server to put him in it -- the server must receive, validate and apply it
    off = log_size(host)
    console(client, f"MO.Possess.Take {m.group(1)}")
    asked = wait_log(host, r"\[MOPlayerController\] \S+ asked to possess pawn", since=off, timeout=40)
    did = wait_log(host, r"\[MOPossession\] \S+ possessed (\S+)", since=off, timeout=40)
    rep.step("possession menu: the remote client's possess request reaches the server and is applied",
             bool(asked) and bool(did) and did.group(1) == name,
             f"server: {'received' if asked else 'NOT received'}, possessed {did.group(1) if did else '-'} (wanted {name})")
    out = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                       "out('CLIENTPAWNONHOST=' + (p.get_name() if p else 'none'))")
    rep.step("host: the client's controller now drives the chosen pawn", f"CLIENTPAWNONHOST={name}" in out,
             (re.search(r"CLIENTPAWNONHOST=\S+", out) or re.search(r"$", out)).group(0))


def _wait_state(name, netmode, map_path, timeout):
    deadline, st = time.time() + timeout, None
    while time.time() < deadline:
        st = state(name)
        if in_world(st, netmode, map_path):
            return True, st
        time.sleep(3)
    return False, st


def _rehost_after_menu(rep, host, client, map_path):
    """The stale-session scenario: both players quit to the main menu (same OpenLevel the in-game menu uses), then the
    same two processes host and join AGAIN. Before the fix the old session stayed registered, so the second host
    failed with "Already in a session" until the whole game was restarted."""
    offs = {n: log_size(n) for n in (host, client)}
    for n in (client, host):
        call(n, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{MENU_MAP}")'], timeout=30)
    ok = True
    for n in (host, client):
        reached, st = _wait_state(n, "Standalone", MENU_MAP, 120)
        ok = rep.step(f"{n}: back at the main menu", reached, str(st)) and ok
    if not ok:
        return False
    for n in (host, client):
        call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)

    released = wait_log(host, r"ReleaseStaleSession: leaving a session|DestroySession 'GameSession' complete", since=offs[host], timeout=20)
    if not rep.step("host: the session was destroyed on returning to the menu", bool(released),
                    released.group(0) if released else "no teardown logged"):
        return False

    off = log_size(host)
    accepted, ev = host_session(host, "LanTest2", map_path)
    created = wait_log(host, r"CreateSession succeeded|Already in a session|already active", since=off, timeout=45)
    if not rep.step("host: can host a second session after quitting to the menu",
                    accepted and bool(created) and "succeeded" in created.group(0),
                    created.group(0) if created else ev):
        return False
    reached, st = _wait_state(host, "ListenServer", map_path, 150)
    if not rep.step("host: second session is a ListenServer", reached, str(st)):
        return False

    found = None
    for _ in range(4):
        off = log_size(client)
        console(client, "MO.Session.Find")
        m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
        found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
        if found:
            break
        time.sleep(3)
    if not rep.step("client: finds the second session (and only it, not a ghost of the first)", found == 1,
                    f"{found} result(s)"):
        return False
    console(client, "MO.Session.Join 0")
    reached, st = _wait_state(client, "Client", map_path, 180)
    return rep.step("client: joins the second session", reached, str(st))


# One line on purpose: the bridge takes one command per line, so a for-loop (or any block) cannot be sent.
PROBE_CONTROLLERS = (
    "pcs = [unreal.GameplayStatics.get_player_controller(world, i) for i in range(6)]; "
    'out("PCS=%d WITHPAWN=%d" % (len([p for p in pcs if p]), '
    'len([i for i in range(6) if unreal.GameplayStatics.get_player_pawn(world, i)])))')
PROBE_PAWNS = (
    "cls = getattr(unreal, 'MOCharacter', unreal.Pawn)\n"
    'out("PAWNS=%d" % len(unreal.GameplayStatics.get_all_actors_of_class(world, cls)))')
PROBE_Z = (
    "p = unreal.GameplayStatics.get_player_pawn(world, 0)\n"
    'out("Z=%f" % (p.get_actor_location().z if p else -999999.0))')
PROBE_Z_PLAYER1 = (
    "p = unreal.GameplayStatics.get_player_pawn(world, 1)\n"
    'out("Z=%f" % (p.get_actor_location().z if p else -999999.0))')
PROBE_XY = (
    "p = unreal.GameplayStatics.get_player_pawn(world, 0)\n"
    "l = p.get_actor_location() if p else unreal.Vector(0, 0, 0)\n"
    'out("X=%f Y=%f Z=%f" % (l.x, l.y, l.z))')

SEED_RE = r"\[MOQUERY\] VOXEL Seed has=(\d) seed=(-?\d+) ready=(\d) netmode=(\d)"
SURFACE_RE = r"\[MOQUERY\] VOXEL SurfaceZ x=(-?\d+) y=(-?\d+) hit=(\d) z=(-?[\d.]+) skipped=(\d+) firstOther=(\S+)(?: comp=(\S+))?"


def _seed_query(name):
    off = log_size(name)
    console(name, "MO.Voxel.Seed")
    m = wait_log(name, SEED_RE, since=off, timeout=15)
    return (int(m.group(1)), int(m.group(2)), int(m.group(3))) if m else None


def _surface_z(name, x, y, z_hi, z_lo):
    off = log_size(name)
    console(name, f"MO.Voxel.SurfaceZ {x:.0f} {y:.0f} {z_hi:.0f} {z_lo:.0f}")
    m = wait_log(name, SURFACE_RE, since=off, timeout=15)
    return (int(m.group(3)), float(m.group(4)), int(m.group(5)), m.group(6), m.group(7)) if m else None


GRID_RE = r"\[MOQUERY\] VOXEL SurfaceGrid cx=(-?\d+) cy=(-?\d+) half=(\d+) step=(\d+) hits=(\d+)/(\d+) z=(\S+)"


def _grid(name, cx, cy, half, step, z_hi, z_lo):
    off = log_size(name)
    console(name, f"MO.Voxel.SurfaceGrid {cx:.0f} {cy:.0f} {half} {step} {z_hi:.0f} {z_lo:.0f}")
    m = wait_log(name, GRID_RE, since=off, timeout=40)
    if not m:
        return None
    return [None if t == "x" else float(t) for t in m.group(7).split(";") if t]


def compare_grids(host, client):
    """Signed (client - host) height differences over the points both machines have. Pure function (unit-tested)."""
    diffs = [None if h is None or c is None else c - h for h, c in zip(host, client)]
    have = sorted(abs(d) for d in diffs if d is not None)
    signed = [d for d in diffs if d is not None]
    return dict(n=len(have), points=len(diffs),
                mean_signed=(sum(signed) / len(signed)) if signed else float("nan"),
                median_abs=have[len(have) // 2] if have else float("inf"),
                max_abs=have[-1] if have else float("inf"),
                exact=sum(1 for a in have if a <= 5.0), diffs=diffs)


def _terrain_grid(rep, host, client, x0, y0, z_hi, z_lo, half=5, step=200):
    """Many heights, not three: if host and client ground differ, WHERE and BY HOW MUCH (a constant offset, a slope-related
    shift, local features) says which layer is responsible. Reports a compact signed (client - host) map in cm."""
    gh = _grid(host, x0, y0, half, step, z_hi, z_lo)
    gc = _grid(client, x0, y0, half, step, z_hi, z_lo)
    if not gh or not gc or len(gh) != len(gc):
        rep.step("terrain grid sampled on both machines", False, f"host {len(gh or [])} pts, client {len(gc or [])} pts")
        return
    a = compare_grids(gh, gc)
    side = 2 * half + 1
    print(f"[nettest]   terrain grid ({side}x{side}, {step} cm apart): both have {a['n']}/{a['points']} points; "
          f"signed mean (client-host) {a['mean_signed']:.1f} cm, median |d| {a['median_abs']:.1f}, max |d| {a['max_abs']:.1f}, "
          f"within 5 cm: {a['exact']}", flush=True)
    for r in range(side):
        row = a["diffs"][r * side:(r + 1) * side]
        print("[nettest]     " + " ".join("  x" if d is None else f"{d:+4.0f}" for d in row), flush=True)
    rep.step("terrain grid: client and host heights agree over the whole patch (median |d| <= 5 cm, max <= 50 cm)",
             a["n"] >= 0.8 * a["points"] and a["median_abs"] <= 5.0 and a["max_abs"] <= 50.0,
             f"n={a['n']}/{a['points']} mean(c-h)={a['mean_signed']:.1f} median|d|={a['median_abs']:.1f} max|d|={a['max_abs']:.1f}")


def _terrain_settle(rep, host, client, x0, y0, z_hi, z_lo, samples=8):
    """WHEN does each machine's ground collision become final? The 3-spot check and the grid taken a few seconds later
    disagreed with each other on the same machines (319 cm apart, then 0.0 cm): collision keeps changing for a while after
    `ready`. Samples a small grid on both machines repeatedly and prints, per sample, each machine's mean height and the
    worst client-host difference, so the log shows who moves and for how long."""
    series = []
    t0 = time.time()
    def standing_on(label):
        for who, name in (("host", host), ("client", client)):
            r = _surface_z(name, x0, y0, z_hi, z_lo)
            print(f"[nettest]   {label}: {who} ground at the pawn spot = {r[1] if r and r[0] else None} cm on "
                  f"{r[4] if r and r[0] else 'no voxel collision'}", flush=True)
    standing_on("at 'ready'")
    for _ in range(samples):
        gh = _grid(host, x0, y0, 1, 200, z_hi, z_lo)
        gc = _grid(client, x0, y0, 1, 200, z_hi, z_lo)
        t = time.time() - t0
        if not gh or not gc or len(gh) != len(gc):
            series.append((t, None, None, None))
            continue
        a = compare_grids(gh, gc)
        mh = [h for h in gh if h is not None]
        mc = [c for c in gc if c is not None]
        series.append((t, sum(mh) / len(mh) if mh else None, sum(mc) / len(mc) if mc else None, a["max_abs"]))
    standing_on("after settling")
    for t, mh, mc, mx in series:
        print(f"[nettest]   settle t+{t:5.1f}s  host mean z {mh if mh is None else round(mh)}  client mean z "
              f"{mc if mc is None else round(mc)}  worst |c-h| {mx if mx is None else round(mx)} cm", flush=True)
    worst = [s[3] for s in series if s[3] is not None]
    final_ok = bool(worst) and worst[-1] <= 5.0
    first_bad = next((s[0] for s in series if s[3] is not None and s[3] > 5.0), None)
    rep.step("ground collision settles to identical on host and client", final_ok,
             f"first sample worst {worst[0] if worst else '?'} cm, last {worst[-1] if worst else '?'} cm"
             + (f"; mismatch seen at t+{first_bad:.0f}s after 'ready'" if first_bad is not None else "; never mismatched"))


CLOCK_RE = r"GameDateTime=(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d)"
WEATHER_RE = r"\[MOWeather\] ([^|\n]*)\| Cloud=([\d.]+)"


def _game_clock(name):
    """The instance's game DateTime as seconds-of-day plus a day number (a datetime would also do; this keeps it dependency-free)."""
    off = log_size(name)
    console(name, "MO.Clock.Info")
    m = wait_log(name, CLOCK_RE, since=off, timeout=15)
    if not m:
        return None
    y, mo, d, h, mi, sec = (int(x) for x in m.groups())
    import datetime
    return datetime.datetime(y, mo, d, h, mi, sec)


def _weather_label(name):
    off = log_size(name)
    console(name, "MO.Weather.Info")
    m = wait_log(name, WEATHER_RE, since=off, timeout=15)
    return m.group(1).strip() if m else None


SKYTIME_RE = r"\[MOWeather\] SkyTime=(\d\d):(\d\d):(\d\d)"


def _sky_hour(name):
    """The hour (0-24, fractional) the SKY is actually showing, asked of the weather bridge (MO.Weather.SkyTime -> BP_WeatherBridge ::
    GetDateTime, which reads the Ultra Dynamic Sky actor) -- not the game clock's label. A clock can agree while the sky is still on
    another time of day; the sky is what a player looks at."""
    off = log_size(name)
    console(name, "MO.Weather.SkyTime")
    m = wait_log(name, SKYTIME_RE, since=off, timeout=15)
    return int(m.group(1)) + int(m.group(2)) / 60.0 + int(m.group(3)) / 3600.0 if m else None


def _skies_agree(host, client, tolerance_h=0.5, wait_s=30.0):
    """Poll until the client's sky is within `tolerance_h` hours of the host's (circular: 23.9 vs 0.1 is close). -> (agreed, host_h, client_h)"""
    deadline, h, c = time.time() + wait_s, None, None
    while True:
        h, c = _sky_hour(host), _sky_hour(client)
        if h is not None and c is not None:
            d = abs(h - c)
            if min(d, 24.0 - d) <= tolerance_h:
                return True, h, c
        if time.time() > deadline:
            return False, h, c
        time.sleep(2)


def _clocks_agree(host, client, tolerance_s=8.0, wait_s=30.0):
    """Poll until the client's clock is within `tolerance_s` of the host's (the two console queries are a couple of real seconds
    apart, which is the floor on how tight this can be measured). Returns (agreed, host_time, client_time)."""
    deadline, h, c = time.time() + wait_s, None, None
    while True:
        c = _game_clock(client)
        h = _game_clock(host)
        if h and c and abs((h - c).total_seconds()) <= tolerance_s:
            return True, h, c
        if time.time() > deadline:
            return False, h, c
        time.sleep(2)


def _weathers_agree(host, client, wait_s=90.0):
    deadline, h, c = time.time() + wait_s, None, None
    while True:
        h, c = _weather_label(host), _weather_label(client)
        if h and c and h == c:
            return True, h, c
        if time.time() > deadline:
            return False, h, c
        time.sleep(5)


BRIDGE_FOLLOW_RE = r"\[MOWorldSync\] client following the host's weather preset (\S+)"
BRIDGE_DISPATCH_RE = r"\[MOWeatherIntegration\] SetWeatherPreset: PresetObject=(\S+) .*dispatching to provider"


def _world_sync(rep, host, client, host_set_before_join, join_off=0, expect_disagreement=False):
    """Co-op clients used to run a private day and sky. The host set a time and weather BEFORE the client joined (so the client
    must adopt them on arrival), then changes both AFTER (a time skip and a weather change must reach the client live)."""
    if expect_disagreement:
        # NEGATIVE CONTROL (host withholds): the client runs its own clock and sky, so it must NOT match the host's. Short waits:
        # we are proving the check can fail, not waiting for something to converge.
        ok_c, h, c = _clocks_agree(host, client, wait_s=6.0)
        rep.step("NEGATIVE CONTROL: with the host's publish withheld the client's clock does NOT match the host's", not ok_c, f"host {h}, client {c}")
        ok_s, hs, cs = _skies_agree(host, client, wait_s=6.0)
        rep.step("NEGATIVE CONTROL: ... and the SKY's time of day does NOT match the host's", not ok_s,
                 f"host sky {hs if hs is None else round(hs, 2)} h, client sky {cs if cs is None else round(cs, 2)} h")
        # Weather LABEL: not asserted here. The Ultra Dynamic Weather actor replicates by itself and its Change Weather is server-only
        # (measured: with the native replication switched off the client dispatched the host's preset to the bridge and stayed on its
        # own weather), so the label follows the host even when the MO publish is withheld. What the withhold CAN prove is that the MO
        # weather path did not run.
        ok_w, hl, cl = _weathers_agree(host, client, wait_s=15.0)
        print(f"[nettest]   (info) publish withheld: host weather '{hl}', client '{cl}' -- "
              f"{'the client follows anyway (UDS replicates the weather itself)' if ok_w else 'the client does not follow'}", flush=True)
        cl_log = read_log_from(client, join_off)
        follow = re.search(BRIDGE_FOLLOW_RE, cl_log)
        rep.step("NEGATIVE CONTROL: with the publish withheld the client never ran the MO weather sync", not follow,
                 follow.group(0) if follow else "no '[MOWorldSync] client following' line on the client")
        return
    ok, h, c = _clocks_agree(host, client)
    rep.step(f"client joined mid-day and runs the host's clock (host set {host_set_before_join[0]} before it joined)", ok,
             f"host {h}, client {c}")
    ok, hs, cs = _skies_agree(host, client)
    rep.step("client's SKY shows the host's time of day after a mid-day join (the sky, not just the clock label)", ok,
             f"host sky {hs if hs is None else round(hs, 2)} h, client sky {cs if cs is None else round(cs, 2)} h")
    ok, hl, cl = _weathers_agree(host, client)
    rep.step("client shows the weather the host set before it joined", ok and hl.lower() != "clear skies", f"host '{hl}', client '{cl}'")
    cl_log = read_log_from(client, join_off)
    follow, dispatch = re.search(BRIDGE_FOLLOW_RE, cl_log), re.search(BRIDGE_DISPATCH_RE, cl_log)
    rep.step("the MO weather sync handed the host's preset to the client's weather bridge (sync -> SetWeatherPreset -> BP_WeatherBridge)",
             bool(follow) and bool(dispatch) and follow.group(1).split(".")[0].split("/")[-1] == dispatch.group(1).split(".")[0].split("/")[-1],
             f"{follow.group(0) if follow else 'no follow line'}; {dispatch.group(0)[:110] if dispatch else 'no dispatch line'}")

    console(host, "MO.Clock.SetTime 20 30")  # a skip: the clock JUMPS
    ok, h, c = _clocks_agree(host, client, wait_s=20.0)
    rep.step("a host time skip (14:00 -> 20:30) reaches the client", ok and h.hour == 20, f"host {h}, client {c}")
    ok, hs, cs = _skies_agree(host, client, wait_s=20.0)
    rep.step("... and the client's SKY follows the skip too (day -> night)", ok and hs is not None and 19.5 <= hs <= 21.5,
             f"host sky {hs if hs is None else round(hs, 2)} h, client sky {cs if cs is None else round(cs, 2)} h")

    console(host, "MO.Weather.SetPreset Clear_Skies")
    ok, hl, cl = _weathers_agree(host, client)
    rep.step("a host weather change (-> Clear_Skies) reaches the client", ok and "clear" in hl.lower(), f"host '{hl}', client '{cl}'")


def _terrain_agreement(rep, host, client, client_join_off, withheld=False, host_since=0, expected_seed=None, skewed=False):
    """Every machine generates its OWN voxel terrain, so a client only stands on the host's ground if it generated it from
    the HOST's seed. Checks the seed handoff in the logs, then measures the ground itself: the surface height at the same
    XY on both machines. (Judging the client by its own pawn's z cannot see this bug: a pawn resting on the client's
    different terrain looks perfectly stable locally.)"""
    published = re.search(r"\[MOWorldSeed\] host published world seed (-?\d+)", read_log_from(host, host_since))
    if withheld:
        rep.step("NEGATIVE CONTROL: the host really withheld its seed", not published,
                 "no publish line" if not published else published.group(0))
        host_seed = None
    else:
        rep.step("host: published its world seed to the clients", bool(published),
                 published.group(0) if published else "no '[MOWorldSeed] host published' line")
        if not published:
            return
        host_seed = int(published.group(1))
        if expected_seed is not None:
            rep.step("host: the world runs on the seed stored in the save", host_seed == expected_seed,
                     f"saved {expected_seed}, published {host_seed}")

    applied = wait_log(client, r"\[MOWorldSeed\] client applied world seed (-?\d+): voxel runtime created",
                       since=client_join_off, timeout=(10 if withheld else 60))
    deferred = re.search(r"(\d+) AVoxelWorld\(s\) in the level; voxel runtime creation deferred on (\d+)", read_log_from(client, client_join_off))
    if withheld:
        rep.step("NEGATIVE CONTROL: the client never received a seed", not applied,
                 f"client applied {applied.group(1) if applied else 'nothing'}")
    else:
        rep.step("client: received the host's seed and generated terrain from it",
                 bool(applied) and int(applied.group(1)) == host_seed,
                 f"host seed {host_seed}, client applied {applied.group(1) if applied else 'NOTHING'}"
                 f" (client: {deferred.group(1) if deferred else '?'} AVoxelWorld(s), deferred its own runtime on {deferred.group(2) if deferred else '?'})")

    if withheld:
        return  # a client with no seed never creates a runtime (by design): there is no ground to compare

    cq = None
    deadline = time.time() + 180
    while time.time() < deadline:
        cq = _seed_query(client)
        if cq and cq[2]:
            break
        time.sleep(4)
    hq = _seed_query(host)
    if skewed:
        rep.step("NEGATIVE CONTROL: the client's terrain was generated from a DIFFERENT seed than the host's",
                 bool(cq) and bool(hq) and cq[2] == 1 and cq[0] == 1 and cq[1] != hq[1],
                 f"host {hq}, client {cq} (has, seed, ready)")
    else:
        rep.step("client: voxel terrain ready, generated from the same seed as the host",
                 bool(cq) and bool(hq) and cq[2] == 1 and cq[0] == 1 and cq[1] == hq[1] == host_seed,
                 f"host {hq}, client {cq} (has, seed, ready)")

    hxy = _probe(host, PROBE_XY)
    mx, my, mz = (re.search(rf"{k}=(-?[\d.]+)", hxy) for k in "XYZ")
    if not rep.step("host: located the host pawn to sample ground around it", bool(mx and my and mz),
                    f"x={mx.group(1) if mx else '?'} y={my.group(1) if my else '?'} z={mz.group(1) if mz else '?'}"):
        return
    x0, y0, z0 = float(mx.group(1)), float(my.group(1)), float(mz.group(1))
    z_hi, z_lo = 30000.0, -30000.0  # wide on purpose: a client on different terrain may be far above/below; non-voxel hits are skipped by class
    if not skewed:
        _terrain_settle(rep, host, client, x0, y0, z_hi, z_lo)
    rows, worst, all_hit = [], 0.0, True
    for dx, dy in ((0, 0), (400, 0), (0, 400)):
        zh = _surface_z(host, x0 + dx, y0 + dy, z_hi, z_lo)
        zc = None
        for _ in range(8):  # client collision builds a little after "ready"
            zc = _surface_z(client, x0 + dx, y0 + dy, z_hi, z_lo)
            if zc and zc[0]:
                break
            time.sleep(4)
        rows.append((zh, zc))
        if zh and zc and zh[0] and zc[0]:
            worst = max(worst, abs(zh[1] - zc[1]))
        else:
            all_hit = False
            worst = float("inf")
    detail = "; ".join(f"host z={r[0][1]:.0f}/client z={r[1][1]:.0f}" if r[0] and r[1] and r[0][0] and r[1][0]
                       else f"host {r[0]} client {r[1]}" for r in rows)
    if skewed:
        # In general a missing hit is "no data", never "different". Here the client is ALREADY verified ready on a different
        # seed (the step above), and since the collision invoker its collision exists only within ~30 m of its pawn while the
        # skewed terrain's surface can be ~150 m away: "the host has ground at this spot and the client has none" is then real
        # evidence that the ground differs. Accept either a measured height difference or that.
        host_hits = sum(1 for r in rows if r[0] and r[0][0])
        client_hits = sum(1 for r in rows if r[1] and r[1][0])
        measured = all_hit and worst > 50.0
        disjoint = host_hits == len(rows) and client_hits == 0
        rep.step("NEGATIVE CONTROL: with a skewed seed the client's ground differs from the host's", measured or disjoint,
                 (f"measured |dz| {worst:.0f} cm" if measured else
                  f"host has ground at {host_hits}/{len(rows)} spots, client at {client_hits}/{len(rows)} (its terrain is elsewhere)")
                 + f"; {detail}")
    else:
        rep.step("client and host agree on the ground height at the same spots (<= 50 cm)", worst <= 50.0 and all_hit,
                 f"worst |dz| = {worst:.1f} cm; {detail}")
        _terrain_grid(rep, host, client, x0, y0, z_hi, z_lo)


def _probe(name, code, timeout=30):
    return call(name, ["py", "-c", code], timeout=timeout)[1]


def _num(text, key):
    m = re.search(rf"^{key}=(-?[\d.]+)", text, re.M)
    return float(m.group(1)) if m else None


NETWALK_RE = re.compile(r"\[NETWALK\] t=([\d.]+) role=(\w+) x=(-?\d+) y=(-?\d+) z=(-?[\d.]+) mode=(\w+) speed=(\d+)")
WALK_PY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Python")


def _walk_samples(text, role):
    return [dict(t=float(m.group(1)), x=int(m.group(3)), y=int(m.group(4)), z=float(m.group(5)), mode=m.group(6),
                 speed=int(m.group(7))) for m in NETWALK_RE.finditer(text) if m.group(2) == role]


def analyse_walk(client, server, max_dt=0.3):
    """Align the client's samples with the server's by time. Pure function (unit-tested)."""
    pairs = []
    for c in client:
        near = min(server, key=lambda s: abs(s["t"] - c["t"]), default=None)
        if near is not None and abs(near["t"] - c["t"]) <= max_dt:
            pairs.append((c, near))
    dz = sorted(abs(c["z"] - s["z"]) for c, s in pairs)
    walked = 0.0
    if len(client) >= 2:
        walked = ((client[-1]["x"] - client[0]["x"]) ** 2 + (client[-1]["y"] - client[0]["y"]) ** 2) ** 0.5
    def frac(samples):
        return sum(1 for s in samples if "FALLING" in s["mode"].upper()) / len(samples) if samples else 0.0
    return dict(pairs=len(pairs), walked_cm=walked, max_dz=dz[-1] if dz else float("inf"),
                median_dz=dz[len(dz) // 2] if dz else float("inf"),
                client_falling=frac(client), server_falling=frac(server))


def _walk_client(rep, host, client):
    """WALK the joiner and compare the client's view of the pawn with the server's, sample by sample. A pawn standing
    still can hide a client with no ground (it sat 'stable' at z=43 on a client with no terrain at all); walking onto
    fresh ground is where a missing or different client collision shows: falling frames, z disagreement, rubber-banding."""
    import threading
    out = {}
    def run(name, script):
        out[name] = call(name, ["seq", os.path.abspath(os.path.join(WALK_PY, script))], timeout=120)
    off_c, off_h = log_size(client), log_size(host)
    threads = [threading.Thread(target=run, args=(host, "nettest_observe.py")),
               threading.Thread(target=run, args=(client, "nettest_walk.py"))]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=150)
    time.sleep(1)
    cs = _walk_samples(read_log_from(client, off_c), "client")
    ss = _walk_samples(read_log_from(host, off_h), "server")
    a = analyse_walk(cs, ss)
    evidence = (f"{len(cs)} client / {len(ss)} server samples, {a['pairs']} aligned; walked {a['walked_cm']:.0f} cm; "
                f"|dz| median {a['median_dz']:.0f} max {a['max_dz']:.0f} cm; falling: client {a['client_falling']:.0%}, "
                f"server {a['server_falling']:.0%}")
    if not (a["pairs"] >= 10 and a["walked_cm"] >= 500):
        # say WHY: the sequence runner's own output (a failed/never-started sequence prints a traceback or a timeout)
        for who, label in ((client, "client"), (host, "host")):
            res = out.get(who)
            evidence += f" | {label} seq -> rc={res[0] if res else '?'}: {(res[1] if res else '')[-260:]!r}"
        # ... and the pawn's own state. Once a client walked 0 cm in 13 s (turning 5 times, so not a wall in front): a pawn that cannot
        # move in ANY direction is input ignored by an open menu, a capsule stuck in geometry, or movement disabled.
        diag = _probe(client, "p = unreal.GameplayStatics.get_player_pawn(world, 0); c = p.get_controller() if p else None; "
                              "cm = p.get_component_by_class(unreal.CharacterMovementComponent) if p else None; "
                              "out('PAWNDIAG pawn=%s ctrl=%s ignore_move=%s mode=%s maxspeed=%s ongnd=%s overlaps=%d' % ("
                              "p.get_name() if p else None, c.get_name() if c else None, c.is_move_input_ignored() if c else None, "
                              "cm.movement_mode if cm else None, cm.get_max_speed() if cm else None, cm.is_moving_on_ground() if cm else None, "
                              "len(p.get_overlapping_actors()) if p else -1))")
        m_d = re.search(r"PAWNDIAG [^\r\n]*", diag)
        evidence += " | " + (m_d.group(0) if m_d else f"pawn diagnostic failed: {diag.strip()[-160:]}")
    if not rep.step("walk test ran: the client moved and both machines logged it", a["pairs"] >= 10 and a["walked_cm"] >= 500, evidence):
        return
    rep.step("walking client: its view of its pawn stays on the server's ground (|dz| <= 150 cm, rarely falling)",
             a["max_dz"] <= 150.0 and a["client_falling"] <= 0.25, evidence)


PROBE_XYZ_PLAYER1 = (
    "p = unreal.GameplayStatics.get_player_pawn(world, 1)\n"
    "l = p.get_actor_location() if p else unreal.Vector(0, 0, 0)\n"
    'out("X=%f Y=%f Z=%f" % (l.x, l.y, l.z))')


def _pawn1_xyz(host):
    out = _probe(host, PROBE_XYZ_PLAYER1)
    ms = [re.search(rf"{k}=(-?[\d.]+)", out) for k in "XYZ"]
    return tuple(float(m.group(1)) for m in ms) if all(ms) else None


SPAWN_RE = r"\[SpawnManager\] (?:Category \d+ first spawn|Fallback spawned|Spawned)"


def _client_spawns_nothing(rep, host, client, host_since, client_since):
    """Spawning is the server's job. Found in the packaged smoke test: the spawn manager ran on the CLIENT too and filled its copy of the
    world with creatures that exist nowhere else ("Wolf_C: NO CONTROLLER"). Control first: the HOST's spawn manager must have started
    (else a silent client proves nothing); then the client's log since it joined must hold no spawn line."""
    started = wait_log(host, SPAWN_RE, since=host_since, timeout=60)
    rep.step("CONTROL: the host's spawn manager is running (so a silent client is meaningful)", bool(started),
             started.group(0) if started else "no [SpawnManager] spawn line on the host")
    time.sleep(10)
    bad = re.search(SPAWN_RE + r"[^\n]*", read_log_from(client, client_since))
    rep.step("client: its spawn manager does not spawn (the server's creatures arrive by replication)", not bad,
             bad.group(0)[:160] if bad else "no spawn line on the client")


def _embedded_pawn(rep, host, client):
    """Reported by Wes: the fall check / spawns put a pawn INTO a tree and it is stuck there. Wrap the joiner's pawn in a block and PIN it
    (MO.Test.EmbedPawn sets MOVE_None: character movement's own depenetration frees a pawn from a block this size in some runs and not
    in others -- 22 cm in one, 829 cm in the next -- so unpinned, the test could not tell the rescue from luck) and require the server
    to free it. A control first disables the rescue, to prove the pinned pawn really does stay put without it."""
    start = _pawn1_xyz(host)
    if not rep.step("host: located the joiner's pawn (for the embedded test)", start is not None, str(start)):
        return

    def dist_from(a, b):
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5

    # control: rescue off -> the pawn must still be inside the block after 10 s
    console(host, "MO.EmbeddedRescue.Disable 1")
    console(host, "MO.Test.EmbedPawn 1 800")
    time.sleep(10)
    now = _pawn1_xyz(host)
    rep.step("CONTROL: with the embedded rescue disabled the pawn stays stuck inside the block", now is not None and dist_from(now, start) < 300.0,
             f"moved {dist_from(now, start):.0f} cm" if now else "no pawn")

    # the real thing: rescue on -> freed within ~15 s (3 s to confirm it is stuck + the move)
    off = log_size(host)
    console(host, "MO.EmbeddedRescue.Disable 0")
    # either rescue outcome is logged with this prefix; "and no clear ground" is a failure to find a spot, not a rescue
    stuck = wait_log(host, r"is stuck inside ([^\n]*)", since=off, timeout=30)
    freed = stuck if stuck and " -- moving it from" in stuck.group(0) else None
    time.sleep(2)
    end = _pawn1_xyz(host)
    rep.step("the server frees a pawn stuck in solid geometry", bool(freed) and end is not None and dist_from(end, start) > 800.0,
             f"moved {dist_from(end, start):.0f} cm; host logged: {stuck.group(0)[:160] if stuck else 'no stuck line at all'}" if end else "no pawn")
    console(host, "MO.Test.ClearEmbed")


def _far_remote_pawn(rep, host, client, distance_cm=15000.0):
    """Voxel collision exists only where the Voxel world has an invoker. The host's invoker is its own camera, so unless
    remote players' pawns are invokers too, a remote player who walks away from the host has NO ground under them on the
    SERVER -- which owns every pawn's position -- and falls (then gets teleported by the fall-through rescue).

    Teleports the joiner's pawn (on the server) 40 m above the host pawn's height at a spot `distance_cm` from the host (a
    short drop, so the fall-through rescue stays out of it), lets it fall, and compares the server's and the client's view
    of where it ended up."""
    hxy = _probe(host, PROBE_XY)
    mx, my, mz = (re.search(rf"{k}=(-?[\d.]+)", hxy) for k in "XYZ")
    if not rep.step("host: located the host pawn (origin for the far-away test)", bool(mx and my and mz),
                    f"x={mx.group(1) if mx else '?'} y={my.group(1) if my else '?'} z={mz.group(1) if mz else '?'}"):
        return
    tx, ty, tz = float(mx.group(1)) + distance_cm, float(my.group(1)), float(mz.group(1)) + 4000.0
    off = log_size(host)
    who = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); out('PAWNNAME=' + (p.get_name() if p else 'none'))")
    mname = re.search(r"PAWNNAME=(\S+)", who)
    pawn_name = mname.group(1) if mname else "none"
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 f"p.set_actor_location(unreal.Vector({tx:.0f}, {ty:.0f}, {tz:.0f}), False, True)")
    samples = []
    for _ in range(8):
        time.sleep(4)
        zs = _num(_probe(host, PROBE_Z_PLAYER1), "Z")
        zc = _num(_probe(client, PROBE_Z), "Z")
        samples.append((zs, zc))
    # ONLY the joiner's rescues: the host's log also carries every NPC colonist/creature that has no ground collision around it
    # (those fall and are rescued continuously -- a known design question, not this test's subject).
    rescued = len(re.findall(rf"\[MOCharacter\] {re.escape(pawn_name)}: Fall-through detected!", read_log_from(host, off)))
    last_s, last_c = samples[-1]
    ok = (last_s is not None and last_c is not None and abs(last_s - last_c) <= 150.0
          and -5000.0 < last_s < 24000.0 and rescued == 0)
    rep.step(f"a remote player {distance_cm / 100:.0f} m from the host stands on ground the SERVER also has "
             "(no fall-through, server and client agree)", ok,
             "server/client z (cm): " + ", ".join(f"{s:.0f}/{c:.0f}" for s, c in samples if s is not None and c is not None)
             + f"; fall-through rescues on the host: {rescued}")


def game_test(uproject, editor_exe, keep=False, boot_timeout=300, withhold_seed=False, skew_seed=False, withhold_sync=False):
    """The REAL gameplay map, the way a player hits it: the Host button's own path, a late join by a second process,
    pawn counts, the joiner's control setup, fall-through sampling, then leave and rejoin (pawn reuse).

    Needs the Voxel GEditor guard (a -game process of the editor binary has no GEditor)."""
    rep = Report()
    host, client = names = ("gamehost", "gameclient")
    try:
        print("[nettest] game: launching host + client on the REAL gameplay map (Null OSS, -NoSteam) ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not rep.step("both instances boot and expose a world", all(wait_ready(n, boot_timeout) for n in names)):
            return False
        for n in names:
            call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(2)

        if withhold_seed:
            console(host, "MO.WorldSeed.Withhold 1")  # TEST ONLY: the client gets no seed (and so no terrain)
        if withhold_sync:
            console(host, "MO.WorldSync.Withhold 1")  # TEST ONLY: the client gets no clock / weather from the host
        if skew_seed:
            console(host, "MO.WorldSeed.Skew 1")  # TEST ONLY: the client gets a valid but DIFFERENT seed (different terrain)
        # the exact call the Host button makes (AMOMainMenuPlayerController::HostSession -> real GameplayLevelPath)
        off = log_size(host)
        console(host, "MO.Session.Host GameTest 4")
        created = wait_log(host, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=45)
        if not rep.step("host: Host path creates the session", bool(created) and "succeeded" in created.group(0),
                        created.group(0) if created else "no result"):
            return False
        reached, st = _wait_state(host, "ListenServer", "", 300)
        if not rep.step("host: real map loaded and the host has a pawn", reached, str(st)):
            return False

        # Tutorial: the possession hint must be the first popup. FindWidget matches TextBlock text, so look for the hint's
        # body on screen, and make sure the Movement hint is not showing in its place.
        hint_text, walk_text = None, None
        deadline = time.time() + 90
        while time.time() < deadline and not hint_text:
            off = log_size(host)
            console(host, 'MO.Test.FindWidget "open the possession menu"')
            m = wait_log(host, r"WIDGET '[^']*' \([^)]*\) text='([^']*open the possession menu[^']*)'", since=off, timeout=6)
            hint_text = m.group(1) if m else None
            if not hint_text:
                time.sleep(4)
        off = log_size(host)
        console(host, 'MO.Test.FindWidget "to walk"')
        wait_log(host, r"FindWidget\('to walk'\): (\d+) match", since=off, timeout=6)
        m = re.search(r"FindWidget\('to walk'\): (\d+) match", read_log_from(host, off))
        walk_text = int(m.group(1)) if m else None
        rep.step("host: the first tutorial popup is 'press <key> to open the possession menu'",
                 bool(hint_text) and walk_text == 0,
                 f"popup: {hint_text!r}; Movement hint on screen at the same time: {walk_text}")

        host_set_before_join = ("14:00", "Rain")
        if not (withhold_seed or skew_seed):
            console(host, "MO.Clock.SetTime 14 0")
            console(host, "MO.Weather.SetPreset Rain")
            time.sleep(6)  # the host publishes weather every few seconds; be sure it is on the wire before the client arrives
        found = None
        for _ in range(4):
            off = log_size(client)
            console(client, "MO.Session.Find")
            m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
            found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
            if found:
                break
            time.sleep(3)
        if not rep.step("client: finds the session", bool(found), f"{found} result(s)"):
            return False
        join_off = log_size(client)
        host_join_off = log_size(host)
        console(client, "MO.Session.Join 0")
        reached, cst = _wait_state(client, "Client", "", 300)
        if not rep.step("client: joined the real map and possesses a pawn", reached, str(cst)):
            return False
        time.sleep(5)
        _terrain_agreement(rep, host, client, join_off, withheld=withhold_seed, skewed=skew_seed)
        if not (withhold_seed or skew_seed):
            _world_sync(rep, host, client, host_set_before_join, join_off=join_off, expect_disagreement=withhold_sync)

        ctl = _probe(host, PROBE_CONTROLLERS)
        m = re.search(r"PCS=(\d+) WITHPAWN=(\d+)", ctl)
        pcs, with_pawn = (int(m.group(1)), int(m.group(2))) if m else (None, None)
        rep.step("host: two players, each with a pawn", pcs == 2 and with_pawn == 2, f"PCS={pcs} WITHPAWN={with_pawn}")
        # NOT asserted: MOCharacter also counts wandering survivors, which keep spawning as the world runs, so a raw
        # count cannot prove "no new pawn". The join/leave/rejoin decisions are asserted from the game's own log lines.
        print(f"[nettest]   MOCharacter actors in the world (includes NPC survivors): {_num(_probe(host, PROBE_PAWNS), 'PAWNS')}",
              flush=True)
        first_join = read_log_from(host, host_join_off)
        rep.step("host: first join with no free colonist spawned a pawn for the joiner",
                 "No available pawn for remote player" in first_join or "took existing pawn" in first_join,
                 "spawned a new one" if "No available pawn" in first_join else "reused an existing one")

        client_log = read_log_from(client, join_off)
        rep.step("client: controller finished pawn setup (input routing + UI caches)",
                 "AMOPlayerController: Possessed" in client_log,
                 "" if "AMOPlayerController: Possessed" in client_log else "no 'Possessed ...' line on the client")

        # The joiner's pawn as the CLIENT sees it vs as the SERVER sees it. (A stable client-side z proves nothing: with no
        # terrain at all the client's pawn sat at z=43, "stable". Two views of one pawn can only agree on shared ground.)
        pairs = []
        for _ in range(4):
            zc = _num(_probe(client, PROBE_Z), "Z")
            zs = _num(_probe(host, PROBE_Z_PLAYER1), "Z")
            pairs.append((zc, zs))
            time.sleep(5)
        complete = all(c is not None and s is not None for c, s in pairs)
        agree = complete and all(abs(c - s) <= 100.0 and c > -50000 for c, s in pairs)
        evidence = "client/server z (cm): " + ", ".join(
            f"{c:.0f}/{s:.0f}" for c, s in pairs if c is not None and s is not None)
        if withhold_seed:
            # (An earlier version expected the client's pawn z to DISAGREE with the server's here. It does not: with no terrain
            # at all the client just reports the server's position, so that check is vacuous for terrain. What a withheld seed
            # CAN prove is that the client has no seed and generated no terrain.)
            cq = _seed_query(client)
            rep.step("NEGATIVE CONTROL: the client has no world seed and generated no terrain", bool(cq) and cq[0] == 0 and cq[2] == 0,
                     f"client (has, seed, ready) = {cq}; client/server pawn z agree anyway ({evidence}) -- which is why z agreement is not a terrain check")
        elif not skew_seed:
            rep.step("client: the joiner's pawn is where the server says it is (not fallen through / not on other ground)", agree, evidence)
            _walk_client(rep, host, client)
            _far_remote_pawn(rep, host, client)
            _embedded_pawn(rep, host, client)
            _client_spawns_nothing(rep, host, client, host_join_off, join_off)
        else:
            print(f"[nettest]   (skew control) {evidence}", flush=True)

        # ---- leave, then rejoin: the pawn must survive and be reused, not respawned
        leave_off = log_size(host)
        call(client, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{MENU_MAP}")'], timeout=30)
        reached, st = _wait_state(client, "Standalone", MENU_MAP, 120)
        rep.step("client: back at the main menu", reached, str(st))
        stayed = wait_log(host, r"left: pawn \S+ stays in the world as an idle colonist", since=leave_off, timeout=30)
        rep.step("host: the leaving player's pawn stays in the world (idle colonist), not destroyed", bool(stayed),
                 stayed.group(0) if stayed else "no 'stays in the world' line")

        call(client, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        for _ in range(4):
            off = log_size(client)
            console(client, "MO.Session.Find")
            m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
            if m and int(FOUND_RE.search(m.group(0)).group(1)):
                break
            time.sleep(3)
        host_off = log_size(host)
        console(client, "MO.Session.Join 0")
        reached, cst = _wait_state(client, "Client", "", 300)
        rep.step("client: rejoins and possesses a pawn", reached, str(cst))
        time.sleep(4)
        rejoin_log = read_log_from(host, host_off)
        _possession_menu_rpcs(rep, host, client)
        reused = re.search(r"took existing pawn [^ ]+ \(previous=(yes|none)\)", rejoin_log)
        rep.step("host: the rejoining player took an existing pawn (their previous one) and no new pawn spawned",
                 bool(reused) and reused.group(1) == "yes" and "spawning a new one" not in rejoin_log,
                 reused.group(0) if reused else "no 'took existing pawn' line")
        return rep.ok
    finally:
        print(f"[nettest] game: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not keep:
            for n in names:
                stop(n)
        else:
            print("[nettest] --keep: instances left running (ue.py inst stop gamehost / gameclient)")


HOSTSAVE_SLOT = "zz_nettest_hostsave"


def hostsave_test(uproject, editor_exe, keep=False, boot_timeout=300):
    """The Load panel's Host button, end to end: make a world worth saving (a recruited colonist besides the host's pawn),
    save it, go back to the main menu, host THE SAVE (MO.Session.HostSave = what the button calls), then join a client.

    Asserts: the save loads on the listen server, the host gets its pawn back, the world runs on the SAVED seed, the
    client's terrain matches the host's, and the joiner takes a saved colonist instead of a freshly spawned pawn."""
    rep = Report()
    host, client = names = ("savehost", "saveclient")
    try:
        print("[nettest] hostsave: launching host + client on the REAL gameplay map (Null OSS, -NoSteam) ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not rep.step("both instances boot and expose a world", all(wait_ready(n, boot_timeout) for n in names)):
            return False
        for n in names:
            call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(2)

        # ---- phase 1: a fresh hosted world with a recruited colonist, then save it
        off = log_size(host)
        console(host, "MO.Session.Host SaveSource 4")
        created = wait_log(host, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=45)
        if not rep.step("host: hosts a fresh world to have something to save", bool(created) and "succeeded" in created.group(0),
                        created.group(0) if created else "no result"):
            return False
        reached, st = _wait_state(host, "ListenServer", "", 300)
        if not rep.step("host: fresh world loaded and the host has a pawn", reached, str(st)):
            return False
        first = wait_log(host, r"\[MOWorldSeed\] host published world seed (-?\d+)", since=off, timeout=30)
        if not rep.step("host: fresh world published a seed", bool(first), first.group(0) if first else "none"):
            return False
        saved_seed = int(first.group(1))

        off = log_size(host)
        console(host, "MO.Colony.SpawnSurvivor 500")
        spawned = wait_log(host, r"COLONY SpawnSurvivor (\S+)", since=off, timeout=20)
        name = spawned.group(1).strip() if spawned else None
        if not rep.step("host: spawned a survivor to recruit", bool(name) and name != "FAILED", str(name)):
            return False
        console(host, f"MO.Colony.Recruit {name}")
        time.sleep(2)
        out = _probe(host, "ps = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) "
                           f"if p.get_name() == '{name}']; "
                           "out('SPAREGUID=' + (unreal.GuidLibrary.conv_guid_to_string(ps[0].get_component_by_class("
                           "unreal.MOIdentityComponent).get_or_create_guid()) if ps else 'none'))")
        m = re.search(r"SPAREGUID=([0-9A-Fa-f]{32})", out)
        if not rep.step("host: the recruited colonist has a GUID", bool(m), m.group(1) if m else out.strip()[-160:]):
            return False
        spare_guid = m.group(1)

        off = log_size(host)
        console(host, f"MO.Save.SaveAs {HOSTSAVE_SLOT}")
        saved = wait_log(host, r"\[MO\.Save\.SaveAs\] \w+ '[^']*' -> (OK|FAILED)", since=off, timeout=90)
        thumb = wait_log(host, r"\[MOPersist\] Save thumbnail for '[^']*': (\d+)x(\d+) capture -> (\d+) bytes PNG, slot re-written ok=1", since=off, timeout=30)
        # A BLANK thumbnail (the old FViewport::ReadPixels bug) is a ~200 byte PNG; a real 128x128 picture is thousands.
        rep.step("host: the save has a real thumbnail (engine screenshot path), not a blank image",
                 bool(thumb) and int(thumb.group(3)) > 1500 and "Handled ensure" not in read_log_from(host, off),
                 thumb.group(0)[-80:] if thumb else "no '[MOPersist] Save thumbnail' line")
        if not rep.step("host: saved the world", bool(saved) and saved.group(1) == "OK", saved.group(0) if saved else "no result"):
            return False

        # ---- phase 2: back to the menu, then host THE SAVE
        call(host, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{MENU_MAP}")'], timeout=30)
        reached, st = _wait_state(host, "Standalone", MENU_MAP, 120)
        if not rep.step("host: back at the main menu", reached, str(st)):
            return False
        call(host, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(3)

        load_off = log_size(host)
        console(host, f"MO.Session.HostSave {HOSTSAVE_SLOT}")
        created = wait_log(host, r"CreateSession succeeded|Already in a session|FAILED|could not be found", since=load_off, timeout=60)
        if not rep.step("host: Host-this-save creates the session", bool(created) and "succeeded" in created.group(0),
                        created.group(0) if created else "no result"):
            return False
        reached, st = _wait_state(host, "ListenServer", "", 300)
        if not rep.step("host: the saved world loaded as a listen server", reached, str(st)):
            return False
        loaded = wait_log(host, r"Save loaded successfully: (\d+) pawns", since=load_off, timeout=120)
        rep.step("host: the save loaded on the listen server with its pawns", bool(loaded) and int(loaded.group(1)) >= 2,
                 loaded.group(0) if loaded else "no 'Save loaded successfully' line")
        rep.step("loading a save does NOT apply the new-game start conditions (it restores its own weather)",
                 "New game: applied weather preset" not in read_log_from(host, load_off) and "New game: weather provider not registered" not in read_log_from(host, load_off),
                 "no 'New game' weather line after the load")
        back = wait_log(host, r"Auto-possessed last-played pawn '([^']+)'", since=load_off, timeout=240)
        rep.step("host: got its saved pawn back (re-possessed once the terrain was ready)", bool(back),
                 back.group(1) if back else "no 'Auto-possessed last-played pawn' line")
        out = _probe(host, "ps = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) "
                           "if p.get_component_by_class(unreal.MOIdentityComponent) and "
                           "unreal.GuidLibrary.conv_guid_to_string(p.get_component_by_class(unreal.MOIdentityComponent)"
                           f".get_or_create_guid()) == '{spare_guid}']; out('SPARE_BACK=%d' % len(ps))")
        rep.step("host: the recruited colonist from the save is back in the world", "SPARE_BACK=1" in out, out.strip()[-80:])

        # ---- phase 3: a client joins the resumed world
        found = None
        for _ in range(4):
            off = log_size(client)
            console(client, "MO.Session.Find")
            m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
            found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
            if found:
                break
            time.sleep(3)
        if not rep.step("client: finds the hosted save", bool(found), f"{found} result(s)"):
            return False
        join_off = log_size(client)
        host_join_off = log_size(host)
        console(client, "MO.Session.Join 0")
        reached, cst = _wait_state(client, "Client", "", 300)
        if not rep.step("client: joined the resumed world and possesses a pawn", reached, str(cst)):
            return False
        time.sleep(5)

        _terrain_agreement(rep, host, client, join_off, host_since=load_off, expected_seed=saved_seed)

        joined = read_log_from(host, host_join_off)
        rep.step("host: the joiner took a saved colonist; no new pawn was spawned for them",
                 "took existing pawn" in joined and "spawning a new one" not in joined,
                 "took existing pawn" if "took existing pawn" in joined else
                 ("spawned a new pawn" if "spawning a new one" in joined else "no join decision logged"))
        return rep.ok
    finally:
        print(f"[nettest] hostsave: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        try:
            console(host, f"MO.Save.Delete {HOSTSAVE_SLOT}")  # never leave the test save in the player's Load list
        except Exception as e:  # noqa: BLE001 - cleanup must not mask the verdict
            print(f"[nettest] could not delete test save {HOSTSAVE_SLOT}: {e}", flush=True)
        if not keep:
            for n in names:
                stop(n)
        else:
            print(f"[nettest] --keep: instances left running (ue.py inst stop savehost / saveclient); test save {HOSTSAVE_SLOT} deleted")


def _host_and_join(rep, host, client, display, boot_timeout=300):
    """Host a FRESH world on the real map and join `client` into it (the Host button's path, then the Join path). Returns
    True when both are in the world with a pawn each. Used by the modes that need a joined pair and nothing else."""
    if not rep.step("both instances boot and expose a world", all(wait_ready(n, boot_timeout) for n in (host, client))):
        return False
    for n in (host, client):
        call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    time.sleep(2)
    off = log_size(host)
    console(host, f"MO.Session.Host {display} 4")
    created = wait_log(host, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=60)
    if not rep.step("host: creates the session", bool(created) and "succeeded" in created.group(0), created.group(0) if created else "no result"):
        return False
    reached, st = _wait_state(host, "ListenServer", "", 300)
    if not rep.step("host: real map loaded and the host has a pawn", reached, str(st)):
        return False
    found = None
    for _ in range(5):
        off = log_size(client)
        console(client, "MO.Session.Find")
        m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
        found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
        if found:
            break
        time.sleep(3)
    if not rep.step("client: finds the session", bool(found), f"{found} result(s)"):
        return False
    console(client, "MO.Session.Join 0")
    reached, cst = _wait_state(client, "Client", "", 300)
    if not rep.step("client: joined and possesses a pawn", reached, str(cst)):
        return False
    time.sleep(5)
    out = _probe(host, PROBE_CONTROLLERS)
    m = re.search(r"PCS=(\d+) WITHPAWN=(\d+)", out)
    return rep.step("host: two players, each with a pawn", bool(m) and m.group(1) == "2" and m.group(2) == "2", out.strip()[-80:])


def _client_pickup(rep, host, client):
    """CLIENT pickup, item identity across the wire: the host (authority) gives the joiner's pawn a stick with a known GUID and
    drops it; the CLIENT picks it up through MO.Test.PickupNearest (deferred, so the interact RPC really transports); the host-side
    inventory of the joiner's pawn must get the SAME GUID back. Control: right after the drop, before the client acts, the GUID is
    NOT in the inventory -- so a GUID that is there afterwards got there through the client's pickup."""
    setup = _probe(host,
        "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); "
        "inv = p.get_component_by_class(unreal.MOInventoryComponent); g = unreal.GuidLibrary.new_guid(); builtins.MO_ACT_GUID = g; "
        "gave = inv.add_item_by_guid(g, 'Stick01', 1); "
        "w = inv.drop_item_by_guid(g, p.get_actor_location() + unreal.Vector(120.0, 0.0, 10.0), unreal.Rotator(0.0, 0.0, 0.0)); "
        "out('PICKUP_SETUP gave=%s item=%s' % (gave, w.get_name() if w else 'none'))")
    m = re.search(r"PICKUP_SETUP gave=(\w+) item=(\S+)", setup)
    if not rep.step("host: gave the joiner's pawn a stick and dropped it in front of them", bool(m) and m.group(1) == "True" and m.group(2) != "none",
                    m.group(0) if m else setup.strip()[-160:]):
        return
    probe_have = ("import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                  "inv = p.get_component_by_class(unreal.MOInventoryComponent); r = inv.try_get_entry_by_guid(builtins.MO_ACT_GUID); "
                  "out('HAVE=%s' % (r[0] if isinstance(r, tuple) else bool(r)))")
    time.sleep(6)  # the dropped item replicates to the client
    before = "HAVE=True" in _probe(host, probe_have)
    rep.step("CONTROL: before the client acts, the dropped stick is NOT in the joiner's inventory", not before,
             "already in the inventory" if before else "not in the inventory (it is on the ground)")
    off = log_size(client)
    console(client, "MO.Test.PickupNearest 600")
    wait_log(client, r"\[MOTEST\] (PickupNearest: interacting|FAIL PickupNearest)[^\n]*", since=off, timeout=15)
    cl = read_log_from(client, off)
    mm = re.search(r"\[MOTEST\] (?:PickupNearest: interacting|FAIL PickupNearest)[^\n]*", cl)
    got = False
    for _ in range(10):  # next-tick + interact RPC + pickup
        time.sleep(1.5)
        if "HAVE=True" in _probe(host, probe_have):
            got = True
            break
    rep.step("client pickup: the SAME item GUID is in the joiner's inventory on the host", got,
             (mm.group(0) if mm else "client printed no PickupNearest line") + ("" if got else " -- GUID never arrived"))


def _client_craft(rep, host, client):
    """CLIENT crafting: authority-side setup on the joiner's pawn (ingredient + skill), then the CLIENT enqueues through MO.Test.Craft.
    The proof is the host-side queue of the joiner's pawn growing -- the client's own 'PASS' only says the RPC was sent."""
    probe_q = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); qc = p.get_component_by_class(unreal.MOCraftingQueueComponent); "
               "out('QUEUE=%d' % (len(qc.get_all_queue_entries()) if qc else -1))")
    # CONTROL: the same client request BEFORE the pawn has the ingredients/skill must be refused by the server (otherwise a later
    # "queue grew" proves nothing about the grant, and an ungated enqueue would be a real hole).
    q_before = _num(_probe(host, probe_q), "QUEUE")
    console(client, "MO.Test.Craft KnapFlintFlakes")
    time.sleep(6)
    q_ctl = _num(_probe(host, probe_q), "QUEUE")
    rep.step("CONTROL: a client craft request for a pawn WITHOUT the ingredients/skill is refused by the server",
             q_before is not None and q_ctl is not None and q_ctl <= q_before, f"queue {q_before} -> {q_ctl}")
    off = log_size(host)
    console(host, "MO.Test.GrantRecipe 1 KnapFlintFlakes")  # the recipe's own ingredients / skill / knowledge, from the authority
    granted = wait_log(host, r"\[MOTEST\] (PASS|FAIL) GrantRecipe[^\n]*", since=off, timeout=15)
    rep.step("host: gave the joiner's pawn everything the recipe needs", bool(granted) and granted.group(1) == "PASS",
             granted.group(0) if granted else "no GrantRecipe line")
    q0 = _num(_probe(host, probe_q), "QUEUE")
    if not rep.step("host: can read the joiner's crafting queue", q0 is not None and q0 >= 0, f"queue length {q0}"):
        return
    off = log_size(client)
    console(client, "MO.Test.Craft KnapFlintFlakes")
    wait_log(client, r"\[MOTEST\] (PASS|FAIL) Craft[^\n]*", since=off, timeout=15)
    mm = re.search(r"\[MOTEST\] (?:PASS|FAIL) Craft[^\n]*", read_log_from(client, off))
    q1 = q0
    for _ in range(10):
        time.sleep(1.5)
        q1 = _num(_probe(host, probe_q), "QUEUE")
        if q1 is not None and q1 > q0:
            break
    rep.step("client craft: the recipe reached the joiner's crafting queue on the host", q1 is not None and q1 > q0,
             f"queue {q0:.0f} -> {q1}; client said: {mm.group(0) if mm else 'nothing'}")


def _combat_line(text):
    m = re.search(r"INCOMBAT=\S+ STATE=\S+", text)
    return m.group(0) if m else text.strip()[-80:]


def _client_attack(rep, host, client):
    """CLIENT attack: the client calls StartLightAttack (forwarded as ServerStartAttack); the host's combat component for the joiner's
    pawn must enter combat. Control: not in combat before. (bInCombat holds for ~5 s after an action, so poll right away.)"""
    probe_c = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); c = p.get_component_by_class(unreal.MOCombatComponent); "
               "out('INCOMBAT=%s STATE=%s' % (c.is_in_combat_state() if c else 'nocomp', c.combat_state if c else '?'))")
    before = _probe(host, probe_c)
    rep.step("CONTROL: the joiner's pawn is not in combat before the client attacks", "INCOMBAT=False" in before, _combat_line(before))
    off = log_size(client)
    console(client, "MO.Test.Attack")
    wait_log(client, r"\[MOTEST\] (PASS|FAIL) Attack[^\n]*", since=off, timeout=15)
    mm = re.search(r"\[MOTEST\] (?:PASS|FAIL) Attack[^\n]*", read_log_from(client, off))
    seen = ""
    for _ in range(8):
        seen = _probe(host, probe_c)
        if "INCOMBAT=True" in seen:
            break
        time.sleep(0.4)
    rep.step("client attack: the host put the joiner's pawn in combat", "INCOMBAT=True" in seen,
             f"{_combat_line(seen)}; client said: {mm.group(0) if mm else 'nothing'}")


def _pawn_xyz(host, idx):
    out = _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, {idx}); l = p.get_actor_location(); out('PAWNXYZ %f %f %f' % (l.x, l.y, l.z))")
    m = re.search(r"PAWNXYZ (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)", out)
    return tuple(float(v) for v in m.groups()) if m else None


def _wait_on_surface(host, idx, ground_z, timeout):
    """Poll until pawn `idx` stands within [ground-20, ground+600] (the rescue puts it at ground+200, then it settles), or the timeout. -> last (x, y, z)."""
    deadline, last = time.time() + timeout, None
    while time.time() < deadline:
        last = _pawn_xyz(host, idx)
        if last and ground_z - 20 <= last[2] <= ground_z + 600:
            return last
        time.sleep(2)
    return _pawn_xyz(host, idx) or last


def _buried_pawns(rep, host, client):
    """A pawn UNDER the terrain surface must be found and lifted back onto it by the fall-through rescue, even with a solid block over the ground (a stand-in for a tree's
    collision, a roof, any non-terrain blocker) -- host's own pawn first, then the joiner's, as the server sees them.

    The bug (Wes: the client could not see the host's pawn): the ground trace stopped at the first BLOCKING hit, so with something solid over the ground the voxel terrain behind it
    was never returned, the rescue said 'No valid terrain found' and put the pawn at a fixed height -- under the surface on a map whose ground is often above it -- over and over.
    The CONTROL runs the old first-hit rule (MO.Rescue.FirstHitOnly 1) and asserts on what the rescue LOGGED (not on where the pawn ends up: the fixed fallback height happens to be above
    the ground in some worlds, which would hide the failure): the old rule must say 'No valid terrain found' and name the block; the fixed rule must say 'Found safe terrain'."""
    for who, idx, with_control in (("the host's own pawn", 0, True), ("the joiner's pawn", 1, False)):
        p = _pawn_xyz(host, idx)
        if not rep.step(f"{who}: located by the host", bool(p), str(p)):
            continue
        x, y, z = p
        ground = _surface_z(host, x, y, 30000, -30000)
        if not rep.step(f"{who}: the voxel ground under it is measurable", bool(ground) and ground[0] == 1, str(ground)):
            continue
        gz = ground[1]

        def bury():
            _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, {idx}); "
                         f"p.set_actor_location(unreal.Vector({x}, {y}, {gz - 1500.0}), False, True); p.get_character_movement().stop_movement_immediately(); out('BURIED')")

        console(host, f"MO.Test.RoofAbove {idx} 1500 600")  # a block 15 m over the ground, 12 m wide
        try:
            if with_control:
                console(host, "MO.Rescue.FirstHitOnly 1")
                off = log_size(host)
                bury()
                wait_log(host, r"Fall-through detected! (Found safe terrain|No valid terrain found)", since=off, timeout=20)
                log = read_log_from(host, off)
                first = re.search(r"Fall-through detected! (Found safe terrain|No valid terrain found)", log)
                blocker = re.search(r"rescue found no ground at [^;]*; blocking hits on the sky line \(top first\):(.*)", log)
                rep.step(f"CONTROL ({who}): with the OLD first-hit rule and a block over the ground, the rescue cannot find the ground",
                         bool(first) and first.group(1) == "No valid terrain found", first.group(0) if first else "no rescue line within 20 s")
                first_blocker = (blocker.group(1).split("[1]")[0] if blocker else "")
                rep.step(f"the failed rescue names what is in the way ({who}): the first thing on the sky line is NOT the terrain (the roof, or a forest tree's collision)",
                         bool(blocker) and "[0]" in first_blocker and "Voxel" not in first_blocker, (blocker.group(1)[:200] if blocker else "no 'rescue found no ground' line"))
                console(host, "MO.Rescue.FirstHitOnly 0")
                _wait_on_surface(host, idx, gz, 30)  # whatever the old rule left behind is put right by the fixed one
            off = log_size(host)
            bury()
            wait_log(host, r"Fall-through detected! (Found safe terrain|No valid terrain found)", since=off, timeout=20)
            first = re.search(r"Fall-through detected! (Found safe terrain|No valid terrain found)", read_log_from(host, off))
            freed = _wait_on_surface(host, idx, gz, 30)
            rep.step(f"{who}: buried 15 m under the surface with a block over the ground, the rescue FINDS the ground and lifts it back onto it",
                     bool(first) and first.group(1) == "Found safe terrain" and bool(freed) and gz - 20 <= freed[2] <= gz + 600,
                     f"ground z={gz:.0f}; pawn z={freed[2] if freed else None}; first rescue line: {first.group(0) if first else None}")
        finally:
            console(host, "MO.Test.ClearRoof")


def _remote_copy(client, hx, hy):
    """The CLIENT's copy of the host's pawn: (z, movement mode, vertical velocity) of the character near (hx, hy) that is not the local pawn, or None."""
    out = _probe(client,
        "me = unreal.GameplayStatics.get_player_pawn(world, 0); "
        "cs = [c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if me is None or c.get_name() != me.get_name()]; "
        f"near = [c for c in cs if abs(c.get_actor_location().x - {hx}) < 200 and abs(c.get_actor_location().y - {hy}) < 200]; "
        "c = near[0] if near else None; "
        "cm = c.get_editor_property('character_movement') if c else None; "
        "out('COPY found=%s z=%s mm=%s vz=%s' % (bool(c), ('%.0f' % c.get_actor_location().z) if c else '?', str(cm.get_editor_property('movement_mode')).split('.')[-1].split(':')[0] if cm else '?', ('%.0f' % cm.get_editor_property('velocity').z) if cm else '?'))")
    m = re.search(r"COPY found=(\w+) z=(-?[\d.]+|\?) mm=(\S+) vz=(-?[\d.]+|\?)", out)
    if not m or m.group(1) != "True":
        return None
    return float(m.group(2)), m.group(3), float(m.group(4))


def _client_ground(client, on):
    """Turn the client's voxel terrain collision on/off (what a remote pawn standing outside the client's own terrain radius, or before its runtime exists, has no floor)."""
    flag = "True" if on else "False"
    _probe(client, f"n = [a.set_actor_enable_collision({flag}) for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if a.get_name().startswith('VoxelLandscape')]; "
                   f"out('GROUND {flag} actors=%d' % len(n))")


def _remote_pawn_stays_put(rep, host, client):
    """The bug Wes reported ("the client can't see the host's pawn"), second half: a CLIENT draws the host's pawn as a simulated proxy; with no floor under it the engine predicts a fall,
    and a pawn that stands still gets no position updates to correct that -- it fell at terminal velocity for ever (z=-498790 after a minute). Gravity is never simulated on a proxy now.
    No local floor is made by switching the client's voxel collision off. CONTROL: MO.RemotePawn.SimGravity 1 (the stock engine behaviour) must make the copy fall, so the check can fail."""
    p = _pawn_xyz(host, 0)
    if not rep.step("host: located its own pawn", bool(p), str(p)):
        return
    hx, hy, hz = p

    def nudge():  # a tiny server-side move: the next replication reaches the client as a position update (where the flag is decided)
        _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, 0); l = p.get_actor_location(); p.set_actor_location(unreal.Vector(l.x + 1.0, l.y, l.z), False, True); out('NUDGED')")

    copy = _remote_copy(client, hx, hy)
    rep.step("client: sees the host's pawn at the host's height (standing, not falling)", bool(copy) and abs(copy[0] - hz) < 250 and copy[1] != "MOVE_FALLING",
             f"host z={hz:.0f}; client's copy {copy}")
    try:
        console(client, "MO.RemotePawn.SimGravity 1")
        _client_ground(client, False)
        nudge()
        time.sleep(7)
        fell = _remote_copy(client, hx + 1.0, hy) or _remote_copy(client, hx, hy)
        rep.step("CONTROL: with the stock engine behaviour and no floor under it, the client's copy of the host's pawn FALLS",
                 bool(fell) and fell[0] < hz - 1500, f"host z={hz:.0f}; client's copy {fell}")
        console(client, "MO.RemotePawn.SimGravity 0")
        _client_ground(client, True)
        nudge()  # the server's position snaps the copy back
        time.sleep(4)
        back = _remote_copy(client, hx + 2.0, hy) or _remote_copy(client, hx, hy)
        rep.step("client: the next position update puts the copy back at the host's height", bool(back) and abs(back[0] - hz) < 250, f"client's copy {back}")
        _client_ground(client, False)
        nudge()
        time.sleep(7)
        held = _remote_copy(client, hx + 3.0, hy) or _remote_copy(client, hx, hy)
        rep.step("client: with NO floor under it the copy of the host's pawn now stays where the server says", bool(held) and abs(held[0] - hz) < 250,
                 f"host z={hz:.0f}; client's copy {held}")
    finally:
        console(client, "MO.RemotePawn.SimGravity 0")
        _client_ground(client, True)


def _count_buildables(host):
    out = _probe(host, "out('BUILDABLES=%d' % len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor)))")
    m = re.search(r"BUILDABLES=(\d+)", out)
    return int(m.group(1)) if m else None


def _trust_placement(rep, host, client):
    """Audit H19: ServerPlaceBuilding trusted the client's transform, so a hostile client could place buildings anywhere. The client
    sends the same RPC twice, near (what the placement UI sends) and 200 m away (what a hostile client sends). With the server reach check
    switched OFF the far request must succeed (the control: the refusal below is the check's doing); with it on, only the near one does."""
    base = _count_buildables(host)
    if not rep.step("host: can count buildings", base is not None, str(base)):
        return

    def send(forward_cm):
        off = log_size(host)
        console(client, f"MO.Test.PlaceBuildingRPC auto {forward_cm}")
        time.sleep(6)  # next tick + RPC + the host spawning it
        return _count_buildables(host), read_log_from(host, off)

    after, log = send(300)
    rep.step("trust: a placement 3 m in front of the pawn (the UI's own request) IS accepted", after == base + 1 and "ServerPlaceBuilding rejected" not in log,
             f"buildings {base} -> {after}")
    base = after

    console(host, "MO.Building.ServerReach.Disable 1")
    after, log = send(20000)
    rep.step("CONTROL: with the server reach check disabled a placement 200 m away IS accepted (a hostile client could do this)", after == base + 1, f"buildings {base} -> {after}")
    base = after
    console(host, "MO.Building.ServerReach.Disable 0")

    after, log = send(20000)
    refused = re.search(r"ServerPlaceBuilding rejected: [^\n]*", log)
    rep.step("trust: a placement 200 m away is REFUSED by the server", after == base and bool(refused), refused.group(0)[:150] if refused else f"buildings {base} -> {after}")


def _trust_pickup_far(rep, host, client):
    """The server holds ServerPickUpWorldItem to a reach (3000 cm). The host drops a stick 45 m from the joiner (inside network relevancy, outside
    the reach); the client asks for the nearest world item at least 35 m away."""
    setup = _probe(host,
        "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
        "g = unreal.GuidLibrary.new_guid(); gave = inv.add_item_by_guid(g, 'Stone01', 1); "
        # a STONE, not a stick: a stick would merge into the joiner's existing stack, so its GUID would not exist to drop.
        # The drop itself is only accepted near the owner (a good thing): drop beside the pawn, then carry the world item 45 m away.
        "w = inv.drop_item_by_guid(g, p.get_actor_location() + unreal.Vector(120.0, 0.0, 10.0), unreal.Rotator(0.0, 0.0, 0.0)); "
        "w and w.set_actor_location(p.get_actor_location() + unreal.Vector(4500.0, 0.0, 10.0), False, True); "
        "out('FARDROP gave=%s item=%s' % (gave, w.get_name() if w else 'none'))")
    m = re.search(r"FARDROP gave=(\w+) item=(\S+)", setup)
    if not rep.step("host: dropped an item 45 m from the joiner (out of the server's 30 m reach)", bool(m) and m.group(1) == "True" and m.group(2) != "none",
                    m.group(0) if m else setup.strip()[-160:]):
        return
    time.sleep(8)  # replicates to the client
    off = log_size(host)
    console(client, "MO.Test.PickupNearest 12000 3500")
    time.sleep(6)
    log = read_log_from(host, off)
    refused = re.search(r"\[MOInteract\] ServerPickUpWorldItem: \S+ out of reach", log)
    took = re.search(r"\[MOInteract\] ServerPickUpWorldItem: \S+ -> picked up", log)
    cl = read_log_from(client, 0)
    asked = re.search(r"\[MOTEST\] PickupNearest: interacting with \S+ at (\d+)uu", cl[-4000:])
    if not asked:
        rep.step("trust: there was a world item 35 m+ away to ask for", False, "client found none within 120 m (the world is empty here)")
        return
    rep.step("trust: a pickup request for an item 35 m+ away is REFUSED by the server", bool(refused) and not took,
             f"asked at {asked.group(1)} uu; host: {refused.group(0) if refused else 'no out-of-reach line'}{'; ...and picked it up' if took else ''}")


def _trust_possess_other(rep, host, client):
    """A client asks to possess the HOST's pawn (another human is driving it). The server must refuse and leave both players where they are."""
    probe = ("h = unreal.GameplayStatics.get_player_pawn(world, 0); c = unreal.GameplayStatics.get_player_pawn(world, 1); "
             "g = unreal.GuidLibrary.conv_guid_to_string(h.get_component_by_class(unreal.MOIdentityComponent).get_or_create_guid()); "
             "out('PAWNS host=%s client=%s guid=%s' % (h.get_name(), c.get_name(), g))")
    before = re.search(r"PAWNS host=(\S+) client=(\S+) guid=([0-9A-Fa-f]{32})", _probe(host, probe))
    if not rep.step("host: located both players' pawns", bool(before), before.group(0)[:100] if before else "no PAWNS line"):
        return
    off = log_size(host)
    console(client, f"MO.Possess.Take {before.group(3)}")
    time.sleep(6)
    log = read_log_from(host, off)
    after = re.search(r"PAWNS host=(\S+) client=(\S+)", _probe(host, probe))
    refused = re.search(r"\[MOPossession\] \S+ may not take [^\n]*", log)
    rep.step("trust: taking a pawn another PLAYER is driving is REFUSED, and nobody moved",
             bool(refused) and bool(after) and after.group(1) == before.group(1) and after.group(2) == before.group(2),
             (refused.group(0)[:120] if refused else "no 'may not take' line") + f"; pawns {before.group(1)}/{before.group(2)} -> {after.group(1) if after else '?'}/{after.group(2) if after else '?'}")


def _trust_terraform(rep, host, client):
    """ServerApplyTerraform holds the client to 2500 cm and clamps FlattenHeight. Dig 3 m ahead must change the ground (control); Dig 100 m away must be refused."""
    probe = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); l = p.get_actor_location(); f = p.get_actor_forward_vector(); "
             "out('SPOT x=%f y=%f z=%f fx=%f fy=%f' % (l.x, l.y, l.z, f.x, f.y))")
    m = re.search(r"SPOT x=(-?[\d.]+) y=(-?[\d.]+) z=(-?[\d.]+) fx=(-?[\d.]+) fy=(-?[\d.]+)", _probe(host, probe))
    if not rep.step("host: located the joiner for the terraform test", bool(m), m.group(0) if m else "no SPOT line"):
        return
    x, y, z, fx, fy = (float(v) for v in m.groups())
    tx, ty = x + fx * 300.0, y + fy * 300.0
    before = _surface_z(host, tx, ty, z + 2000.0, z - 2000.0)
    if not rep.step("host: can measure the ground 3 m ahead of the joiner", bool(before) and before[0] == 1, str(before)):
        return
    off = log_size(host)
    console(client, "MO.Test.TerraformRPC Dig 300")
    time.sleep(8)
    after = _surface_z(host, tx, ty, z + 2000.0, z - 2000.0)
    dug = bool(after) and after[0] == 1 and before[1] - after[1] > 1.0  # one dig lowers the surface by a couple of cm (measured: 2.7)
    rep.step("trust: a dig 3 m ahead (the UI's own request) changes the ground", dug and "target out of reach" not in read_log_from(host, off),
             f"ground z {before[1]:.0f} -> {after[1] if after else None}")
    off = log_size(host)
    console(client, "MO.Test.TerraformRPC Dig 10000")
    time.sleep(6)
    log = read_log_from(host, off)
    refused = re.search(r"\[MOTerraforming\] ServerApplyTerraform: target out of reach -- rejected", log)
    rep.step("trust: a dig 100 m away is REFUSED by the server", bool(refused), refused.group(0) if refused else "no 'target out of reach' line")


def actions_test(uproject, editor_exe, keep=False, boot_timeout=300):
    """Gameplay verbs from a real CLIENT process against a real host (the PIE-era MO.Test.* suite never left one editor): pickup
    identity, crafting, attack. Each one's proof is read on the HOST -- what the authority ended up with -- not from the client's
    optimistic 'PASS'."""
    rep = Report()
    host, client = names = ("acthost", "actclient")
    try:
        print("[nettest] actions: host + client on the REAL gameplay map; verbs run on the CLIENT ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not _host_and_join(rep, host, client, "ActionsTest", boot_timeout):
            return False
        for fn in (_client_pickup, _client_craft, _client_attack, _trust_placement, _trust_pickup_far, _trust_possess_other, _trust_terraform, _buried_pawns, _remote_pawn_stays_put):
            try:
                fn(rep, host, client)
            except Exception as e:  # noqa: BLE001 - one verb's harness error must not hide the others
                rep.step(f"{fn.__name__} ran without a harness error", False, f"{type(e).__name__}: {e}")
        return rep.ok
    finally:
        print(f"[nettest] actions: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not keep:
            for n in names:
                stop(n)
        else:
            print("[nettest] --keep: instances left running (ue.py inst stop acthost / actclient)")


# ---------------------------------------------------------------------------------------------------------------------------------
# nettest packaged: the REAL packaged Development game, two copies, from the logs alone
# ---------------------------------------------------------------------------------------------------------------------------------
PKG_ROOT = os.path.abspath(os.path.join(HERE, "..", "Saved", "StagedBuilds_DevTest", "Windows"))
PKG_EXE = os.path.join(PKG_ROOT, "MO57.exe")
PKG_LOGS = os.path.join(PKG_ROOT, "MO57", "Saved", "Logs")
PKG_FATAL_RE = re.compile(r"Fatal error|Unhandled Exception|Handled ensure|Assertion failed")
PKG_HOST_SEED_RE = re.compile(r"\[MOWorldSeed\] host published world seed (-?\d+)")
PKG_CLIENT_SEED_RE = re.compile(r"\[MOWorldSeed\] client applied world seed (-?\d+): voxel runtime created")
PKG_CLOCK_SNAP_RE = re.compile(r"\[MOWorldSync\] client clock was (-?[\d.]+) game-s off the host's; snapping")
PKG_WEATHER_RE = re.compile(r"\[MOWorldSync\] client following the host's weather preset (\S+)")


def _first_line(text, regex):
    m = regex.search(text)
    if not m:
        return None
    start = text.rfind("\n", 0, m.start()) + 1
    end = text.find("\n", m.end())
    return text[start:end if end >= 0 else len(text)].strip()[:200]


def analyse_packaged_logs(host_log, client_log, withheld=False):
    """The packaged-build smoke verdicts, from the two game logs alone. Pure (unit-tested offline). -> [(label, ok, evidence)].

    `withheld` = the host ran with MO.WorldSync.Withhold 1: the sync checks flip into NEGATIVE CONTROLS (the client must NOT have followed),
    which proves the positive checks can fail."""
    rows = []
    for who, text in (("host", host_log), ("client", client_log)):
        bad = _first_line(text, PKG_FATAL_RE)
        rows.append((f"{who}: no crash, ensure or assertion in its log", bad is None, bad or f"{len(text.splitlines())} log lines, none"))

    hs, cs = PKG_HOST_SEED_RE.search(host_log), PKG_CLIENT_SEED_RE.search(client_log)
    rows.append(("host: published its world seed", bool(hs), hs.group(0) if hs else "no '[MOWorldSeed] host published' line"))
    rows.append(("client: applied the SAME seed and built its voxel runtime", bool(hs) and bool(cs) and hs.group(1) == cs.group(1),
                 f"host {hs.group(1) if hs else '-'}, client {cs.group(1) if cs else '-'}"))
    rows.append(("client: possessed a pawn", "AMOPlayerController: Possessed" in client_log,
                 "" if "AMOPlayerController: Possessed" in client_log else "no 'AMOPlayerController: Possessed' line"))

    host_spawn, client_spawn = _first_line(host_log, SPAWN_RE_COMPILED), _first_line(client_log, SPAWN_RE_COMPILED)
    rows.append(("CONTROL: the host's spawn manager ran (so a silent client means something)", host_spawn is not None, host_spawn or "no [SpawnManager] spawn line on the host"))
    rows.append(("client: its spawn manager spawned nothing", client_spawn is None, client_spawn or "no spawn line on the client"))

    snap, weather = PKG_CLOCK_SNAP_RE.search(client_log), PKG_WEATHER_RE.search(client_log)
    if withheld:
        rows.append(("NEGATIVE CONTROL: with the host's publish withheld the client never snapped its clock to the host's", snap is None,
                     "no clock snap on the client" if snap is None else snap.group(0)))
        rows.append(("NEGATIVE CONTROL: ... and never followed a weather preset", weather is None,
                     "no weather follow on the client" if weather is None else weather.group(0)))
    else:
        rows.append(("client: adopted the host's game clock", bool(snap), snap.group(0) if snap else "no '[MOWorldSync] client clock was ... snapping' line"))
        rows.append(("client: followed the host's weather preset through the bridge", bool(weather), weather.group(0) if weather else "no '[MOWorldSync] client following' line"))
    return rows


SPAWN_RE_COMPILED = re.compile(SPAWN_RE)


def _pkg_alive_count():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq MO57.exe", "/NH", "/FO", "CSV"], capture_output=True, text=True).stdout
    return sum(1 for line in out.splitlines() if line.lower().startswith('"mo57.exe"'))


def _pkg_read(name):
    try:
        with open(os.path.join(PKG_LOGS, name), "rb") as f:
            return f.read().decode("utf-8", errors="replace")
    except OSError:
        return ""


def _pkg_launch(args):
    """Start a packaged copy WITHOUT a console window; returns the stub's pid. The command line is passed as ONE string: UE parses its own
    quoting (-ExecCmds="a b,c d") and Python's list quoting would escape the inner quotes."""
    cmd = f'"{PKG_EXE}" {args}'
    flags = getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    return subprocess.Popen(cmd, creationflags=flags, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).pid


def _pkg_stop(pid):
    """Stop ONE process tree this module launched (the stub and the real game it spawned), by recorded pid."""
    if pid:
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True)


def _package_build():
    """Re-package the Development build (RunUAT BuildCookRun, ~90 s). The editor must be closed."""
    uat = r"D:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
    project = os.path.abspath(os.path.join(HERE, "..", "MO57.uproject"))
    log = os.path.abspath(os.path.join(HERE, "..", "Saved", "Logs", "package.log"))
    stage = os.path.abspath(os.path.join(HERE, "..", "Saved", "StagedBuilds_DevTest"))
    cmd = (f'"{uat}" BuildCookRun -project="{project}" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore '
           f'-compressed -stagingdirectory="{stage}" -unattended -utf8output')
    print("[nettest] packaged: packaging the Development build (~90 s) ...", flush=True)
    with open(log, "wb") as f:
        rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT).returncode
    ok = rc == 0 and "BUILD SUCCESSFUL" in open(log, encoding="utf-8", errors="replace").read()
    print(f"[nettest] packaged: packaging {'ok' if ok else 'FAILED (see Saved/Logs/package.log)'}", flush=True)
    return ok


def packaged_test(package=False, withhold_sync=False, run_seconds=60, boot_timeout=180):
    """Two copies of the PACKAGED Development game (host via -ExecCmds, client via the address), judged from their logs alone: no crash / ensure,
    same seed, the client follows the host's clock and weather, the client's spawn manager is silent while the host's runs.

    Why it exists: the editor-binary `-game` processes the other modes use are not the shipped game; this found a startup crash (intro timer),
    client-side creature spawning and a blank save thumbnail by hand -- now it is a command. Refuses to run if any MO57.exe is already running
    (it only ever stops the two process trees it started)."""
    rep = Report()
    host_pid = client_pid = None
    try:
        if package and not _package_build():
            return rep.step("the Development package builds", False, "see Saved/Logs/package.log")
        if not rep.step("the staged packaged build exists", os.path.exists(PKG_EXE), PKG_EXE):
            return False
        running = _pkg_alive_count()
        if not rep.step("no other MO57.exe is running (this mode must own the logs and never touches others)", running == 0, f"{running} running"):
            return False

        cmds = ("MO.WorldSync.Withhold 1," if withhold_sync else "") + "MO.Session.Host PackagedTest 4"
        print(f"[nettest] packaged: launching the host (-ExecCmds=\"{cmds}\")" + (" -- NEGATIVE CONTROL: sync withheld" if withhold_sync else "") + " ...", flush=True)
        host_pid = _pkg_launch(f'-NoSteam -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="{cmds}"')
        deadline, seeded = time.time() + boot_timeout, False
        while time.time() < deadline and not seeded:
            seeded = bool(PKG_HOST_SEED_RE.search(_pkg_read("MO57.log")))
            time.sleep(2)
        if not rep.step("host: the packaged game booted, hosted a world and published its seed", seeded,
                        "" if seeded else _pkg_read("MO57.log")[-300:].replace("\n", " | ")):
            return False

        client_pid = _pkg_launch("127.0.0.1 -NoSteam -windowed -ResX=960 -ResY=540 -WinX=980 -WinY=0")
        deadline, joined = time.time() + boot_timeout, False
        while time.time() < deadline and not joined:
            joined = bool(PKG_CLIENT_SEED_RE.search(_pkg_read("MO57_2.log")))
            time.sleep(2)
        rep.step("client: the second packaged copy joined and applied the host's seed", joined,
                 "" if joined else _pkg_read("MO57_2.log")[-300:].replace("\n", " | "))

        print(f"[nettest] packaged: running both for {run_seconds} s (spawn manager first-spawn, sync) ...", flush=True)
        time.sleep(run_seconds)
        alive = _pkg_alive_count()
        rep.step("both packaged copies are still running (4 processes: two stubs + two games)", alive == 4, f"{alive} MO57.exe process(es)")

        for row in analyse_packaged_logs(_pkg_read("MO57.log"), _pkg_read("MO57_2.log"), withheld=withhold_sync):
            rep.step(*row)
        return rep.ok
    finally:
        print(f"[nettest] packaged: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        _pkg_stop(client_pid)
        _pkg_stop(host_pid)


# ---------------------------------------------------------------------------------------------------------------------------------
# nettest churn: join/leave cycles, two players grabbing the same item at once, saving while a client is connected
# ---------------------------------------------------------------------------------------------------------------------------------
CHURN_SLOT = "zz_nettest_churn"
CHURN_FATAL_RE = re.compile(r"Fatal error|Unhandled Exception|Ensure condition failed|Assertion failed")
CHURN_MO_ERROR_RE = re.compile(r"\bLogMO\w*: Error:")
FALLTHROUGH_RE = re.compile(r"Fall-through detected")
LOG_STAMP_RE = re.compile(r"^\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]")


def distinct_log_lines(text, regex):
    """The distinct lines of `text` matching `regex`, with the timestamp / frame prefix removed (the same fault logged ten times is one line)."""
    out = {}
    for line in text.splitlines():
        if regex.search(line):
            key = re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", line).strip()[:220]
            out[key] = out.get(key, 0) + 1
    return out


def log_stamp_ms(line):
    """Milliseconds since midnight of a log line's timestamp, or None (day rollover is ignored: a test lasts minutes)."""
    m = LOG_STAMP_RE.match(line)
    if not m:
        return None
    _, _, _, hh, mm, ss, ms = (int(g) for g in m.groups())
    return ((hh * 60 + mm) * 60 + ss) * 1000 + ms


def analyse_churn_logs(host_log, client_log):
    """What the host and the client logged WHILE the churn ran -> [(label, ok, evidence)]. Pure (unit-tested): nobody fell through the ground and was
    rescued, nothing crashed or hit an ensure, and the game's own code logged no Error-level line."""
    rows = []
    for who, text in (("host", host_log), ("client", client_log)):
        rescued = len(FALLTHROUGH_RE.findall(text))
        rows.append((f"{who}: no fall-through rescue during the churn", rescued == 0, f"{rescued} rescue line(s)"))
        fatal = distinct_log_lines(text, CHURN_FATAL_RE)
        rows.append((f"{who}: no crash, ensure or assertion during the churn", not fatal, "; ".join(list(fatal)[:2]) or f"{len(text.splitlines())} log lines, none"))
        errors = distinct_log_lines(text, CHURN_MO_ERROR_RE)
        rows.append((f"{who}: the game's own code logged no Error-level line during the churn", not errors,
                     "; ".join(f"{k} (x{n})" for k, n in list(errors.items())[:3]) or "none"))
    return rows


def _leave_and_rejoin(rep, host, client, label):
    """The client goes back to the main menu and joins again. The host must keep the leaver's pawn as an idle colonist and hand the SAME pawn back
    (no second pawn for the same player), and end with two players that each have a pawn."""
    leave_off = log_size(host)
    call(client, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{MENU_MAP}")'], timeout=30)
    reached, st = _wait_state(client, "Standalone", MENU_MAP, 120)
    if not rep.step(f"{label}: client is back at the main menu", reached, str(st)):
        return False
    stayed = wait_log(host, r"left: pawn \S+ stays in the world as an idle colonist", since=leave_off, timeout=30)
    rep.step(f"{label}: host keeps the leaver's pawn as an idle colonist", bool(stayed), stayed.group(0) if stayed else "no 'stays in the world' line")

    call(client, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    found = None
    for _ in range(5):
        off = log_size(client)
        console(client, "MO.Session.Find")
        m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
        found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
        if found:
            break
        time.sleep(3)
    if not rep.step(f"{label}: client finds the session again", bool(found), f"{found} result(s)"):
        return False
    host_off = log_size(host)
    console(client, "MO.Session.Join 0")
    reached, cst = _wait_state(client, "Client", "", 300)
    if not rep.step(f"{label}: client rejoins and possesses a pawn", reached, str(cst)):
        return False
    time.sleep(4)
    rejoin = read_log_from(host, host_off)
    took = re.search(r"took existing pawn [^ ]+ \(previous=(yes|none)\)", rejoin)
    rep.step(f"{label}: host handed the player their previous pawn back and spawned no new one",
             bool(took) and took.group(1) == "yes" and "spawning a new one" not in rejoin, took.group(0) if took else "no 'took existing pawn' line")
    out = _probe(host, PROBE_CONTROLLERS)
    m = re.search(r"PCS=(\d+) WITHPAWN=(\d+)", out)
    return rep.step(f"{label}: host sees two players, each with a pawn", bool(m) and m.group(1) == "2" and m.group(2) == "2", m.group(0) if m else out.strip()[-60:])


def _race_trial(host, client, item_id):
    """One race: an item on the ground exactly between the host's pawn and the joiner's, both asked to pick it up at the same moment. -> dict."""
    import threading
    setup = _probe(host,
        "import builtins; h = unreal.GameplayStatics.get_player_pawn(world, 0); c = unreal.GameplayStatics.get_player_pawn(world, 1); "
        "hi = h.get_component_by_class(unreal.MOInventoryComponent); g = unreal.GuidLibrary.new_guid(); builtins.MO_RACE_GUID = g; "
        "hl = h.get_actor_location(); c.set_actor_location(hl + unreal.Vector(0.0, 130.0, 0.0), False, True); "
        f"gave = hi.add_item_by_guid(g, '{item_id}', 1); "
        # a different material every trial: a second item of a type the host already holds MERGES into that stack, its GUID stops existing
        # and the drop fails ("entry not found") -- which is what trials 2-4 of the first version did. Dropped equidistant from both pawns
        "w = hi.drop_item_by_guid(g, hl + unreal.Vector(110.0, 65.0, 10.0), unreal.Rotator(0.0, 0.0, 0.0)); "
        # "nearest world item" must be THE item: clear every other world item near the pawns first (this is a throw-away test world; the first
        # version left earlier trials' stones and world scatter on the ground, and the two players then targeted different items)
        # (compare by NAME: every get_all_actors call returns fresh wrapper objects, so `a != w` is True for the very item just dropped)
        "wn = w.get_name() if w else 'none'; "
        "others = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if a.get_name() != wn and (a.get_actor_location() - hl).size() < 2500.0]; "
        "[a.destroy_actor() for a in others]; "
        "out('RACE_SETUP gave=%s item=%s cleared=%d' % (gave, wn, len(others)))")
    m = re.search(r"RACE_SETUP gave=(\w+) item=(\S+)", setup)
    if not (m and m.group(1) == "True" and m.group(2) != "none"):
        # the probe's output is mostly game log noise: keep only what says why
        why = [ln.strip()[:200] for ln in setup.splitlines() if re.search(r"RACE_SETUP|Traceback|Error|Exception|not found|DropItem|AddItem|py-err", ln)]
        return {"setup": False, "evidence": " | ".join(why[-4:]) or setup.strip()[-160:]}
    time.sleep(6)  # the item and the moved pawn replicate

    probe = ("import builtins; h = unreal.GameplayStatics.get_player_pawn(world, 0); c = unreal.GameplayStatics.get_player_pawn(world, 1); g = builtins.MO_RACE_GUID; "
             "def_has = lambda p: (lambda r: (r[0] if isinstance(r, tuple) else bool(r)))(p.get_component_by_class(unreal.MOInventoryComponent).try_get_entry_by_guid(g)); "
             "out('HOLD host=%s joiner=%s' % (def_has(h), def_has(c)))")
    off_h, off_c = log_size(host), log_size(client)
    barrier = threading.Barrier(2)

    def fire(name):
        barrier.wait()
        console(name, "MO.Test.PickupNearest 600")

    threads = [threading.Thread(target=fire, args=(n,)) for n in (host, client)]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=60)
    time.sleep(2)
    lines = {}
    for who, name, off in (("host", host, off_h), ("joiner", client, off_c)):
        mm = re.search(r"^.*\[MOTEST\] (?:PickupNearest: interacting with (\S+)|FAIL PickupNearest[^\n]*)", read_log_from(name, off), re.M)
        lines[who] = (mm.group(0), mm.group(1)) if mm else (None, None)
    holders = None
    for _ in range(10):  # let the slower side finish too: a duplicate would show up late
        time.sleep(1.5)
        holders = _probe(host, probe)
    hm = re.search(r"HOLD host=(\w+) joiner=(\w+)", holders or "")
    stamps = [log_stamp_ms(lines[w][0]) if lines[w][0] else None for w in ("host", "joiner")]
    skew = abs(stamps[0] - stamps[1]) if all(s is not None for s in stamps) else None
    return {"setup": True, "item_host": lines["host"][1], "item_joiner": lines["joiner"][1], "skew_ms": skew,
            "host_has": hm.group(1) == "True" if hm else None, "joiner_has": hm.group(2) == "True" if hm else None,
            "evidence": f"host: {lines['host'][0] and lines['host'][0][-70:]} | joiner: {lines['joiner'][0] and lines['joiner'][0][-70:]}"}


def _concurrent_pickup(rep, host, client, trials=4):
    """Two players ask for the SAME dropped item at the same moment: exactly one may end up holding it (an item duplicated by a race is the classic
    multiplayer inventory bug). A trial only counts when both really targeted the same item."""
    valid = 0
    materials = ["Stone01", "Sandstone01", "Clay01", "Sand01", "Gravel01", "Silt01"]
    for i in range(1, trials + 1):
        r = _race_trial(host, client, materials[(i - 1) % len(materials)])
        if not r["setup"]:
            rep.step(f"race {i}: the host could set the item and the two pawns up", False, r["evidence"])
            continue
        same = bool(r["item_host"]) and r["item_host"] == r["item_joiner"]
        if not same:
            print(f"[nettest]   race {i}: inconclusive -- the players targeted different items ({r['item_host']} / {r['item_joiner']})", flush=True)
            continue
        valid += 1
        both, neither = bool(r["host_has"]) and bool(r["joiner_has"]), not r["host_has"] and not r["joiner_has"]
        skew = f"{r['skew_ms']} ms apart" if r["skew_ms"] is not None else "skew unknown"
        rep.step(f"race {i}: exactly one of the two players ended up holding the item ({skew})", not both and not neither,
                 f"host has it: {r['host_has']}, joiner has it: {r['joiner_has']}" + (" -- DUPLICATED" if both else " -- NOBODY got it" if neither else ""))
    rep.step("the race ran with both players targeting the same item at least twice", valid >= 2, f"{valid} valid of {trials} trial(s)")


def _save_while_connected(rep, host):
    """Saving the world while a client is connected must succeed and must not disturb the connection."""
    off = log_size(host)
    console(host, f"MO.Save.SaveAs {CHURN_SLOT}")
    m = wait_log(host, r"\[MO\.Save\.SaveAs\] (?:Overwrote|Created) '[^']+' -> (OK|FAILED)", since=off, timeout=60)
    rep.step("host: saving the world while a client is connected succeeds", bool(m) and m.group(1) == "OK", m.group(0) if m else "no SaveAs result line")
    time.sleep(3)
    out = _probe(host, PROBE_CONTROLLERS)
    mm = re.search(r"PCS=(\d+) WITHPAWN=(\d+)", out)
    rep.step("host: the client is still connected, with its pawn, after the save", bool(mm) and mm.group(1) == "2" and mm.group(2) == "2", mm.group(0) if mm else out.strip()[-60:])


def churn_test(uproject, editor_exe, rounds=3, keep=False, boot_timeout=300):
    """Host + one client on the real gameplay map, then: N leave/rejoin cycles (the player's pawn is kept and handed back, never duplicated), several races
    for one dropped item between the host's pawn and the joiner's (exactly one winner), and a save while both are connected. The logs of both machines
    must show no fall-through rescue, no crash/ensure and no Error from the game's code over the whole run.
    (A second client was the plan; the host's own pawn is the second racer instead -- a third editor-binary process does not fit in 24 GB of VRAM next to the desktop.)"""
    rep = Report()
    host, client = names = ("churnhost", "churnclient")
    try:
        print(f"[nettest] churn: host + client on the REAL gameplay map; {rounds} leave/rejoin rounds, item races, a save ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not _host_and_join(rep, host, client, "ChurnTest", boot_timeout):
            return False
        base_host, base_client = log_size(host), log_size(client)
        for r in range(1, rounds + 1):
            if not _leave_and_rejoin(rep, host, client, f"round {r}/{rounds}"):
                break
        _concurrent_pickup(rep, host, client)
        _save_while_connected(rep, host)
        for row in analyse_churn_logs(read_log_from(host, base_host), read_log_from(client, base_client)):
            rep.step(*row)
        return rep.ok
    finally:
        print(f"[nettest] churn: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        try:
            console(host, f"MO.Save.Delete {CHURN_SLOT}")  # never leave the test save in the player's Load list
        except Exception as e:  # noqa: BLE001 - cleanup must not mask the verdict
            print(f"[nettest] could not delete test save {CHURN_SLOT}: {e}", flush=True)
        if not keep:
            for n in names:
                stop(n)
        else:
            print(f"[nettest] --keep: instances left running (ue.py inst stop {host} / {client}); test save {CHURN_SLOT} deleted")


# ---------------------------------------------------------------------------------------------------------------------------------------------
# bugreport: the in-game bug report form's upload, end to end, with the PACKAGED game and a local stand-in for the crash endpoint
# ---------------------------------------------------------------------------------------------------------------------------------------------

BUGREPORT_RESULT_RE = re.compile(r"\[MOBugReport\] (SENT|NOT SENT) id=(\w+) http=(\d+): (.*?)(?:  \[saved: (.*?)\])?\s*$", re.M)


def analyse_bugreport_upload(body, meta, title, user_name="", computer_name="", expect_screenshot=True, expect_world=True):
    """Judge one received bug report from its bytes and the receiver's metadata. Pure: returns [(label, ok, evidence)].

    Checks the contract that matters to whoever reads these on the website: it is a CR1 bundle, marked as a bug report (not a crash), carries the
    player's words, the game-state fields, the log and the screenshot, arrived with the query the crash endpoint expects -- and contains nothing that
    names this machine's user or computer (the log is the dangerous part: UE writes both into its own header lines)."""
    import ue_crash_bundle as cb
    rows = []
    try:
        bundle = cb.parse_bundle(body)
    except cb.BundleError as e:
        return [("the upload is a well-formed CR1 bundle", False, str(e))]
    rows.append(("the upload is a well-formed CR1 bundle", True, f"{bundle.directory}; files {bundle.names()}"))

    info = cb.summarise(bundle)
    rows.append(("it is marked as a bug report, not a crash", info["kind"] == "bugreport", info["kind"]))
    rows.append(("the title the player typed arrived intact", info["title"] == title, repr(info["title"])))
    q = (meta or {}).get("query", {})
    rows.append(("the query carries what the crash endpoint expects (AppID, UploadType, AppVersion, UserID) plus ReportKind=bugreport",
                 q.get("AppID") == "CrashReporter" and q.get("UploadType") == "crashreports" and q.get("ReportKind") == "bugreport"
                 and bool(q.get("AppVersion")) and bool(q.get("UserID")), json.dumps(q)))

    needed = ["CrashContext.runtime-xml", "BugReport.txt", "game.log"] + (["Screenshot.jpg"] if expect_screenshot else [])
    missing = [n for n in needed if bundle.get(n) is None]
    rows.append(("the bundle holds the context, the readable report, the log" + (" and the screenshot" if expect_screenshot else ""), not missing,
                 f"missing {missing}" if missing else ", ".join(f"{n} {len(bundle.get(n))} B" for n in needed)))

    if expect_screenshot and bundle.get("Screenshot.jpg") is not None:
        jpg = bundle.get("Screenshot.jpg")
        rows.append(("the screenshot is a real JPEG of plausible size", jpg[:2] == b"\xff\xd8" and jpg[-2:] == b"\xff\xd9" and 5_000 < len(jpg) < 3_000_000, f"{len(jpg)} B"))

    fields = info.get("fields", {})
    wanted = ["Build.Version", "Build.Commit", "System.OS", "System.GPU", "Session.NetMode", "Session.Map"]
    if expect_world:
        wanted += ["Session.WorldSeed", "Clock.GameTime", "Player.Pawn", "Player.Location"]
    absent = [k for k in wanted if not fields.get(k)]
    rows.append(("the game-state fields are filled in", not absent, f"absent {absent}" if absent else f"{len(fields)} fields"))
    if expect_world:
        rows.append(("the report came from a listen-server session with a spawned pawn",
                     fields.get("Session.NetMode") == "ListenServer" and fields.get("Player.Pawn", "(none)") != "(none)",
                     f"NetMode={fields.get('Session.NetMode')} Pawn={fields.get('Player.Pawn')}"))

    log = bundle.text("game.log")
    rows.append(("the log tail is this game's log", "LogInit" in log or "LogMOFramework" in log, f"{len(log)} chars"))
    rows.append(("the readable report lists the attachments and the player's category",
                 "--- Attachments ---" in bundle.text("BugReport.txt") and bool(info.get("category")), info.get("category", "")))

    def leaks(name):
        out = []
        for fname, data in bundle.files:
            if fname.endswith(".jpg"):
                continue
            text = data.decode("utf-8", errors="replace")
            if len(name) >= 3 and re.search(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])", text, re.I):
                out.append(fname)
        return out

    if user_name:
        found = leaks(user_name)
        rows.append((f"no file names this machine's user ('{user_name}')", not found, f"found in {found}" if found else "scrubbed"))
    if computer_name:
        found = leaks(computer_name)
        rows.append((f"no file names this machine's computer ('{computer_name}')", not found, f"found in {found}" if found else "scrubbed"))
    return rows


def _free_port():
    import socket
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _start_receiver(out_dir, port, status=200):
    import urllib.request
    script = os.path.join(HERE, "bugreport_receiver.py")
    log = open(os.path.join(out_dir, "receiver.log"), "wb")
    proc = subprocess.Popen([sys.executable, script, "--port", str(port), "--out", os.path.join(out_dir, "received"), "--status", str(status)],
                            stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    for _ in range(50):
        try:
            urllib.request.urlopen(f"http://127.0.0.1:{port}/health", timeout=1).read()
            return proc
        except OSError:
            time.sleep(0.2)
    proc.terminate()
    return None


def _received(out_dir):
    """[(body bytes, meta dict)] in arrival order."""
    items = []
    for root, _, files in os.walk(os.path.join(out_dir, "received")):
        for f in files:
            if f.endswith(".json"):
                with open(os.path.join(root, f), encoding="utf-8") as fh:
                    meta = json.load(fh)
                with open(os.path.join(root, f[:-5] + ".bin"), "rb") as fh:
                    body = fh.read()
                items.append((meta["receivedAt"], body, meta))
    return [(b, m) for _, b, m in sorted(items, key=lambda t: t[0])]


def bugreport_test(package=False, boot_timeout=180):
    """The bug report upload with the PACKAGED game, three ways, judged from what a receiver gets and from the game's log:
      1. accepted      host a world, send a report with a screenshot -> the receiver holds a valid bundle with the state, the log and no identity
      2. server error  the endpoint answers 500 -> the game says NOT SENT and keeps the bundle in Saved/BugReports, which parses
      3. unreachable   nothing listens -> same fallback, and the player is told why
    A report is sent by `MO.BugReport.SendTest` (the same Submit the form's Send button calls) through -ExecCmds, so no window is driven; the form
    itself is checked by hand in a real window (Docs/AUTONOMOUS_TOOLING.md)."""
    import getpass
    import shutil
    sys.path.insert(0, HERE)
    import ue_crash_bundle as cb

    rep = Report()
    pid = recv = None
    work = tempfile.mkdtemp(prefix="mo_bugreport_")
    saved_dir = os.path.join(PKG_ROOT, "MO57", "Saved", "BugReports")
    user, computer = getpass.getuser(), os.environ.get("COMPUTERNAME", "")

    def kept_files():
        return set(os.listdir(saved_dir)) if os.path.isdir(saved_dir) else set()

    try:
        if package and not _package_build():
            return rep.step("the Development package builds", False, "see Saved/Logs/package.log")
        if not rep.step("the staged packaged build exists", os.path.exists(PKG_EXE), PKG_EXE):
            return False
        running = _pkg_alive_count()
        if not rep.step("no other MO57.exe is running (this mode must own the logs and never touches others)", running == 0, f"{running} running"):
            return False

        phases = [("accepted", 200, True), ("server error", 500, False), ("unreachable", None, False)]
        for index, (label, status, with_world) in enumerate(phases, 1):
            out = os.path.join(work, f"phase{index}")
            os.makedirs(out)
            port = _free_port()
            title = f"PackagedBugReport {label} {int(time.time())}"
            if status is not None:
                recv = _start_receiver(out, port, status)
                if not rep.step(f"[{label}] the local receiver is up on 127.0.0.1:{port} (answering {status})", recv is not None):
                    return False
            before = kept_files()
            # The previous phase's game may still own MO57.log for a moment after the new one starts: only a result with an id we have not seen counts.
            stale_ids = {m.group(2) for m in BUGREPORT_RESULT_RE.finditer(_pkg_read("MO57.log"))}

            host = "MO.Session.Host BugReportTest 4," if with_world else ""
            delay = 55 if with_world else 25
            cmds = (f"{host}MO.BugReport.EndpointOverride http://127.0.0.1:{port}/datarouter/crashes/test,MO.BugReport.CooldownSeconds 0,"
                    f"MO.BugReport.SendTest -shot -delay={delay} {title}")
            print(f"[nettest] bugreport [{label}]: launching the packaged game ...", flush=True)
            pid = _pkg_launch(f'-NoSteam -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="{cmds}"')
            deadline, result = time.time() + boot_timeout + delay + 30, None
            while time.time() < deadline and result is None:
                result = next((m for m in BUGREPORT_RESULT_RE.finditer(_pkg_read("MO57.log")) if m.group(2) not in stale_ids), None)
                time.sleep(2)
            if not rep.step(f"[{label}] the game finished the send and logged the outcome", result is not None,
                            result.group(0) if result else _pkg_read("MO57.log")[-300:].replace("\n", " | ")):
                return False
            sent, http, message, saved_path = result.group(1) == "SENT", int(result.group(3)), result.group(4), result.group(5)
            rep.step(f"[{label}] outcome as expected", sent == (status == 200), f"{result.group(1)} http={http}: {message}")

            if status == 200:
                got = _received(out)
                if rep.step(f"[{label}] the receiver holds exactly one upload", len(got) == 1, f"{len(got)}"):
                    for row in analyse_bugreport_upload(got[0][0], got[0][1], title, user, computer, expect_screenshot=True, expect_world=with_world):
                        rep.step(f"[{label}] {row[0]}", row[1], row[2])
                raw = _pkg_read("MO57.log")
                rep.step(f"[{label}] CONTROL: the game's own raw log DOES name this machine's user and computer (so 'scrubbed' above is a real result)",
                         bool(user) and bool(computer) and re.search(re.escape(user), raw, re.I) is not None and re.search(re.escape(computer), raw, re.I) is not None,
                         f"user '{user}' / computer '{computer}'")
                rep.step(f"[{label}] nothing was kept on disk for a report that was sent", kept_files() == before)
            else:
                if status is not None:
                    rep.step(f"[{label}] the receiver did see the POST (so the 500 was a real answer)", len(_received(out)) == 1, f"{len(_received(out))}")
                kept = sorted(kept_files() - before)
                if rep.step(f"[{label}] the unsent report was kept in Saved/BugReports", len(kept) == 1 and bool(saved_path), f"{kept}"):
                    try:
                        with open(os.path.join(saved_dir, kept[0]), "rb") as fh:
                            info = cb.summarise(cb.parse_bundle(fh.read()))
                        rep.step(f"[{label}] the kept file is the complete report (it parses, same title)", info["kind"] == "bugreport" and info["title"] == title, info["title"])
                    except (cb.BundleError, OSError) as e:
                        rep.step(f"[{label}] the kept file is the complete report", False, str(e))
                rep.step(f"[{label}] the player is told what happened and where the file is",
                         "saved to" in message and ("server answered" in message or "reach" in message), message)
            _pkg_stop(pid)
            pid = None
            if recv:
                recv.terminate()
                recv = None
            time.sleep(3)
        return rep.ok
    finally:
        print(f"[nettest] bugreport: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        _pkg_stop(pid)
        if recv:
            recv.terminate()
        shutil.rmtree(work, ignore_errors=True)


def soak_test(uproject, editor_exe, rounds=6, boot_timeout=300):
    """Host a FRESH random-seed world, join a client, compare the ground over a grid -- `rounds` times with the same two
    processes (back to the menu between rounds). One agreeing round proves little: a host/client terrain mismatch was seen
    once (client ground 130-180 cm higher than the host's) and not again; this measures how often it happens and what the
    difference looks like (a printed signed map per round)."""
    rep = Report()
    host, client = names = ("soakhost", "soakclient")
    try:
        print(f"[nettest] soak: {rounds} rounds, REAL gameplay map, fresh random seed each round ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not rep.step("both instances boot and expose a world", all(wait_ready(n, boot_timeout) for n in names)):
            return False
        for r in range(1, rounds + 1):
            print(f"[nettest] --- round {r}/{rounds}", flush=True)
            for n in names:
                call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
            time.sleep(2)
            host_off = log_size(host)
            console(host, f"MO.Session.Host Soak{r} 4")
            created = wait_log(host, r"CreateSession succeeded|Already in a session|FAILED", since=host_off, timeout=60)
            if not rep.step(f"round {r}: host creates the session", bool(created) and "succeeded" in created.group(0),
                            created.group(0) if created else "no result"):
                return False
            reached, st = _wait_state(host, "ListenServer", "", 300)
            if not rep.step(f"round {r}: fresh world loaded", reached, str(st)):
                return False
            found = None
            for _ in range(5):
                off = log_size(client)
                console(client, "MO.Session.Find")
                m = wait_log(client, FOUND_RE.pattern, since=off, timeout=30)
                found = int(FOUND_RE.search(m.group(0)).group(1)) if m else None
                if found:
                    break
                time.sleep(3)
            if not rep.step(f"round {r}: client finds the session", bool(found), f"{found} result(s)"):
                return False
            join_off = log_size(client)
            console(client, "MO.Session.Join 0")
            reached, cst = _wait_state(client, "Client", "", 300)
            if not rep.step(f"round {r}: client joined", reached, str(cst)):
                return False
            time.sleep(5)
            _terrain_agreement(rep, host, client, join_off, host_since=host_off)
            # to the menu again: client first, then the host (ends the session)
            for n in (client, host):
                call(n, ["py", "-c", f'unreal.GameplayStatics.open_level(world, "{MENU_MAP}")'], timeout=30)
            for n in names:
                reached, st = _wait_state(n, "Standalone", MENU_MAP, 120)
                if not rep.step(f"round {r}: {n} back at the menu", reached, str(st)):
                    return False
            time.sleep(3)
        bad = [row for row in rep.rows if not row[1]]
        print(f"[nettest] soak: {rounds} rounds, {len(bad)} failing step(s)", flush=True)
        return rep.ok
    finally:
        print(f"[nettest] soak: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        for n in names:
            stop(n)


def features_test(uproject, editor_exe, keep=False, boot_timeout=300):
    """New game starts at 08:00 under clear skies; the `starter` kit; the Tilde console popup. One -game instance, real map,
    hosting a FRESH world the way the Host button does (that is the new-game path)."""
    rep = Report()
    name = "feat"
    try:
        start(name, uproject, editor_exe, pos=(0, 0), nosteam=True)
        if not rep.step("instance boots and exposes a world", wait_ready(name, boot_timeout)):
            return False
        call(name, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(2)

        # the engine console must not own Tilde (it consumes the key before the controller sees it)
        out = _probe(name, "ks = [str(k.key_name) for k in unreal.InputSettings.get_input_settings().console_keys]; out('CONSOLEKEYS=' + ','.join(ks))")
        m = re.search(r"CONSOLEKEYS=(\S*)", out)
        rep.step("the engine console no longer owns Tilde", bool(m) and "Tilde" not in m.group(1).split(","),
                 f"ConsoleKeys = [{m.group(1) if m else out.strip()[-120:]}]")

        off = log_size(name)
        console(name, "MO.Session.Host Features 4")
        created = wait_log(name, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=60)
        if not rep.step("hosts a FRESH world (the new-game path)", bool(created) and "succeeded" in created.group(0),
                        created.group(0) if created else "no result"):
            return False
        reached, st = _wait_state(name, "ListenServer", "", 300)
        if not rep.step("fresh world loaded and the player has a pawn", reached, str(st)):
            return False

        # ---- 08:00 under clear skies
        applied = wait_log(name, r"New game: applied weather preset 'Clear_Skies'", since=off, timeout=60)
        rep.step("a new game applies the 'Clear_Skies' weather preset", bool(applied), applied.group(0) if applied else "no 'applied' line")
        off2 = log_size(name)
        console(name, "MO.Clock.Info")
        clk = wait_log(name, r"GameDateTime=(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d)", since=off2, timeout=15)
        hh, mm = (int(clk.group(4)), int(clk.group(5))) if clk else (None, None)
        rep.step("a new game starts at 08:00 (not a minute past a few after the world loaded)", clk is not None and hh == 8 and mm < 10,
                 clk.group(0) if clk else "no clock line")
        # A preset is a TRANSITION, not a switch: watch the weather for up to 90 s and report how it settles.
        timeline, clear_after = [], None
        t_w = time.time()
        while time.time() - t_w < 90:
            off2 = log_size(name)
            console(name, "MO.Weather.Info")
            w = wait_log(name, r"\[MOWeather\] ([^|\n]*)\| Cloud=([\d.]+) Fog=([\d.]+) Rain=([\d.]+)", since=off2, timeout=15)
            if w:
                label, cloud, rain = w.group(1).strip(), float(w.group(2)), float(w.group(4))
                timeline.append(f"t+{time.time() - t_w:.0f}s {label} cloud={cloud:.2f} rain={rain:.2f}")
                if "clear" in label.lower() and rain == 0:
                    clear_after = time.time() - t_w
                    break
            time.sleep(6)
        print("[nettest]   weather: " + " -> ".join(timeline), flush=True)
        rep.step("the sky actually clears after the new-game preset (transition finishes within 90 s)", clear_after is not None,
                 f"clear after ~{clear_after:.0f}s" if clear_after is not None else "never reported Clear: " + (timeline[-1] if timeline else "no reading"))

        # ---- starter kit
        def counts():
            o = _probe(name, "p = unreal.GameplayStatics.get_player_pawn(world, 0); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                             "out('KIT sticks=%d stones=%d' % (inv.get_item_count_by_definition_id('Stick01'), inv.get_item_count_by_definition_id('Stone01')))")
            mm_ = re.search(r"KIT sticks=(\d+) stones=(\d+)", o)
            return (int(mm_.group(1)), int(mm_.group(2))) if mm_ else None
        before = counts()
        off2 = log_size(name)
        console(name, "starter")
        kit = wait_log(name, r"starter kit:([^\n]*)", since=off2, timeout=15)
        after = counts()
        rep.step("`starter` gives 50 sticks and 20 stones", bool(kit) and before is not None and after is not None
                 and after[0] - before[0] == 50 and after[1] - before[1] == 20,
                 f"inventory sticks/stones {before} -> {after}; log:{kit.group(1) if kit else ' none'}")

        # ---- the console popup's execution path (sanitising + capture)
        off2 = log_size(name)
        console(name, "MO.Console.Run ~ MO.Clock.Info")
        ran = wait_log(name, r"\[MOConsole\] > MO\.Clock\.Info\s+=>\s+(\S[^\n]*)", since=off2, timeout=15)
        rep.step("the popup path strips a leading Tilde and reports the command's result", bool(ran), ran.group(0)[:200] if ran else "no [MOConsole] result line")

        # ---- Tilde opens the popup (a real Slate key event, the same route as the keyboard)
        off2 = log_size(name)
        console(name, "MO.Test.PressKey Tilde")
        opened = wait_log(name, r"\[MOConsole\] popup opened", since=off2, timeout=15)
        rep.step("pressing Tilde opens the console popup", bool(opened),
                 "popup opened" if opened else "no '[MOConsole] popup opened' line (the key never reached the controller)")
        if opened:
            off3 = log_size(name)
            console(name, "MO.Test.PressKey Tilde")  # a second press must NOT stack a second popup
            time.sleep(2)
            console(name, "MO.Test.PressKey Escape")
            rep.step("a second Tilde does not stack another popup", "popup opened" not in read_log_from(name, off3))
        return rep.ok
    finally:
        print(f"[nettest] features: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not keep:
            stop(name)


def steam_host_test(uproject, editor_exe, keep=False, boot_timeout=240, map_path=DEFAULT_MAP):
    """One standalone process on REAL Steam: init, lobby creation, listen-server travel. A single Steam account
    cannot join its own lobby, so this proves everything up to (not including) the second player."""
    rep = Report()
    name = "steamhost"
    try:
        print("[nettest] launching one instance on real Steam ...", flush=True)
        start(name, uproject, editor_exe)
        if not rep.step("instance boots and exposes a world", wait_ready(name, boot_timeout)):
            return False
        log = read_log_from(name, 0)
        init = re.search(r"STEAM: \[AppId: (\d+)\] Client API initialized (\d)", log)
        rep.step("SteamAPI initializes", bool(init and init.group(2) == "1"),
                 f"AppId {init.group(1)}" if init else "no 'Client API initialized' line")
        rep.step("account owns the app license", "Steam User is subscribed 1" in log)
        rep.step("MOSession activates the Steam subsystem", "active online subsystem = STEAM" in log)
        if not rep.ok:
            return False

        call(name, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
        time.sleep(2)
        off = log_size(name)
        ok, ev = host_session(name, "SteamTest", map_path)
        if not rep.step(f"HostSession accepted ({map_path or 'real gameplay map'})", ok, ev):
            return False
        created = wait_log(name, r"CreateSession succeeded|CreateSession FAILED|HostSession FAILED", since=off, timeout=60)
        rep.step("Steam lobby created (CreateSession succeeded)", bool(created and "succeeded" in created.group(0)),
                 created.group(0) if created else read_log_from(name, off)[-400:])
        if not rep.ok:
            return False
        deadline, st = time.time() + 150, None
        while time.time() < deadline:
            st = state(name)
            if in_world(st, "ListenServer", map_path):
                break
            time.sleep(4)
        rep.step("travels as a ListenServer on the expected map", in_world(st, "ListenServer", map_path), str(st))
        drv = net_driver(name, off)
        rep.step("GameNetDriver is the Steam driver (lobby addresses are Steam P2P)",
                 bool(drv and drv.startswith("Steam")), drv or "not seen in log")
        return rep.ok
    finally:
        print(f"[nettest] steam-host: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        if not keep:
            stop(name)
