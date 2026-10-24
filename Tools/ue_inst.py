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
import math
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


def build_launch_args(uproject, log_path, pos=(0, 0), res=(640, 360), nosteam=False, extra=(), creator=False, sound=False):
    """Command line (after the exe) of a standalone game process. Pure: unit-tested offline.

    -forcelogflush so the log can be polled line by line; no -log (no console window to clutter the desktop);
    -NoSteam makes the Steam OSS refuse to start, so the Null OSS (LAN beacon) and the plain IP net driver
    take over -- exactly what the editor itself runs on.
    -NoCharacterCreator (unless creator=True): a new game starts straight away instead of waiting in the character creator
    for a Begin no harness presses (AMOGameMode::OnPawnLandedSafely).
    -nosound unless sound=True: harness instances stay silent; audio investigations need the real device (MO.Audio.Diag, the
    [MOAudio] HB heartbeat).
    -MOHoldContextMenus: the instance's menus leave the real cursor alone (no warp onto a menu, no close when it leaves): it belongs to
    whoever sits at the machine, whose next mouse move closed a menu mid-test (Oct 22, UMOContextMenuBase::IgnoresRealCursor).
    """
    args = [uproject, "-game", "-windowed", f"-ResX={res[0]}", f"-ResY={res[1]}", f"-WinX={pos[0]}",
            f"-WinY={pos[1]}", "-nosplash", "-forcelogflush", f"-abslog={log_path}", "-NoQualityBenchmark", "-MOHoldContextMenus"]
    if not sound:
        args.append("-nosound")
    if nosteam:
        args.append("-NoSteam")
    if not creator:
        args.append("-NoCharacterCreator")
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
    # The last run's log survives one restart as game.prev.log: a one-off failure (an empty world, a crash) found only after a
    # restart keeps its evidence (Oct 19: a world with no vegetation lost its log to the restart that checked it).
    try:
        os.replace(game_log(name), os.path.join(d, "game.prev.log"))
    except OSError:
        pass
    for f in ("ue_cmd.txt", "ue_out.txt", "game.log"):
        try:
            os.remove(os.path.join(d, f))
        except OSError:
            pass
    # MO_INST_EXTRA_<NAME> adds launch flags to one instance of any harness run, e.g. MO_INST_EXTRA_ACTCLIENT=-rhivalidation for the joiner's
    # render crash after far teleports (N-185) without touching the host.
    extra = os.environ.get(f"MO_INST_EXTRA_{name.upper()}", "").split()
    if extra:
        kw["extra"] = tuple(kw.get("extra", ())) + tuple(extra)
    cmd = [editor_exe] + build_launch_args(uproject, game_log(name), **kw)
    flags = getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    zen_was_up = _zen_running()
    p = subprocess.Popen(cmd, env=env_for(name), creationflags=flags, stdin=subprocess.DEVNULL,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(_pid_file(name), "w", encoding="ascii") as f:
        f.write(str(p.pid))
    if not zen_was_up:
        _wait_zen_ready(name)
    return p.pid


def _zen_running():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq zenserver.exe", "/NH", "/FO", "CSV"], capture_output=True, text=True).stdout
    return "zenserver.exe" in out


def _wait_zen_ready(name, timeout=90):
    """With no Zen server running (the editor closed, or a crash took it down), the first instance launches one: wait for that before the
    next instance starts. Two instances bringing Zen up at once hung the one that launched it at "Waiting for ZenServer to be ready..."
    for good (Oct 21: the host of a nettest pair never got past it; the same pair failed to boot twice right after a crash)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with open(game_log(name), encoding="utf-8", errors="replace") as f:
                if "ZenServer AutoLaunch initialization completed" in f.read():
                    return True
        except OSError:
            pass
        time.sleep(1)
    print(f"[inst] {name}: no 'ZenServer AutoLaunch initialization completed' line in {timeout} s; starting the next instance anyway", flush=True)
    return False


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


def wait_log(name, regex, since=0, timeout=60, step=0.25):
    """Poll the instance log from byte offset `since` for `regex`; returns the match or None. Fails fast if the
    process died (a crashed instance would otherwise burn the whole timeout). Polls every 0.25 s (was 1 s: a line that landed just after
    a poll cost up to a second, at 136 call sites) and asks whether the process lives every 2 s (`tasklist` is a process of its own)."""
    pat = re.compile(regex)
    deadline = time.time() + timeout
    next_alive_check = time.time() + 2.0
    while time.time() < deadline:
        m = pat.search(read_log_from(name, since))
        if m:
            return m
        if time.time() >= next_alive_check:
            next_alive_check = time.time() + 2.0
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
    # No log wait in the bridge: it returns once the command has RUN (its end marker follows the command in the same queue), and every
    # caller that wants a result waits for its own line (wait_log). The old --grep MOQUERY --logwait 1 waited a full second after most
    # commands for a line they never print (the loop deep dive, Oct 22).
    call(name, ["run", cmd, "--logwait", "0"], timeout=30)
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


def _far_remote_pawn_9km(rep, host, client):
    """Steam playtest Oct 16 2026: "confirmed client character will fall through world if i get far enough away" -- 8.8 km from the host. Like
    _far_remote_pawn (150 m), at 9 km, and dropped 15 m above the GENERATOR's ground there (the host's own height means nothing 9 km away)."""
    hxy = _probe(host, PROBE_XY)
    mx, my = (re.search(rf"{k}=(-?[\d.]+)", hxy) for k in "XY")
    if not rep.step("host: located the host pawn", bool(mx and my), hxy.strip()[-80:]):
        return
    hx, hy = float(mx.group(1)), float(my.group(1))
    target = None
    # 8 directions x 9 / 10 / 11 km: a coastal host has sea on several sides (Oct 16: the four 9 km points were all sea).
    candidates = [(r * math.cos(k * math.pi / 4.0), r * math.sin(k * math.pi / 4.0)) for r in (900000.0, 1000000.0, 1100000.0) for k in range(8)]
    for dx, dy in candidates:
        off = log_size(host)
        console(host, f"MO.WorldGen.Sample {hx + dx:.0f} {hy + dy:.0f}")
        m = wait_log(host, r"WORLDGEN Sample x=\S+ y=\S+ seed=\S+ z=(-?[\d.]+)", since=off, timeout=5)
        if m and 200.0 < float(m.group(1)) < 80000.0:   # dry land, not the sea
            target = (hx + dx, hy + dy, float(m.group(1)))
            break
    if not rep.step("found dry land 9 km from the host (generator ground)", target is not None, str(target)):
        return
    tx, ty, gz = target
    who = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); out('PAWNNAME=' + (p.get_name() if p else 'none'))")
    mname = re.search(r"PAWNNAME=(\S+)", who)
    pawn_name = mname.group(1) if mname else "none"
    off = log_size(host)
    _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, 1); p.set_actor_location(unreal.Vector({tx:.0f}, {ty:.0f}, {gz + 1500.0:.0f}), False, True)")
    samples = []
    for _ in range(8):
        time.sleep(4)
        samples.append((_num(_probe(host, PROBE_Z_PLAYER1), "Z"), _num(_probe(client, PROBE_Z), "Z")))
    rescued = len(re.findall(rf"\[MOCharacter\] {re.escape(pawn_name)}: Fall-through detected!", read_log_from(host, off)))
    last_s, last_c = samples[-1]
    ok = (last_s is not None and last_c is not None and abs(last_s - last_c) <= 150.0 and gz - 300.0 < last_s < gz + 1000.0 and rescued == 0)
    rep.step("a remote player 9 km from the host stands on ground the SERVER has (no fall-through, server and client agree)", ok,
             f"generator ground {gz:.0f}; server/client z: " + ", ".join(f"{s:.0f}/{c:.0f}" for s, c in samples if s is not None and c is not None)
             + f"; fall-through rescues on the host: {rescued}")


def _joiner_long_run(rep, host, client):
    """#9 as it was played (Steam playtest Oct 16 2026: "client character will fall through world if i get far enough away"): not a teleport -- the
    joiner SPRINTS away from the host for 150 s over land, while the server's ground has to be built around it as it goes. Every 10 s: the joiner's
    height on the server and on the client (they must agree), how far it got, and any fall-through rescue on the host."""
    if not _worlds_built(rep, host, client):   # run first after the join (MO_ACTIONS_ONLY), the joiner stopped at 51 m on its unbuilt world
        return
    pxy = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); l = p.get_actor_location() if p else unreal.Vector(0, 0, 0); "
           "out('JX=%.0f JY=%.0f JZ=%.0f' % (l.x, l.y, l.z))")
    m = re.search(r"JX=(-?\d+) JY=(-?\d+) JZ=(-?\d+)", _probe(host, pxy))
    if not rep.step("host: located the joiner", bool(m)):
        return
    jx, jy = float(m.group(1)), float(m.group(2))
    land = []   # every heading with land 0.3, 0.7 and 1.1 km out
    for k in range(8):
        a = k * math.pi / 4.0
        ok = True
        for r in (30000.0, 70000.0, 110000.0):
            off = log_size(host)
            console(host, f"MO.WorldGen.Sample {jx + r * math.cos(a):.0f} {jy + r * math.sin(a):.0f}")
            s = wait_log(host, r"WORLDGEN Sample x=\S+ y=\S+ seed=\S+ z=(-?[\d.]+)", since=off, timeout=5)
            if not s or not (200.0 < float(s.group(1)) < 80000.0):
                ok = False
                break
        if ok:
            land.append(a)
    if not rep.step("a heading with 1.1 km of land ahead of the joiner", bool(land), f"{len(land)} of 8 headings: " + ", ".join(f"{math.degrees(a):.0f}" for a in land)):
        return
    pawn_name = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); out('PAWNNAME=' + (p.get_name() if p else 'none'))")) or [None, "none"])[1]
    # Of those, the one whose first 100 m is clearest; a run that stalls (under 10 m in a sample) turns to the clearest again from where it stands,
    # as a player steps round a rock (Oct 24: the run stopped at 49 m against one for all 150 s). Where it runs does not matter: that it gets far.
    _face_clear_heading(host, client, land, 10000.0)
    console(client, "MO.Test.Input ToggleJog")
    console(client, "MO.Test.Input SprintStart")
    off = log_size(host)
    console(client, "MO.Test.MoveFor 0 1 150")
    rows, worst, turns, last = [], 0.0, 0, None
    for _ in range(15):
        time.sleep(10)
        sm = re.search(r"JX=(-?\d+) JY=(-?\d+) JZ=(-?\d+)", _probe(host, pxy))
        cz = _num(_probe(client, PROBE_Z), "Z")
        if sm and cz is not None:
            sz = float(sm.group(3))
            here = (float(sm.group(1)), float(sm.group(2)))
            dist = math.hypot(here[0] - jx, here[1] - jy) / 100.0
            worst = max(worst, abs(sz - cz))
            rows.append(f"{dist:.0f}m:{sz:.0f}/{cz:.0f}")
            if last and math.hypot(here[0] - last[0], here[1] - last[1]) < 1000.0:
                _face_clear_heading(host, client, land, 10000.0)
                turns += 1
            last = here
    console(client, "MO.Test.Input SprintEnd")
    console(client, "MO.Test.Input ToggleJog")
    rescued = len(re.findall(rf"\[MOCharacter\] {re.escape(pawn_name)}: Fall-through detected!", read_log_from(host, off)))
    # Back beside the host: the later steps assume the two players are within relevancy of each other (9 km away, neither sees the other's pawn).
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); j = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "j.set_actor_location(h.get_actor_location() + unreal.Vector(300.0, 0.0, 150.0), False, True)")
    time.sleep(5)
    far = float(rows[-1].split("m:")[0]) if rows else 0.0
    # The two heights are read ~0.5 s apart: running up a steep slope at 7 m/s they differ by a metre or two without any disagreement. A fall-through
    # is thousands of cm; at rest they must agree closely.
    rest = rows[-1] if rows else ""
    rm = re.search(r":(-?\d+)/(-?\d+)$", rest)
    rest_gap = abs(int(rm.group(1)) - int(rm.group(2))) if rm else 9999
    rep.step("the joiner sprinted away for 150 s on ground the server has: server and client agree, no fall-through",
             bool(rows) and far > 400.0 and worst <= 500.0 and rest_gap <= 60 and rescued == 0,
             f"distance/server z/client z: {', '.join(rows)}; worst gap while running {worst:.0f} cm, at rest {rest_gap} cm; rescues on the host {rescued}; "
             f"turned past an obstacle {turns}x")


def game_test(uproject, editor_exe, keep=False, boot_timeout=300, withhold_seed=False, skew_seed=False, withhold_sync=False, seed=None):
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
        console(host, "MO.Session.Host GameTest 4" + (f" {seed}" if seed else ""))
        created = wait_log(host, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=45)
        if not rep.step("host: Host path creates the session", bool(created) and "succeeded" in created.group(0),
                        created.group(0) if created else "no result"):
            return False
        reached, st = _wait_state(host, "ListenServer", "", 300)
        if not rep.step("host: real map loaded and the host has a pawn", reached, str(st)):
            return False

        # Tutorial: a new player's first popup is "Move and Look" (58bcd369, Oct 15 2026: the possession hint waits until a
        # survivor has joined -- with one pawn there is nobody to switch to). FindWidget matches TextBlock text, so look for
        # the hint's body on screen, and make sure the possession hint is not showing in its place.
        hint_text, possess_text = None, None
        deadline = time.time() + 90
        while time.time() < deadline and not hint_text:
            off = log_size(host)
            console(host, 'MO.Test.FindWidget "to walk"')
            m = wait_log(host, r"WIDGET '[^']*' \([^)]*\) text='([^']*to walk[^']*)'", since=off, timeout=6)
            hint_text = m.group(1) if m else None
            if not hint_text:
                time.sleep(4)
        off = log_size(host)
        console(host, 'MO.Test.FindWidget "open the possession menu"')
        wait_log(host, r"FindWidget\('open the possession menu'\): (\d+) match", since=off, timeout=6)
        m = re.search(r"FindWidget\('open the possession menu'\): (\d+) match", read_log_from(host, off))
        possess_text = int(m.group(1)) if m else None
        rep.step("host: the first tutorial popup is 'Move and Look' (the possession hint waits for a survivor)",
                 bool(hint_text) and possess_text == 0,
                 f"popup: {hint_text!r}; possession hint on screen at the same time: {possess_text}")

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


def _host_and_join(rep, host, client, display, boot_timeout=300, seed=None):
    """Host a FRESH world on the real map and join `client` into it (the Host button's path, then the Join path). Returns
    True when both are in the world with a pawn each. Used by the modes that need a joined pair and nothing else."""
    if not rep.step("both instances boot and expose a world", all(wait_ready(n, boot_timeout) for n in (host, client))):
        return False
    for n in (host, client):
        call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    time.sleep(2)
    off = log_size(host)
    console(host, f"MO.Session.Host {display} 4" + (f" {seed}" if seed else ""))   # a seed = a known world (nettest --seed); else random
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
        "gave = inv.add_item_by_guid_no_stack(g, 'Stick01', 1); "
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
    hoff = log_size(host)
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
    if got:
        # Quests are the player's: the joiner's pickup happens on the host but must count on the JOINER's machine (UMOQuestSubsystem::ReportForActor ->
        # ClientReportQuestEvent), not the host's. Before: it counted for the host and the joiner's gather tutorial could never finish.
        time.sleep(2)
        on_client = re.search(r"\[MOQuestSubsystem\] HandleItemPickedUp: Stick01 x1[^\n]*", read_log_from(client, off))
        on_host = re.search(r"\[MOQuestSubsystem\] HandleItemPickedUp: Stick01[^\n]*", read_log_from(host, hoff))
        rep.step("client pickup: the quest pickup is credited on the JOINER's machine", bool(on_client), on_client.group(0) if on_client else "no HandleItemPickedUp line on the client")
        rep.step("client pickup: ... and NOT on the host's", on_host is None, "nothing on the host" if on_host is None else on_host.group(0))


def _client_press_e(rep, host, client):
    """CLIENT presses E on a low object it aims at, through the real input path (TryInteract -> ServerRequestInteract -> the
    server's check of the claimed target). Until Oct 15 2026 the server re-derived the target with its own 12 cm sweep while the
    client aims with a 30 cm one, and refused E on low objects the crosshair covered (a built campfire, a stick on the ground).
    _client_pickup cannot catch that: MO.Test.PickupNearest uses the separate distance-only pickup RPC.
    The joiner works on clear ground (Oct 24: on a slope by a rock the camera's spring arm swung with every turn and the aim never settled,
    15.9 cm off after 7 tries) and goes back where it was after, where the next cases expect it."""
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); builtins.MO_E_HOME = p.get_actor_location(); out('E_HOME_SAVED')")
    _to_clear_ground(host, 1, bring_host=False)
    try:
        _client_press_e_here(rep, host, client)
    finally:
        _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_E_HOME', None); "
                     "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('E_HOME')")
        time.sleep(2)


def _client_press_e_here(rep, host, client):
    """_client_press_e where the joiner stands (its caller moves it to clear ground and back)."""
    setup = _probe(host,
        "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); "
        "inv = p.get_component_by_class(unreal.MOInventoryComponent); g = unreal.GuidLibrary.new_guid(); builtins.MO_E_GUID = g; "
        # Bark: an item the joiner does not carry -- a stick merged into the stick stack _client_pickup leaves behind, and its GUID was gone
        "gave = inv.add_item_by_guid_no_stack(g, 'Bark01', 1); "
        "w = inv.drop_item_by_guid(g, p.get_actor_location() + unreal.Vector(150.0, 60.0, 10.0), unreal.Rotator(0.0, 0.0, 0.0)); "
        "out('E_SETUP gave=%s item=%s' % (gave, w.get_name() if w else 'none'))")
    m = re.search(r"E_SETUP gave=(\w+) item=(\S+)", setup)
    if not rep.step("host: dropped a piece of bark in front of the joiner's pawn", bool(m) and m.group(1) == "True" and m.group(2) != "none",
                    m.group(0) if m else setup.strip()[-160:]):
        return
    probe_have = ("import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                  "inv = p.get_component_by_class(unreal.MOInventoryComponent); r = inv.try_get_entry_by_guid(builtins.MO_E_GUID); "
                  "out('HAVE=%s' % (r[0] if isinstance(r, tuple) else bool(r)))")
    time.sleep(6)  # the dropped item replicates to the client
    rep.step("CONTROL: before the client presses E, the stick is NOT in the joiner's inventory", "HAVE=True" not in _probe(host, probe_have), "")
    # A stick that fell on the host (no ground collision yet under a far joiner) is a different failure from a refused E:
    # one run had the server's copy 15 m from where the client saw it. Name it instead of blaming the interaction.
    rest = _probe(host,
        "p = unreal.GameplayStatics.get_player_pawn(world, 1); pl = p.get_actor_location(); "
        "items = sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if str(a.get_item_component().get_editor_property('item_definition_id')) == 'Bark01' and not a.get_editor_property('hidden')], "
        "key=lambda a: (a.get_actor_location() - pl).length()); l = items[0].get_actor_location() if items else None; "
        "out('E_REST d=%.0f dz=%.0f at=%.1f,%.1f,%.1f' % (((l - pl).length(), l.z - pl.z, l.x, l.y, l.z) if l else (-1, 0, 0, 0, 0)))")
    rm = re.search(r"E_REST d=(-?\d+) dz=(-?\d+) at=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)", rest)
    # On a slope it rolls (Oct 20: 5.5 m and 2 m down, twice): the claim is that it did not fall through, which ends it far away or far below.
    if not rep.step("host: the dropped stick rests near the joiner (it did not fall through the ground)", bool(rm) and 0 <= int(rm.group(1)) < 1200 and int(rm.group(2)) > -600,
                    rm.group(0) if rm else rest.strip()[-160:]):
        return
    # On a slope the bark rolls on (3.6 m downhill, 545 cm from the camera: out of the 515 cm reach -- a run that tested nothing). Stand the
    # joiner 170 cm from where it came to rest, on the side it came from; the move is the server's (it owns the pawn) and replicates.
    # The caller puts it back afterwards (E_HOME): the next steps walk the joiner forward, and where the bark stopped rolling there is
    # usually something it hit.
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "l = unreal.Vector(%s, %s, %s); d = p.get_actor_location() - l; d.z = 0.0; d.normalize(); " % rm.group(3, 4, 5) +
                 "p.set_actor_location(l + d * 170.0 + unreal.Vector(0.0, 0.0, 110.0), False, True); out('E_STAND')")
    time.sleep(3)
    # Aim the way a player does: until the crosshair is ON the item. One look-at from the current camera is not enough -- turning swings the
    # spring arm, and the new camera's ray missed by 86-378 cm (Oct 16 2026 probe), so "E failed" measured the harness, not the game.
    # The item nearest where the HOST has the bark (actor names differ between machines; picked-up items linger hidden).
    aim_code = ("pc = unreal.GameplayStatics.get_player_controller(world, 0); pl = unreal.Vector(%s, %s, %s); " % rm.group(3, 4, 5) +
        "items = sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if not a.get_editor_property('hidden')], key=lambda a: (a.get_actor_location() - pl).length()); "
        "t = items[0].get_actor_location() if items else None; cam = pc.player_camera_manager.get_camera_location(); "
        "fwd = pc.player_camera_manager.get_camera_rotation().get_forward_vector(); "
        "miss = ((t - cam) - fwd * (t - cam).dot(fwd)).length() if t else -1; "
        "pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(cam, t)) if t else None; "
        "out('E_AIM item=%s dist=%.0f miss=%.1f' % (items[0].get_name() if items else 'none', (t - cam).length() if t else -1, miss))")
    am = None
    for _ in range(7):
        aim = _probe(client, aim_code)
        am = re.search(r"E_AIM item=(\S+) dist=(-?\d+) miss=(-?[\d.]+)", aim)
        if not am or am.group(1) == "none" or float(am.group(3)) < 4.0:
            break
        time.sleep(0.4)
    if not rep.step("client: the crosshair is on the nearest item on the ground (aim error < 10 cm)",
                    bool(am) and am.group(1) != "none" and float(am.group(3)) < 10.0, am.group(0) if am else aim.strip()[-160:]):
        return
    time.sleep(1.5)  # the new view reaches the server with the next moves
    off, coff = log_size(host), log_size(client)
    console(client, "mo.Interact.Debug 1")   # every targeting pass in the client log, for the diagnosis below
    console(client, "MO.Test.PressKey E")
    got = False
    for _ in range(8):
        time.sleep(1.5)
        if "HAVE=True" in _probe(host, probe_have):
            got = True
            break
    reject = re.search(r"\[MOInteract\] Reject:[^\n]*", read_log_from(host, off))
    diag = ""
    if not got:  # where each side thinks the joiner, its view and the item are
        diag = " | host: " + _probe(host,
            "pc = unreal.GameplayStatics.get_player_controller(world, 1); p = pc.get_controlled_pawn(); v = pc.get_player_view_point(); "
            "items = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem), key=lambda a: (a.get_actor_location() - p.get_actor_location()).length()); "
            "out('pawn=%s view=%s item=%s' % (p.get_actor_location(), v[0] if isinstance(v, tuple) else v, items[0].get_actor_location() if items else None))").strip()[-400:]
        passes = re.findall(r"\[MOInteractor\] (?:target passes|  near the aim point)[^\n]*", read_log_from(client, coff))
        diag += " | client targeting (last press): " + " // ".join(passes[-4:])
        diag += " | client: " + _probe(client,
            "pc = unreal.GameplayStatics.get_player_controller(world, 0); p = pc.get_controlled_pawn(); "
            "out('pawn=%s cam=%s' % (p.get_actor_location(), pc.player_camera_manager.get_camera_location()))").strip()[-300:]
    console(client, "mo.Interact.Debug 0")
    rep.step("client E on a stick it aims at: the host picks it up for the joiner (no 'Reject')", got and not reject,
             ("picked up" if got else (reject.group(0) if reject else "no pickup and no Reject line on the host")) + diag)


GROUND_ITEM_RE = re.compile(r"\[MOTEST\] GROUND_ITEM i=\d+ item=(\S+) mesh=(\S+) at=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) dist=(\d+)")
# The centre of an item's bounds, by its place (at=): a case aims there, not at the origin (a small stone's origin is at the ground, and the line
# to it grazed the terrain first -- N-187, Oct 22). Filled by _ground_items_both.
GROUND_ITEM_CENTRE_RE = re.compile(r"\[MOTEST\] GROUND_ITEM i=\d+ item=\S+ mesh=\S+ at=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) dist=\d+ centre=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)")
_GROUND_CENTRES = {}
INSTANCE_AT_RE = r"\[MOTEST\] INSTANCE_AT present=(\d)[^\n]*"
SWEEPABLE_RE = r"\[MOTEST\] SWEEPABLE n=(\d+)[^\n]*"


def _motest(name, cmd, regex, timeout=12):
    """Run a [MOTEST] verb in an instance; the match of `regex` in what it logged, or None."""
    off = log_size(name)
    console(name, cmd)
    return wait_log(name, regex, since=off, timeout=timeout)


def _worlds_built(rep, host, client, timeout=300):
    """Both worlds finished building (UMOWorldReadinessSubsystem: 'READY after N s'). PCG instances -- ground items, resource nodes, grass -- exist only
    after it: the joiner builds its world behind the loading overlay for a minute after it joins."""
    for name in (host, client):
        # The READY of the GAME map: the menu world (LoadingLevel) logs its own READY a minute before the joiner travels.
        m = wait_log(name, r"Load map complete /Game/(?!Penumbra/Maps/LoadingLevel)\S+[\s\S]*?(READY after [\d.]+s[^\r\n]*)", since=0, timeout=timeout, step=2.0)
        if not rep.step(f"{name}: its world is built (terrain and vegetation)", bool(m), m.group(1) if m else f"no READY line on the game map within {timeout} s"):
            return False
    # READY can be the readiness cap (60 s) with PCG still busy: a slow joiner fills in the small things (sticks, stones) for minutes after. Wait for them.
    # A joiner can also spawn on bare ground (sand, rock): after two minutes with none, stand it by the host's pawn (the host's spawn had items) and wait again.
    deadline, n, moved = time.time() + timeout, 0, False
    while time.time() < deadline:
        m = _motest(client, "MO.Test.GroundItems 3000 1", r"\[MOTEST\] GROUND_ITEMS n=(\d+)")
        n = int(m.group(1)) if m else 0
        if n >= 5:
            break
        if not moved and time.time() > deadline - timeout + 120:
            moved = True
            _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); j = unreal.GameplayStatics.get_player_pawn(world, 1); "
                         "j.set_actor_location(h.get_actor_location() + unreal.Vector(300.0, 0.0, 150.0), False, True); out('JOINER_TO_HOST')")
            deadline = max(deadline, time.time() + 180)
        time.sleep(10)
    detail = f"{n} within 30 m" + (" (moved by the host's pawn)" if moved else "")
    if n < 5:
        # N-031 (a world whose PCG never generates): keep the evidence -- the bug report's WorldBuild fields (components generated vs generating).
        off = log_size(client)
        console(client, "MO.BugReport.Preview")
        wait_log(client, r"preview \| WorldBuild\.NearInstances", since=off, timeout=15)
        wb = re.findall(r"preview \| WorldBuild\.(PcgComponents|PcgExecuting|NearInstances|LastReady): ([^\r\n]*)", read_log_from(client, off))
        detail += " | " + "; ".join(f"{k}: {v}" for k, v in wb)
        # Which components never finished (Oct 20: 16 of 17 "generating" for 5 min, nothing placed): owner, graph, trigger, by graph.
        stuck = _probe(client, "import collections; cs = [c for c in unreal.ObjectIterator(unreal.PCGComponent) if c.get_owner() and c.get_world() == world]; "
                               "bad = [c for c in cs if not c.get_editor_property('generated')]; "
                               "out('PCGSTUCK %d of %d: %s' % (len(bad), len(cs), dict(collections.Counter('%s/%s/%s' % (c.get_owner().get_class().get_name(), "
                               "c.get_graph().get_name() if c.get_graph() else '?', c.get_editor_property('generation_trigger')) for c in bad))))")
        sm = re.search(r"PCGSTUCK [^\r\n]*", stuck)
        detail += " | " + (sm.group(0) if sm else "PCGSTUCK probe: " + stuck.strip()[-160:])
    return rep.step(f"{client}: its PCG has generated ground items around its pawn", n >= 5, detail)


def _ground_items_both(rep, host, client, radius, lines, want=None, keep=None):
    """PCG ground items (MOItem_ tags) the CLIENT lists near its pawn, each confirmed on the HOST at the same place: [(item, mesh, x, y, z, dist)].
    Instances are named differently on each machine, so they are matched by mesh + place (MOInstanceRef.h). Each confirmation is a round trip
    to the host: `keep(dist)` drops a listed item before it is confirmed, and the walk stops after `want` confirmed ones (_trust_instance_far
    confirmed all 40 of its list to use the first far one: 107 s, the loop deep dive Oct 22)."""
    off = log_size(client)
    console(client, f"MO.Test.GroundItems {radius} {lines}")
    wait_log(client, r"\[MOTEST\] GROUND_ITEMS n=\d+", since=off, timeout=15)
    both = []
    text = read_log_from(client, off)
    for x, y, z, cx, cy, cz in GROUND_ITEM_CENTRE_RE.findall(text):
        _GROUND_CENTRES[(x, y, z)] = (cx, cy, cz)
    for item, mesh, x, y, z, d in GROUND_ITEM_RE.findall(text):
        if keep and not keep(int(d)):
            continue
        m = _motest(host, f"MO.Test.InstanceAt {x} {y} {z} {mesh}", INSTANCE_AT_RE)
        if m and m.group(1) == "1":
            both.append((item, mesh, x, y, z, int(d)))
            if want and len(both) >= want:
                break
    return both


def _joiner_remove(host, item, n):
    """Take back what a case gave the joiner (inventories have a capacity: a full pack makes the LATER cases' gives fail)."""
    if n > 0:
        _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                     f"out('TOOK=%s' % inv.remove_item_by_definition_id('{item}', {int(n)}))")


TEST_GIFTS = ("Stick01", "Stick02", "Stone01", "Bark01", "RiverCobble01", "PlantFiber01", "Berries01", "FlintNodule01", "Dirt01")


def _joiner_make_room(host):
    """Empty the joiner's pack of what earlier cases gave it (inventories have a slot capacity since the audit, 7f73356b: one no-stack give per slot,
    and a full pack fails the later cases' gives for a reason that is not theirs)."""
    ids = "[" + ", ".join(f"'{i}'" for i in TEST_GIFTS) + "]"
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                 f"n = [inv.remove_item_by_definition_id(i, inv.get_item_count_by_definition_id(i)) for i in {ids} if inv.get_item_count_by_definition_id(i) > 0]; "
                 "out('ROOM %d entries left of %d slots' % (inv.get_entry_count(), inv.get_slot_count()))")


def _joiner_item_count(host, item):
    inv = "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
    m = re.search(r"HAVE=(-?\d+)", _probe(host, inv + f"out('HAVE=%d' % inv.get_item_count_by_definition_id('{item}'))"))
    return int(m.group(1)) if m else -1


def _client_e_on_ground_item(rep, host, client, aim_offset_cm=0.0, pick=0):
    """CLIENT presses E on a PCG ground item (a stick, stone or flint the biome spawner scattered: an instanced-mesh instance tagged MOItem_<ItemId>),
    through the real input path. Wes's Steam test, Oct 18 2026: nothing happened -- the RPC carried the PCG component pointer, which the host cannot
    resolve ("Unable to resolve default guid from client: PCGPartitionActor_2147442263": partition actors are named differently on every machine).
    _client_press_e cannot catch it: its bark is a replicated actor. Proof on the HOST: the joiner's inventory holds one more. Controls: the host has
    the same instance at the same place before E, and the count before E."""
    if not _worlds_built(rep, host, client):
        return
    both = _ground_items_both(rep, host, client, 2500, 8, want=pick + 3)   # this pick and two fallbacks behind it
    chosen = both[min(pick, len(both) - 1)] if both else None
    if not rep.step("CONTROL: a PCG ground item near the joiner exists at the same place on host and client", bool(both),
                    f"{chosen[1]} at {chosen[2]},{chosen[3]},{chosen[4]} ({chosen[0]}; #{both.index(chosen)} of {len(both)})" if both else "none found on both"):
        return
    for attempt, cand in enumerate(both[min(pick, len(both) - 1):][:3]):
        blocked = _ground_item_attempt(rep, host, client, cand, aim_offset_cm, last=attempt == 2 or cand is both[-1])
        if not blocked:
            return
        print(f"[nettest] ground item {cand[0]} ({cand[1]}): the joiner's line hits {blocked}, not the item: the next candidate", flush=True)
    rep.step("CONTROL: a ground item the joiner's line reaches (not a plant in front of it)", False, "every candidate tried was behind something")


def _ground_item_attempt(rep, host, client, chosen, aim_offset_cm, last):
    """One candidate of _client_e_on_ground_item. Returns the mesh the line hit instead of the item (the candidate is skipped), or None
    when the case ran on it."""
    item, mesh, x, y, z, _d = chosen
    before = _joiner_item_count(host, item)
    # Stand the joiner 150 cm from the item (the server owns its pawn; the move replicates), then turn the client's camera onto it until the crosshair is on it.
    # The host's own pawn steps 25 m aside first: standing between the joiner and the item it took the joiner's line (Oct 20, Stone01: the
    # client's trace hit the host's body).
    _probe(host, "import builtins; h = unreal.GameplayStatics.get_player_pawn(world, 0); builtins.MO_G_HOSTHOME = h.get_actor_location(); "
                 f"h.set_actor_location(unreal.Vector({x}, {y}, {z}) + unreal.Vector(0.0, 2500.0, 300.0), False, True); out('G_HOSTAWAY')")
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); builtins.MO_G_HOME = p.get_actor_location(); "
                 f"l = unreal.Vector({x}, {y}, {z}); d = p.get_actor_location() - l; d.z = 0.0; d.normalize(); "
                 "p.set_actor_location(l + d * 150.0 + unreal.Vector(0.0, 0.0, 110.0), False, True); out('G_STAND')")
    time.sleep(3)
    cx, cy, cz = _GROUND_CENTRES.get((x, y, z), (x, y, z))
    aim_code = (f"pc = unreal.GameplayStatics.get_player_controller(world, 0); t = unreal.Vector({float(cx) + aim_offset_cm}, {cy}, {cz}); "
                "cam = pc.player_camera_manager.get_camera_location(); fwd = pc.player_camera_manager.get_camera_rotation().get_forward_vector(); "
                "miss = ((t - cam) - fwd * (t - cam).dot(fwd)).length(); pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(cam, t)); "
                "out('G_AIM dist=%.0f miss=%.1f' % ((t - cam).length(), miss))")
    am = None
    for _ in range(7):
        am = re.search(r"G_AIM dist=(-?\d+) miss=(-?[\d.]+)", _probe(client, aim_code))
        if not am or float(am.group(2)) < 4.0:
            break
        time.sleep(0.4)
    # What the joiner's line is on before E (the targeting line names the instance's mesh since Oct 22): a plant in front of the item is the
    # world's content, not a failure of the game -- the next candidate is tried (the caller), unless this is the last one.
    if not aim_offset_cm and not last:
        time.sleep(1.5)
        poff = log_size(client)
        console(client, "mo.Interact.Debug 1")
        time.sleep(0.8)
        console(client, "mo.Interact.Debug 0")
        on = re.findall(r"\[MOInteractor\] target \S+ via [^:\r\n]*: (\S+) #\d+", read_log_from(client, poff))
        if on and mesh not in on[-1]:
            _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_G_HOME', None); "
                         "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); "
                         "q = unreal.GameplayStatics.get_player_pawn(world, 0); hh = getattr(builtins, 'MO_G_HOSTHOME', None); "
                         "hh and q.set_actor_location(hh + unreal.Vector(0.0, 0.0, 30.0), False, True); out('G_HOME')")
            time.sleep(2)
            return on[-1]
    # The interaction trace sweeps 30 cm and assists 40 cm around the aim: 15 cm of aim error is on the item.
    if rep.step(f"client: the crosshair is {f'{aim_offset_cm:.0f} cm beside' if aim_offset_cm else 'on'} the ground item (aim error < 15 cm)", bool(am) and float(am.group(2)) < 15.0, am.group(0) if am else "no G_AIM line"):
        time.sleep(1.5)  # the new view reaches the server with the next moves
        hoff, coff = log_size(host), log_size(client)
        console(client, "mo.Interact.Debug 1")   # the client's targeting passes, for the diagnosis below
        console(client, "MO.Test.PressKey E")
        got = False
        for _ in range(8):
            time.sleep(1.5)
            if _joiner_item_count(host, item) > before:
                got = True
                break
        console(client, "mo.Interact.Debug 0")
        passes = re.findall(r"\[MOInteractor\] (?:target |  near the aim point|no target)[^\n]*", read_log_from(client, coff))
        hlog = read_log_from(host, hoff)
        unresolved = re.search(r"Unable to resolve default guid from client[^\n]*", hlog)
        served = re.search(r"\[MOInteractor\] (?:instance interact|ServerRequestInstanceInteract)[^\n]*", hlog)
        rep.step(f"client E on a PCG ground item ({item}): the host puts it in the joiner's inventory", got,
                 f"{item} {before} -> {_joiner_item_count(host, item)} | host: " + (served.group(0) if served else "no [MOInteractor] line")
                 + ("" if got else " | client targeting: " + " // ".join(p[:220] for p in passes[-8:])))
        rep.step("... and the host resolved every reference the client sent (no 'Unable to resolve')", unresolved is None,
                 unresolved.group(0)[:200] if unresolved else "none")
        if got:
            time.sleep(2)
            m = _motest(client, f"MO.Test.InstanceAt {x} {y} {z} {mesh}", INSTANCE_AT_RE)
            rep.step("... and the item is gone from the CLIENT's world too (the removal record)", bool(m) and m.group(1) == "0", m.group(0) if m else "no INSTANCE_AT line")
            _joiner_remove(host, item, _joiner_item_count(host, item) - before)
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_G_HOME', None); "
                 "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); "
                 "q = unreal.GameplayStatics.get_player_pawn(world, 0); hh = getattr(builtins, 'MO_G_HOSTHOME', None); "
                 "hh and q.set_actor_location(hh + unreal.Vector(0.0, 0.0, 30.0), False, True); out('G_HOME')")
    time.sleep(2)


RESOURCE_NODE_RE = re.compile(r"\[MOTEST\] RESOURCE_NODE i=\d+ mesh=(\S+) at=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) dist=(\d+) action=(\S+) yields=(\S+) seconds=([\d.]+)")
HARVEST_OUTCOME_RE = r"\[MOCraftUI\] Harvest '[^']*' finished: produced (\d+) kind\(s\), failed=(\d) depleted=(\d) outOfSeason=(\d)"


def _client_harvest(rep, host, client):
    """CLIENT harvests a resource node (strip bark, gather twigs ...): the KeepOnHarvest menu's request, AMOPlayerController::ServerBeginHarvest.
    Wes's Steam test, Oct 18 2026: every harvest a joiner made said "Inventory Full!" -- it ran in the joiner's UI on the joiner's own copy of the
    bush, and the joiner's inventory is not the authority. Proof on the HOST: a yield of the action arrives in the joiner's inventory there; and the
    joiner's UI gets the outcome. Controls: the node is at the same place on the host; the counts before. A node whose yields are out of season or
    used up (a loose-stone node can roll none), or one the server refuses as out of sight from the joiner's view (Oct 24: a rock between),
    proves nothing either way, so the next one is tried."""
    if not _worlds_built(rep, host, client):
        return
    off = log_size(client)
    console(client, "MO.Test.ResourceNodes 2500 12")
    wait_log(client, r"\[MOTEST\] RESOURCE_NODES n=\d+", since=off, timeout=15)
    nodes = []
    for mesh, x, y, z, d, action, yields, secs in RESOURCE_NODE_RE.findall(read_log_from(client, off)):
        m = _motest(host, f"MO.Test.InstanceAt {x} {y} {z} {mesh}", INSTANCE_AT_RE)
        if m and m.group(1) == "1":
            nodes.append((mesh, x, y, z, int(d), action, yields.split(","), float(secs)))
    if not rep.step("CONTROL: a resource node the joiner can work exists at the same place on host and client", bool(nodes),
                    f"{len(nodes)} node(s); first {nodes[0][0]} {nodes[0][5]}" if nodes else "none on both"):
        return
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); builtins.MO_H_HOME = p.get_actor_location(); out('H_HOME_SAVED')")
    tried = []
    for mesh, x, y, z, d, action, yields, secs in nodes[:4]:
        before = {i: _joiner_item_count(host, i) for i in yields}
        # Stand the joiner a metre from the node (the server owns its pawn; the move replicates).
        _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                     f"l = unreal.Vector({x}, {y}, {z}); d = p.get_actor_location() - l; d.z = 0.0; d.normalize(); "
                     "p.set_actor_location(l + d * 100.0 + unreal.Vector(0.0, 0.0, 120.0), False, True); out('H_STAND')")
        time.sleep(3)
        hoff, coff = log_size(host), log_size(client)
        console(client, f"MO.Test.Harvest {x} {y} {z} {mesh} {action}")
        deadline = time.time() + secs + 12
        got, after, outcome = False, dict(before), None
        while time.time() < deadline and not outcome:
            time.sleep(1.5)
            after = {i: _joiner_item_count(host, i) for i in yields}
            got = any(after[i] > before[i] for i in yields)
            outcome = re.search(HARVEST_OUTCOME_RE, read_log_from(client, coff))
        if outcome and not got:
            # The outcome can land between this pass's host count and its outcome check: count once more (Oct 20: "Nuts01 x2" picked up, read 0 -> 0)
            time.sleep(0.5)
            after = {i: _joiner_item_count(host, i) for i in yields}
            got = any(after[i] > before[i] for i in yields)
        hlog = read_log_from(host, hoff)
        began = re.search(r"\[MOHarvest\] [^\n]*server-timed[^\n]*", hlog) or re.search(r"\[MOHarvest\] [^\n]*refused[^\n]*", hlog)
        tried.append(f"{action} on {mesh}: {before} -> {after}; host: {began.group(0)[-140:] if began else 'no [MOHarvest] line'}; "
                     f"client: {outcome.group(0) if outcome else 'no outcome'}")
        if outcome and outcome.group(1) == "0" and (outcome.group(3) == "1" or outcome.group(4) == "1"):
            continue  # used up / out of season: inconclusive, try the next node
        if began and "out of sight" in began.group(0):
            continue  # the server's line from the joiner's view to the node is blocked (a rock or a plant between): the world's content, the next node
        rep.step(f"client harvest ({action} on {mesh}): a yield arrives in the joiner's inventory on the HOST", got, tried[-1])
        for i in yields:
            _joiner_remove(host, i, after[i] - before[i])
        rep.step("... and the joiner's UI got the outcome, without 'Inventory Full'", bool(outcome) and outcome.group(2) == "0",
                 outcome.group(0) if outcome else "no outcome line on the client")
        break
    else:
        rep.step("client harvest: a node with something to give in season", False, " || ".join(tried))
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_H_HOME', None); "
                 "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('H_HOME')")
    time.sleep(2)


def _client_harvest_full_pack(rep, host, client):
    """CLIENT harvests with a FULL pack (BR23-15, Wes Oct 23 2026: "harvesting with full inventory -- make it so the items drop to the ground"):
    before, the server refused the whole harvest ("Inventory Full!") and the work counted for nothing. Now what does not fit lies on the ground at
    the harvester's feet. Proof on the HOST: the "laid on the ground" line and a dropped item of that kind within 3 m of the joiner, with the pack
    unchanged. Controls: the pack really is full (the host can add no more of the yield); the filler is taken back after."""
    if not _worlds_built(rep, host, client):
        return
    _joiner_make_room(host)
    off = log_size(client)
    console(client, "MO.Test.ResourceNodes 2500 12")
    wait_log(client, r"\[MOTEST\] RESOURCE_NODES n=\d+", since=off, timeout=15)
    nodes = []
    for mesh, x, y, z, d, action, yields, secs in RESOURCE_NODE_RE.findall(read_log_from(client, off)):
        m = _motest(host, f"MO.Test.InstanceAt {x} {y} {z} {mesh}", INSTANCE_AT_RE)
        if m and m.group(1) == "1":
            nodes.append((mesh, x, y, z, action, yields.split(","), float(secs)))
    if not rep.step("CONTROL: a resource node the joiner can work exists on both machines", bool(nodes), f"{nodes[0][0]} {nodes[0][4]}" if nodes else "none"):
        return
    # Up to 4 nodes: one used up (a loose-stone node can roll none) or out of the joiner's sight proves nothing, as in _client_harvest (Oct 24:
    # the first node was a Beach Boulder with no loose stones, the only one tried, and the case failed for the world's content).
    cands = nodes[:4]
    kinds = sorted({i for c in cands for i in c[5]})
    filler = next(i for i in ("Dirt01", "Bark01", "Stick02") if i not in kinds)   # not a yield: a yield would stack onto it and fit
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); builtins.MO_HF_HOME = p.get_actor_location(); out('HF_HOME_SAVED')")
    # Fill every free slot with a one-off filler (no stacking), then check the yields' kinds no longer fit.
    fill = ("import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
            f"n = sum(1 for _ in range(max(0, inv.get_slot_count() - inv.get_entry_count()) + 2) if inv.add_item_by_guid_no_stack(unreal.GuidLibrary.new_guid(), '{filler}', 1)); "
            f"builtins.MO_HF_FILLED = n; out('FILLED %d fits=%s' % (n, ','.join('%s:%d' % (i, inv.can_add_item_by_definition_id(i, 1)) for i in {kinds!r})))")
    filled = _probe(host, fill)
    m = re.search(r"FILLED (\d+) fits=(\S*)", filled)
    full = bool(m) and all(part.endswith(":0") for part in m.group(2).split(","))
    if not rep.step("CONTROL: the joiner's pack is full (on the host: no room for the yield's kinds)", full, (m.group(0) if m else filled[-160:])):
        _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                     f"out('TOOK=%s' % inv.remove_item_by_definition_id('{filler}', getattr(builtins, 'MO_HF_FILLED', 0)))")
        return
    tried = []
    for mesh, x, y, z, action, yields, secs in cands:
        _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                     f"l = unreal.Vector({x}, {y}, {z}); d = p.get_actor_location() - l; d.z = 0.0; d.normalize(); "
                     "p.set_actor_location(l + d * 100.0 + unreal.Vector(0.0, 0.0, 120.0), False, True); out('HF_STAND')")
        before = {i: _joiner_item_count(host, i) for i in yields}
        time.sleep(2)
        hoff, coff = log_size(host), log_size(client)
        console(client, f"MO.Test.Harvest {x} {y} {z} {mesh} {action}")
        laid = wait_log(host, r"\[MOHarvest\] '(\w+)' x\d+: no room in \S+ pack \(reason \d+\) -- (\d+) laid on the ground", since=hoff, timeout=secs + 15)
        outcome = wait_log(client, HARVEST_OUTCOME_RE + r" dropped=(\d)", since=coff, timeout=10)
        refused = re.search(r"\[MOHarvest\] [^\n]*refused[^\n]*", read_log_from(host, hoff))
        tried.append(f"{action} on {mesh}: " + (refused.group(0)[-160:] if refused else outcome.group(0) if outcome else "no outcome"))
        if not laid and ((outcome and outcome.group(1) == "0" and (outcome.group(3) == "1" or outcome.group(4) == "1"))
                         or (refused and "out of sight" in refused.group(0))):
            continue  # inconclusive: the next node
        near = ""
        if laid:
            near = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); pl = p.get_actor_location(); "
                                f"out('NEAR %d' % len([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) "
                                f"if (a.get_actor_location() - pl).length() < 300 and str(a.get_component_by_class(unreal.MOItemComponent).get_editor_property('item_definition_id')) == '{laid.group(1)}']))")
        after = {i: _joiner_item_count(host, i) for i in yields}
        rep.step(f"client harvest with a full pack ({action} on {mesh}): the yield lies on the ground by the joiner (host)",
                 bool(laid) and laid.group(2) != "0" and bool(re.search(r"NEAR [1-9]", near)) and after == before,
                 (laid.group(0) if laid else "no 'laid on the ground' line on the host") + f" | {(re.search(r'NEAR \d+', near) or re.search(r'$', near)).group(0)} within 3 m | pack {before} -> {after}")
        rep.step("... and the joiner is told it lies at its feet (not 'Inventory Full')", bool(outcome) and outcome.group(5) == "1" and outcome.group(2) == "0",
                 outcome.group(0) if outcome else "no outcome line on the client")
        break
    else:
        rep.step("client harvest with a full pack: a node with something to give in season, in the joiner's sight", False, " || ".join(tried))
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                 f"took = inv.remove_item_by_definition_id('{filler}', getattr(builtins, 'MO_HF_FILLED', 0)); pl = p.get_actor_location(); "
                 "[a.destroy_actor() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if (a.get_actor_location() - pl).length() < 500]; "
                 "h = getattr(builtins, 'MO_HF_HOME', None); h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('HF_TIDY %s' % took)")
    time.sleep(2)


def _client_inspect(rep, host, client):
    """CLIENT inspects a carried item (the inventory's Inspect). Wes's Steam test, Oct 18 2026: "client leaving game resets all skills/knowledge on
    pawn" -- a joiner's inspection ran in the joiner's UI on the joiner's own copy of its knowledge and skills, which the host never had and never
    saved. Proof on the HOST: the joiner's pawn there has studied the item. Controls: it had not before; the host gave the item (it is carried)."""
    know = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); k = p.get_component_by_class(unreal.MOKnowledgeComponent); "
            "r = k.get_all_inspected_items(); r = r if isinstance(r, (list, tuple)) or hasattr(r, '__iter__') else []; out('STUDIED=' + ','.join(str(x) for x in r))")
    m = re.search(r"STUDIED=(\S*)", _probe(host, know))
    studied = set(filter(None, (m.group(1) if m else "").split(",")))
    item = next((i for i in ("Bark01", "Stick02", "FlintNodule01", "Stone01", "RiverCobble01", "Stick01") if i not in studied), None)
    if not rep.step("CONTROL: an item the joiner's pawn has never studied (on the host)", bool(item), f"{item}; studied so far: {sorted(studied)}"):
        return
    gave = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                        f"out('GAVE=%s' % inv.add_item_by_guid_no_stack(unreal.GuidLibrary.new_guid(), '{item}', 1))")
    if not rep.step(f"host: gave the joiner a {item}", "GAVE=True" in gave, gave.strip()[-120:]):
        return
    time.sleep(3)  # the inventory replicates to the joiner
    hoff, coff = log_size(host), log_size(client)
    console(client, f"MO.Test.Inspect {item}")
    done = False
    for _ in range(12):
        time.sleep(1.0)
        m = re.search(r"STUDIED=(\S*)", _probe(host, know))
        if m and item in m.group(1).split(","):
            done = True
            break
    hl = re.search(r"\[MOInspection\] [^\n]*(?:server-timed|refused)[^\n]*", read_log_from(host, hoff))
    cl = re.search(r"\[MOCharUI\] Inspection of '[^']*' finished[^\n]*", read_log_from(client, coff))
    rep.step(f"client inspects {item}: the joiner's pawn ON THE HOST has studied it (the server's copy, the one saved)", done,
             f"host: {hl.group(0) if hl else 'no [MOInspection] line'} | client: {cl.group(0) if cl else 'no result line'}")
    rep.step("... and the joiner's UI got the result", bool(cl) and "Success=true" in cl.group(0), cl.group(0) if cl else "no result line on the client")
    _joiner_remove(host, item, 1)


def _client_survivor_command(rep, host, client):
    """CLIENT orders a recruited survivor (the survivor context menu's Follow Me, the task menu's Gather Wood). Wes's Steam test, Oct 18 2026: "cant
    give comand" -- the menus called the survivor's AI controller and job queue directly, which exist / accept changes only on the server. Proof on
    the HOST: the order is carried out there (its log), the survivor's job queue there holds the task. Controls: a recruited survivor by the joiner
    (the host spawns and recruits one), and its queue before."""
    off = log_size(host)
    console(host, "MO.Colony.SpawnSurvivor 300")
    m = wait_log(host, r"\[MOQUERY\] COLONY SpawnSurvivor (\S+)", since=off, timeout=15)
    if not rep.step("host: spawned a survivor", bool(m) and m.group(1) != "FAILED", m.group(0) if m else "no SpawnSurvivor line"):
        return
    name = m.group(1)
    console(host, f"MO.Colony.Recruit {name}")
    r = wait_log(host, r"\[MOQUERY\] COLONY Recruit \S+ possessable=(\d)", since=off, timeout=15)
    if not rep.step("CONTROL: the survivor is recruited (the menus offer orders only to recruited survivors)", bool(r) and r.group(1) == "1", r.group(0) if r else "no Recruit line"):
        return
    sv = f"sv = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{name}'][0]; "
    _probe(host, sv + "j = unreal.GameplayStatics.get_player_pawn(world, 1); sv.set_actor_location(j.get_actor_location() + unreal.Vector(250.0, 0.0, 60.0), False, True); out('SV_MOVED')")
    time.sleep(4)  # the move replicates to the joiner
    jobs = lambda: int((re.search(r"JOBS=(\d+)", _probe(host, sv + "q = sv.get_component_by_class(unreal.MOSurvivorJobQueueComponent); out('JOBS=%d' % len(q.get_all_jobs()))")) or re.search("(-1)", "-1")).group(1))
    hoff = log_size(host)
    console(client, "MO.Test.SurvivorCommand FollowMe")
    follow = wait_log(host, r"\[MOSurvivorCommand\] [^\n]*order 0 [^\n]*", since=hoff, timeout=10)
    rep.step("client orders Follow Me: the HOST's survivor AI takes it", bool(follow) and "done" in follow.group(0),
             follow.group(0) if follow else "no [MOSurvivorCommand] line on the host")
    before = jobs()
    hoff = log_size(host)
    console(client, "MO.Test.SurvivorCommand Job GatherWood")
    job = wait_log(host, r"\[MOSurvivorCommand\] [^\n]*order 4 [^\n]*", since=hoff, timeout=10)
    time.sleep(1)
    after = jobs()
    rep.step("client orders a task (Gather Wood): it is in the survivor's job queue ON THE HOST", after > before and bool(job) and "done" in job.group(0),
             f"jobs {before} -> {after} | " + (job.group(0) if job else "no [MOSurvivorCommand] line on the host"))


def _host_possession_click(rep, host, client):
    """The HOST selects a recruited survivor in the possession window with a REAL mouse click (Tools/win_input.ps1). Wes, Oct 18 2026: "still can't
    select any pawns from the possession window" -- the row's blueprint lays a full-row EntryButton over the Select button, and nothing handled it,
    so every real click was swallowed; the earlier fix was proven with MO.Possess.Take (the RPC), never a click. Proof: the host now plays the
    survivor. Controls: Slate's hit test names the button a click at Select lands on; the host's pawn before the click."""
    off = log_size(host)
    console(host, "MO.Colony.SpawnSurvivor 400")
    m = wait_log(host, r"\[MOQUERY\] COLONY SpawnSurvivor (\S+)", since=off, timeout=15)
    if not rep.step("host: spawned a survivor", bool(m) and m.group(1) != "FAILED", m.group(0) if m else "no SpawnSurvivor line"):
        return
    name = m.group(1)
    console(host, f"MO.Colony.Recruit {name}")
    if not rep.step("host: recruited it", bool(wait_log(host, r"\[MOQUERY\] COLONY Recruit \S+ possessable=1", since=off, timeout=15))):
        return
    pawn_now = lambda: (re.search(r"HOSTPAWN (\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('HOSTPAWN %s' % (p.get_name() if p else 'none'))")) or re.search("(none)", "none")).group(1)
    before = pawn_now()
    off = log_size(host)
    console(host, "MO.UI.Open Possession")
    pop = wait_log(host, r"\[MOPossessionMenu\] Populated with (\d+) of your / free living", since=off, timeout=10)
    time.sleep(1.5)  # laid out and painted
    off = log_size(host)
    console(host, "MO.Test.FindWidget PossessButton")
    wait_log(host, r"\[MOQUERY\] FindWidget\('PossessButton'\)", since=off, timeout=10)
    centers = [(int(x), int(y)) for x, y in re.findall(r"WIDGET 'PossessButton' \([^)]*\) text='[^']*' center=\((\d+),(\d+)\)", read_log_from(host, off))]
    if not rep.step("host: the possession window lists its rows", bool(pop) and len(centers) >= 2, f"{pop.group(0) if pop else 'not populated'}; {len(centers)} row button(s)"):
        return
    hit = _motest(host, "MO.Test.HitTest PossessButton", r"\[MOQUERY\] HITTEST 'PossessButton' at[^\n]*")
    rep.step("CONTROL: Slate's hit test says where a click on Select lands", bool(hit), hit.group(0) if hit else "no HITTEST line")
    clicked = None
    for x, y in centers:  # the rows top down: the current survivor, then the free ones; a click on the current one is ignored
        off = log_size(host)
        r = subprocess.run(["powershell", "-NoProfile", "-File", os.path.join(os.path.dirname(os.path.abspath(__file__)), "win_input.ps1"),
                            "-ProcessId", str(read_pid(host)), "-Action", "click", "-X", str(x), "-Y", str(y)], capture_output=True, text=True, timeout=30)
        if "REFUSED" in (r.stdout or ""):
            rep.step("the host window took the foreground for a real click", False, r.stdout.strip()[-160:])
            return
        clicked = wait_log(host, r"\[MOPossessionMenu\] Possess clicked for pawn[^\n]*", since=off, timeout=4)
        if clicked:
            break
    time.sleep(3)
    after = pawn_now()
    rep.step("a REAL click on a free survivor's row in the possession window: the host plays it", bool(clicked) and after != before,
             f"pawn {before} -> {after} | " + (clicked.group(0) if clicked else "no 'Possess clicked' line for any row"))


def _client_e_beside_ground_item(rep, host, client):
    """The same, with the crosshair 20 cm BESIDE the item: the line and the sweep meet the terrain, so only the near-the-aim-point pass can find it --
    the pass that took no instances until Oct 18 2026 (a flint flush with the ground could not be picked up with E). The instance bodies do not answer
    that pass's overlap query, so it looks them up by bounds."""
    _client_e_on_ground_item(rep, host, client, aim_offset_cm=20.0, pick=1)


def _client_drink(rep, host, client):
    """CLIENT drinks from open water with E. Wes's Oct 18 report (BR18-3, "client can't drink from water sources"): the joiner's E went to a PCG
    instance in front of the water first, whose reference the host could not resolve. The joiner works out the nearest bank (MO.Water.GoToShore),
    the HOST moves the joiner's pawn there (the server owns it), the joiner turns to the water (MO.Water.LookAtWater) and presses E. Proof on the
    HOST: "[MOWater] <the joiner's pawn> drank" when the draught ends (DraughtSeconds, 20 s). Control: the client sees the water in reach."""
    if not _worlds_built(rep, host, client):
        return
    who = _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); builtins.MO_D_HOME = p.get_actor_location(); "
                       "out('PAWNNAME=' + p.get_name())")
    pawn = (re.search(r"PAWNNAME=(\S+)", who) or [None, "?"])[1]
    # The nearest fresh bank, else the sea's. A bank whose view of the water is blocked (the crosshair's line meets a rock or the bank itself
    # first: FindWaterAtCrosshair sees no water) is the world's content, not the game: it is named and the sea's bank is tried (Oct 24).
    found, blocked, shore, look, here = "", [], None, None, None
    for kind, metres in (("fresh", 500), ("sea", 2500)):
        shore = _motest(client, f"MO.Water.GoToShore {kind} {metres}",
                        r"\[MOQUERY\] WATER GoToShore:? ?(?:(\w+): water at [^\r\n]*standing at \((-?\d+), (-?\d+), (-?\d+)\)|[^\r\n]*no \w+ water)", timeout=40)
        if not (shore and shore.group(1)):
            continue
        found = found or shore.group(0)[:220]
        x, y, z = shore.group(2), shore.group(3), shore.group(4)
        _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, 1); p.set_actor_location(unreal.Vector({x}, {y}, {float(z) + 120.0}), False, True); out('D_STAND')")
        for _ in range(10):   # the bank's terrain and collision come up on both machines first
            time.sleep(2)
            look = _motest(client, "MO.Water.LookAtWater", r"\[MOQUERY\] WATER LookAtWater: ([^\r\n]*)")
            if look and look.group(1).startswith("water"):
                break
        here = _motest(client, "MO.Water.Here", r"\[MOQUERY\] WATER Here ([^\r\n]*)")
        if look and look.group(1).startswith("water") and here and "(in reach)" in here.group(1):
            break
        hit = re.search(r"VIEWHIT [^\r\n]*", _probe(client,
            "pc = unreal.GameplayStatics.get_player_controller(world, 0); me = pc.get_controlled_pawn(); "
            "cam = pc.player_camera_manager.get_camera_location(); fwd = pc.player_camera_manager.get_camera_rotation().get_forward_vector(); "
            "r = unreal.SystemLibrary.line_trace_single(world, cam, cam + fwd * 1200.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [me], unreal.DrawDebugTrace.NONE, True); "
            "hr = r[1] if isinstance(r, tuple) else r; t = hr.to_tuple() if hr else None; "
            "out('VIEWHIT %s' % (('%s/%s %.0f cm from the camera' % (t[9].get_name() if t[9] else '?', t[10].get_name() if t[10] else '?', t[3])) if (t and t[0]) else 'nothing within 12 m'))"))
        blocked.append(f"{kind} bank at ({x}, {y}): " + (hit.group(0) if hit else "no VIEWHIT answer"))
        print(f"[nettest] drink: the joiner's view of the {kind} water is blocked -- {blocked[-1]}", flush=True)
    if rep.step("CONTROL: open water with a bank near the joiner (fresh within 500 m, else sea within 2.5 km)", bool(found),
                found or (shore.group(0)[:220] if shore else "no GoToShore line")):
        if rep.step("client: on the bank, looking at the water within reach", bool(look and look.group(1).startswith("water") and here and "(in reach)" in here.group(1)),
                    (look.group(0) if look else "no LookAtWater line") + " | " + (here.group(1)[:160] if here else "no Here line")
                    + (" | blocked: " + " // ".join(blocked) if blocked else "")):
            time.sleep(1.5)   # the new view reaches the server with the next moves
            hoff = log_size(host)
            console(client, "MO.Test.PressKey E")
            drank = wait_log(host, rf"\[MOWater\] {re.escape(pawn)} drank [^\r\n]*", since=hoff, timeout=45)
            hlog = read_log_from(host, hoff)
            refused = re.search(r"\[MOWater\] [^\r\n]*drink refused[^\r\n]*", hlog)
            unresolved = re.search(r"Unable to resolve default guid from client[^\r\n]*", hlog)
            rep.step("client E at the water: the host's copy of the joiner drinks", bool(drank),
                     (drank.group(0) if drank else "no drink on the host") + (" | " + refused.group(0) if refused else ""))
            rep.step("... and the host resolved every reference the client sent (no 'Unable to resolve')", unresolved is None,
                     unresolved.group(0)[:200] if unresolved else "none")
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_D_HOME', None); "
                 "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('D_HOME')")
    time.sleep(2)


def _client_glide(rep, host, client):
    """The dev glider (Wes, Oct 18 2026, BR18-17: MO.Dev.Glider) flown by the JOINER. The wish rides in the saved moves (FLAG_Custom_2) and the server
    judges the developer gate, so the host's copy must fly the joiner's flight. The host lifts the joiner 80 m above the generator's ground there; the
    joiner turns the glider on and holds W. Proof on the HOST: the joiner's pawn flies at about trim speed (MO.Dev.GlideSpeed 1100) and sinks about the
    sink rate (MO.Dev.GlideSink 100), and the host corrects the joiner at most a couple of times. Control: the same drop without the glider falls."""
    if not _worlds_built(rep, host, client):
        return
    vel = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); v = p.get_velocity(); l = p.get_actor_location(); "
           "out('GV vx=%.0f vy=%.0f vz=%.0f z=%.0f' % (v.x, v.y, v.z, l.z))")
    home = _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); l = p.get_actor_location(); builtins.MO_GL_HOME = l; "
                        "out('GL_AT x=%.0f y=%.0f' % (l.x, l.y))")
    at = re.search(r"GL_AT x=(-?\d+) y=(-?\d+)", home)
    if not at:
        rep.step("host: the joiner's pawn is there", False, home[-200:])
        return
    x, y = float(at.group(1)), float(at.group(2))
    off = log_size(host)
    console(host, f"MO.WorldGen.Sample {x:.0f} {y:.0f}")
    g = wait_log(host, r"WORLDGEN Sample x=\S+ y=\S+ seed=\S+ z=(-?[\d.]+)", since=off, timeout=5)
    gz = float(g.group(1)) if g else 0.0
    lift = f"p = unreal.GameplayStatics.get_player_pawn(world, 1); p.set_actor_location(unreal.Vector({x:.0f}, {y:.0f}, {gz + 8000.0:.0f}), False, True); out('GL_UP')"

    def flight(glide):
        console(client, f"MO.Dev.Glider {1 if glide else 0}")
        _probe(host, lift)
        time.sleep(0.5)
        hoff = log_size(host)
        console(client, "MO.Footstep.Walk 8")
        time.sleep(4.0)
        a = re.search(r"GV vx=(-?\d+) vy=(-?\d+) vz=(-?\d+) z=(-?\d+)", _probe(host, vel))
        time.sleep(2.5)
        b = re.search(r"GV vx=(-?\d+) vy=(-?\d+) vz=(-?\d+) z=(-?\d+)", _probe(host, vel))
        corr = sum(int(c) for c in re.findall(r"\[MONet\] role=host [^\r\n]*? corr=(\d+)", read_log_from(host, hoff)))
        return a, b, corr

    on = _motest(client, "MO.Dev.Glider 1", r"\[MODev\] glider ([^\r\n]*)")
    if not rep.step("client: MO.Dev.Glider turns the glider on (the host is in developer mode)", bool(on and on.group(1).startswith("ON")),
                    on.group(0) if on else "no [MODev] glider line"):
        return
    a, b, corr = flight(True)
    if rep.step("host: samples of the joiner's flight", bool(a and b), f"{a.group(0) if a else None} / {b.group(0) if b else None}"):
        speed_held = math.hypot(float(a.group(1)), float(a.group(2)))   # W held (MO.Footstep.Walk 8)
        speed = math.hypot(float(b.group(1)), float(b.group(2)))        # ~2.5 s later, after the walk let go
        sink = -float(b.group(3))
        rep.step("host: the joiner's pawn flies at about trim speed (700-1400 cm/s), W held and hands off",
                 700.0 <= speed_held <= 1400.0 and 700.0 <= speed <= 1400.0, f"{speed_held:.0f} then {speed:.0f} cm/s, sink {sink:.0f} cm/s")
        rep.step("... sinking about the glider's sink rate (40-250 cm/s), not falling", 40.0 <= sink <= 250.0, f"vz {-sink:.0f} cm/s, z {a.group(4)} -> {b.group(4)} (ground {gz:.0f})")
        rep.step("... and the host hardly corrected the joiner (<= 3 corrections in the flight)", corr <= 3, f"{corr} correction(s)")
    # Control: the same drop with the glider off falls.
    ca, cb, _c = flight(False)
    if cb:
        rep.step("CONTROL: the same drop without the glider falls (descent > 600 cm/s)", -float(cb.group(3)) > 600.0 or float(cb.group(4)) < gz + 3000.0, cb.group(0))
    console(client, "MO.Dev.Glider 0")
    time.sleep(6)
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_GL_HOME', None); "
                 "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('GL_HOME')")
    time.sleep(2)


def _client_footstep_surface(rep, host, client):
    """The JOINER's own footsteps follow the ground (Wes's Oct 18 report BR18-8: "client footsteps never change in their audio but i hear their
    different audio"). The host finds two spots of different biomes near the joiner (MO.WorldGen.Sample), stands the joiner's pawn on each, and the
    joiner says what its ground sounds like (MO.Footstep.Probe on the JOINER's machine: set + reason). Expected: a biome reason on the joiner (not
    "no world seed: fallback") and a different set on the two grounds. Control: the host's own probe at its pawn names a biome too."""
    if not _worlds_built(rep, host, client):
        return
    probe_re = r"\[MOQUERY\] FOOTSTEP Probe pos=[^\r\n]*? set=(\S+) \(([^)]*)\)"
    hp = _motest(host, "MO.Footstep.Probe", probe_re)
    rep.step("CONTROL: the host's own ground has a footstep set from the generator", bool(hp and hp.group(2).startswith(("biome", "structure", "underground", "actor"))),
             hp.group(0)[:200] if hp else "no probe line on the host")
    at = re.search(r"GL_AT x=(-?\d+) y=(-?\d+)", _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); l = p.get_actor_location(); "
                                                              "builtins.MO_F_HOME = l; out('GL_AT x=%.0f y=%.0f' % (l.x, l.y))"))
    if not at:
        rep.step("host: the joiner's pawn is there", False, "no position")
        return
    jx, jy = float(at.group(1)), float(at.group(2))
    spots, seen = [], set()
    for r in (0.0, 30000.0, 60000.0, 120000.0, 200000.0, 300000.0):
        for k in range(8 if r else 1):
            x, y = jx + r * math.cos(k * math.pi / 4.0), jy + r * math.sin(k * math.pi / 4.0)
            off = log_size(host)
            console(host, f"MO.WorldGen.Sample {x:.0f} {y:.0f}")
            m = wait_log(host, r"WORLDGEN Sample x=\S+ y=\S+ seed=\S+ z=(-?[\d.]+) [^\r\n]*biome=(\w+)", since=off, timeout=5)
            if m and float(m.group(1)) > 300.0 and m.group(2) not in seen:   # dry land, a biome not seen yet
                seen.add(m.group(2))
                spots.append((x, y, float(m.group(1)), m.group(2)))
            if len(spots) >= 4:
                break
        if len(spots) >= 4:
            break
    if not rep.step("CONTROL: two dry spots of different biomes near the joiner (generator)", len(spots) >= 2, str([(round(a), round(b), c) for a, b, _z, c in spots])):
        return
    # Up to four spots, stopping once two sound different: two biomes may share a footstep set (two woodlands: LeafCrunch, on seed 670711536), and
    # a probe only counts once the joiner's pawn stands at the spot (one taken before the teleport landed read the previous spot's biome).
    heard = []
    for x, y, z, biome in spots[:4]:
        _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, 1); p.set_actor_location(unreal.Vector({x:.0f}, {y:.0f}, {z + 200.0:.0f}), False, True); out('F_STAND')")
        m = None
        for _ in range(8):   # the ground there comes up on the joiner and the pawn lands
            time.sleep(3)
            m = _motest(client, "MO.Footstep.Probe", probe_re)
            at = re.search(r"pos=\((-?[\d.]+), (-?[\d.]+),", m.group(0)) if m else None
            if m and "grounded=1" in m.group(0) and at and math.hypot(float(at.group(1)) - x, float(at.group(2)) - y) < 500.0:
                break
            m = None
        heard.append((biome, m.group(1) if m else None, m.group(2) if m else "no probe line at the spot"))
        if len({s_ for _b, s_, w in heard if s_ and w.startswith("biome")}) >= 2:   # a spot on a log / rock ("structure") says nothing about biomes
            break
    rep.step("client: its own ground's footstep set comes from the world on every spot -- the biome, or the log / rock it stands on (not the fallback)",
             all(w.startswith(("biome", "structure", "underground", "actor")) for _b, _s, w in heard), str(heard))   # the host CONTROL's reasons
    rep.step("client: two biomes sound different on the joiner's own machine", len({s_ for _b, s_, w in heard if s_ and w.startswith("biome")}) >= 2, str(heard))
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_F_HOME', None); "
                 "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('F_HOME')")
    time.sleep(3)


def _trust_instance_far(rep, host, client):
    """The instance RPC names a place (mesh + location), so a modified client could name any place: the server must hold it to interact reach. A
    joiner asking for a ground item 15+ m away is refused and gets nothing. Control: the host has that item there (the request was not refused for
    naming nothing)."""
    if not _worlds_built(rep, host, client):
        return
    far = _ground_items_both(rep, host, client, 6000, 40, want=1, keep=lambda d: d > 1500)
    if not rep.step("CONTROL: a PCG ground item 15-60 m from the joiner exists on both machines", bool(far), f"{far[0][1]} {far[0][5]} cm away" if far else "none"):
        return
    item, mesh, x, y, z, d = far[0]
    before = _joiner_item_count(host, item)
    hoff = log_size(host)
    console(client, f"MO.Test.InstanceInteract {x} {y} {z} {mesh}")
    refused = wait_log(host, r"\[MOInteractor\] ServerRequestInstanceInteract rejected: instance \d+cm away[^\n]*", since=hoff, timeout=10)
    time.sleep(1)
    after = _joiner_item_count(host, item)
    rep.step(f"trust: E on a ground item {d} cm away is REFUSED by the server", bool(refused) and after == before,
             (refused.group(0) if refused else "no 'rejected: instance ... away' line") + f" | {item} {before} -> {after}")


def _trust_instance_out_of_sight(rep, host, client):
    """C8: the server holds an instance request to the player's line of sight as well as its reach. The joiner asks for a ground item 2.5-7 m away
    through a slab the HOST alone has, between the server's view of the joiner and the item (MO.Test.SightWall): refused "out of sight", nothing
    given. Control: the wall taken down, the same request picks the item up -- the refusal was the wall, not the item, the reach or the rate. The
    control is also the check against a false refusal of an item in plain sight (the sight rule tries the pivot, the axis, the bounds' centre and
    top: UMOInteractorComponent::IsInstanceInSight)."""
    if not _worlds_built(rep, host, client):
        return
    near = _ground_items_both(rep, host, client, 3000, 20, want=1)
    if not rep.step("CONTROL: a PCG ground item within 30 m of the joiner exists on both machines", bool(near),
                    f"{near[0][0]} {near[0][5]} cm away" if near else "none"):
        return
    _joiner_make_room(host)
    item, mesh, x, y, z, _d = near[0]
    # Stand the joiner 3 m from it (the server owns its pawn; the move replicates), within reach: the earlier cases take the items nearest it.
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 f"l = unreal.Vector({x}, {y}, {z}); d = p.get_actor_location() - l; d.z = 0.0; d.normalize(); "
                 "p.set_actor_location(l + d * 300.0 + unreal.Vector(0.0, 0.0, 110.0), False, True); out('S_STAND')")
    time.sleep(3)
    d = 300
    hoff = log_size(host)
    console(host, f"MO.Test.SightWall {x} {y} {z} 1")
    wall = wait_log(host, r"\[MOTEST\] (?:SIGHT_WALL|FAIL SightWall)[^\n]*", since=hoff, timeout=10)
    before = _joiner_item_count(host, item)
    hoff = log_size(host)
    console(client, f"MO.Test.InstanceInteract {x} {y} {z} {mesh}")
    refused = wait_log(host, r"\[MOInteractor\] ServerRequestInstanceInteract rejected: [^\n]*", since=hoff, timeout=10)
    time.sleep(1)
    after = _joiner_item_count(host, item)
    rep.step(f"trust: E on a ground item {d} cm away through a wall is REFUSED by the server (out of sight)",
             bool(wall) and "SIGHT_WALL at=" in wall.group(0) and bool(refused) and "out of sight" in refused.group(0) and after == before,
             f"wall: {wall.group(0)[:160] if wall else 'no SIGHT_WALL line'} | host: {refused.group(0)[:200] if refused else 'no rejected line'} | "
             f"{item} {before} -> {after}")
    console(host, "MO.Test.SightWall clear")
    time.sleep(1)
    hoff = log_size(host)
    console(client, f"MO.Test.InstanceInteract {x} {y} {z} {mesh}")
    got, deadline = after, time.time() + 10
    while time.time() < deadline and got <= before:
        time.sleep(1)
        got = _joiner_item_count(host, item)
    served = re.search(r"\[MOInteractor\] (?:instance interact|ServerRequestInstanceInteract rejected)[^\n]*", read_log_from(host, hoff))
    rep.step("CONTROL: with the wall down, the same request picks the item up", got > before,
             f"{item} {before} -> {got}; host: {served.group(0)[:200] if served else 'no [MOInteractor] line'}")
    _joiner_remove(host, item, got - before)


def _host_foliage_reaches_client(rep, host, client):
    """The HOST clears foliage (the terraform tool's Remove Foliage, by the joiner) and the CLIENT's grass there goes too. Wes's Steam test, Oct 18
    2026: "remove foliage replicates from client to host but not host to client" -- the worked-ground zones that keep it clear lived only on the
    host. Controls: both machines had sweepable grass there first, and the host's own count drops (the tool worked)."""
    if not _worlds_built(rep, host, client):
        return
    m = re.search(r"SPOT x=(-?[\d.]+) y=(-?[\d.]+) z=(-?[\d.]+)", _probe(host,
        "p = unreal.GameplayStatics.get_player_pawn(world, 1); l = p.get_actor_location(); out('SPOT x=%f y=%f z=%f' % (l.x, l.y, l.z))"))
    if not rep.step("host: located the joiner for the foliage test", bool(m), m.group(0) if m else "no SPOT line"):
        return
    spot = None
    # The grass nearest the joiner, as the CLIENT has it, then the same spot on the host (a joiner can spawn on bare sand: no fixed offset finds grass).
    n = _motest(client, "MO.Test.NearestSweepable 3000", r"\[MOTEST\] NEAREST_SWEEPABLE found=(\d) at=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)")
    if n and n.group(1) == "1":
        tx, ty, tz = (float(v) for v in n.group(2, 3, 4))
        hb = _motest(host, f"MO.Test.Sweepable {tx:.0f} {ty:.0f} {tz:.0f} 150", SWEEPABLE_RE)
        cb = _motest(client, f"MO.Test.Sweepable {tx:.0f} {ty:.0f} {tz:.0f} 150", SWEEPABLE_RE)
        if hb and cb and int(hb.group(1)) > 0 and int(cb.group(1)) > 0:
            spot = (tx, ty, tz, int(hb.group(1)), int(cb.group(1)))
    # Remove Foliage works around the host's pawn: stand it by the spot (the host owns its own pawn).
    if spot:
        _probe(host, f"p = unreal.GameplayStatics.get_player_pawn(world, 0); p.set_actor_location(unreal.Vector({spot[0]} + 200.0, {spot[1]}, {spot[2]} + 120.0), False, True); out('HOST_MOVED')")
        time.sleep(2)
    if not rep.step("CONTROL: host and client both have grass (sweepable instances) at one spot by the joiner", bool(spot),
                    f"host {spot[3]}, client {spot[4]} within 1.5 m" if spot else "no spot with grass on both"):
        return
    tx, ty, tz, _hn, _cn = spot
    coff = log_size(client)
    cleared = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); tc = p.get_component_by_class(unreal.MOTerraformingComponent); "
                           f"ok = tc.terraform_at_location(unreal.Vector({tx}, {ty}, {tz}), unreal.MOTerraformMode.REMOVE_FOLIAGE) if tc else None; out('CLEAR ok=%s' % ok)")
    time.sleep(5)
    ha = _motest(host, f"MO.Test.Sweepable {tx:.0f} {ty:.0f} {tz:.0f} 150", SWEEPABLE_RE)
    ca = _motest(client, f"MO.Test.Sweepable {tx:.0f} {ty:.0f} {tz:.0f} 150", SWEEPABLE_RE)
    cm = re.search(r"CLEAR ok=\S+", cleared)
    rep.step("CONTROL: the host's Remove Foliage cleared the grass on the host", bool(ha) and int(ha.group(1)) == 0,
             (cm.group(0) if cm else cleared.strip()[-120:]) + " | " + (ha.group(0) if ha else "no SWEEPABLE line"))
    applied = re.search(r"\[MOTerrainMod\] client: \d+ worked-ground zone\(s\) from the host[^\n]*", read_log_from(client, coff))
    rep.step("host Remove Foliage: the grass is gone on the CLIENT too", bool(ca) and int(ca.group(1)) == 0,
             (ca.group(0) if ca else "no SWEEPABLE line") + " | " + (applied.group(0) if applied else "client logged no zones from the host"))


def _appearance_replicates(rep, host, client):
    """A survivor's look (the character creator's colours: AMOMetaHumanCharacter::CurrentAppearance, replicated) set on the authority reaches a co-op
    partner -- read on the CLIENT, values and the procedural eye material its OnRep put on. Control: the client's copy had the default look first."""
    probe_looks = ("ps = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOMetaHumanCharacter); "
                   "out('LOOKS ' + ';'.join('%.2f/%.2f/%s/%s' % (x.get_appearance().skin_tone, x.get_appearance().hair_melanin, x.get_appearance().eye_color, "
                   "[c for c in x.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == 'FaceMesh'][0].get_material(2).get_name()) for x in ps))")
    before = _probe(client, probe_looks)
    rep.step("CONTROL: before the host sets a look, no survivor on the client has it", "0.66/0.22/Blue" not in before, before.strip()[-200:])
    setv = _probe(host,
        "p = unreal.GameplayStatics.get_player_pawn(world, 0); a = p.get_appearance(); a.skin_tone = 0.66; a.hair_melanin = 0.22; a.eye_color = 'Blue'; "
        "p.set_appearance_colors(a); out('LOOK_SET %s' % p.get_name())")
    if not rep.step("host: set its survivor's look (skin 0.66, hair 0.22, blue eyes)", "LOOK_SET" in setv, setv.strip()[-160:]):
        return
    got = ""
    for _ in range(8):
        time.sleep(1.0)
        got = _probe(client, probe_looks)
        if "0.66/0.22/Blue" in got:
            break
    m = re.search(r"0\.66/0\.22/Blue/([^;\s]+)", got)
    rep.step("client: the host's survivor has the look it chose", bool(m), got.strip()[-200:])
    rep.step("client: ... with the procedural eye on (the colour is not just data)", bool(m) and "Procedural" in m.group(1), m.group(1) if m else "no matching survivor")


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


def _client_forage(rep, host, client):
    """CLIENT foraging (the ground menu's Search / Dig): the menu used to run the foraging subsystem ON THE CLIENT, so a joiner's finds existed on the
    joiner only. Now it asks AMOPlayerController::ServerForage. Control first: the old direct call on the client must be refused and reach nothing on
    the host. Then the real call: the HOST runs the dig and logs it, the items it made replicate to the client, and an immediate second forage is
    refused (MinForageIntervalSeconds)."""
    items_near = ("p = unreal.GameplayStatics.get_player_pawn(world, %d); loc = p.get_actor_location(); "
                  "n = sum(1 for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if (a.get_actor_location() - loc).length() < 600.0); "
                  "out('NEARITEMS=%%d' %% n)")
    hoff, coff = log_size(host), log_size(client)
    console(client, "MO.Test.Forage dig 150 direct")
    wait_log(client, r"\[MOTEST\] Forage direct[^\n]*", since=coff, timeout=15)
    time.sleep(2)
    cl, hl = read_log_from(client, coff), read_log_from(host, hoff)
    refused = re.search(r"\[MOForagingSubsystem\] \S+ refused on a client", cl)
    direct = re.search(r"\[MOTEST\] Forage direct \(dig\) on a CLIENT: (\d+) item", cl)
    rep.step("CONTROL: the old path (the foraging subsystem called on the CLIENT) is refused and spawns nothing",
             bool(refused) and bool(direct) and direct.group(1) == "0", (refused.group(0) if refused else "no refusal line") + (f"; {direct.group(0)}" if direct else ""))
    rep.step("CONTROL: ... and the host saw no forage", "[MOForaging]" not in hl, "no [MOForaging] line on the host" if "[MOForaging]" not in hl else "host logged a forage")

    m0 = re.search(r"NEARITEMS=(\d+)", _probe(client, items_near % 0))
    before = int(m0.group(1)) if m0 else -1
    hoff, coff = log_size(host), log_size(client)
    console(client, "MO.Test.Forage dig 150")
    done = wait_log(host, r"\[MOForaging\] \S+: dig at [^\n]*-> (\d+) item", since=hoff, timeout=20)
    hl = read_log_from(host, hoff)
    dug = re.search(r"\[MOForaging\] \S+: dig at [^\n]*-> (\d+) item", hl)
    if not rep.step("client dig: the HOST ran it (ServerForage) and logged the result", bool(dug), dug.group(0)[:140] if dug else "no '[MOForaging] ...: dig at' line on the host"):
        return
    count = int(dug.group(1))
    if count > 0:
        seen = -1
        for _ in range(10):
            time.sleep(1.5)
            m1 = re.search(r"NEARITEMS=(\d+)", _probe(client, items_near % 0))
            seen = int(m1.group(1)) if m1 else -1
            if seen >= before + count:
                break
        rep.step(f"client dig: the {count} item(s) the host made replicate to the client", seen >= before + count, f"world items within 6 m of the joiner: {before} -> {seen}")
    else:
        rep.step("client dig: the host's roll found nothing this time (no items to replicate)", True, dug.group(0)[:140])
    result = re.search(r"\[MOInventoryUI\] Foraging dig complete - (\d+) item", read_log_from(client, coff))
    rep.step("client dig: the result came back to the client's UI (ClientForageResult)", bool(result) and int(result.group(1)) == count,
             result.group(0) if result else "no '[MOInventoryUI] Foraging dig complete' line on the client")
    time.sleep(3)   # past MinForageIntervalSeconds: the first of the next two is a fresh forage
    hoff = log_size(host)
    console(client, "MO.Test.Forage dig 150 twice")   # two requests in the same tick
    wait_log(host, r"\[MOForaging\] [^\n]*forage refused[^\n]*", since=hoff, timeout=15)
    time.sleep(1)
    again = read_log_from(host, hoff)
    n_dug = len(re.findall(r"\[MOForaging\] \S+: dig at", again))
    n_refused = len(re.findall(r"\[MOForaging\] [^\n]*forage refused", again))
    rep.step("trust: of two forages sent in the same tick the server takes ONE and refuses the other (MinForageIntervalSeconds)", n_dug == 1 and n_refused == 1,
             f"host: {n_dug} dig(s) accepted, {n_refused} refused")


def _host_beside_joiner_building(host):
    """Stand the host right beside the building nearest the JOINER: MO.Test.Build state on the host reads the building nearest the HOST's pawn,
    and with the pawns moved to clear ground an older building (a stick floor laid in front of the host by an earlier case) was nearer than the
    joiner's new campfire (Oct 24)."""
    out = _probe(host, "j = unreal.GameplayStatics.get_player_pawn(world, 1); h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                       "bs = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor), key=lambda a: (a.get_actor_location() - j.get_actor_location()).length()); "
                       "b = bs[0] if bs else None; "
                       "b and h.set_actor_location(b.get_actor_location() + unreal.Vector(0.0, 150.0, 100.0), False, True); "
                       "out('HOSTBESIDE %s' % (b.get_name() if b else 'none'))")
    time.sleep(1.0)
    m = re.search(r"HOSTBESIDE \S+", out)
    return m.group(0) if m else "HOSTBESIDE no answer"


def _build_state(name):
    """What `name` sees of the placed building nearest its pawn: dict(state, elapsed, deposited) from MO.Test.Build state, or None."""
    off = log_size(name)
    console(name, "MO.Test.Build state")
    m = wait_log(name, r"\[MOTEST\] (BuildState \S+ recipe=\S+ state=(\d) elapsed=([\d.]+) total=[\d.]+ deposited=(\S+)[^\n]*|FAIL Build state[^\n]*)", since=off, timeout=15)
    if not m or m.group(2) is None:
        return None
    return {"state": int(m.group(2)), "elapsed": float(m.group(3)), "deposited": m.group(4), "line": m.group(1)[:200]}


def _client_build(rep, host, client):
    """CLIENT construction (Wes, Oct 13 2026: "co-op build for sure needs to be server/host driven"). The ghost menu used to gather, deposit, start and
    refund ON THE CLIENT's copy of the building -- the host never saw any of it, so the building never finished for anyone else -- and a joiner's
    right-click opened the menu on the host's copy of the joiner's controller, i.e. nowhere. Now the menu's actions are requests the HOST runs
    (UMOBuildingComponent::RequestBuildAction), the progress replicates (UMOBuildProgressComponent::RepState), and UI requests made on the server
    go to the owning client (AMOPlayerController::ClientUIContractRequest). Proof is read on the HOST; the client's view is checked against it."""
    inv = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); ")

    def cobbles():
        m = re.search(r"COBBLES=(-?\d+)", _probe(host, inv + "out('COBBLES=%d' % inv.get_item_count_by_definition_id('RiverCobble01'))"))
        return int(m.group(1)) if m else -1

    def give(n):
        return "GAVE=True" in _probe(host, inv + f"out('GAVE=%s' % inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), 'RiverCobble01', {n}))")

    def place(forward_cm):
        base = _count_buildables(host)
        console(client, f"MO.Test.PlaceBuildingRPC BuildCampfire {forward_cm}")
        for _ in range(10):
            time.sleep(1.0)
            if (_count_buildables(host) or 0) > (base or 0):
                break
        time.sleep(3)   # replicates to the client
        return base, _count_buildables(host)

    base, after = place(300)
    if not rep.step("client build: a campfire ghost the joiner placed exists on the host", after == (base or 0) + 1, f"buildings {base} -> {after}"):
        return
    _host_beside_joiner_building(host)
    hs, cs = _build_state(host), _build_state(client)
    rep.step("client build: host and client see the same empty ghost (state Ghost, nothing deposited)",
             bool(hs and cs) and hs["state"] == 0 == cs["state"] and hs["deposited"] == "-" == cs["deposited"], f"host: {hs and hs['line']} | client: {cs and cs['line']}")

    # The right-click: the server handles the interaction, the menu must open on the JOINER's screen.
    coff = log_size(client)
    console(client, "MO.Test.Build menu")
    fwd = wait_log(client, r"\[MOUIContract\] the server asked this client for UI request \d+ on [^\n]*", since=coff, timeout=15)
    shown = wait_log(client, r"\[MOBuildUI\] Projection: [^\n]*", since=coff, timeout=5)
    rep.step("client build: the joiner's right-click opens the build menu ON THE JOINER (the server forwards the UI request)", bool(fwd) and bool(shown),
             (fwd.group(0) if fwd else "no [MOUIContract] line on the client") + (" + menu shown" if shown else " -- menu never shown"))

    _joiner_make_room(host)
    if not rep.step("host: gave the joiner 10 river cobbles", give(10), ""):
        return
    before = cobbles()
    # CONTROL: the old way -- the component called on the client -- is refused there and reaches nothing on the host.
    coff = log_size(client)
    console(client, "MO.Test.Build direct")
    wait_log(client, r"\[MOTEST\] Build direct[^\n]*", since=coff, timeout=15)
    cl = read_log_from(client, coff)
    refused = re.search(r"\[MOBuildProgressComponent\] DepositMaterial refused[^\n]*", cl)
    time.sleep(1)
    hs = _build_state(host)
    rep.step("CONTROL: the old path (the build component called on the CLIENT) is refused and the host's ledger stays empty",
             bool(refused) and bool(hs) and hs["deposited"] == "-", (refused.group(0)[:120] if refused else "no refusal on the client") + f" | host: {hs and hs['deposited']}")

    hoff = log_size(host)
    console(client, "MO.Test.Build add")
    got = wait_log(host, r"\[MOBuildProgressComponent\] GatherMissingMaterials: (\d+) unit[^\n]*", since=hoff, timeout=15)
    time.sleep(2)
    hs, cs, now = _build_state(host), _build_state(client), cobbles()
    rep.step("client build: Add materials ran ON THE HOST: 10 cobbles went from the joiner's inventory (host's copy) into the building",
             bool(got) and got.group(1) == "10" and before - now == 10 and bool(hs) and "RiverCobble01:10" in hs["deposited"],
             f"{got.group(0)[:110] if got else 'no GatherMissingMaterials line on the host'} | joiner cobbles {before} -> {now} | host ledger {hs and hs['deposited']}")
    rep.step("client build: ... and the deposits replicate to the client", bool(cs) and "RiverCobble01:10" in cs["deposited"], f"client ledger {cs and cs['deposited']}")

    console(client, "MO.Test.Build start")
    time.sleep(4)
    hs, cs = _build_state(host), _build_state(client)
    rep.step("client build: Build started construction on the HOST and the client sees it constructing (state 1, elapsed moving)",
             bool(hs and cs) and hs["state"] == 1 and cs["state"] == 1 and cs["elapsed"] > 0.5, f"host: {hs and hs['line']} | client: {cs and cs['line']}")
    coff, hoff = log_size(client), log_size(host)
    done = wait_log(host, r"\[MOBuildProgressComponent\] Construction complete![^\n]*", since=hoff, timeout=45)
    time.sleep(3)
    hs, cs = _build_state(host), _build_state(client)
    rep.step("client build: the campfire finished on the HOST (30 s build) and the client sees it complete", bool(done) and bool(hs and cs) and hs["state"] == 3 == cs["state"],
             f"host: {hs and hs['line']} | client: {cs and cs['line']}")
    quest = re.search(r"\[MOQuestSubsystem\] event \S*ItemCraft BuildCampfire x1[^\n]*", read_log_from(client, coff))
    rep.step("client build: the construction counts for the JOINER's quests (on the joiner's machine)", bool(quest), quest.group(0) if quest else "no ItemCraft BuildCampfire event on the client")

    # Cancel before starting: a full refund to the joiner, the building removed -- both by the host. Placed nearer than the finished one
    # (MO.Test.Build works on the nearest building).
    base, after = place(150)
    give(10)
    console(client, "MO.Test.Build add")
    time.sleep(3)
    before = cobbles()
    hoff = log_size(host)
    console(client, "MO.Test.Build cancel")
    canc = wait_log(host, r"\[MOBuildProgressComponent\] CancelAndRemove [^\n]*", since=hoff, timeout=15)
    time.sleep(3)
    rep.step("client build: Cancel ran ON THE HOST: the 10 deposited cobbles came back to the joiner and the building is gone",
             bool(canc) and cobbles() - before == 10 and _count_buildables(host) == base,
             f"{canc.group(0)[:120] if canc else 'no CancelAndRemove line on the host'} | buildings {after} -> {_count_buildables(host)}")


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

    # The pawn punches where the camera looks (Wes, Oct 16 2026): the joiner's body faces 120 deg away from its view, it attacks, and the HOST's copy of the
    # joiner turns to the swing (UMOCombatComponent::BeginFacingSwing on the server; the client turns its own copy at once). Before, the body kept facing
    # its last walk, and a camera-aimed hit outside the body's arc was thrown away.
    time.sleep(2.5)   # the previous swing's recovery
    view = _probe(client, "pc = unreal.GameplayStatics.get_player_controller(world, 0); out('VIEWYAW=%.1f' % pc.get_control_rotation().yaw)")
    vm = re.search(r"VIEWYAW=(-?[\d.]+)", view)
    if not vm:
        rep.step("client: read its view yaw", False, view.strip()[-160:])
        return
    vyaw = float(vm.group(1))
    away = vyaw + 120.0
    set_away = "p = unreal.GameplayStatics.get_player_pawn(world, %d); p.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=%f), False); out('AWAY')"
    _probe(host, set_away % (1, away))
    _probe(client, set_away % (0, away))
    time.sleep(1.0)
    yaw_probe = "p = unreal.GameplayStatics.get_player_pawn(world, %d); out('BODYYAW=%%.1f' %% p.get_actor_rotation().yaw)"
    def off_view(name, idx):
        m = re.search(r"BODYYAW=(-?[\d.]+)", _probe(name, yaw_probe % idx))
        return None if not m else (float(m.group(1)) - vyaw + 180.0) % 360.0 - 180.0
    before_host = off_view(host, 1)
    console(client, "MO.Test.Attack")
    time.sleep(1.5)
    after_host, after_client = off_view(host, 1), off_view(client, 0)
    rep.step("client attack: the joiner's pawn turns to its camera's aim for the swing -- on the HOST (what others see) and on the joiner",
             before_host is not None and abs(before_host) > 90 and after_host is not None and abs(after_host) < 25 and after_client is not None and abs(after_client) < 25,
             f"body vs view yaw on the host: {before_host} -> {after_host}; on the joiner -> {after_client}")


def _client_eat(rep, host, client):
    """Eating is the pawn's timed action (Wes, Oct 16 2026: "put eating on a timer"): the joiner's Eat runs on the SERVER for the food's time
    (FMOItemActions::EatSeconds: a 50 g handful of berries 25 s) and the berry is gone only at the end. Proof is read on the host."""
    inv = "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
    count = lambda: int((re.search(r"BERRIES=(-?\d+)", _probe(host, inv + "out('BERRIES=%d' % inv.get_item_count_by_definition_id('Berries01'))")) or re.search("(-1)", "-1")).group(1))
    _probe(host, inv + "inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Berries01', 2); out('GAVE')")
    time.sleep(2)
    c0 = count()
    off = log_size(host)
    console(client, "MO.Item.Do Berries01 Use")
    time.sleep(4)
    c1 = count()
    began = re.search(r"\[MOTimedAction\] \S+: Use \(([\d.]+) s\)", read_log_from(host, off))
    rep.step("client eat: the HOST started the joiner's eating as a timed action, and after 4 s the berry is still there",
             bool(began) and c1 == c0 and c0 > 0, f"{began.group(0) if began else 'no [MOTimedAction] Use line on the host'} | berries {c0} -> {c1}")
    if not began:
        return
    done = wait_log(host, r"\[MOItemActions\] \S+ finished eating", since=off, timeout=float(began.group(1)) + 8)
    time.sleep(1)
    c2 = count()
    rep.step("client eat: ... the berry is eaten when the time is up (one fewer, on the host)", bool(done) and c2 == c0 - 1, f"berries {c1} -> {c2}")


def _client_light_fire(rep, host, client):
    """A joiner lights a fire with a hand drill: the attempt runs on the SERVER (FMOFireActions, timed and chancy), it teaches Firemaking (Wes, F33 A:
    25 XP a failure, 40 a fire), and the JOINER's own screen shows the skill toast -- the server -> joiner ClientShowSkillPopup path, which nothing
    else reaches (N-183, Oct 20 2026). The 3-minute drill is shortened by MO.Test.FireAttemptSeconds (test builds only)."""
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                 "inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Tinder01', 1); inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Branch01', 2); out('GAVE')")
    console(host, "MO.Test.FireAttemptSeconds 4")
    console(host, "MO.Fire.SpawnLit laid 1")
    # MO.Fire.Do lights the fire NEAREST the joiner, which may be one an earlier case built (_client_build): fuel every finished fire within 5 m.
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "fs = [f for f in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCraftingStationActor) "
                 "if f.get_editor_property('requires_fuel') and (f.get_actor_location() - p.get_actor_location()).length() < 500]; "
                 "[f.set_editor_property('current_fuel', f.get_editor_property('max_fuel')) for f in fs]; out('FUELLED %d' % len(fs))")
    time.sleep(2)
    # The drill is work at the ground: the joiner kneels for it (N-181, UMOTimedActionComponent::BeginKneeling). The joiner's own machine crouches its
    # pawn (from the replicated action state) and its moves carry the crouch to the server, whose copy is what everyone else sees.
    down = "p = unreal.GameplayStatics.get_player_pawn(world, %d); out('DOWN=%%s' %% p.get_editor_property('is_crouched'))"
    before = _probe(host, down % 1)
    rep.step("CONTROL: the joiner stands before it drills (host's copy)", "DOWN=False" in before, " ".join(re.findall(r"DOWN=\w+", before)) or before.strip()[-120:])
    hoff, coff = log_size(host), log_size(client)
    console(client, "MO.Fire.Do Light")
    time.sleep(1.0)
    during = _probe(host, down % 1) + _probe(client, down % 0)
    rep.step("client fire: the joiner kneels to drill -- on the HOST's copy and on its own", during.count("DOWN=True") == 2, " / ".join(re.findall(r"DOWN=\w+", during)))
    lit = wait_log(host, r"\[MOFire\] \S+ lighting \S+ \(improvised hand drill[^)]*\): chance \d+ % -> (failed|CAUGHT)", since=hoff, timeout=20)
    rep.step("client fire: the HOST ran the joiner's hand-drill attempt (timed, chancy)", bool(lit), lit.group(0) if lit else "no [MOFire] lighting line on the host")
    toast = wait_log(client, r"Skill toast: \"Firemaking \+(25|40) XP\"", since=coff, timeout=10)
    rep.step("client fire: ... and the JOINER's own screen showed what it taught (the server -> joiner skill toast)", bool(toast),
             toast.group(0) if toast else "no Firemaking skill toast in the client's log")
    time.sleep(1.0)
    after = _probe(host, down % 1)
    rep.step("client fire: ... and stands up when the attempt is over (host's copy)", "DOWN=False" in after, " ".join(re.findall(r"DOWN=\w+", after)) or after.strip()[-120:])
    console(host, "MO.Test.FireAttemptSeconds 0")


def _client_sees_creature_flinch(rep, host, client):
    """A wolf hit by the joiner flinches away from it, and bites, ON THE JOINER'S SCREEN (N-169, Oct 20 2026): creature animations went to the
    server's AnimInstance only (the bite was a BT task's Montage_Play), so a joiner saw wolves glide in and hurt it with no bite. Both now go
    through AMOCreature::PlayAnimationEverywhere (a reliable cue event). The host spawns a wolf beside the joiner and hurts it in the joiner's
    name a tick later (MO.Test.HitCreature: from editor Python the flinch cue never left the host): the host plays the flinch for the side the
    blow came from, and the joiner's own log shows it played there; then the angry wolf bites someone, and the joiner's log shows the bite.
    Runs late (the bite wounds the joiner), before the cases that end the joiner. The joiner first moves to clear ground (Oct 24: by the
    joiner's earlier builds and the larger rock tiers the angry wolf stuck 5 m off for 19 s, backing up, and gave up without a bite)."""
    _to_clear_ground(host, 1, bring_host=False)
    hoff = log_size(host)
    console(host, "MO.AI.StressSpawn 1 500 800")
    spawned = wait_log(host, r"\[MO.AI.StressSpawn\] Done[^\r\n]*", since=hoff, timeout=20)
    if not rep.step("CONTROL: the host spawned a wolf", bool(spawned), spawned.group(0) if spawned else "no StressSpawn Done line"):
        return
    # Stand the wolf by the joiner and wait until the JOINER's world has it: a cue sent before the client has the creature is dropped (Oct 21:
    # the wolf was teleported and hit in one step; the joiner logged the bite but never the flinch).
    _probe(host, "j = unreal.GameplayStatics.get_player_pawn(world, 1); jl = j.get_actor_location(); "
                 "w = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), key=lambda a: (a.get_actor_location() - jl).length()); "
                 "w.set_actor_location(jl + unreal.Vector(500.0, 0.0, 120.0), False, True); out('WOLF_PLACED')")
    seen_code = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); pl = p.get_actor_location(); "
                 "out('CLIENTWOLF %d' % len([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature) "
                 "if (a.get_actor_location() - pl).length() < 1500]))")
    for _ in range(10):
        if re.search(r"CLIENTWOLF [1-9]", _probe(client, seen_code)):
            break
        time.sleep(1.0)
    time.sleep(1.0)
    hit = None
    coff = log_size(client)
    for _ in range(4):   # a wolf mid-bite does not flinch (its bite's damage lands anyway): try again after it
        hoff = log_size(host)
        _probe(host, "j = unreal.GameplayStatics.get_player_pawn(world, 1); jl = j.get_actor_location(); "
                     "ws = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature)); "
                     "w = min(ws, key=lambda a: (a.get_actor_location() - jl).length()); "
                     "w.set_actor_location(jl + unreal.Vector(500.0, 0.0, 120.0), False, True); "
                     # the joiner lies at -X from the wolf: facing yaw 270 (-Y) puts it on the wolf's left
                     "w.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=270.0), False); out('WOLF %s' % w.get_name())")
        # The blow from a console verb that runs a tick later, outside script execution: an inflict_damage from editor Python ran the blow's
        # own RPCs (the flinch cue) on the host only (Oct 21: "GetFunctionCallspace ScriptExecutionInEditor: MulticastAnimCue").
        console(host, "MO.Test.HitCreature 1 2 Wolf")
        hit = wait_log(host, r"\[MOCreature\] \S+ hit from the (\w+) by \S+: (\S+) \(([\d.]+) s\)", since=hoff, timeout=4)
        if hit:
            break
        time.sleep(1.5)
    # The reaction for the side the game computed at the blow (the wolf turns in the tick before the deferred blow lands, so not always the
    # left it was set up with); the side's geometry is unit-tested (MOFramework.Creature.HitSide). Back falls back to Front without its own clip.
    side_ok = bool(hit) and (hit.group(2) == "ANIM_Wolf_GetHit" + hit.group(1) or (hit.group(1) == "Back" and hit.group(2) == "ANIM_Wolf_GetHitFront"))
    rep.step("host: the wolf flinches with the reaction for the side the blow came from (ANIM_Wolf_GetHit<side>)",
             side_ok and float(hit.group(3)) > 0.0, hit.group(0) if hit else "no hit-reaction line on the host")
    wolf_code = ("min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), "
                 "key=lambda w: (w.get_actor_location() - unreal.GameplayStatics.get_player_pawn(world, 1).get_actor_location()).length())")
    nav_at_hit = _nav_between(host, wolf_code, "unreal.GameplayStatics.get_player_pawn(world, 1)")
    print(f"[nettest] wolf at the blow: {nav_at_hit}", flush=True)
    seen = wait_log(client, r"\[MOCreature\] \S+: anim cue \d+ on this client: ANIM_Wolf_GetHit\w+ \(([\d.]+) s\)", since=coff, timeout=10)
    rep.step("client: ... and the flinch plays on the JOINER's copy of the wolf", bool(seen) and float(seen.group(1)) > 0.0,
             seen.group(0) if seen else "no anim cue line on the client")
    bite = wait_log(client, r"\[MOCreature\] \S+: anim cue \d+ on this client: AM_WolfBite \(([\d.]+) s\)", since=coff, timeout=40)
    why = ""
    if not bite:   # a wolf that never reached the joiner fails for its ground, not the cue: say how far it got and what stands round it
        chase = re.findall(r"\[(?:BTChaseTarget|MOCreatureController)\] [^\r\n]*", read_log_from(host, hoff))
        near = re.search(r"WOLFNEAR [^\r\n]*", _probe(host,
            "j = unreal.GameplayStatics.get_player_pawn(world, 1); jl = j.get_actor_location(); "
            "w = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), key=lambda a: (a.get_actor_location() - jl).length()); "
            "res = unreal.SystemLibrary.sphere_overlap_components(world, w.get_actor_location(), 400.0, [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2], None, [w, j]); "
            "cs = [c for c in (res[1] if isinstance(res, tuple) else []) if 'Voxel' not in c.get_class().get_name()]; "
            "out('WOLFNEAR %s %.0f m from the joiner; within 4 m: %s' % (w.get_name(), (w.get_actor_location() - jl).length() / 100.0, "
            "sorted({'%s/%s' % (c.get_owner().get_name() if c.get_owner() else '?', c.get_name()) for c in cs})[:6]))"))
        why = (" | " + (" // ".join(c[:140] for c in chase[-4:]) if chase else "no chase line") + " | " + (near.group(0) if near else "no WOLFNEAR answer")
               + " | at the blow: " + nav_at_hit)
    rep.step("client: the angry wolf's bite plays on the joiner's copy too", bool(bite) and float(bite.group(1)) > 0.0,
             bite.group(0) if bite else "no bite cue on the client within 40 s" + why)
    # A killed wolf leaves a carcass to skin and butcher (DT_Carcasses "Wolf", Oct 20; before, only the deer had one and a wolf left nothing).
    # The wolf is stood 5 m from the joiner first: after its bite it can be anywhere (Oct 22: 178 m off, past the joiner's 150 m net cull
    # distance, so the joiner never got the carcass and the case failed for its setup, not the game).
    hoff = log_size(host)
    _probe(host, "j = unreal.GameplayStatics.get_player_pawn(world, 1); jl = j.get_actor_location(); "
                 "w = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), key=lambda a: (a.get_actor_location() - jl).length()); "
                 "w.set_actor_location(jl + unreal.Vector(500.0, 0.0, 120.0), False, True); "
                 "out('KILL %s' % w.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.HEART, 1000.0, unreal.MOWoundType.PUNCTURE, j))")
    laid = wait_log(host, r"\[MOCreature\] Spawned carcass '([^']*)' for \S+", since=hoff, timeout=15)
    rep.step("host: the dead wolf leaves a carcass", bool(laid) and laid.group(1) == "Wolf Carcass", laid.group(0) if laid else "no Spawned carcass line on the host")
    # The wolf dies where it stood when the probe killed it -- after the bite it may be tens of metres off (Oct 20: none within 30 m): look within
    # 150 m, and give the replication a few tries.
    rows = ""
    for _ in range(5):
        time.sleep(2)
        rows = _probe(client, "p = unreal.GameplayStatics.get_player_pawn(world, 0); "
                              "out('CARCASSES %s' % sorted(str(c.get_editor_property('carcass_definition').row_name) for c in "
                              "unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCarcassActor) if (c.get_actor_location() - p.get_actor_location()).length() < 15000))")
        if "'Wolf'" in rows:
            break
    where = "" if "'Wolf'" in rows else _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); "
        "out('HOSTCARCASS %s' % [(c.get_name(), round((c.get_actor_location() - p.get_actor_location()).length()), round(c.get_actor_location().z)) "
        "for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCarcassActor)])")
    rep.step("client: ... and the joiner's copy of it knows its parts (the Wolf carcass row)", "'Wolf'" in rows,
             (re.search(r"CARCASSES [^\r\n]*", rows) or re.search(r"[\s\S]{0,160}$", rows)).group(0)
             + ((" | host: " + (re.search(r"HOSTCARCASS[^\r\n]*", where) or re.search(r"[\s\S]{0,120}$", where)).group(0)) if where else ""))
    _probe(host, "[a.destroy_actor() for c in (unreal.MOPredatorCreature, unreal.MOPreyCreature, unreal.MOCarcassActor) "
                 "for a in unreal.GameplayStatics.get_all_actors_of_class(world, c) "
                 "if (a.get_actor_location() - unreal.GameplayStatics.get_player_pawn(world, 1).get_actor_location()).length() < 3000]; out('TIDY')")


def _client_treats_own_wound(rep, host, client):
    """A joiner bandages its own wound from the Status panel's Wounds tab (N-061, Oct 20 2026): before, no UI could treat a wound at all (only
    the dev verb MO.Medical.Treat). Under each wound a button per treatment for it (FMOMedicalTreatmentActions::GetOptions: the server's own
    checks), greyed with the reason; a click asks the server (ServerTreatWound), which runs the timed treatment on the joiner's pawn.
    Control: without a bandage the button is greyed (ClickWidget refuses a disabled button)."""
    pawn = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); out('PAWNNAME=' + p.get_name())")) or [None, "?"])[1]
    bleed = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); a = p.get_component_by_class(unreal.MOAnatomyComponent); "
             "ws = [w for w in a.get_all_wounds() if w.body_part == unreal.MOBodyPartType.FOREARM_RIGHT]; "
             "out('BLEED %.3f BANDAGES %d' % (ws[0].bleed_rate if ws else -1, p.get_component_by_class(unreal.MOInventoryComponent).count_ingredient('Bandage01')))")
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                 "out('CUT %s' % p.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.FOREARM_RIGHT, 15.0, unreal.MOWoundType.LACERATION, h))")
    before = re.search(r"BLEED (-?[\d.]+) BANDAGES (\d+)", _probe(host, bleed))
    if not rep.step("CONTROL: the joiner has a bleeding cut on its right forearm and no bandage (host)", bool(before) and float(before.group(1)) > 0 and before.group(2) == "0",
                    before.group(0) if before else "no wound"):
        return
    console(client, "MO.UI.Open Status")
    time.sleep(1.5)
    console(client, "MO.Test.ClickWidget WoundsTabButton")
    time.sleep(2.5)   # the tab refreshes its rows once a second
    m = _motest(client, "MO.Test.ClickWidget text=Apply Bandage", r"\[MOTEST\] (PASS|FAIL) ClickWidget[^\r\n]*")
    rep.step("client: the Wounds tab offers Apply Bandage, greyed without a bandage", bool(m) and m.group(1) == "FAIL" and "not interactable" in m.group(0),
             m.group(0) if m else "no ClickWidget line")
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); p.get_component_by_class(unreal.MOInventoryComponent)"
                 ".add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Bandage01', 1); out('GAVE')")
    time.sleep(3.0)   # the bandage replicates; the tab refreshes
    hoff = log_size(host)
    m = _motest(client, "MO.Test.ClickWidget text=Apply Bandage", r"\[MOTEST\] (PASS|FAIL) ClickWidget[^\r\n]*")
    begun = wait_log(host, rf"\[MOTimedAction\] {re.escape(pawn)}: MedicalTreatment \(([\d.]+) s\)", since=hoff, timeout=10)
    rep.step("client: with a bandage the button works, and the HOST starts the joiner's timed treatment", bool(begun),
             (m.group(0) if m else "no click line") + " | " + (begun.group(0) if begun else "no MedicalTreatment on the host"))
    if begun:
        time.sleep(float(begun.group(1)) + 2.0)
        after = re.search(r"BLEED (-?[\d.]+) BANDAGES (\d+)", _probe(host, bleed))
        rep.step("... and when it is done the cut bleeds less and the bandage is used (host)",
                 bool(after) and 0 <= float(after.group(1)) < float(before.group(1)) and after.group(2) == "0",
                 f"bleed {before.group(1)} -> {after.group(1) if after else '?'}, bandages {after.group(2) if after else '?'}")
    console(client, "MO.UI.Close")


def _client_footstep_on_rock(rep, host, client):
    """A boulder underfoot sounds like stone (N-132, Oct 20 2026): every placed mesh stood on rang as bare wood. The host finds the nearest PCG
    rock / boulder instance to the joiner, stands the joiner on its top, and the joiner's own probe names the set (UMOFootstepComponent::PropRules)."""
    if not _worlds_built(rep, host, client):
        return
    find = ("import builtins; j = unreal.GameplayStatics.get_player_pawn(world, 1); jl = j.get_actor_location(); builtins.MO_R_HOME = jl; best = None\n"
            "for c in unreal.ObjectIterator(unreal.InstancedStaticMeshComponent):\n"
            "    m = c.static_mesh\n"
            "    mp = m.get_path_name() if m else ''\n"
            "    if c.get_world() != world or not ('Boulder' in mp or 'MossyRocks' in mp):\n"   # plain tests: an exec'd generator cannot see exec's locals
            "        continue\n"
            "    for i in range(min(c.get_instance_count(), 400)):\n"
            "        t = c.get_instance_transform(i, True)\n"
            "        d = (t.translation - jl).length()\n"
            "        if d < 6000 and (best is None or d < best[0]):\n"
            "            best = (d, t, m)\n"
            "if best:\n"
            "    d, t, m = best; b = m.get_bounds(); top = t.translation.z + b.box_extent.z * 2.0 * t.scale3d.z\n"
            "    j.set_actor_location(unreal.Vector(t.translation.x, t.translation.y, top + 150.0), False, True)\n"
            "    out('ROCK %s at %.0f m, top %.0f' % (m.get_name(), d / 100.0, top))\n"
            "else:\n"
            "    out('ROCK none')")
    found = _probe(host, find)
    m = re.search(r"ROCK (\S+) at ([\d.]+) m", found)
    if not rep.step("CONTROL: a boulder near the joiner, and the joiner stood on it (host)", bool(m), (re.search(r"ROCK[^\r\n]*", found) or re.search(r"[\s\S]{0,160}$", found)).group(0)):
        return
    probe_re = r"\[MOQUERY\] FOOTSTEP Probe pos=[^\r\n]*? set=(\S+) \(([^)]*)\)"
    heard = None
    for _ in range(6):
        time.sleep(2.5)
        heard = _motest(client, "MO.Footstep.Probe", probe_re)
        if heard and "grounded=1" in heard.group(0):
            break
    rep.step("client: on the boulder its footsteps are stone (a prop rule, not bare wood)", bool(heard) and heard.group(1) == "StoneStep" and heard.group(2).startswith("prop"),
             heard.group(0)[-160:] if heard else "no probe line")
    _probe(host, "import builtins; p = unreal.GameplayStatics.get_player_pawn(world, 1); h = getattr(builtins, 'MO_R_HOME', None); "
                 "h and p.set_actor_location(h + unreal.Vector(0.0, 0.0, 30.0), False, True); out('R_HOME')")
    time.sleep(2)


def _host_torch_in_rain(rep, host, client):
    """Rain puts a carried torch out (N-056, Oct 20 2026): light rain does nothing, heavier rain at the flame (the weather's exposure there, so a
    roof keeps it off) puts it out at random -- UDW "Rain" (0.7) after ~4 min on average. MO.Test.RainOnFlameSpeedup 30 makes that seconds.
    Control: under a clear sky the same speed-up leaves the torch burning."""
    console(host, "MO.Weather.SetPreset Clear_Skies")
    console(host, "MO.Light.Carried give")
    time.sleep(3.0)   # the equip swap
    lit = _motest(host, "MO.Light.Carried light", r"\[MOQUERY\] LIGHT Carried light: (\d)")
    if not rep.step("CONTROL: the host holds a lit torch", bool(lit) and lit.group(1) == "1", lit.group(0) if lit else "no light line"):
        return
    console(host, "MO.Test.RainOnFlameSpeedup 30")
    hoff = log_size(host)
    time.sleep(15.0)
    rep.step("CONTROL: under a clear sky the sped-up torch burns on", "went out" not in read_log_from(host, hoff), "no '[MOLight] ... went out' in 15 s")
    hoff = log_size(host)
    console(host, "MO.Weather.SetPreset Rain")
    out = wait_log(host, r"\[MOLight\] \S+: rain ([\d.]+) at the flame put it out", since=hoff, timeout=120)
    rep.step("rain at the flame puts the torch out", bool(out) and float(out.group(1)) > 0.4, out.group(0) if out else "still burning after 120 s of rain")
    console(host, "MO.Test.RainOnFlameSpeedup 1")
    console(host, "MO.Weather.SetPreset Clear_Skies")


def _host_wolf_holds_off_fire(rep, host, client):
    """A wolf will not close on someone holding fire (N-096 fire-fear, Oct 20 2026): within its species' FireFearRadiusCm of a flame (a carried
    torch, a campfire) the chase stops short and no bite is made; it circles. The host holds a lit torch with the joiner moved far off, a wolf is
    spawned beside the host: it holds off. Control: the torch put out, the same wolf bites."""
    wounds = "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('WOUNDS %d' % len(p.get_component_by_class(unreal.MOAnatomyComponent).get_all_wounds()))"
    _probe(host, "import builtins; j = unreal.GameplayStatics.get_player_pawn(world, 1); builtins.MO_W_HOME = j.get_actor_location(); "
                 "j.set_actor_location(j.get_actor_location() + unreal.Vector(6000.0, 6000.0, 300.0), False, True); out('JOINER_AWAY')")
    console(host, "MO.Light.Carried give")
    time.sleep(3.0)
    lit = _motest(host, "MO.Light.Carried light", r"\[MOQUERY\] LIGHT Carried light: (\d)")
    if not rep.step("CONTROL: the host holds a lit torch, the joiner is 85 m off", bool(lit) and lit.group(1) == "1", lit.group(0) if lit else "no light line"):
        return
    w0 = int((re.search(r"WOUNDS (\d+)", _probe(host, wounds)) or [0, "0"])[1])
    hoff = log_size(host)
    console(host, "MO.AI.StressSpawn 1 600 900")
    wait_log(host, r"\[MO.AI.StressSpawn\] Done", since=hoff, timeout=15)
    # StressSpawn adds a deer with every wolf, and the wolf hunts the deer (Oct 20: it chased it 200 m off): only the host is left to close on
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); [a.destroy_actor() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPreyCreature) "
                 "if (a.get_actor_location() - h.get_actor_location()).length() < 5000]; out('NO_DEER')")
    # A wolf left alone did not come for the host at all (Oct 20: no bite in 40 s even with the torch out, two runs): it turns on whoever hurts
    # it (AMOCreatureController::NotifyDamaged), so the host's blow makes it want the host -- the torch decides whether it closes.
    provoke = ("h = unreal.GameplayStatics.get_player_pawn(world, 0); hl = h.get_actor_location(); "
               "w = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), key=lambda a: (a.get_actor_location() - hl).length()); "
               "w.set_actor_location(hl + unreal.Vector(600.0, 0.0, 120.0), False, True); "
               "out('PROVOKED %s %s' % (w.get_name(), w.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.TORSO, 2.0, unreal.MOWoundType.BLUNT, h)))")
    angry = re.search(r"PROVOKED (\S+) (\w+)", _probe(host, provoke))
    rep.step("CONTROL: the host's blow turns the wolf on the host", bool(angry), angry.group(0) if angry else "no wolf to provoke")
    console(host, "MO.Footstep.LogAll 1")   # the wolf's own footfalls too (N-132: a creature has no foot bones; it steps by distance)
    time.sleep(30.0)
    console(host, "MO.Footstep.LogAll 0")
    hlog = read_log_from(host, hoff)
    held = re.search(r"\[MOCreature\] (\S+) holds off from \S+: fire within (\d+) cm", hlog)
    w1 = int((re.search(r"WOUNDS (\d+)", _probe(host, wounds)) or [0, "-1"])[1])
    rep.step("the wolf holds off from the torch-bearer (no bite in 30 s)", bool(held) and w1 == w0, (held.group(0) if held else "no hold-off line") + f"; host wounds {w0} -> {w1}")
    paw = re.search(r"\[MOQUERY\] FOOTSTEP step stride [^\r\n]*vol=([\d.]+)[^\r\n]* pawn=(\S*(?:Wolf|Predator)\S*)", hlog)
    rep.step("the host hears the wolf's steps as it comes (a creature steps by distance, softly)", bool(paw),
             f"{paw.group(2)} vol={paw.group(1)}" if paw else "no stride step from a wolf")
    console(host, "MO.Light.Carried out")
    _probe(host, provoke)   # its anger may have run out while it circled
    bitten = False
    for _ in range(8):
        time.sleep(5.0)
        if int((re.search(r"WOUNDS (\d+)", _probe(host, wounds)) or [0, "0"])[1]) > w1:
            bitten = True
            break
    rep.step("CONTROL: with the torch out, the wolf closes and bites", bitten, "a new wound on the host" if bitten else "no bite in 40 s | " + _nav_between(host,
             "min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), "
             "key=lambda w: (w.get_actor_location() - unreal.GameplayStatics.get_player_pawn(world, 0).get_actor_location()).length())",
             "unreal.GameplayStatics.get_player_pawn(world, 0)"))
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                 "[a.destroy_actor() for c in (unreal.MOPredatorCreature, unreal.MOPreyCreature) for a in unreal.GameplayStatics.get_all_actors_of_class(world, c) "
                 "if (a.get_actor_location() - h.get_actor_location()).length() < 4000]; import builtins; j = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "hm = getattr(builtins, 'MO_W_HOME', None); hm and j.set_actor_location(hm + unreal.Vector(0.0, 0.0, 30.0), False, True); out('W_TIDY')")
    time.sleep(2)


def _to_clear_ground(host, pawn=0, radius_cm=1200.0, bring_host=True):
    """Move a player pawn (0 the host's, 1 the joiner's) to flat ground near it with nothing solid within `radius_cm` (Oct 24: the larger rock tiers
    put boulders round the test site, and cases that lay things in front of a pawn, aim a click or walk straight ahead hit them: a joiner pinned
    against a rock, a click that opened a Beach Boulder's menu, cobbles rolling off a stone). Rings of 8 spots out to 80 m; a spot passes when the
    ground there is dry, its relief within 2 m under 50 cm, and an overlap at chest height finds only the voxel terrain. Stays put if none does.
    Moving the joiner brings the host along, 3 m to its side (`bring_host`): host-side checks read the building / item nearest the HOST's pawn
    (MO.Test.Build state), and a joiner moved 15 m away left the host reading an older one."""
    out = _probe(host, "import math\n"
                       f"p = unreal.GameplayStatics.get_player_pawn(world, {pawn})\n"
                       "o = p.get_actor_location()\n"
                       "ign = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter))\n"
                       "def ground(x, y):\n"
                       "    r = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, o.z + 5000.0), unreal.Vector(x, y, o.z - 5000.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ign, unreal.DrawDebugTrace.NONE, True)\n"
                       "    hr = r[1] if isinstance(r, tuple) else r\n"
                       "    t = hr.to_tuple() if hr else None\n"
                       "    return t[5].z if (t and t[0]) else None\n"
                       "found = None\n"
                       "for rad in (0.0, 1500.0, 3000.0, 4500.0, 6000.0, 8000.0):\n"
                       "    for k in range(1 if rad == 0.0 else 8):\n"
                       "        a = k * math.pi / 4.0\n"
                       "        x, y = o.x + rad * math.cos(a), o.y + rad * math.sin(a)\n"
                       "        gz = ground(x, y)\n"
                       "        if gz is None or gz < 50.0:\n"
                       "            continue\n"
                       "        around = [ground(x + 200.0 * math.cos(b * math.pi / 2.0), y + 200.0 * math.sin(b * math.pi / 2.0)) for b in range(4)]\n"
                       "        if any(g is None for g in around) or max(around + [gz]) - min(around + [gz]) > 50.0:\n"
                       "            continue\n"
                       f"        res = unreal.SystemLibrary.sphere_overlap_components(world, unreal.Vector(x, y, gz + 150.0), {radius_cm}, [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2], None, ign)\n"
                       "        comps = res[1] if isinstance(res, tuple) else []\n"
                       "        solid = [c for c in comps if 'Voxel' not in c.get_class().get_name() and not (c.get_owner() and 'Voxel' in c.get_owner().get_class().get_name())]\n"
                       "        if not solid:\n"
                       "            found = (x, y, gz)\n"
                       "            break\n"
                       "    if found:\n"
                       "        break\n"
                       "if found:\n"
                       "    p.set_actor_location(unreal.Vector(found[0], found[1], found[2] + 100.0), False, True)\n"
                       f"    if {pawn} == 1 and {bring_host}:\n"
                       "        h = unreal.GameplayStatics.get_player_pawn(world, 0)\n"
                       "        h.set_actor_location(unreal.Vector(found[0], found[1] + 300.0, found[2] + 100.0), False, True)\n"
                       "    out('CLEARGROUND moved %.0f m to x=%.0f y=%.0f z=%.0f' % (((found[0] - o.x) ** 2 + (found[1] - o.y) ** 2) ** 0.5 / 100.0, found[0], found[1], found[2]))\n"
                       "else:\n"
                       "    out('CLEARGROUND none within 80 m: stays')\n")
    m = re.search(r"CLEARGROUND [^\r\n]*", out)
    print(f"[nettest] {m.group(0) if m else 'CLEARGROUND no answer'} (pawn {pawn})", flush=True)
    time.sleep(2.0)   # the move replicates; the ground under a new spot streams in
    return bool(m) and "moved" in m.group(0)


def _face_clear_heading(host, client, angles=None, reach_cm=7000.0):
    """Turn the joiner to the heading (of `angles`, radians; default the 8 compass points) whose first `reach_cm` at chest height is clearest
    (a sphere trace from its pawn on the host), as a player looks for a way past a rock. Returns the clear distance in cm, -1 with no answer.
    A case that walks or runs straight ahead starts with it (Oct 24: the larger rock tiers stopped the gait's walk at ~30 m and a long run at 49 m)."""
    angles = list(angles) if angles else [k * math.pi / 4.0 for k in range(8)]
    out = _probe(host, "import math\n"
                       "p = unreal.GameplayStatics.get_player_pawn(world, 1)\n"
                       "l = p.get_actor_location() + unreal.Vector(0.0, 0.0, 30.0)\n"
                       "best = (-1.0, l.x, l.y)\n"
                       f"for a in {angles!r}:\n"
                       f"    e = l + unreal.Vector({reach_cm} * math.cos(a), {reach_cm} * math.sin(a), 0.0)\n"
                       "    r = unreal.SystemLibrary.sphere_trace_single(world, l, e, 45.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p], unreal.DrawDebugTrace.NONE, True)\n"
                       "    hr = r[1] if isinstance(r, tuple) else r\n"
                       "    t = hr.to_tuple() if hr else None\n"
                       f"    d = (t[2] * {reach_cm}) if (t and t[0]) else {reach_cm}\n"
                       "    if d > best[0]:\n"
                       "        best = (d, e.x, e.y)\n"
                       "out('CLEARHEADING clear=%.0f x=%.0f y=%.0f' % best)\n")
    m = re.search(r"CLEARHEADING clear=(\d+) x=(-?\d+) y=(-?\d+)", out)
    if m:
        console(client, f"MO.Player.Face {m.group(2)} {m.group(3)}")
        time.sleep(0.5)
    return int(m.group(1)) if m else -1


def _nav_between(host, a_code, b_code):
    """On the host: is each of two actors on the navmesh, and is there a full path from a to b? `a_code` / `b_code` are Python expressions over
    `world` naming the actors. An AI that does not close on a pawn (a wolf "stuck while chasing", a medic that "can't reach") fails for the
    navmesh round them (tiles exist only round navigation invokers and build after a teleport) or for the AI; this tells them apart."""
    out = _probe(host,
        "def on(x):\n"
        "    r = unreal.NavigationSystemV1.project_point_to_navigation(world, x.get_actor_location(), None, None, unreal.Vector(100.0, 100.0, 250.0))\n"
        "    r = r[0] if isinstance(r, tuple) else r\n"
        "    return 0 if (r is None or r is False) else 1\n"
        "def under(x):\n"   # what it stands on: a boulder's top is a navmesh island of its own (N-205)
        "    l = x.get_actor_location()\n"
        "    r = unreal.SystemLibrary.line_trace_single(world, l, l - unreal.Vector(0.0, 0.0, 400.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [x], unreal.DrawDebugTrace.NONE, True)\n"
        "    hr = r[1] if isinstance(r, tuple) else r\n"
        "    t = hr.to_tuple() if hr else None\n"
        "    return ('%s/%s' % (t[9].get_name() if t[9] else '?', t[10].get_name() if t[10] else '?')) if (t and t[0]) else 'nothing within 4 m'\n"
        "try:\n"
        f"    a = {a_code}; b = {b_code}\n"
        "    p = unreal.NavigationSystemV1.find_path_to_actor_synchronously(world, a.get_actor_location(), b, 100.0, a) if (a and b) else None\n"
        "    out('NAVBETWEEN %s onNav=%d -> %s onNav=%d: %.0f m apart, path %s; standing on %s / %s' % (a.get_name() if a else 'none', on(a) if a else 0, b.get_name() if b else 'none', on(b) if b else 0, "
        "(a.get_actor_location() - b.get_actor_location()).length() / 100.0 if (a and b) else -1, "
        "('none' if not p or not p.is_valid() else ('PARTIAL' if p.is_partial() else 'full') + ' %.0f m' % (p.get_path_length() / 100.0)), "
        "under(a) if a else '-', under(b) if b else '-'))\n"
        "except Exception as e:\n"
        "    out('NAVBETWEEN error %s: %s' % (type(e).__name__, str(e)[:200]))\n")
    m = re.search(r"NAVBETWEEN [^\r\n]*", out)
    return m.group(0) if m else "NAVBETWEEN no answer: " + out.strip()[-200:]


def _clear_around_host(host, keep="", within=600):
    """Earlier cases leave people where the next one works: the joiner and a survivor both stood 1.5 m in front of the host, so a medic
    stalled 2 m short and a joiner's line hit the survivor (Oct 20). The host first moves to clear flat ground (_to_clear_ground: rocks round
    the test site took clicks, Oct 24); then the joiner goes 20 m aside, any other person (not `keep`) within `within` cm of the host 12 m away."""
    _to_clear_ground(host, 0)
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); j = unreal.GameplayStatics.get_player_pawn(world, 1); hl = h.get_actor_location(); "
                 "j.set_actor_location(hl + h.get_actor_right_vector() * 2000.0 + unreal.Vector(0.0, 0.0, 200.0), False, True); "
                 f"o = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a not in (h, j) and a.get_name() != '{keep}' "
                 f"and (a.get_actor_location() - hl).length() < {within}]; "
                 "[a.set_actor_location(hl - h.get_actor_right_vector() * 1200.0 + unreal.Vector(0.0, 0.0, 200.0), False, True) for a in o]; out('CLEARED %d' % len(o))")


def _menu_labels(inst):
    """The labels of the MO buttons on screen, for a failed text= click to say what the menu offered instead."""
    out = _probe(inst, "bs = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.MOCommonButton, False); "
                       "out('LABELS %s' % [str(b.get_button_text()) for b in bs if b.is_visible()])")
    m = re.search(r"LABELS [^\r\n]*", out)
    return m.group(0)[:600] if m else "no LABELS line"


def _client_aim_at_host(client):
    """Turn the joiner's camera onto ITS copy of the host's pawn (the other player-owned pawn), chest high, from where its camera really is.
    The host's server position missed (Oct 20: after the host moved the joiner, the client's copies stood elsewhere and the line hit the
    ground 3.7 m off). Repeats until the aim settles."""
    code = ("pc = unreal.GameplayStatics.get_player_controller(world, 0); me = pc.get_controlled_pawn(); "
            "hs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a != me and a.get_editor_property('player_state') is not None]; "
            "h = min(hs, key=lambda a: (a.get_actor_location() - me.get_actor_location()).length()); t = h.get_actor_location() + unreal.Vector(0.0, 0.0, 30.0); "
            "cam = pc.player_camera_manager.get_camera_location(); fwd = pc.player_camera_manager.get_camera_rotation().get_forward_vector(); "
            "miss = ((t - cam) - fwd * (t - cam).dot(fwd)).length(); pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(cam, t)); "
            "out('H_AIM %s dist=%.0f miss=%.1f' % (h.get_name(), (t - cam).length(), miss))")
    m = None
    for _ in range(6):
        m = re.search(r"H_AIM \S+ dist=(-?\d+) miss=(-?[\d.]+)", _probe(client, code))
        if not m or float(m.group(2)) < 4.0:
            break
        time.sleep(0.4)
    time.sleep(0.8)
    return m.group(0) if m else "no H_AIM line"


def _host_wolf_pack(rep, host, client):
    """Wolves hunt and fight as a pack (N-096, Oct 20 2026): a wolf that is hurt, or takes a new quarry, calls the wolves within 15 m
    (AMOPredatorCreature::AlertPack -> AMOCreatureController::NotifyPackThreat). AlertPack existed but nothing called it, and it wrote
    blackboard keys the controller overwrote the next frame. Two wolves 8 m from the host, 4 m apart; the host hurts one; the OTHER turns on
    the host. Host-side; the host is bitten."""
    hoff = log_size(host)
    # The wolves THIS case spawns, by name: the two nearest predators could be an earlier case's (dead, or on someone else) -- N-188
    _probe(host, "import builtins; builtins.MO_PACK_BEFORE = set(a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature)); out('PACK_BEFORE')")
    console(host, "MO.AI.StressSpawn 2 600 900")
    if not rep.step("CONTROL: the host spawned two wolves", bool(wait_log(host, r"\[MO.AI.StressSpawn\] Done", since=hoff, timeout=20)), "StressSpawn 2"):
        return
    soff = log_size(host)   # a wolf that sees the host when it is placed calls the other right away (Oct 20): count calls from here
    setup = _probe(host, "import builtins; h = unreal.GameplayStatics.get_player_pawn(world, 0); hl = h.get_actor_location(); "
                         "[a.destroy_actor() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPreyCreature) if (a.get_actor_location() - hl).length() < 5000]; "
                         "ws = sorted([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature) if a.get_name() not in builtins.MO_PACK_BEFORE], "
                         "key=lambda a: (a.get_actor_location() - hl).length())[:2]; "
                         "[w.set_actor_location(hl + unreal.Vector(800.0, -200.0 + 400.0 * i, 120.0), False, True) for i, w in enumerate(ws)]; "
                         "out('PACK %s' % ' '.join(w.get_name() for w in ws))")
    pack = (re.search(r"PACK (\S+) (\S+)", setup) or [None])
    if not rep.step("CONTROL: two wolves stand together 8 m from the host", pack[0] is not None, setup.strip()[-160:]):
        return
    first, second = pack.group(1), pack.group(2)
    time.sleep(2.0)
    hoff = log_size(host)
    _probe(host, f"h = unreal.GameplayStatics.get_player_pawn(world, 0); w = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature) if a.get_name() == '{first}'][0]; "
                 "out('HURT %s' % w.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.TORSO, 2.0, unreal.MOWoundType.BLUNT, h))")
    pair = f"(?:{re.escape(first)}|{re.escape(second)})"
    # The packmate answers the call (turns on the host), or was on the host already by itself (it saw the host too): either way the pack is
    # together. Before Oct 22 the second was silent, and the step failed on a pack that worked (N-188).
    answered = wait_log(host, rf"\[MOCreatureController\] {pair} (?:answers {pair}'s call: turns on|hears {pair}'s call: already on) \S+", since=soff, timeout=10)
    rep.step("one wolf calls the other against the host (it saw the host, or the host hurt it) and the packmate answers (or is on the host already)",
             bool(answered), answered.group(0) if answered else f"no call answered between {first} and {second}")
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                 "[a.destroy_actor() for c in (unreal.MOPredatorCreature, unreal.MOPreyCreature) for a in unreal.GameplayStatics.get_all_actors_of_class(world, c) "
                 "if (a.get_actor_location() - h.get_actor_location()).length() < 5000]; out('PACK_TIDY')")


def _host_colony_alerts(rep, host, client):
    """Colony alerts, tier 1 (N-095, Oct 20 2026): every player is told when a colonist nobody is playing is attacked by an animal, bleeds
    badly or is dying (UMOColonyManagerSubsystem::RunAlertPass, each upkeep tick; once a minute per colonist and kind). A recruited colonist
    is bitten by a wolf (a deep forearm cut with the wolf as its cause). Needs the settlement _host_colony_medic founds, so it runs after it."""
    off = log_size(host)
    console(host, "MO.Colony.SpawnSurvivor 900")
    m = wait_log(host, r"\[MOQUERY\] COLONY SpawnSurvivor (\S+)", since=off, timeout=15)
    if not rep.step("CONTROL: the host spawned a survivor", bool(m) and m.group(1) != "FAILED", m.group(0) if m else "no SpawnSurvivor line"):
        return
    name = m.group(1)
    console(host, f"MO.Colony.Recruit {name}")
    wait_log(host, r"\[MOQUERY\] COLONY Recruit \S+ possessable=(\d)", since=off, timeout=15)
    console(host, "MO.AI.StressSpawn 1 2000 2500")
    wait_log(host, r"\[MO.AI.StressSpawn\] Done", since=off, timeout=20)
    time.sleep(12.0)   # one upkeep tick sees the colonist unhurt (the wound count it compares with)
    aoff = log_size(host)
    bit = _probe(host, f"c = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{name}'][0]; "
                       "w = min(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOPredatorCreature), key=lambda a: (a.get_actor_location() - c.get_actor_location()).length()); "
                       "out('BITTEN %s by %s' % (c.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.FOREARM_LEFT, 60.0, unreal.MOWoundType.LACERATION, w), w.get_name()))")
    if not rep.step("CONTROL: a wolf's bite cuts the colonist's forearm", "BITTEN True" in bit, (re.search(r"BITTEN[^\r\n]*", bit) or re.search(r"[\s\S]{0,120}$", bit)).group(0)):
        return
    attacked = wait_log(host, r"\[MOColony\] Alert \(Attacked\): [^\r\n]*", since=aoff, timeout=25)
    rep.step("every player is told the colonist is being attacked by a wolf", bool(attacked) and "wolf" in attacked.group(0).lower(),
             attacked.group(0) if attacked else "no Attacked alert")
    bleeding = wait_log(host, r"\[MOColony\] Alert \((?:Bleeding|Dying)\): [^\r\n]*", since=aoff, timeout=25)
    rep.step("... and that it is bleeding badly", bool(bleeding), bleeding.group(0) if bleeding else "no Bleeding alert")
    shown = wait_log(host, r"\[MONotification\] Queued: [^\r\n]*being attacked by a [^\r\n]*", since=aoff, timeout=5)
    rep.step("... on the host's screen (the notification)", bool(shown), shown.group(0) if shown else "no notification line")
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                 "[a.destroy_actor() for c in (unreal.MOPredatorCreature, unreal.MOPreyCreature) for a in unreal.GameplayStatics.get_all_actors_of_class(world, c) "
                 "if (a.get_actor_location() - h.get_actor_location()).length() < 6000]; out('ALERT_TIDY')")


def _client_treats_host(rep, host, client):
    """A joiner dresses the HOST's wound (N-061 co-op, Oct 20 2026): wounds replicate to every machine, and a right-click on a wounded pawn opens
    its treatment menu (AMOCharacter: IMOWorldActionProvider, the same GetOptions as Status > Wounds). The joiner faces the host, right-clicks,
    picks "Apply Bandage"; the host runs the treatment as the joiner's (the healer's) timed action. Control: the joiner's copy sees the wound."""
    hp = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('PAWNNAME=' + p.get_name())")) or [None, "?"])[1]
    jp = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); out('PAWNNAME=' + p.get_name())")) or [None, "?"])[1]
    bleed = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); a = p.get_component_by_class(unreal.MOAnatomyComponent); "
             "ws = [w for w in a.get_all_wounds() if w.body_part == unreal.MOBodyPartType.FOREARM_LEFT]; out('HBLEED %.3f' % (ws[0].bleed_rate if ws else -1))")
    _clear_around_host(host)
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); j = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "h.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.FOREARM_LEFT, 15.0, unreal.MOWoundType.LACERATION, j); "
                 "j.get_component_by_class(unreal.MOInventoryComponent).add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Bandage01', 1); "
                 "j.set_actor_location(h.get_actor_location() + h.get_actor_forward_vector() * 150.0 + unreal.Vector(0.0, 0.0, 20.0), False, True); out('SET')")
    before = re.search(r"HBLEED (-?[\d.]+)", _probe(host, bleed))
    time.sleep(3.0)
    seen = _probe(client, "ps = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a != unreal.GameplayStatics.get_player_pawn(world, 0)]; "
                          "out('CLIENTSEES %s' % [len(p.get_component_by_class(unreal.MOAnatomyComponent).get_all_wounds()) for p in ps])")
    rep.step("CONTROL: the host is cut, and the joiner's copy of the host shows the wound (wounds replicate to all)",
             bool(before) and float(before.group(1)) > 0 and re.search(r"CLIENTSEES \[[^\]]*[1-9]", seen) is not None,
             (before.group(0) if before else "no wound") + " | " + (re.search(r"CLIENTSEES[^\r\n]*", seen) or re.search(r"[\s\S]{0,120}$", seen)).group(0))
    aim = _client_aim_at_host(client)
    coff = log_size(client)
    console(client, "mo.Interact.Debug 1")
    console(client, "MO.Test.Input SecondaryInteract")
    # The client's own name for the host's pawn (actor names differ between machines: BP_..._C_0 on the host can be _C_1 on the joiner)
    seen_as = (re.search(r"H_AIM (\S+)", aim) or [None, hp])[1]
    shown = wait_log(client, rf"\[MOCraftUI\] Showing world action menu for {re.escape(seen_as)}[^\r\n]*", since=coff, timeout=10)
    console(client, "mo.Interact.Debug 0")
    aimed = " // ".join(dict.fromkeys(re.findall(r"\[MOInteractor\] [^\r\n]*", read_log_from(client, coff))))[:300]
    rep.step("client: right-clicking the wounded host opens its treatment menu on the joiner", bool(shown), shown.group(0) if shown else "no world action menu line | " + aim + " | " + (aimed or "no targeting lines"))
    if not shown:
        return
    hoff = log_size(host)
    m = _motest(client, "MO.Test.ClickWidget text=Laceration: Apply Bandage", r"\[MOTEST\] (PASS|FAIL) ClickWidget[^\r\n]*")
    begun = wait_log(host, rf"\[MOTimedAction\] {re.escape(jp)}: MedicalTreatment \(([\d.]+) s\)", since=hoff, timeout=10)
    rep.step("client: its pick runs on the HOST as the joiner's timed treatment", bool(begun), (m.group(0) if m else "no click") + " | " +
             (begun.group(0) if begun else "no MedicalTreatment | " + _menu_labels(client)))
    if begun:
        time.sleep(float(begun.group(1)) + 2.0)
        after = re.search(r"HBLEED (-?[\d.]+)", _probe(host, bleed))
        rep.step("... and the host's cut bleeds less when it is done", bool(after) and 0 <= float(after.group(1)) < float(before.group(1)),
                 f"bleed {before.group(1)} -> {after.group(1) if after else '?'}")
    console(client, "MO.UI.Close")


def _client_lights_from_host(rep, host, client):
    """A joiner lights its torch from the HOST's burning one (N-056 torch from torch, Oct 20 2026): a right-click on a pawn carrying a flame
    offers "Light your Torch from their Torch" when you hold an unlit one (AMOCharacter's world actions, FMOFireActions::GetBearerActions,
    built on the joiner from the replicated flame); the pick runs on the host as the joiner's timed action, and the joiner's torch burns."""
    hp = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('PAWNNAME=' + p.get_name())")) or [None, "?"])[1]
    jp = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); out('PAWNNAME=' + p.get_name())")) or [None, "?"])[1]
    _clear_around_host(host)
    console(host, "MO.Light.Carried give")
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); j = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "inv = j.get_component_by_class(unreal.MOInventoryComponent); g = unreal.GuidLibrary.new_guid(); "
                 "inv.add_item_by_guid_with_durability(g, 'Torch01', 1, -1); "
                 "j.get_component_by_class(unreal.MOEquipmentComponent).equip_from_inventory(inv, g, unreal.MOEquipmentSlot.RIGHT_HAND); "
                 "j.set_actor_location(h.get_actor_location() + h.get_actor_forward_vector() * 150.0 + unreal.Vector(0.0, 0.0, 20.0), False, True); out('SET')")
    time.sleep(3.0)
    lit = _motest(host, "MO.Light.Carried light", r"\[MOQUERY\] LIGHT Carried light: (\d)")
    clit = "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('CLIT %d' % p.get_component_by_class(unreal.MOCarriedLightComponent).is_lit())"
    before = re.search(r"CLIT (\d)", _probe(client, clit))
    if not rep.step("CONTROL: the host's torch burns, the joiner holds an unlit one in front of it", bool(lit) and lit.group(1) == "1" and bool(before) and before.group(1) == "0",
                    (lit.group(0) if lit else "no light line") + " | " + (before.group(0) if before else "no client probe")):
        return
    aim = _client_aim_at_host(client)
    coff = log_size(client)
    console(client, "mo.Interact.Debug 1")
    console(client, "MO.Test.Input SecondaryInteract")
    # The client's own name for the host's pawn (actor names differ between machines: BP_..._C_0 on the host can be _C_1 on the joiner)
    seen_as = (re.search(r"H_AIM (\S+)", aim) or [None, hp])[1]
    shown = wait_log(client, rf"\[MOCraftUI\] Showing world action menu for {re.escape(seen_as)}[^\r\n]*", since=coff, timeout=10)
    console(client, "mo.Interact.Debug 0")
    aimed = " // ".join(dict.fromkeys(re.findall(r"\[MOInteractor\] [^\r\n]*", read_log_from(client, coff))))[:300]
    rep.step("client: right-clicking the torch-bearing host opens its menu on the joiner", bool(shown), shown.group(0) if shown else "no world action menu line | " + aim + " | " + (aimed or "no targeting lines"))
    if shown:
        hoff = log_size(host)
        m = _motest(client, "MO.Test.ClickWidget text=Light your Torch from their Torch", r"\[MOTEST\] (PASS|FAIL) ClickWidget[^\r\n]*")
        caught = wait_log(host, rf"\[MOFire\] {re.escape(jp)} lit its Torch01 from {re.escape(hp)}'s flame", since=hoff, timeout=20)
        rep.step("client: its pick runs on the HOST, and the joiner's torch catches from the host's flame", bool(caught),
                 (m.group(0) if m else "no click") + " | " + (caught.group(0) if caught else "no lit-from line on the host"))
        time.sleep(1.5)
        after = re.search(r"CLIT (\d)", _probe(client, clit))
        rep.step("... and the joiner's own copy shows its torch burning", bool(after) and after.group(1) == "1", after.group(0) if after else "no client probe")
        console(client, "MO.UI.Close")
    # Tidy: both torches back in the packs (a torch that leaves the hand goes out).
    _probe(host, "for i in (0, 1):\n p = unreal.GameplayStatics.get_player_pawn(world, i)\n "
                 "p.get_component_by_class(unreal.MOEquipmentComponent).unequip_to_inventory(unreal.MOEquipmentSlot.RIGHT_HAND, p.get_component_by_class(unreal.MOInventoryComponent))\nout('UNEQUIPPED')")


def _host_treats_survivor(rep, host, client):
    """A player dresses a recruited survivor's wound from its command menu (N-061, Oct 20 2026): a survivor's right-click is its command menu,
    so the menu lists its wounds' treatments after the orders (the same options as a stranger's treatment menu). Host-side."""
    # The joiner steps 20 m aside first: the previous case left it on the very spot in front of the host where the survivor goes, and the
    # host's right-click took the joiner (Oct 20).
    _clear_around_host(host)
    off = log_size(host)
    console(host, "MO.Colony.SpawnSurvivor 250")
    m = wait_log(host, r"\[MOQUERY\] COLONY SpawnSurvivor (\S+)", since=off, timeout=15)
    if not rep.step("CONTROL: the host spawned a survivor", bool(m) and m.group(1) != "FAILED", m.group(0) if m else "no SpawnSurvivor line"):
        return
    name = m.group(1)
    console(host, f"MO.Colony.Recruit {name}")
    wait_log(host, r"\[MOQUERY\] COLONY Recruit \S+ possessable=(\d)", since=off, timeout=15)
    sv = f"sv = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{name}'][0]; "
    bleed = sv + ("ws = [w for w in sv.get_component_by_class(unreal.MOAnatomyComponent).get_all_wounds() if w.body_part == unreal.MOBodyPartType.FOREARM_RIGHT]; "
                  "out('SBLEED %.3f' % (ws[0].bleed_rate if ws else -1))")
    _probe(host, sv + "h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                 "sv.set_actor_location(h.get_actor_location() + h.get_actor_forward_vector() * 150.0 + unreal.Vector(0.0, 0.0, 30.0), False, True); "
                 "sv.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.FOREARM_RIGHT, 15.0, unreal.MOWoundType.LACERATION, h); "
                 "h.get_component_by_class(unreal.MOInventoryComponent).add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Bandage01', 1); "
                 "l = sv.get_actor_location(); out('SVAT x=%.0f y=%.0f z=%.0f' % (l.x, l.y, l.z + 40.0))")
    at = re.search(r"SVAT x=(-?\d+) y=(-?\d+) z=(-?\d+)", _probe(host, sv + "l = sv.get_actor_location(); out('SVAT x=%.0f y=%.0f z=%.0f' % (l.x, l.y, l.z + 40.0))"))
    before = re.search(r"SBLEED (-?[\d.]+)", _probe(host, bleed))
    if not rep.step("CONTROL: the recruited survivor is cut and stands in front of the host", bool(at) and bool(before) and float(before.group(1)) > 0,
                    (before.group(0) if before else "no wound") + (f" at {at.group(0)}" if at else "")):
        return
    time.sleep(2.0)
    console(host, f"MO.Player.Face {at.group(1)} {at.group(2)} {at.group(3)}")
    time.sleep(0.8)
    hoff = log_size(host)
    console(host, "MO.Test.Input RightClick")   # the controller's right-click: the survivor menu is chosen there (SecondaryInteract skips it)
    menu = wait_log(host, rf"\[MOSurvivorContextMenu\] Initialized for survivor {re.escape(name)}[^\r\n]*", since=hoff, timeout=10)
    rep.step("host: right-clicking the survivor opens its command menu", bool(menu), menu.group(0) if menu else "no survivor menu line")
    if not menu:
        return
    time.sleep(1.0)
    hoff = log_size(host)
    console(host, "MO.Test.ClickWidget text=Laceration: Apply Bandage")
    hp = (re.search(r"PAWNNAME=(\S+)", _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('PAWNNAME=' + p.get_name())")) or [None, "?"])[1]
    begun = wait_log(host, rf"\[MOTimedAction\] {re.escape(hp)}: MedicalTreatment \(([\d.]+) s\)", since=hoff, timeout=10)
    rep.step("host: the menu's Apply Bandage starts the host's timed treatment of the survivor", bool(begun),
             begun.group(0) if begun else "no MedicalTreatment on the host | " + _menu_labels(host))
    if begun:
        time.sleep(float(begun.group(1)) + 2.0)
        after = re.search(r"SBLEED (-?[\d.]+)", _probe(host, bleed))
        rep.step("... and when it is done the survivor's cut bleeds less", bool(after) and 0 <= float(after.group(1)) < float(before.group(1)),
                 f"bleed {before.group(1)} -> {after.group(1) if after else '?'}")
    console(host, "MO.UI.Close")


def _host_colony_medic(rep, host, client):
    """A colony villager dresses a bleeding player's wound (N-061 medic job, Oct 20 2026): the colony's upkeep pass (every 10 s) sends the
    nearest idle villager that has the skill and the supplies -- here a bandage in its own pack -- to the bleeding host; it walks over and runs
    the same server treatment a player would (FMOMedicalTreatmentActions::Begin). Founds a settlement, so it runs last. Host-side."""
    _to_clear_ground(host, 0)   # the medic walks 6 m to the host: no boulder between them (Oct 24)
    off = log_size(host)
    # A 10 m settlement, everyone else moved out of it below: the medic treats the worst bleeder in the settlement, and an earlier case left
    # the host's previous pawn bleeding from a wolf's bite (Oct 21: the medic went to it, and the host was never treated)
    console(host, "MO.Colony.Found MedicTest 1000")
    m = wait_log(host, r"\[MOQUERY\] COLONY Found (\S+)", since=off, timeout=10)
    if not rep.step("CONTROL: the host founded a settlement", bool(m) and m.group(1) != "FAILED", m.group(0) if m else "no Found line"):
        return
    console(host, "MO.Colony.SpawnSurvivor 600")
    m = wait_log(host, r"\[MOQUERY\] COLONY SpawnSurvivor (\S+)", since=off, timeout=15)
    if not rep.step("CONTROL: the host spawned a survivor", bool(m) and m.group(1) != "FAILED", m.group(0) if m else "no SpawnSurvivor line"):
        return
    name = m.group(1)
    console(host, f"MO.Colony.Recruit {name}")
    wait_log(host, r"\[MOQUERY\] COLONY Recruit \S+ possessable=(\d)", since=off, timeout=15)
    sv = f"sv = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{name}'][0]; "
    _probe(host, sv + "sv.get_component_by_class(unreal.MOInventoryComponent).add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Bandage01', 1); out('GAVE')")
    bleed = ("h = unreal.GameplayStatics.get_player_pawn(world, 0); "
             "ws = [w for w in h.get_component_by_class(unreal.MOAnatomyComponent).get_all_wounds() if w.body_part == unreal.MOBodyPartType.FOREARM_LEFT]; "
             "out('HBLEED %.3f' % (max(w.bleed_rate for w in ws) if ws else -1))")
    _clear_around_host(host, keep=name, within=1100)
    me = (re.search(r"HOSTPAWN=(\S+)", _probe(host, "out('HOSTPAWN=' + unreal.GameplayStatics.get_player_pawn(world, 0).get_name())")) or [None, "?"])[1]
    off = log_size(host)
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                 "h.get_component_by_class(unreal.MOAnatomyComponent).inflict_damage(unreal.MOBodyPartType.FOREARM_LEFT, 30.0, unreal.MOWoundType.LACERATION, h); out('CUT')")
    before = re.search(r"HBLEED (-?[\d.]+)", _probe(host, bleed))
    if not rep.step("CONTROL: the host's forearm is cut and bleeds at least 0.2 mL/s (the medic threshold)", bool(before) and float(before.group(1)) >= 0.2,
                    before.group(0) if before else "no wound"):
        return
    sent = wait_log(host, rf"\[MOColony\] Medic {re.escape(name)} goes to treat {re.escape(me)}\b[^\r\n]*", since=off, timeout=25)
    rep.step("the colony's upkeep sends the villager with the bandage to the bleeding host", bool(sent), sent.group(0) if sent else "no Medic line for " + name)
    if not sent:
        return
    told = wait_log(host, r"\[MONotification\] Queued: [^\r\n]*is coming to dress your wound[^\r\n]*", since=off, timeout=5)
    rep.step("... and the bleeding host is told help is coming", bool(told), told.group(0) if told else "no notification line")
    done = wait_log(host, r"\[MOSurvivorController\] Treat job (?:done|FAILED|refused|WEDGED)[^\r\n]*", since=off, timeout=90)
    nav = ""
    if not (done and " done:" in done.group(0)):   # a path that failed: the navmesh round them, or the medic (Oct 24: "can't reach", 610 cm off)
        nav = " | " + _nav_between(host, f"[a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{name}'][0]",
                                   "unreal.GameplayStatics.get_player_pawn(world, 0)")
    rep.step("... the villager walks over and finishes the treatment", bool(done) and " done:" in done.group(0), (done.group(0) if done else "no Treat job outcome") + nav)
    after = re.search(r"HBLEED (-?[\d.]+)", _probe(host, bleed))
    rep.step("... and the host's cut bleeds less", bool(after) and 0 <= float(after.group(1)) < float(before.group(1)),
             f"bleed {before.group(1)} -> {after.group(1) if after else '?'}")


def _client_gait(rep, host, client):
    """CLIENT jog / sprint (Steam playtest Oct 16 2026: "the second person can't jog or sprint"). The gait is movement input carried by the saved moves
    (UMOCharacterMovementComponent, FLAG_Custom_0 / _1), so the HOST must see the joiner's pawn move at the gait's speed. It used to be set on the
    pressing machine only: the server simulated a walk and corrected every faster step. Control: the walk first (walk 400 / jog 550 / sprint 700 cm/s)."""
    def host_speed():
        out = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); v = p.get_velocity(); out('HSPEED %.0f' % ((v.x * v.x + v.y * v.y) ** 0.5))")
        m = re.search(r"HSPEED (\d+)", out)
        return int(m.group(1)) if m else -1

    def face_clear_heading():
        # The runs walk straight ahead ~40-65 m each: into a rock they stop and the gait reads 0 (Oct 24: the walk hit one of the new large boulders
        # after ~30 m and the jog and sprint stood pinned against it).
        return _face_clear_heading(host, client)

    def run_and_measure(setup):
        for cmd in setup:
            console(client, cmd)
        face_clear_heading()
        console(client, "MO.Test.MoveFor 0 1 9.0")   # each host probe takes ~1.5 s: 9 s keeps all four samples inside the move (5 s caught only the first, N-184)
        time.sleep(1.5)
        samples = []
        for _ in range(4):
            samples.append(host_speed())
        time.sleep(3.0)  # the move input ends; the pawn stops before the next run
        return max(samples), samples

    # Right after the join the joiner's pawn may still be landing: the walk control read 0 four times when this ran first (N-184). Wait until the
    # host sees it move at all (a short nudge, up to five tries), then measure.
    for _ in range(5):
        console(client, "MO.Test.MoveFor 0 1 1.0")
        time.sleep(0.8)
        if host_speed() > 50:
            break
        time.sleep(1.5)
    time.sleep(2.0)
    walk, ws = run_and_measure([])
    rep.step("CONTROL: the HOST sees the joiner walk at about the walk speed (400)", 250 <= walk <= 460, f"host samples {ws}")
    jog, js = run_and_measure(["MO.Test.Input ToggleJog"])
    rep.step("client jog: the HOST sees the joiner jogging (well above the walk)", jog >= walk + 90, f"host samples {js} (walk max {walk})")
    sprint, ss = run_and_measure(["MO.Test.Input SprintStart"])
    console(client, "MO.Test.Input SprintEnd")
    console(client, "MO.Test.Input ToggleJog")
    rep.step("client sprint: the HOST sees the joiner sprint (faster than the jog)", sprint >= jog + 70, f"host samples {ss} (jog max {jog})")


def _client_inventory_requests(rep, host, client):
    """CLIENT inventory UI (Steam playtest Oct 16 2026: "multiplayer inventory won't allow dropping or anything"). The UI's door is
    FMOInventoryRequests::Submit: on a joiner it sends the change to the server (AMOPlayerController::ServerInventoryRequest). Proof on the HOST
    (the joiner's inventory there). Control: a DIRECT component call from the client -- what the UI used to make -- changes nothing on the host."""
    count = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
             "rs = [inv.try_get_entry_by_guid(g) for g in inv.get_all_item_guids()]; "
             "es = [(r[1] if isinstance(r, tuple) else r) for r in rs]; "   # the engine's Python returns the out entry alone (or a pair)
             "st = [e for e in es if e is not None and str(e.item_definition_id) == 'Stone01']; "
             "out('STONES n=%d stacks=%d' % (sum(e.quantity for e in st), len(st)))")

    def stones():
        m = re.search(r"STONES n=(\d+) stacks=(\d+)", _probe(host, count))
        return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)

    setup = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                         "g = unreal.GuidLibrary.new_guid(); out('INV_SETUP gave=%s' % inv.add_item_by_guid(g, 'Stone01', 6))")
    if not rep.step("host: gave the joiner 6 stones", "gave=True" in setup, setup.strip()[-160:]):
        return
    time.sleep(3)  # the inventory replicates to the client
    n0, s0 = stones()
    direct = _probe(client, "pc = unreal.GameplayStatics.get_player_controller(world, 0); p = pc.get_controlled_pawn(); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                            "en = lambda r: r[1] if isinstance(r, tuple) else r; "
                            "gs = [g for g in inv.get_all_item_guids() if en(inv.try_get_entry_by_guid(g)) is not None and str(en(inv.try_get_entry_by_guid(g)).item_definition_id) == 'Stone01']; "
                            "w = inv.drop_item_by_guid(gs[0], p.get_actor_location() + unreal.Vector(150.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)) if gs else None; "
                            "out('DIRECT dropped=%s' % (w is not None))")
    time.sleep(2)
    rep.step("CONTROL: a direct drop from the client (the old UI path) changes nothing on the host", stones() == (n0, s0), f"{direct.strip()[-80:]}; host stones {stones()} (before {n0}/{s0})")
    console(client, "MO.Test.Inventory Split Stone01")
    time.sleep(3)
    n1, s1 = stones()
    rep.step("client split (through the request): the host now has the stones in one more stack", n1 == n0 and s1 == s0 + 1, f"host stones {n1} in {s1} stacks (before {n0} in {s0})")
    console(client, "MO.Test.Inventory Drop Stone01")
    time.sleep(3)
    n2, _ = stones()
    near = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); pl = p.get_actor_location(); "
                        "items = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if (a.get_actor_location() - pl).length() < 500]; "
                        "out('NEAR %d' % len(items))")
    rep.step("client drop (through the request): the host took stones out of the joiner's inventory and put them on the ground by the joiner",
             0 <= n2 < n1 and "NEAR 0" not in near, f"host stones {n2} (before {n1}); {near.strip()[-40:]}")


def _client_carcass(rep, host, client):
    """Steam playtest Oct 16 2026: "right clicking deer starts butchering immediately instead of giving option to click". A carcass is a world-action
    target (IMOWorldActionProvider): right-click lists its parts on the JOINER (CarcassDefinition replicates), a choice goes to the server
    (AMOPlayerController::ServerWorldAction) and runs on the joiner's pawn's timed action there. Butchering used to run in the client's UI, so a
    joiner's cut never progressed."""
    _to_clear_ground(host, 1)   # the carcass is laid in front of the joiner: not behind a boulder (Oct 24)
    hoff = log_size(host)
    console(host, "MO.Test.Carcass Deer 1")
    laid = wait_log(host, r"\[MOQUERY\] CARCASS (\S+) \(Deer\) at \((-?\d+), (-?\d+), (-?\d+)\)", since=hoff, timeout=10)
    if not rep.step("host: laid a deer carcass in front of the joiner", bool(laid), laid.group(0) if laid else "no CARCASS line"):
        return
    time.sleep(3)  # replicates to the client
    coff = log_size(client)
    console(client, "MO.Test.WorldAction list")
    head = wait_log(client, r"\[MOQUERY\] WORLDACTION \S+ \"([^\"]*)\" options=(\d+) authority=(\d)", since=coff, timeout=10)
    rep.step("the joiner's copy of the carcass lists its parts (the definition replicated)",
             bool(head) and int(head.group(2)) > 0 and head.group(3) == "0" and head.group(1) != "Carcass", head.group(0) if head else "no WORLDACTION line")
    lines = read_log_from(client, coff)
    bones = re.search(r"WORLDACTION   Bones \| [^|]+\| enabled", lines)
    rep.step("the joiner may cut the bones (no tool needed) and is told why a knife part is greyed", bool(bones) and re.search(r"disabled: Needs a tool: Knife", lines) is not None,   # "... Knife (Sharp Stone Flake or better)." since tools are named
             (bones.group(0) if bones else "Bones not enabled") + (" + knife reason" if "Needs a tool: Knife" in lines else " -- no knife reason"))
    count = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
             "out('BONES %d' % inv.count_ingredient('Bone01'))")
    m = re.search(r"BONES (\d+)", _probe(host, count))
    before = int(m.group(1)) if m else -1
    hoff = log_size(host)
    console(client, "MO.Test.WorldAction Bones")
    begun = wait_log(host, r"\[MOWorldAction\] Bones on \S+ by \S+: begun", since=hoff, timeout=10)
    rep.step("the joiner's choice ran ON THE HOST (ServerWorldAction -> the joiner's pawn's timed action)", bool(begun), begun.group(0) if begun else "no begun line on the host")
    time.sleep(12)  # Bones: 10 s
    m = re.search(r"BONES (\d+)", _probe(host, count))
    after = int(m.group(1)) if m else -1
    rep.step("after the 10 s cut the joiner carries 6 more bones (host's view)", after == before + 6, f"bones {before} -> {after}")
    # The right-click itself: the server handles the interaction, the menu must open on the joiner's screen.
    x, y, z = laid.group(2), laid.group(3), laid.group(4)
    console(client, f"MO.Player.Face {x} {y} {z}")
    time.sleep(0.5)
    coff = log_size(client)
    console(client, "MO.Test.Input SecondaryInteract")
    shown = wait_log(client, r"\[MOCraftUI\] Showing world action menu for [^\n]*", since=coff, timeout=10)
    # The carcass lies wherever the joiner stands: a bush in the line takes the click first (Oct 20: "Common Hazel" -- its own harvest menu,
    # correctly). That is the spot, not the carcass path: say so as a CONTROL failure, naming what the click reached.
    other = None if shown else re.search(r"\[MOKeepOnHarvestContextMenu\] Found resource definition: ([^(\r\n]+)", read_log_from(client, coff))
    if not rep.step("CONTROL: the right-click reached the carcass (nothing harvestable in the line)", other is None,
                    f"it reached {other.group(1).strip()} and opened that menu" if other else "nothing else opened"):
        console(client, "MO.UI.Close")
        _probe(host, "[a.destroy_actor() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCarcassActor)]")
        return
    rep.step("the joiner's right-click opens the parts menu ON THE JOINER", bool(shown), shown.group(0) if shown else "no menu line on the client")
    # MO.UI.Close, not Escape: a context menu closes itself when the real cursor is outside the window, and Escape then opened the in-game
    # menu, which held the joiner's input for the next case (the long sprint went 0 m, Oct 20)
    console(client, "MO.UI.Close")
    # Tidy: a carcass left by the joiner would catch the next steps' interaction traces (the host's E on the joiner hit it).
    _probe(host, "[a.destroy_actor() for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCarcassActor)]")


def _client_building_look(rep, host, client):
    """Steam playtest Oct 16 2026: a host saw "a big ass green box" over finished roofs, and an unbuilt stick floor looked finished (players fell
    through it). One look per build state (AMOBuildableActor::RefreshBuildStateVisual): until Complete every mesh wears the planned tint and lets
    pawns through; once built, the building's own materials and BlockAll -- on the JOINER too (replicated state)."""
    hoff = log_size(host)
    console(host, "MO.Test.PlaceBuildingRPC BuildStickFloor 300")
    placed = wait_log(host, r"ServerPlaceBuilding: spawned (\S+) for recipe BuildStickFloor", since=hoff, timeout=10)
    if not rep.step("host: placed a stick floor", bool(placed), placed.group(0) if placed else "no spawn line"):
        return
    name = placed.group(1)
    time.sleep(3)
    look = ("b = [x for x in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor) if x.get_class().get_name().startswith('BP_BuildableFloorBase')]; "
            "b = sorted(b, key=lambda a: (a.get_actor_location() - unreal.GameplayStatics.get_player_pawn(world, 0).get_actor_location()).length() if unreal.GameplayStatics.get_player_pawn(world, 0) else 0)[0] if b else None; "
            "ms = [c for c in b.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible() and c.static_mesh] if b else []; "
            "ghost = sum(1 for c in ms if any(m and 'BuildPreview' in m.get_name() for m in c.get_materials())); "
            "cube = [c for c in b.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Cube'] if b else []; "
            "out('LOOK state=%s visible=%d ghosted=%d cube=%s' % (b.get_build_state() if b else None, len(ms), ghost, cube[0].get_collision_profile_name() if cube else '-'))")
    h, c = _probe(host, look), _probe(client, look)
    hm, cm = re.search(r"LOOK state=.*? visible=(\d+) ghosted=(\d+) cube=(\S+)", h), re.search(r"LOOK state=.*? visible=(\d+) ghosted=(\d+) cube=(\S+)", c)
    rep.step("unbuilt: every visible mesh wears the planned look and pawns pass through, on host AND joiner",
             bool(hm and cm) and hm.group(1) == hm.group(2) != "0" and cm.group(1) == cm.group(2) != "0" and hm.group(3) == cm.group(3) == "IgnoreOnlyPawn",
             f"host {hm.groups() if hm else h.strip()[-80:]} | client {cm.groups() if cm else c.strip()[-80:]}")
    _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                 "inv.add_item_by_guid(unreal.GuidLibrary.new_guid(), 'Stick01', 20)")
    console(host, "MO.Test.Build add")
    time.sleep(2)
    console(host, "MO.Test.Build start")
    time.sleep(9)  # 5 s build
    h, c = _probe(host, look), _probe(client, look)
    hm, cm = re.search(r"LOOK state=.*? visible=(\d+) ghosted=(\d+) cube=(\S+)", h), re.search(r"LOOK state=.*? visible=(\d+) ghosted=(\d+) cube=(\S+)", c)
    rep.step("built: its own materials back and solid (BlockAll), on host AND joiner",
             bool(hm and cm) and hm.group(2) == cm.group(2) == "0" and hm.group(3) == cm.group(3) == "BlockAll",
             f"host {hm.groups() if hm else h.strip()[-80:]} | client {cm.groups() if cm else c.strip()[-80:]}")


def _client_lake_clock(rep, host, client):
    """Steam playtest Oct 16 2026 (#12): "(joiner) lake water frozen in place". Every machine spawns its own upland lakes, and Oceanology never
    registered a spawned lake's wave solver, so the lake wave clock (MPC_Waves.GameTimeInSeconds_Lake) stood at 0. The host walks to a lake and
    brings the joiner; the JOINER's clock must advance. Both go back where they were afterwards: the cases after it put a wolf 5 m from the
    joiner and a medic beside the host, which at the lake landed in the water (Oct 20: no bite, a medic stalled 7 m short)."""
    _probe(host, "import builtins; builtins.MO_LAKE_HOME = [unreal.GameplayStatics.get_player_pawn(world, i).get_actor_location() for i in (0, 1)]; out('LAKE_HOME')")
    try:
        _client_lake_clock_at_lake(rep, host, client)
    finally:
        _probe(host, "import builtins; hs = getattr(builtins, 'MO_LAKE_HOME', None); "
                     "hs and [unreal.GameplayStatics.get_player_pawn(world, i).set_actor_location(hs[i] + unreal.Vector(0.0, 0.0, 60.0), False, True) for i in (0, 1)]; out('LAKE_BACK')")
        time.sleep(8)   # the ground there is built already; let both machines settle the pawns on it


def _client_lake_clock_at_lake(rep, host, client):
    off = log_size(host)
    console(host, "MO.Water.GoToLake 0 dry")
    lake = wait_log(host, r"GoToLake rank \d+: lake at \((-?\d+), (-?\d+)\)[^\n]*", since=off, timeout=10)
    if not rep.step("host: went to the nearest upland lake", bool(lake), lake.group(0) if lake else "no lake within 6 km of the host"):
        return
    time.sleep(3)
    _probe(host, "h = unreal.GameplayStatics.get_player_pawn(world, 0); j = unreal.GameplayStatics.get_player_pawn(world, 1); "
                 "j.set_actor_location(h.get_actor_location() + unreal.Vector(250.0, 0.0, 100.0), False, True)")
    time.sleep(25)   # the joiner's own lakes spawn around it
    clock = ("mpc = unreal.load_asset('/Oceanology_Plugin/Design/Ocean/MaterialCollections/MPC_Waves'); "
             "lakes = len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OceanologyLake)); "
             "out('LAKECLOCK %.3f lakes=%d' % (unreal.MaterialLibrary.get_scalar_parameter_value(world, mpc, 'GameTimeInSeconds_Lake'), lakes))")
    a = re.search(r"LAKECLOCK (-?[\d.]+) lakes=(\d+)", _probe(client, clock))
    time.sleep(3)
    b = re.search(r"LAKECLOCK (-?[\d.]+) lakes=(\d+)", _probe(client, clock))
    ok = bool(a and b) and int(b.group(2)) > 0 and float(b.group(1)) > float(a.group(1)) + 1.0
    rep.step("the JOINER's lake wave clock advances (its own lakes' water moves)", ok,
             f"client clock {a.group(1) if a else '?'} -> {b.group(1) if b else '?'} over 3 s, {b.group(2) if b else '?'} lake(s) on the client")


def _host_takes_departed_pawn(rep, host, client):
    """Steam playtest Oct 16 2026 (#8): "can't select other pawn from possess menu" -- the host alone after the joiner left. The joiner's survivor
    stays in the world (pawns survive their player); the host's possession list must offer it and taking it must work. ENDS THE CLIENT: run last."""
    info = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); i = p.get_component_by_class(unreal.MOIdentityComponent) if p else None; "
                        "out('JOINERPAWN %s %s' % (p.get_name(), unreal.GuidLibrary.conv_guid_to_string(i.get_guid())) if i else 'JOINERPAWN none none')")
    m = re.search(r"JOINERPAWN (\S+) ([0-9A-Fa-f]{32})", info)
    if not rep.step("host: the joiner's pawn and its GUID", bool(m), info.strip()[-120:]):
        return
    pawn_name, guid = m.group(1), m.group(2)
    stop(client)
    time.sleep(8)
    state = _probe(host, f"ps = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{pawn_name}']; "
                         "p = ps[0] if ps else None; c = p.get_controller() if p else None; r = p.get_component_by_class(unreal.MORecruitmentComponent) if p else None; "
                         "out('LEFTPAWN exists=%s controller=%s possessable=%s' % (p is not None, c.get_class().get_name() if c else None, r.is_possessable() if r else None))")
    rep.step("after the joiner left, its survivor stays in the world with no player in it",
             "exists=True" in state and "PlayerController" not in state, state.strip()[-160:])
    # The menu itself, as Wes opened it (P): its own count of the survivors this player can take.
    hoff = log_size(host)
    console(host, "MO.UI.Open Possession")   # what P does (UIManager TogglePossessionMenu); injected keys do not reach Enhanced Input here
    pop = wait_log(host, r"\[MOPossessionMenu\] Populated with (\d+) of your / free living, (\d+) other players', (\d+) deceased", since=hoff, timeout=10)
    rep.step("host's possession MENU lists the survivor left behind as one it can take (2 = its own + the left one)",
             bool(pop) and int(pop.group(1)) >= 2 and pop.group(2) == "0", pop.group(0) if pop else "the menu did not populate")
    console(host, "MO.Test.PressKey Escape")
    # Something happens to the survivor while nobody plays it (its history: a job it finished, here written by hand), so taking it back has news (N-091)
    _probe(host, f"p = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if a.get_name() == '{pawn_name}'][0]; "
                 "p.get_component_by_class(unreal.MOCharacterHistoryComponent).add_entry(unreal.HistoryEntryType.TASK_COMPLETE, 'Finished: Gather Sticks'); out('NEWS')")
    time.sleep(1)
    hoff = log_size(host)
    console(host, f"MO.Possess.Take {guid}")
    time.sleep(5)
    now = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 0); out('HOSTPAWN %s' % (p.get_name() if p else 'none'))")
    rep.step("the host took the survivor the joiner left behind", f"HOSTPAWN {pawn_name}" in now, now.strip()[-80:] + f" (wanted {pawn_name})")
    away = re.search(r"\[MOHistory\] \S+: back under \S+ -- ([^\r\n]*)", read_log_from(host, hoff))
    rep.step("... and is told what it did while nobody played it (\"While you were away\", N-091)",
             bool(away) and away.group(1).startswith("While you were away") and "Finished: Gather Sticks" in away.group(1),
             away.group(0)[:200] if away else "no [MOHistory] back-under line on the host")


def _dropped_item_falls_on_client(rep, host, client):
    """Steam playtest Oct 16 2026: "dropped items don't show as dropping on client, they hang in the air". The host drops an item by the joiner with
    the drop physics (lifted 50-100 cm, falls, settles -- server-side); the joiner's copy must rest where the host's does (movement replication)."""
    drop = _probe(host, "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
                        "g = unreal.GuidLibrary.new_guid(); inv.add_item_by_guid_no_stack(g, 'FlintKnife01', 1); "   # its own stack: a merged stone kept no guid of its own
                        "w = inv.drop_item_by_guid(g, p.get_actor_location() + unreal.Vector(-170.0, 90.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)); "
                        "w.enable_drop_physics() if w else None; out('DROPPED %s' % (w.get_name() if w else 'none'))")
    m = re.search(r"DROPPED (\S+)", drop)
    if not rep.step("host: dropped a stone by the joiner with the drop physics", bool(m) and m.group(1) != "none", drop.strip()[-120:]):
        return
    time.sleep(6)  # the fall and the settle
    where = ("import builtins; items = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem), "
             "key=lambda a: (a.get_actor_location() - %s.get_actor_location()).length()); l = items[0].get_actor_location() if items else None; "
             "out('ITEMZ %%.0f %%.0f %%.0f' %% (l.x, l.y, l.z) if l else 'ITEMZ none')")
    h = re.search(r"ITEMZ (-?\d+) (-?\d+) (-?\d+)", _probe(host, where % "unreal.GameplayStatics.get_player_pawn(world, 1)"))
    c = re.search(r"ITEMZ (-?\d+) (-?\d+) (-?\d+)", _probe(client, where % "unreal.GameplayStatics.get_player_controller(world, 0).get_controlled_pawn()"))
    ok = bool(h and c) and all(abs(int(h.group(i)) - int(c.group(i))) <= 25 for i in (1, 2, 3))
    rep.step("the joiner sees the dropped stone where the host's came to rest (within 25 cm)", ok,
             f"host {h.groups() if h else None} client {c.groups() if c else None}")


def _host_e_on_joiner(rep, host, client):
    """Steam playtest Oct 16 2026: "if the host interacts with another player's character it kicks the player out of the pawn and you can't possess it
    back". The host stands by the joiner, looks at it and presses E. Proof: the joiner's controller still drives the same pawn, on BOTH machines."""
    who = ("pc = unreal.GameplayStatics.get_player_controller(world, 1); p = pc.get_controlled_pawn() if pc else None; "
           "out('JOINER pawn=%s' % (p.get_name() if p else 'none'))")
    before = re.search(r"JOINER pawn=(\S+)", _probe(host, who))
    if not rep.step("host: the joiner drives a pawn", bool(before) and before.group(1) != "none", before.group(0) if before else "no joiner controller"):
        return
    place = _probe(host, "j = unreal.GameplayStatics.get_player_pawn(world, 1); h = unreal.GameplayStatics.get_player_pawn(world, 0); pc = unreal.GameplayStatics.get_player_controller(world, 0); "
                         "jl = j.get_actor_location(); h.set_actor_location(jl + unreal.Vector(160.0, 0.0, 30.0), False, True); "
                         "pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pc.player_camera_manager.get_camera_location(), jl)); out('PLACED')")
    rep.step("host: stands 1.6 m from the joiner, looking at it", "PLACED" in place, place.strip()[-120:])
    time.sleep(1.5)
    # What can E hit? The interactor traces the Visibility channel from the camera: if the joiner's character does not block it, E cannot target
    # another player at all and the step below proves nothing -- say so instead of passing vacuously.
    hit = _probe(host, "pc = unreal.GameplayStatics.get_player_controller(world, 0); cam = pc.player_camera_manager.get_camera_location(); "
                       "j = unreal.GameplayStatics.get_player_pawn(world, 1); h = unreal.GameplayStatics.get_player_pawn(world, 0); "
                       "r = unreal.SystemLibrary.line_trace_single(world, cam, j.get_actor_location(), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [h], unreal.DrawDebugTrace.NONE, True); "
                       "hr = r[1] if isinstance(r, tuple) else r; a = hr.to_tuple()[9] if hr else None; "
                       "out('ETRACE hit=%s' % (a.get_name() if a else 'none'))")
    mh = re.search(r"ETRACE hit=(\S+)", hit)
    rep.step("diagnostic: what the host's Visibility trace toward the joiner hits", True, mh.group(0) if mh else hit.strip()[-160:])
    off = log_size(host)
    console(host, "MO.Test.Input Interact")
    time.sleep(4)
    after = re.search(r"JOINER pawn=(\S+)", _probe(host, who))
    client_side = _probe(client, "pc = unreal.GameplayStatics.get_player_controller(world, 0); p = pc.get_controlled_pawn(); out('MINE pawn=%s' % (p.get_name() if p else 'none'))")
    trail = " | ".join(re.findall(r"\[(?:MOInteract|MOInteractable|Recruitment|MOCharacter|MOPossess\w*)\][^\n]{0,140}", read_log_from(host, off))[:6])
    rep.step("host E on the joiner: the joiner still drives the same pawn (host side)", bool(after) and after.group(1) == before.group(1),
             f"before {before.group(1)}, after {after.group(1) if after else '?'}; host log: {trail or 'nothing'}")
    rep.step("host E on the joiner: the client still controls a pawn", "MINE pawn=none" not in client_side and "MINE pawn=" in client_side, client_side.strip()[-80:])
    # The right mouse button: on the HOST a recruited character's survivor menu used to be built for the joiner's character, which gave it a survivor
    # AI and threw the joiner out (the actual #4). The controller's own handler, as the mouse runs it.
    off = log_size(host)
    console(host, "MO.Test.Input RightClick")
    time.sleep(4)
    console(host, "MO.UI.Close")   # not Escape: the context menu may have closed itself, and Escape would open the in-game menu
    after_rc = re.search(r"JOINER pawn=(\S+)", _probe(host, who))
    client_rc = _probe(client, "pc = unreal.GameplayStatics.get_player_controller(world, 0); p = pc.get_controlled_pawn(); out('MINE pawn=%s' % (p.get_name() if p else 'none'))")
    host_log = read_log_from(host, off)
    menu = "Opening survivor context menu for" in host_log
    rep.step("host RIGHT-CLICK on the joiner: no survivor menu, the joiner still drives the same pawn (both machines)",
             not menu and bool(after_rc) and after_rc.group(1) == before.group(1) and "MINE pawn=none" not in client_rc and "MINE pawn=" in client_rc,
             f"survivor menu opened: {menu}; host sees {after_rc.group(1) if after_rc else '?'}; client {client_rc.strip()[-40:]}")


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


def _remote_pawn_velocity(rep, host, client):
    """N-160: a remote player's pawn read velocity 0 on every other machine. Sim gravity was switched off on EVERY proxy (the fall-through fix below), and the
    engine's SimulateMovement reads that as "stuck": velocity zeroed each tick, no extrapolation between updates. Since Oct 19 only a proxy with no floor under it
    on this machine has it off. The HOST walks; the JOINER reads its copy of the host's pawn (MO.Test.RemotePawns). CONTROL: standing, it reads ~0."""
    def sample():
        off = log_size(client)
        console(client, "MO.Test.RemotePawns")
        m = wait_log(client, r"REMOTE_PAWN name=\S+ role=\S+ speed=(\d+) simGravityOff=(\d) mode=(\S+)", since=off, timeout=5)
        return (int(m.group(1)), int(m.group(2)), m.group(3)) if m else None

    still = sample()
    rep.step("CONTROL: the joiner reads the standing host's pawn at about 0 cm/s", bool(still) and still[0] < 30, str(still))
    console(host, "MO.Test.MoveFor 0 1 6.0")
    time.sleep(1.5)
    walking = [sample() for _ in range(3)]
    seen = [w for w in walking if w]
    rep.step("joiner: the walking host's pawn reads a walking speed (not 0), with sim gravity on",
             bool(seen) and max(w[0] for w in seen) >= 150 and all(w[1] == 0 for w in seen), str(walking))
    time.sleep(4)   # the walk ends before the next case


def _remote_pawn_stays_put(rep, host, client):
    """The bug Wes reported ("the client can't see the host's pawn"), second half: a CLIENT draws the host's pawn as a simulated proxy; with no floor under it the engine predicts a fall,
    and a pawn that stands still gets no position updates to correct that -- it fell at terminal velocity for ever (z=-498790 after a minute). Gravity is never simulated on a
    proxy with no floor under it on the client (Oct 19, N-160: one WITH a floor simulates it, so it keeps its velocity -- _remote_pawn_velocity).
    No local floor is made by switching the client's voxel collision off. CONTROL: MO.RemotePawn.SimGravity 1 (the stock engine behaviour) must make the copy fall, so the check can fail."""
    _to_clear_ground(host, 0)   # a rock under the host holds the copy up with the voxels off (Oct 24: the larger rock tiers by the test site)
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
        under = ""
        if not (bool(fell) and fell[0] < hz - 1500):   # with the voxels off, what still holds it up (a rock mesh, a building)?
            u = _probe(client,
                "me = unreal.GameplayStatics.get_player_pawn(world, 0); "
                f"near = [c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter) if (me is None or c.get_name() != me.get_name()) and abs(c.get_actor_location().x - {hx}) < 200 and abs(c.get_actor_location().y - {hy}) < 200]; "
                "l = near[0].get_actor_location() if near else None; "
                "h = unreal.SystemLibrary.line_trace_single_for_objects(world, l, l - unreal.Vector(0.0, 0.0, 400.0), [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY4], "
                "False, near[:1], unreal.DrawDebugTrace.NONE, True) if l else None; t = h.to_tuple() if h is not None else None; "
                "out('UNDER %s' % (('%s/%s %.0f cm below' % (t[9].get_name() if t[9] else None, t[10].get_name() if t[10] else None, t[3])) if t else 'nothing'))")
            um = re.search(r"UNDER [^\n]*", u)
            under = " | " + (um.group(0) if um else u.strip()[-160:])
        rep.step("CONTROL: with the stock engine behaviour and no floor under it, the client's copy of the host's pawn FALLS",
                 bool(fell) and fell[0] < hz - 1500, f"host z={hz:.0f}; client's copy {fell}{under}")
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


def _client_build_from_ground(rep, host, client):
    """Add materials draws from LOOSE items on the ground (the Surrounding Area source), not just pockets. The building's source search
    (UMOBuildProgressComponent::FindMaterialSources) is an object-type overlap; a dropped item's mesh is a PhysicsBody, and until Oct 16 2026 only
    the item's floating interaction sphere (WorldDynamic, now removed -- MOWorldItem.h pitfall 3) made a dropped item findable. 10 cobbles are
    dropped beside a joiner's campfire ghost from the joiner's own inventory (so none are left in the pockets); the joiner's Add materials must
    take them off the ground. Proof is read on the host."""
    inv = "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
    have = lambda: int((re.search(r"COBBLES=(-?\d+)", _probe(host, inv + "out('COBBLES=%d' % inv.get_item_count_by_definition_id('RiverCobble01'))")) or re.search("(-1)", "-1")).group(1))
    ground = ("items = [w for w in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if str(w.get_item_component().get_editor_property('item_definition_id')) == 'RiverCobble01' "
              "and not w.get_editor_property('hidden') and (w.get_actor_location() - unreal.GameplayStatics.get_player_pawn(world, 1).get_actor_location()).length() < 1500]; ")
    # Flat clear ground for the ghost and the cobbles round it: on a slope or by a stone they rolled out of build range (Oct 24: 8 of 10 in range).
    _to_clear_ground(host, 1)
    # A fresh ghost nearer than any earlier building (MO.Test.Build acts on the nearest one).
    base = _count_buildables(host)
    console(client, "MO.Test.PlaceBuildingRPC BuildCampfire 120")
    for _ in range(10):
        time.sleep(1.0)
        if (_count_buildables(host) or 0) > (base or 0):
            break
    time.sleep(3)
    _host_beside_joiner_building(host)
    hs = _build_state(host)
    if not rep.step("ground build: a fresh campfire ghost from the joiner, nothing deposited yet", bool(hs) and hs["state"] == 0 and hs["deposited"] == "-",
                    hs and hs["line"] or "no build state on the host"):
        return
    pocket = have()
    if pocket > 0:   # empty the pockets first: every cobble must come from the ground
        _probe(host, inv + "[inv.remove_item_by_definition_id('RiverCobble01', %d)]; out('EMPTIED')" % pocket)
    drop = _probe(host, inv +
        "b = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor), key=lambda a: (a.get_actor_location() - p.get_actor_location()).length())[0]; "
        # one cobble per drop: an add is capped at the stack size (10 asked, 5 arrived)
        "import math; "
        "ws = [inv.drop_item_by_guid(g, b.get_actor_location() + unreal.Vector(60.0 * math.cos(i * 0.6283), 60.0 * math.sin(i * 0.6283), 15.0), unreal.Rotator(0.0, 0.0, 0.0)) "
        "for i, g in enumerate([unreal.GuidLibrary.new_guid() for _ in range(10)]) if inv.add_item_by_guid_no_stack(g, 'RiverCobble01', 1)]; "
        "out('GDROP items=%d' % len([w for w in ws if w]))")
    time.sleep(6)   # the drop falls and settles
    seen = _probe(host, ground + "b = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor), "
                                 "key=lambda a: (a.get_actor_location() - unreal.GameplayStatics.get_player_pawn(world, 1).get_actor_location()).length())[0]; "
                                 "near = [w for w in items if (w.get_actor_location() - b.get_actor_location()).length() < 190.0]; "
                                 "out('GROUND n=%d qty=%d types=%s inrange=%d' % (len(items), sum(w.get_item_component().get_editor_property('quantity') for w in items), "
                                 "sorted(set(str(w.get_item_mesh().get_collision_object_type()) for w in items)), sum(w.get_item_component().get_editor_property('quantity') for w in near)))")
    gm = re.search(r"GROUND n=(\d+) qty=(\d+) types=(.*?) inrange=(\d+)", seen)
    left = have()
    # In range = within the campfire's 200 cm build range (BuildCampfire.BuildRange): a cobble that rolled further is the drop's physics, not the gather.
    if not rep.step("ground build: 10 cobbles lie on the ground within the ghost's build range (a dropped item: PhysicsBody mesh) and none are in the joiner's pockets",
                    bool(gm) and int(gm.group(4)) >= 10 and "PHYSICS_BODY" in gm.group(3).upper() and left == 0,
                    (gm.group(0) if gm else " ".join(re.findall(r"GDROP[^\n]*", drop)) or drop.strip()[-200:]) + f" | pockets {left}"):
        return
    hoff = log_size(host)
    console(client, "MO.Test.Build add")
    got = wait_log(host, r"\[MOBuildProgressComponent\] GatherMissingMaterials: (\d+) unit[^\n]*", since=hoff, timeout=15)
    time.sleep(2)
    hs = _build_state(host)
    after = _probe(host, ground + "out('GROUND n=%d qty=%d' % (len(items), sum(w.get_item_component().get_editor_property('quantity') for w in items)))")
    am = re.search(r"GROUND n=(\d+) qty=(\d+)", after)
    rep.step("ground build: the joiner's Add materials took the 10 cobbles OFF THE GROUND into the building (host ledger), the pockets stayed empty",
             bool(got) and bool(hs) and "RiverCobble01:10" in hs["deposited"] and bool(am) and int(am.group(2)) <= int(gm.group(2)) - 10 and have() == 0,
             f"{got.group(0)[:110] if got else 'no GatherMissingMaterials line on the host'} | ledger {hs and hs['deposited']} | ground {gm.group(2)} -> {am.group(2) if am else '?'}")
    console(client, "MO.Test.Build cancel")   # leaves no ghost for later steps (the refund goes to the joiner)
    time.sleep(2)


def _count_buildables(host):
    out = _probe(host, "out('BUILDABLES=%d' % len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor)))")
    m = re.search(r"BUILDABLES=(\d+)", out)
    return int(m.group(1)) if m else None


def _trust_placement(rep, host, client):
    """Audit H19: ServerPlaceBuilding trusted the client's transform, so a hostile client could place buildings anywhere. The client
    sends the same RPC twice, near (what the placement UI sends) and 200 m away (what a hostile client sends). With the server reach check
    switched OFF the far request must succeed (the control: the refusal below is the check's doing); with it on, only the near one does.
    Every step counts the buildings at the REQUESTED SPOT (within 15 m), never the world's total: world features spawn around the players
    while the test runs, and the total moved by itself in two runs (Oct 14: +2 for one placement, and a change across a refusal)."""
    probe = ("p = unreal.GameplayStatics.get_player_pawn(world, 1); l = p.get_actor_location(); f = p.get_actor_forward_vector(); "
             "out('SPOT x=%f y=%f fx=%f fy=%f' % (l.x, l.y, f.x, f.y))")
    m = re.search(r"SPOT x=(-?[\d.]+) y=(-?[\d.]+) fx=(-?[\d.]+) fy=(-?[\d.]+)", _probe(host, probe))
    if not rep.step("host: located the joiner for the placement test", bool(m), m.group(0) if m else "no SPOT line"):
        return
    x, y, fx, fy = (float(v) for v in m.groups())
    near_spot, far_spot = (x + fx * 300.0, y + fy * 300.0), (x + fx * 20000.0, y + fy * 20000.0)
    at = lambda spot: _buildables_near(host, spot[0], spot[1], 1500.0)

    def send(forward_cm, recipe="auto"):
        off = log_size(host)
        console(client, f"MO.Test.PlaceBuildingRPC {recipe} {forward_cm}")
        time.sleep(6)  # next tick + RPC + the host spawning it
        return read_log_from(host, off)

    before = at(near_spot)
    log = send(300)
    after = at(near_spot)
    rep.step("trust: a placement 3 m in front of the pawn (the UI's own request) IS accepted", before is not None and after == before + 1 and "ServerPlaceBuilding rejected" not in log,
             f"buildings at the spot {before} -> {after}")

    console(host, "MO.Building.ServerReach.Disable 1")
    before = at(far_spot)
    send(20000)
    after = at(far_spot)
    console(host, "MO.Building.ServerReach.Disable 0")
    rep.step("CONTROL: with the server reach check disabled a placement 200 m away IS accepted (a hostile client could do this)",
             before is not None and after == before + 1, f"buildings at the far spot {before} -> {after}")

    before = after
    log = send(20000)
    after = at(far_spot)
    refused = re.search(r"ServerPlaceBuilding rejected: [^\n]*", log)
    rep.step("trust: a placement 200 m away is REFUSED by the server (and nothing new stands at the requested spot)", bool(refused) and after == before,
             (refused.group(0)[:120] if refused else "no 'ServerPlaceBuilding rejected' line") + f"; buildings at the far spot {before} -> {after}")

    # N-171 (Oct 20 2026): a building the joiner has not discovered -- one its build menu does not list -- is refused even within reach.
    before = at(near_spot)
    coff = log_size(client)
    log = send(300, "undiscovered")
    sent = re.search(r"\[MOTEST\] PlaceBuildingRPC: sent recipe=(\S+)", read_log_from(client, coff))
    after = at(near_spot)
    refused = re.search(r"ServerPlaceBuilding rejected: \S+ has not discovered \S+", log)
    if sent and sent.group(1) == "None":
        # No building in the live data needs discovering (Oct 20): nothing to send. The rule itself is
        # MOFramework.Building.PlacementOnlyWhatTheMenuLists; this step counts once a building is discovery-gated.
        print("[SKIP] trust: an undiscovered building -- none in the live data needs discovering (unit test covers the rule)", flush=True)
    else:
        rep.step("trust: a building the joiner has not discovered is REFUSED by the server, even within reach", bool(refused) and after == before,
                 f"sent {sent.group(1) if sent else '?'}: " + (refused.group(0) if refused else "no 'has not discovered' line") + f"; buildings at the spot {before} -> {after}")


def _buildables_near(host, x, y, radius):
    out = _probe(host, "c = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor) "
                       f"if ((a.get_actor_location().x - {x}) ** 2 + (a.get_actor_location().y - {y}) ** 2) ** 0.5 < {radius}]; out('NEAR=%d' % len(c))")
    m = re.search(r"NEAR=(\d+)", out)
    return int(m.group(1)) if m else None


def _trust_pickup_far(rep, host, client):
    """The server holds ServerPickUpWorldItem to a reach (3000 cm). The host drops a stick 45 m from the joiner (inside network relevancy, outside
    the reach); the client asks for the nearest world item at least 35 m away."""
    _joiner_make_room(host)
    setup = _probe(host,
        "p = unreal.GameplayStatics.get_player_pawn(world, 1); inv = p.get_component_by_class(unreal.MOInventoryComponent); "
        "g = unreal.GuidLibrary.new_guid(); gave = inv.add_item_by_guid_no_stack(g, 'Stone01', 1); "
        # its OWN stack: an added item merges into one the joiner already holds (sticks, and stones since _client_inventory_requests), so its GUID would not exist to drop.
        # The drop itself is only accepted near the owner (a good thing): drop beside the pawn, then carry the world item 45 m away.
        "w = inv.drop_item_by_guid(g, p.get_actor_location() + unreal.Vector(120.0, 0.0, 10.0), unreal.Rotator(0.0, 0.0, 0.0)); "
        # 45 m out over DRY ground, set on it: straight along +X from a joiner on the coast it fell into the sea and the joiner's world had nothing to ask for (Oct 24)
        "import math; o = p.get_actor_location(); ign = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCharacter)); far = None\n"
        "for k in range(8):\n"
        "    a = k * math.pi / 4.0; x, y = o.x + 4500.0 * math.cos(a), o.y + 4500.0 * math.sin(a)\n"
        "    r = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, o.z + 5000.0), unreal.Vector(x, y, o.z - 5000.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ign, unreal.DrawDebugTrace.NONE, True)\n"
        "    hr = r[1] if isinstance(r, tuple) else r; t = hr.to_tuple() if hr else None\n"
        "    if t and t[0] and t[5].z > 50.0:\n"
        "        far = unreal.Vector(x, y, t[5].z + 15.0); break\n"
        "w and w.set_actor_location(far if far else o + unreal.Vector(4500.0, 0.0, 10.0), False, True)\n"
        "out('FARDROP gave=%s item=%s' % (gave, w.get_name() if w else 'none'))")
    m = re.search(r"FARDROP gave=(\w+) item=(\S+)", setup)
    if not rep.step("host: dropped an item 45 m from the joiner (out of the server's 30 m reach)", bool(m) and m.group(1) == "True" and m.group(2) != "none",
                    m.group(0) if m else setup.strip()[-160:]):
        return
    # Wait until the JOINER's world has it (up to 30 s): a fixed 8 s missed it when the host was stalled loading (Oct 21: the joiner received
    # ~260 B/s with a 637 ms round trip for those seconds, and the step blamed an empty world).
    far_code = ("p = unreal.GameplayStatics.get_player_pawn(world, 0); pl = p.get_actor_location(); "
                "out('FARITEM %d' % len([a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) "
                "if 3500 <= (a.get_actor_location() - pl).length() <= 12000]))")
    for _ in range(15):
        if re.search(r"FARITEM [1-9]", _probe(client, far_code)):
            break
        time.sleep(2)
    off = log_size(host)
    coff = log_size(client)   # the client's log segment since the command: a fixed "last 4000 chars" tail was flooded out by [MOBiomeSpawner] lines while the client's PCG ran, and the verdict flipped
    console(client, "MO.Test.PickupNearest 12000 3500")
    time.sleep(6)
    log = read_log_from(host, off)
    refused = re.search(r"\[MOInteract\] ServerPickUpWorldItem: \S+ out of reach", log)
    took = re.search(r"\[MOInteract\] ServerPickUpWorldItem: \S+ -> picked up", log)
    cl = read_log_from(client, coff)
    asked = re.search(r"\[MOTEST\] PickupNearest: interacting with \S+ at (\d+)uu", cl)
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


def _possession_rows_name_players(rep, host, client):
    """The possession menu's rows say who is in each survivor (EMOPawnOccupancy, Oct 15 2026): the joiner's own is "you", and the host's is
    LISTED with the host player's name (their Steam name on Steam) instead of being left out. Read from the rows the CLIENT received."""
    names = _probe(host, "pcs = [unreal.GameplayStatics.get_player_controller(world, i) for i in range(2)]; "
                         "out('NAMES ' + '|'.join(p.player_state.get_player_name() for p in pcs if p and p.player_state))")
    m = re.search(r"NAMES ([^|\n]+)\|([^\n]+)", names)
    if not rep.step("host: both players have a name", bool(m), names.strip()[-160:]):
        return
    host_name = m.group(1).strip()
    off = log_size(client)
    console(client, "MO.Possess.List")
    wait_log(client, r"possession row", since=off, timeout=20)
    time.sleep(1.5)
    rows = re.findall(r"possession row '([^']*)' (you|free|player=)([^\n]*)", read_log_from(client, off))
    you = [r for r in rows if r[1] == "you"]
    others = [r[2].strip() for r in rows if r[1] == "player="]
    rep.step("client: its own survivor is the one marked as current", len(you) == 1, f"{len(rows)} row(s): {rows}"[:220])
    rep.step(f"client: the host's survivor is listed, played by '{host_name}'", host_name in others, f"other players' rows: {others}")


def _trust_dev_spawn(rep, host, client):
    """C7: the possession component's dev spawn RPC carries a class the CLIENT chooses. The server honours it for its own player only:
    the host's request spawns a TargetPoint (control), the joiner's is refused and spawns nothing."""
    off = log_size(host)
    console(host, "MO.Test.DevSpawnRPC")
    time.sleep(4)
    spawned = re.search(r"\[MOPossess\] Spawned Actor=\S+ Class=TargetPoint", read_log_from(host, off))
    rep.step("control: the HOST's own dev spawn request spawns (single player / listen host keep the tool)", bool(spawned),
             spawned.group(0) if spawned else "no 'Spawned ... Class=TargetPoint' line on the host")
    off = log_size(host)
    console(client, "MO.Test.DevSpawnRPC")
    time.sleep(6)
    log = read_log_from(host, off)
    refused = re.search(r"\[MOPossession\] ServerSpawnActorNearController rejected: dev spawn RPCs are host-only[^\n]*", log)
    leaked = re.search(r"\[MOPossess\] Spawned Actor=\S+ Class=TargetPoint", log)
    rep.step("trust: a REMOTE client's dev spawn (client-chosen class) is REFUSED and nothing spawns", bool(refused) and not leaked,
             (refused.group(0)[:140] if refused else "no 'host-only' refusal on the host") + ("; but it SPAWNED" if leaked else ""))


def _client_rename(rep, host, client):
    """The status panel's Change: AMOPlayerController::RequestSetPawnIdentity, sent by MO.Test.RenamePawnRPC (C++: editor Python would run the Server
    RPC's body locally on the client). A joiner renames its OWN pawn and the host sees the new name on the identity (the one source a save copies);
    renaming the HOST's pawn (another player's) is refused."""
    name = "Joiner %d" % (int(time.time()) % 10000)
    console(client, "MO.Test.RenamePawnRPC self " + name)
    time.sleep(4)
    seen = _probe(host, "idc = unreal.GameplayStatics.get_player_pawn(world, 1).get_component_by_class(unreal.MOIdentityComponent); "
                        "out('NAME=%s' % idc.get_editor_property('display_name'))")
    m = re.search(r"NAME=(.*)", seen)
    rep.step("a joiner renames its own pawn: the host's identity has the new name", bool(m) and m.group(1).strip() == name,
             m.group(1).strip() if m else "no NAME line from the host")
    off = log_size(host)
    console(client, "MO.Test.RenamePawnRPC other Hijacked")
    time.sleep(4)
    refused = re.search(r"refused identity change of \S+ \(another player's pawn\)", read_log_from(host, off))
    hostname = _probe(host, "out('HOSTNAME=%s' % unreal.GameplayStatics.get_player_pawn(world, 0).get_component_by_class(unreal.MOIdentityComponent).get_editor_property('display_name'))")
    shown = re.search(r"HOSTNAME=.*", hostname)
    rep.step("trust: renaming the HOST's pawn (another player's) is refused", bool(refused) and "Hijacked" not in hostname,
             (refused.group(0) if refused else "no refusal on the host") + "; " + (shown.group(0) if shown else ""))


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
    # Rate: every real action takes at least TerraformDurationSeconds, so two applies in one tick cannot both be real (H17 residual).
    time.sleep(5)
    off = log_size(host)
    console(client, "MO.Test.TerraformRPC Dig 300 twice")
    wait_log(host, r"\[MOTerraforming\] ServerApplyTerraform: [\d.]+ s after the last apply[^\n]*", since=off, timeout=15)
    time.sleep(1)
    log = read_log_from(host, off)
    rejected = re.findall(r"\[MOTerraforming\] ServerApplyTerraform: [\d.]+ s after the last apply[^\n]*", log)
    rep.step("trust: of two terraform applies sent in the same tick the server takes ONE and rejects the other", len(rejected) == 1 and "out of reach" not in log,
             rejected[0] if rejected else "no rate rejection on the host")


def actions_test(uproject, editor_exe, keep=False, boot_timeout=300, seed=None):
    """Gameplay verbs from a real CLIENT process against a real host (the PIE-era MO.Test.* suite never left one editor): pickup
    identity, crafting, attack. Each one's proof is read on the HOST -- what the authority ended up with -- not from the client's
    optimistic 'PASS'."""
    rep = Report()
    host, client = names = ("acthost", "actclient")
    try:
        print("[nettest] actions: host + client on the REAL gameplay map; verbs run on the CLIENT ...", flush=True)
        for n, pos in zip(names, ((0, 0), (680, 0))):
            start(n, uproject, editor_exe, pos=pos, nosteam=True)
        if not _host_and_join(rep, host, client, "ActionsTest", boot_timeout, seed=seed):
            return False
        # MO_ACTIONS_CMDS_<NAME>: console commands (";"-separated) for one instance right after the join -- an A/B switch for an
        # investigation, e.g. MO_ACTIONS_CMDS_ACTCLIENT="voxel.MegaMaterial.DisableMaterialSelection 1" (N-185)
        for n in names:
            for cmd in (c.strip() for c in os.environ.get(f"MO_ACTIONS_CMDS_{n.upper()}", "").split(";") if c.strip()):
                console(n, cmd)
                print(f"[nettest] actions: {n} ran '{cmd}'", flush=True)
        only = {s.strip() for s in os.environ.get("MO_ACTIONS_ONLY", "").split(",") if s.strip()}  # quick rerun of named cases
        case_seconds = []
        for fn in (_client_pickup, _client_press_e, _client_e_on_ground_item, _client_e_beside_ground_item, _client_drink, _client_glide, _client_footstep_surface, _client_footstep_on_rock, _trust_instance_far, _trust_instance_out_of_sight, _client_harvest, _client_harvest_full_pack, _client_inspect, _client_survivor_command, _host_foliage_reaches_client, _client_gait, _remote_pawn_velocity, _client_inventory_requests, _dropped_item_falls_on_client, _client_carcass, _client_building_look, _host_e_on_joiner, _far_remote_pawn_9km, _joiner_long_run, _appearance_replicates, _client_craft, _client_forage, _client_build, _client_build_from_ground, _client_attack, _client_eat, _client_light_fire, _client_treats_own_wound, _client_treats_host, _host_treats_survivor, _host_torch_in_rain, _client_lights_from_host, _trust_placement, _trust_pickup_far, _trust_possess_other, _possession_rows_name_players, _trust_dev_spawn, _client_rename, _trust_terraform, _buried_pawns, _remote_pawn_stays_put, _client_lake_clock, _client_sees_creature_flinch, _host_wolf_holds_off_fire, _host_wolf_pack, _host_takes_departed_pawn, _host_possession_click, _host_colony_medic, _host_colony_alerts):
            if only and fn.__name__ not in only:
                continue
            t0 = time.time()
            try:
                fn(rep, host, client)
            except Exception as e:  # noqa: BLE001 - one verb's harness error must not hide the others
                rep.step(f"{fn.__name__} ran without a harness error", False, f"{type(e).__name__}: {e}")
            case_seconds.append((time.time() - t0, fn.__name__))
            print(f"[nettest] {fn.__name__}: {time.time() - t0:.0f} s", flush=True)
        slowest = sorted(case_seconds, reverse=True)[:12]
        print("[nettest] slowest cases: " + ", ".join(f"{n} {t:.0f} s" for t, n in slowest) + f" | all {sum(t for t, _ in case_seconds):.0f} s", flush=True)
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
# The client logs its FIRST host clock sample whether or not it snapped (a snap line alone appears only when the drift happens to exceed the threshold: the check flaked).
PKG_CLOCK_SYNC_RE = re.compile(r"\[MOWorldSync\] client clock (was (-?[\d.]+) game-s off the host's; snapping|follows the host's: first sample .*)")
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

    snap, weather = PKG_CLOCK_SYNC_RE.search(client_log), PKG_WEATHER_RE.search(client_log)
    if withheld:
        rows.append(("NEGATIVE CONTROL: with the host's publish withheld the client never received the host's clock", snap is None,
                     "no clock sync on the client" if snap is None else snap.group(0)))
        rows.append(("NEGATIVE CONTROL: ... and never followed a weather preset", weather is None,
                     "no weather follow on the client" if weather is None else weather.group(0)))
    else:
        rows.append(("client: adopted the host's game clock", bool(snap), snap.group(0) if snap else "no '[MOWorldSync] client clock follows / was ... snapping' line"))
        rows.append(("client: followed the host's weather preset through the bridge", bool(weather), weather.group(0) if weather else "no '[MOWorldSync] client following' line"))
    return rows


SPAWN_RE_COMPILED = re.compile(SPAWN_RE)


def _pkg_alive_count():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq MO57.exe", "/NH", "/FO", "CSV"], capture_output=True, text=True).stdout
    return sum(1 for line in out.splitlines() if line.lower().startswith('"mo57.exe"'))


def _pkg_wait_none(timeout=30):
    """Wait for every MO57.exe to be gone: the previous run's tree-kill returns before the process has finished exiting, so a run started right after it saw "1 running" and refused (a seed sweep lost 2 of 10 runs to this)."""
    deadline = time.time() + timeout
    while time.time() < deadline and _pkg_alive_count() > 0:
        time.sleep(1)


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


PKG_DEATHFLOW_RE = re.compile(r"\[MOTEST\] (PASS|FAIL) DeathFlow: (.*)")


PKG_SUITE_RE = re.compile(r"\[MOTEST\] ===== RunAll: (\d+) passed, (\d+) failed")


def packaged_suite_test(package=False, seed=7, boot_timeout=420):
    """The PACKAGED Development game, ONE copy: a new game on a fixed seed, then MO.Test.AfterReady MO.Test.RunAll (drop/pickup identity, attack, craft, data + art validation) once the world is ready.
    The cooked game has different data paths than the editor (cooked DataTables, soft references, always-cook folders): this runs the regression gate against the artifact that ships."""
    rep = Report()
    pid = None
    try:
        if package and not _package_build():
            return rep.step("the Development package builds", False, "see Saved/Logs/package.log")
        if not rep.step("the staged packaged build exists", os.path.exists(PKG_EXE), PKG_EXE):
            return False
        _pkg_wait_none()
        running = _pkg_alive_count()
        if not rep.step("no other MO57.exe is running (this mode must own the log and never touches others)", running == 0, f"{running} running"):
            return False
        log_path = os.path.join(PKG_LOGS, "MO57.log")
        if os.path.exists(log_path):
            os.remove(log_path)
        results_path = os.path.join(PKG_ROOT, "MO57", "Saved", "MOTestResults.txt")
        if os.path.exists(results_path):
            os.remove(results_path)
        print(f"[nettest] packaged-suite: launching one packaged copy (MO.NewGame {seed} + MO.Test.RunAll after ready) ...", flush=True)
        pid = _pkg_launch('-NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="MO.NewGame {seed},MO.Test.AfterReady MO.Test.RunAll"'.replace("{seed}", str(seed)))
        deadline, summary = time.time() + boot_timeout, None
        while time.time() < deadline and summary is None:
            summary = PKG_SUITE_RE.search(_pkg_read("MO57.log"))
            time.sleep(3)
        log = _pkg_read("MO57.log")
        if summary is None:
            rep.step("RunAll reported a summary", False, log[-300:].replace(chr(10), " | "))
        else:
            passed, failed = int(summary.group(1)), int(summary.group(2))
            detail = f"{passed} passed, {failed} failed"
            if failed:
                try:
                    with open(results_path, encoding="utf-8", errors="replace") as f:
                        detail += " -- " + " ; ".join(l.strip() for l in f if l.startswith("FAIL"))[:700]
                except OSError:
                    pass
            rep.step("MO.Test.RunAll in the cooked game", failed == 0 and passed > 0, detail)
        fatal = PKG_FATAL_RE.search(log)
        rep.step("no crash, ensure or assertion in the log", fatal is None, "" if fatal is None else fatal.group(0))
        return rep.ok
    finally:
        print(f"[nettest] packaged-suite: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        _pkg_stop(pid)


def packaged_deathflow_test(package=False, seed=1029024224, boot_timeout=420):
    """The PACKAGED Development game, ONE copy: a new game on a FIXED seed (default 1029024224: its spawn used to settle on a stale coarse collision, the terrain rose over the pawn and it swam
    underground), then MO.Test.DeathFlow drives it end to end -- kill the pawn, the death recap shows (the cooked build has WBP_DeathRecap), Continue,
    a new survivor ~5 miles away is possessed and grounded -- and logs [MOTEST] PASS / FAIL DeathFlow, which this reads. Why it exists: a class loaded from a path string (the recap) is exactly what a
    cook can leave out, and the editor cannot tell. Refuses to run if any MO57.exe is already running (it only ever stops the process tree it started)."""
    rep = Report()
    pid = None
    try:
        if package and not _package_build():
            return rep.step("the Development package builds", False, "see Saved/Logs/package.log")
        if not rep.step("the staged packaged build exists", os.path.exists(PKG_EXE), PKG_EXE):
            return False
        _pkg_wait_none()
        running = _pkg_alive_count()
        if not rep.step("no other MO57.exe is running (this mode must own the log and never touches others)", running == 0, f"{running} running"):
            return False
        log_path = os.path.join(PKG_LOGS, "MO57.log")
        if os.path.exists(log_path):
            os.remove(log_path)   # a stale verdict from an earlier run must not be read
        print(f"[nettest] packaged-death: launching one packaged copy (MO.NewGame {seed} + MO.Test.DeathFlow) ...", flush=True)
        pid = _pkg_launch('-NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="MO.NewGame {seed},MO.Test.DeathFlow"'.replace("{seed}", str(seed)))
        deadline, verdict = time.time() + boot_timeout, None
        while time.time() < deadline and verdict is None:
            verdict = PKG_DEATHFLOW_RE.search(_pkg_read("MO57.log"))
            time.sleep(3)
        log = _pkg_read("MO57.log")
        if verdict is None:
            rep.step("DeathFlow reported a verdict", False, log[-300:].replace(chr(10), " | "))
        else:
            rep.step("DeathFlow: kill -> death recap -> Continue -> new survivor ~5 miles away, grounded", verdict.group(1) == "PASS", verdict.group(2))
        fatal = PKG_FATAL_RE.search(log)
        rep.step("no crash, ensure or assertion in the log", fatal is None, "" if fatal is None else fatal.group(0))
        return rep.ok
    finally:
        print(f"[nettest] packaged-death: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        _pkg_stop(pid)


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
        _pkg_wait_none()
        running = _pkg_alive_count()
        if not rep.step("no other MO57.exe is running (this mode must own the logs and never touches others)", running == 0, f"{running} running"):
            return False

        cmds = ("MO.WorldSync.Withhold 1," if withhold_sync else "") + "MO.Session.Host PackagedTest 4"
        print(f"[nettest] packaged: launching the host (-ExecCmds=\"{cmds}\")" + (" -- NEGATIVE CONTROL: sync withheld" if withhold_sync else "") + " ...", flush=True)
        host_pid = _pkg_launch(f'-NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="{cmds}"')
        deadline, seeded = time.time() + boot_timeout, False
        while time.time() < deadline and not seeded:
            seeded = bool(PKG_HOST_SEED_RE.search(_pkg_read("MO57.log")))
            time.sleep(2)
        if not rep.step("host: the packaged game booted, hosted a world and published its seed", seeded,
                        "" if seeded else _pkg_read("MO57.log")[-300:].replace("\n", " | ")):
            return False

        client_pid = _pkg_launch("127.0.0.1 -NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=980 -WinY=0")
        deadline, joined = time.time() + boot_timeout, False
        while time.time() < deadline and not joined:
            joined = bool(PKG_CLIENT_SEED_RE.search(_pkg_read("MO57_2.log")))
            time.sleep(2)
        rep.step("client: the second packaged copy joined and applied the host's seed", joined,
                 "" if joined else _pkg_read("MO57_2.log")[-300:].replace("\n", " | "))

        # A joiner is held until the HOST's pawn has settled on its ground (coastal settle up to 45 s, after a voxel start that took 44 s on one seed with two
        # games sharing the CPU): wait for the client's possession before the fixed observation window, or a slow seed reads as "client never possessed".
        deadline = time.time() + boot_timeout
        while time.time() < deadline and "AMOPlayerController: Possessed" not in _pkg_read("MO57_2.log"):
            time.sleep(2)

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


PKG_REMOVAL_HOST_RE = re.compile(r"\[MOTEST\] (PASS|FAIL) RemovalPersists: (.*)")
PKG_REMOVAL_AUDIT_RE = re.compile(r"\[MOTEST\] (PASS|FAIL) WorldRemovalAudit: (.*)")
PKG_REMOVAL_SWEEP_RE = re.compile(r"\[MOWorldRemoval\] removed (\d+) spawned instance\(s\) for (\d+) recorded removal\(s\)")


def packaged_removal_test(package=False, boot_timeout=180, host_fell_after=70, run_timeout=420):
    """Co-op: a tree felled on the HOST disappears on the CLIENT, and stays gone when PCG regenerates (UMOWorldRemovalSubsystem + AMOGameState's
    replicated FMORemovedInstanceArray). Before Oct 2026 a removal never reached a client at all: each machine generates its own PCG instances.
    Host: MO.Session.Host, then (after its world is ready) MO.Test.RemovalPersists -- fells the tree nearest its pawn through the one removal path and
    regenerates its PCG. Client: joins, then MO.Test.WorldRemovalAudit -- waits for the replicated record and checks nothing recorded still stands there.
    The client's '[MOWorldRemoval] removed N spawned instance(s)' line proves it HAD the tree and the replicated removal took it away."""
    rep = Report()
    host_pid = client_pid = None
    try:
        if package and not _package_build():
            return rep.step("the Development package builds", False, "see Saved/Logs/package.log")
        if not rep.step("the staged packaged build exists", os.path.exists(PKG_EXE), PKG_EXE):
            return False
        _pkg_wait_none()
        if not rep.step("no other MO57.exe is running", _pkg_alive_count() == 0, f"{_pkg_alive_count()} running"):
            return False
        cmds = f"MO.Session.Host RemovalTest 4,MO.Test.AfterReady MO.Delay {host_fell_after} MO.Test.RemovalPersists 20"
        print(f"[nettest] packaged-removal: launching the host (-ExecCmds=\"{cmds}\") ...", flush=True)
        host_pid = _pkg_launch(f'-NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="{cmds}"')
        deadline, seeded = time.time() + boot_timeout, False
        while time.time() < deadline and not seeded:
            seeded = bool(PKG_HOST_SEED_RE.search(_pkg_read("MO57.log")))
            time.sleep(2)
        if not rep.step("host: hosted a world and published its seed", seeded):
            return False
        client_pid = _pkg_launch('127.0.0.1 -NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=980 -WinY=0 -ExecCmds="MO.Test.WorldRemovalAudit 360"')
        deadline = time.time() + run_timeout
        host_line = audit_line = None
        while time.time() < deadline and not (host_line and audit_line):
            host_line = PKG_REMOVAL_HOST_RE.search(_pkg_read("MO57.log"))
            audit_line = PKG_REMOVAL_AUDIT_RE.search(_pkg_read("MO57_2.log"))
            time.sleep(3)
        client_log = _pkg_read("MO57_2.log")
        rep.step("client: joined and applied the host's seed", bool(PKG_CLIENT_SEED_RE.search(client_log)))
        rep.step("host: felled a tree that stayed felled through its own PCG regeneration", bool(host_line) and host_line.group(1) == "PASS",
                 host_line.group(0) if host_line else "no RemovalPersists verdict on the host")
        rep.step("client: the host's removal arrived and nothing recorded stands in the client's world", bool(audit_line) and audit_line.group(1) == "PASS",
                 audit_line.group(0) if audit_line else "no WorldRemovalAudit verdict on the client")
        sweep = PKG_REMOVAL_SWEEP_RE.search(client_log)
        rep.step("client: the replicated removal took away the client's OWN copy of the tree (it had generated it)", bool(sweep),
                 sweep.group(0) if sweep else "no '[MOWorldRemoval] removed N spawned instance(s)' line on the client -- it may never have generated that cell (inconclusive)")
        for who, text in (("host", _pkg_read("MO57.log")), ("client", client_log)):
            bad = CHURN_FATAL_RE.search(text)
            rep.step(f"{who}: no crash, ensure or assertion", bad is None, bad.group(0) if bad else "none")
        return rep.ok
    finally:
        print(f"[nettest] packaged-removal: {'PASS' if rep.rows and rep.ok else 'FAIL'}", flush=True)
        _pkg_stop(client_pid)
        _pkg_stop(host_pid)


# ---------------------------------------------------------------------------------------------------------------------------------
# nettest churn: join/leave cycles, two players grabbing the same item at once, saving while a client is connected
# ---------------------------------------------------------------------------------------------------------------------------------
CHURN_SLOT = "zz_nettest_churn"
CHURN_FATAL_RE = re.compile(r"Fatal error|Unhandled Exception|Ensure condition failed|Assertion failed")
CHURN_MO_ERROR_RE = re.compile(r"\bLogMO\w*: Error:")
FALLTHROUGH_RE = re.compile(r"Fall-through detected")
HUMAN_FALLTHROUGH_RE = re.compile(r"\[MOCharacter\] BP_MOMetaHuman\w*: Fall-through detected")  # players and colonists; the spawn manager's deer/wolves recover the same way
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
        rescued = len(HUMAN_FALLTHROUGH_RE.findall(text))
        creatures = len(FALLTHROUGH_RE.findall(text)) - rescued
        rows.append((f"{who}: no fall-through rescue of a player or colonist during the churn", rescued == 0,
                     f"{rescued} human rescue line(s)" + (f" ({creatures} creature rescue(s): the spawn manager's animals landing off-terrain, recovered by the same code, not counted)" if creatures else "")))
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
        "others = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOWorldItem) if a.get_name() != wn and (((a.get_actor_location().x - hl.x) ** 2 + (a.get_actor_location().y - hl.y) ** 2 + (a.get_actor_location().z - hl.z) ** 2) ** 0.5) < 2500.0]; "
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

        # "Item looked at" (FMOBugReportLookAt): the view is always recorded with a command that rebuilds it; then either what was hit or that nothing was.
        look_ok = bool(fields.get("LookAt.View")) and fields.get("LookAt.Reproduce", "").startswith("MO.Cam.LookAt ")             and bool(fields.get("LookAt.Distance") or fields.get("LookAt.Hit"))
        rows.append(("the report says what the player was looking at (LookAt.*)", look_ok,
                     " | ".join(f"{k}={fields[k]}" for k in sorted(fields) if k.startswith("LookAt."))[:400] or "no LookAt rows"))

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
        _pkg_wait_none()
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
            pid = _pkg_launch(f'-NoSteam -NoQualityBenchmark -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -ExecCmds="{cmds}"')
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
