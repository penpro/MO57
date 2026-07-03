#!/usr/bin/env python3
"""
ue.py — unified CLI for the MO57 autonomous dev loop.

One entry point for the three "hands" of the toolset (see Docs/AUTONOMOUS_TOOLING.md):

  bridge  (file-I/O -> editor python / console cmds / PIE)   ue.py run | py | seq | boot | pie | test
  MCP     (HTTP 127.0.0.1:8000 -> DataTable/asset authoring) ue.py mcp | rows | save
  build   (UBT 5.8 CLI + editor lifecycle)                   ue.py build | editor | cycle

Design rules baked in from the 2026-06/07 fix campaign:
  * Bridge commands are CORRELATED: every call is bracketed with begin/end markers,
    so output is attributed to THE command — no more "tail the file and hope".
  * Game-log output is captured as a DELTA from the command's start offset.
  * MCP row authoring is one-row-at-a-time with readback verify. Batch add_rows is
    all-or-nothing (one pre-existing name rejects the whole batch) and batch
    set_rows fails SILENTLY — both verified 2026-07-02.
  * MCP transport is curl with -m 25 fail-fast (python urllib gets empty bodies
    from the streamable-HTTP endpoint). Session id cached across invocations.
  * Exit codes: 0 = ok, 1 = operation failed, 2 = environment down (editor/bridge/
    MCP unreachable) — so callers can distinguish "test failed" from "no editor".

NOTE: dev-machine tooling — drives arbitrary local execution. Never ship.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time
import uuid

# --- environment constants (match claude_bridge.py / agent_boot_newgame.ps1) ---
ROOT = r"D:\UEProjects\MO57"
UPROJECT = ROOT + r"\MO57.uproject"
GAMELOG = ROOT + r"\Saved\Logs\MO57.log"
RESULTS = ROOT + r"\Saved\MOTestResults.txt"
BOOT_PS1 = ROOT + r"\Tools\agent_boot_newgame.ps1"
TMP = r"C:\Users\penum\AppData\Local\Temp\claude"  # bridge hardcodes this path
CMD_FILE = os.path.join(TMP, "ue_cmd.txt")
OUT_FILE = os.path.join(TMP, "ue_out.txt")
SID_FILE = os.path.join(TMP, "mcp_session.txt")
UBT = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
EDITOR_EXE = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
EDITOR_CMD = r"D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
MCP_URL = "http://127.0.0.1:8000/mcp"

# Toolset aliases -> full MCP toolset names
TOOLSETS = {
    "dt": "editor_toolset.toolsets.data_table.DataTableTools",
    "datatable": "editor_toolset.toolsets.data_table.DataTableTools",
    "asset": "editor_toolset.toolsets.asset.AssetTools",
}

try:  # utf-8 so NSLOCTEXT can't crash the console; line-buffered so piped/
    # backgrounded runs stream instead of dumping everything at exit
    sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
    sys.stderr.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
except Exception:
    pass


def _die(code, msg):
    print(msg)
    sys.exit(code)


def _size(path):
    try:
        return os.path.getsize(path)
    except OSError:
        return 0


def _read_from(path, offset):
    """Shared read from a file another process holds open (UE log, bridge out)."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            f.seek(offset)
            return f.read()
    except OSError:
        return ""


# =============================================================================
# Bridge layer — correlated command execution
# =============================================================================

def bridge_send(lines):
    os.makedirs(TMP, exist_ok=True)
    with open(CMD_FILE, "a", encoding="utf-8", newline="\n") as f:
        for line in lines:
            f.write(line.rstrip("\n") + "\n")


_MARKER_ACK = re.compile(r'\[py-(ok|err)\] out\("<[BE] [0-9a-f]{8}>"\)')


def bridge_run(cmds, timeout=15.0, want_log=True, log_grep=None, log_wait=4.0):
    """Execute command lines via the bridge, correlated.

    Brackets `cmds` with begin/end markers so we return exactly THIS call's
    bridge output, plus the MO57.log delta produced while it ran.
    Returns (ok, bridge_lines, log_delta). ok=False => bridge not responding.
    """
    mid = uuid.uuid4().hex[:8]
    out_off = _size(OUT_FILE)
    log_off = _size(GAMELOG)
    bridge_send([f'py:out("<B {mid}>")'] + list(cmds) + [f'py:out("<E {mid}>")'])

    deadline = time.time() + timeout
    buf = ""
    while time.time() < deadline:
        buf = _read_from(OUT_FILE, out_off)
        if f"<E {mid}>" in buf:
            break
        time.sleep(0.25)
    else:
        return False, [], ""

    time.sleep(0.15)  # let the END marker's [py-ok] ack land, then swallow it
    buf = _read_from(OUT_FILE, out_off)

    lines = []
    seen_begin = False
    for raw in buf.splitlines():
        if f"<B {mid}>" in raw:
            seen_begin = True
            continue
        if f"<E {mid}>" in raw:
            break
        if not seen_begin or _MARKER_ACK.search(raw):
            continue
        lines.append(re.sub(r"^\[\d{2}:\d{2}:\d{2}\] ", "", raw))

    log_delta = ""
    if want_log:
        # UE buffers its log file; poll briefly for the delta (or the grep hit).
        log_deadline = time.time() + log_wait
        while time.time() < log_deadline:
            log_delta = _read_from(GAMELOG, log_off)
            if log_grep and re.search(log_grep, log_delta):
                break
            if not log_grep and log_delta:
                break
            time.sleep(0.4)
        if log_grep:
            log_delta = "\n".join(l for l in log_delta.splitlines() if re.search(log_grep, l))
    return True, lines, log_delta


def bridge_alive(timeout=4.0):
    ok, _, _ = bridge_run([], timeout=timeout, want_log=False)
    return ok


def _wrap_py(code):
    """One bridge line per command: multi-line python rides inside exec()."""
    if "\n" in code:
        return "py:exec(" + json.dumps(code) + ")"
    return "py:" + code


# =============================================================================
# MCP layer — session-cached curl JSON-RPC
# =============================================================================

def _curl(body, sid=None, headers=False, timeout=25):
    cmd = ["curl", "-s", "-m", str(timeout)]
    if headers:
        cmd += ["-D", "-", "-o", os.devnull]
    cmd += ["-X", "POST", MCP_URL,
            "-H", "Content-Type: application/json",
            "-H", "Accept: application/json, text/event-stream"]
    if sid:
        cmd += ["-H", "Mcp-Session-Id: " + sid]
    cmd += ["-d", body]
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 5).stdout
    except subprocess.TimeoutExpired:
        return ""


def mcp_connect():
    hdr = _curl(json.dumps({
        "jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                   "clientInfo": {"name": "ue.py", "version": "1"}},
    }), headers=True, timeout=8)
    sid = next((l.split(":", 1)[1].strip()
                for l in hdr.replace("\r", "").splitlines()
                if l.lower().startswith("mcp-session-id:")), None)
    if sid:
        _curl(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized",
                          "params": {}}), sid, timeout=8)
        try:
            os.makedirs(TMP, exist_ok=True)
            with open(SID_FILE, "w") as f:
                f.write(sid)
        except OSError:
            pass
    return sid


def _mcp_parse(raw):
    if not raw or not raw.strip():
        return None
    raw = raw.replace("\r", "")
    for ln in raw.splitlines():  # tools/call answers arrive as SSE
        if ln.startswith("data: "):
            raw = ln[6:]
            break
    try:
        result = json.loads(raw).get("result", {})
    except Exception:
        return None
    if "content" in result:
        text = result["content"][0]["text"]
        try:
            obj = json.loads(text)
            if isinstance(obj, dict) and "returnValue" in obj:
                rv = obj["returnValue"]
                return json.loads(rv) if isinstance(rv, str) and rv.strip()[:1] in "{[" else rv
            return obj
        except Exception:
            return text
    return result


def mcp_call(toolset, tool, args, _retried=False):
    """Call an MCP tool; reuse the cached session, reconnect once on failure."""
    sid = None
    try:
        with open(SID_FILE) as f:
            sid = f.read().strip() or None
    except OSError:
        pass
    if not sid:
        sid = mcp_connect()
        if not sid:
            return None
    body = json.dumps({"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                       "params": {"name": "call_tool",
                                  "arguments": {"toolset_name": toolset,
                                                "tool_name": tool,
                                                "arguments": args}}})
    out = _mcp_parse(_curl(body, sid))
    if out is None and not _retried:
        if mcp_connect():
            return mcp_call(toolset, tool, args, _retried=True)
    return out


def mcp_alive():
    return mcp_connect() is not None


# --- DataTable verbs ---------------------------------------------------------

DT = TOOLSETS["dt"]
AT = TOOLSETS["asset"]


def _table(ref):
    return {"refPath": ref}


def rows_list(table):
    r = mcp_call(DT, "list_rows", {"data_table": _table(table)})
    if isinstance(r, list):
        return [(n.get("name") if isinstance(n, dict) else n) for n in r]
    return r  # error string / None


def rows_get(table, names):
    return mcp_call(DT, "get_rows", {"data_table": _table(table), "row_names": names})


def rows_set_safe(table, rows, do_save=True):
    """Author rows ONE AT A TIME with readback verify. Returns (ok_count, report)."""
    report, ok = [], 0
    for name, fields in rows.items():
        add = mcp_call(DT, "add_rows", {"data_table": _table(table), "row_names": [name]})
        add_note = "" if (isinstance(add, str) and "already exist" in add) else f" add={add}"
        mcp_call(DT, "set_rows", {"data_table": _table(table),
                                  "values": json.dumps({name: fields})})
        back = rows_get(table, [name])
        row = back.get(name, {}) if isinstance(back, dict) else {}
        bad = []
        for k, v in fields.items():
            if isinstance(v, (str, int, float, bool)):
                got = row.get(k)
                # FText fields wrap plain strings in NSLOCTEXT(...) — substring match
                if isinstance(got, str) and isinstance(v, str):
                    if v not in got:
                        bad.append(k)
                elif got is not None and str(got) != str(v) and not (
                        isinstance(v, (int, float)) and isinstance(got, (int, float))
                        and float(got) == float(v)):
                    bad.append(k)
            elif isinstance(v, (list, dict)) and v and not row.get(k):
                bad.append(k)
        good = not bad
        ok += good
        report.append("%-28s %s%s%s" % (name, "OK" if good else "MISMATCH",
                                        (" " + ",".join(bad)) if bad else "", add_note))
    if do_save and ok:
        saved = mcp_call(AT, "save_assets", {"asset_paths": [table]})
        report.append("save_assets -> %s" % saved)
    return ok, report


# =============================================================================
# Editor / build lifecycle
# =============================================================================

def editor_running():
    r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/NH"],
                       capture_output=True, text=True)
    return "UnrealEditor.exe" in (r.stdout or "")


def editor_stop(wait=60):
    if not editor_running():
        return True
    subprocess.run(["taskkill", "/IM", "UnrealEditor.exe", "/F"], capture_output=True)
    deadline = time.time() + wait
    while time.time() < deadline:
        if not editor_running():
            time.sleep(2)  # let file locks release
            return True
        time.sleep(1)
    return False


def editor_start(wait_bridge_s=360):
    os.makedirs(TMP, exist_ok=True)
    for p in (CMD_FILE, OUT_FILE):  # truncate so no stale commands/markers replay
        open(p, "w").close()
    DETACHED = 0x00000008 | 0x00000200  # DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP
    subprocess.Popen([EDITOR_EXE, UPROJECT], creationflags=DETACHED,
                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                     stderr=subprocess.DEVNULL, close_fds=True)
    deadline = time.time() + wait_bridge_s
    while time.time() < deadline:
        if "bridge] registered" in _read_from(OUT_FILE, 0):
            return True
        time.sleep(4)
    return False


def build(target="MO57Editor"):
    """Run UBT; returns True on success. Streams the tail of the output."""
    proc = subprocess.Popen(
        [UBT, target, "Win64", "Development", f"-Project={UPROJECT}", "-NoHotReload"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        encoding="utf-8", errors="replace")
    tail = []
    for line in proc.stdout:
        line = line.rstrip()
        tail.append(line)
        if len(tail) > 400:
            tail.pop(0)
        if re.match(r"^\[\d+/\d+\]|^Result:|error|warning C", line):
            print(line)
    proc.wait()
    ok = proc.returncode == 0 and any("Result: Succeeded" in l for l in tail)
    if not ok:
        print("\n".join(tail[-40:]))
    return ok


# =============================================================================
# Subcommands
# =============================================================================

def cmd_status(a):
    ed = editor_running()
    br = bridge_alive() if ed else False
    mc = mcp_alive() if ed else False
    pie, ingame, statel = "?", "?", ""
    if br:
        ok, lines, delta = bridge_run(
            ['py:ues=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); '
             'out("PIE="+str(ues.is_in_play_in_editor()))',
             "MO.Test.State"],
            timeout=10, log_grep=r"\[MOQUERY\]")
        for l in lines:
            if l.startswith("PIE="):
                pie = l[4:]
        m = re.search(r"inGame=(\S+)", delta)
        ingame = m.group(1) if m else "?"
        statel = delta.strip().splitlines()[-1] if delta.strip() else ""
    print(f"editor : {'RUNNING' if ed else 'DOWN   -> ue.py editor start'}")
    print(f"bridge : {'ALIVE' if br else ('DEAD   -> restart editor (init_unreal loads it)' if ed else '-')}")
    print(f"mcp    : {'ALIVE' if mc else ('DEAD   -> editor hosts it; restart editor' if ed else '-')}")
    print(f"pie    : {pie}   inGame: {ingame}")
    if statel:
        print(f"state  : {statel[statel.find('[MOQUERY]'):]}")
    sys.exit(0 if (ed and br) else 2)


def cmd_run(a):
    ok, lines, delta = bridge_run([a.command], timeout=a.timeout,
                                  log_grep=a.grep, log_wait=a.logwait)
    if not ok:
        _die(2, "bridge not responding (editor closed, or init_unreal didn't load it)")
    for l in lines:
        print(l)
    if delta.strip():
        print(delta.rstrip())


def cmd_py(a):
    if a.file:
        code = open(a.file, "r", encoding="utf-8-sig").read()
    else:
        code = a.code
    ok, lines, delta = bridge_run([_wrap_py(code)], timeout=a.timeout,
                                  log_grep=a.grep, log_wait=a.logwait if a.grep else 0.5)
    if not ok:
        _die(2, "bridge not responding")
    err = any(l.startswith("[py-err]") for l in lines)
    for l in lines:
        print(l)
    if delta.strip():
        print(delta.rstrip())
    sys.exit(1 if err else 0)


def cmd_seq(a):
    """Run a claude_seq sequence file and wait for its DONE/FAILED marker."""
    path = os.path.abspath(a.file)
    name = os.path.splitext(os.path.basename(path))[0]
    out_off = _size(OUT_FILE)
    ok, _, _ = bridge_run([f'py:import claude_seq; claude_seq.run_file({json.dumps(path)})'],
                          timeout=10, want_log=False)
    if not ok:
        _die(2, "bridge not responding")
    deadline = time.time() + a.timeout
    final = None
    while time.time() < deadline:
        buf = _read_from(OUT_FILE, out_off)
        m = [l for l in buf.splitlines() if f"[seq:{name}]" in l]
        final = next((l for l in m if re.search(r"\] (DONE|FAILED|ABORTED)", l)), None)
        if final:
            for l in m:
                print(re.sub(r"^\[\d{2}:\d{2}:\d{2}\] ", "", l))
            break
        time.sleep(0.5)
    if not final:
        _die(1, f"sequence '{name}' did not finish within {a.timeout}s")
    sys.exit(0 if ("DONE" in final and "fail=0" in final) else 1)


def cmd_boot(a):
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", BOOT_PS1]
    if a.seed:
        cmd += ["-Seed", str(a.seed)]
    if a.name:
        cmd += ["-SurvivorName", a.name]
    r = subprocess.run(cmd, text=True, capture_output=True, timeout=300)
    print((r.stdout or "").strip())
    if r.returncode != 0:
        print((r.stderr or "").strip())
    sys.exit(r.returncode)


def cmd_pie(a):
    if a.action == "end":
        line = ('py:import agent_test_lib as atl; '
                '(atl.end_pie(out) if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)'
                '.is_in_play_in_editor() else out("[atl] no PIE"))')
    else:  # begin
        line = 'py:import agent_test_lib as atl; atl.begin_pie(out)'
    ok, lines, _ = bridge_run([line], timeout=15, want_log=False)
    if not ok:
        _die(2, "bridge not responding")
    for l in lines:
        print(l)


def cmd_test(a):
    command = {"RunAll": "MO.Test.RunAll", "ValidateData": "MO.Test.ValidateData"}[a.suite]
    pre_stat = (os.path.getmtime(RESULTS), _size(RESULTS)) if os.path.exists(RESULTS) else None
    ok, _, _ = bridge_run([command], timeout=10, want_log=False)
    if not ok:
        _die(2, "bridge not responding")
    deadline = time.time() + a.timeout
    while time.time() < deadline:
        if os.path.exists(RESULTS):
            cur = (os.path.getmtime(RESULTS), _size(RESULTS))
            if cur != pre_stat:
                break
        time.sleep(0.5)
    else:
        _die(1, f"no results file update within {a.timeout}s (are you in-game for {a.suite}?)")
    time.sleep(0.3)
    content = open(RESULTS, "r", encoding="utf-8", errors="replace").read()
    print(content.rstrip())
    m = re.search(r"SUMMARY (\d+) passed, (\d+) failed", content)
    sys.exit(0 if (m and m.group(2) == "0") else 1)


def cmd_results(a):
    if not os.path.exists(RESULTS):
        _die(1, "no results file yet")
    print(open(RESULTS, "r", encoding="utf-8", errors="replace").read().rstrip())


def cmd_logs(a):
    text = _read_from(GAMELOG, 0)
    lines = text.splitlines()
    if a.grep:
        lines = [l for l in lines if re.search(a.grep, l)]
    for l in lines[-a.n:]:
        print(l)


def cmd_mcp(a):
    toolset = TOOLSETS.get(a.toolset.lower(), a.toolset)
    args = json.loads(a.args) if a.args else {}
    out = mcp_call(toolset, a.tool, args)
    if out is None:
        _die(2, "MCP not responding (editor closed? -> ue.py editor start)")
    print(json.dumps(out, indent=1) if isinstance(out, (dict, list)) else out)


def cmd_rows(a):
    if a.action == "list":
        r = rows_list(a.table)
        if r is None:
            _die(2, "MCP not responding")
        print("\n".join(r) if isinstance(r, list) else r)
    elif a.action == "get":
        r = rows_get(a.table, a.names)
        if r is None:
            _die(2, "MCP not responding")
        print(json.dumps(r, indent=1) if isinstance(r, (dict, list)) else r)
    elif a.action == "set":
        rows = json.loads(open(a.file, "r", encoding="utf-8-sig").read())
        okc, report = rows_set_safe(a.table, rows, do_save=not a.no_save)
        for l in report:
            print(l)
        print(f"VERIFIED {okc}/{len(rows)}")
        sys.exit(0 if okc == len(rows) else 1)


def cmd_save(a):
    out = mcp_call(AT, "save_assets", {"asset_paths": [a.asset]})
    if out is None:
        _die(2, "MCP not responding")
    print(out)


def cmd_refresh_data(a):
    ok, lines, _ = bridge_run([
        'py:unreal.MORecipeDatabaseSettings.invalidate_cache(); '
        'unreal.MOItemDatabaseSettings.invalidate_cache(); '
        'out("item+recipe caches invalidated")'], timeout=10, want_log=False)
    if not ok:
        _die(2, "bridge not responding")
    err = any(l.startswith("[py-err]") for l in lines)
    for l in lines:
        print(l)
    sys.exit(1 if err else 0)


def cmd_auto(a):
    """Headless UE automation tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST suites).

    Runs UnrealEditor-Cmd -unattended -nullrhi with an Automation RunTests
    filter and parses the -ReportExportPath index.json for a pass/fail exit
    code. Requires the editor CLOSED (second instance on the same project
    contends for DDC/config).
    """
    if editor_running() and not a.force:
        _die(1, "editor is RUNNING — headless automation opens the project a second time. "
                "Close it first (ue.py editor stop) or pass --force.")
    report = os.path.join(TMP, "autoreport")
    os.makedirs(report, exist_ok=True)
    for f in os.listdir(report):
        try:
            os.remove(os.path.join(report, f))
        except OSError:
            pass
    logpath = os.path.join(TMP, "autotest.log")
    # String form (not list) so the inner quotes around ExecCmds reach UE intact.
    cmdline = (f'"{EDITOR_CMD}" "{UPROJECT}" -unattended -nullrhi -nosplash -nop4 -NoLiveCoding '
               f'-ExecCmds="Automation RunTests {a.filter};Quit" '
               f'-ReportExportPath="{report}" -abslog="{logpath}"')
    print(f"[auto] running '{a.filter}' automation tests headless (first run can take a few minutes)...")
    t0 = time.time()
    proc = subprocess.run(cmdline, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")
    idx = os.path.join(report, "index.json")
    if not os.path.exists(idx):
        tail = ""
        try:
            with open(logpath, "r", encoding="utf-8", errors="replace") as f:
                tail = "\n".join(f.read().splitlines()[-25:])
        except OSError:
            pass
        _die(1, f"[auto] no report produced (exit {proc.returncode}). Log tail:\n{tail}")
    with open(idx, "r", encoding="utf-8-sig", errors="replace") as f:
        data = json.load(f)
    succ = int(data.get("succeeded", 0)) + int(data.get("succeededWithWarnings", 0))
    fail = int(data.get("failed", 0))
    for t in data.get("tests", []):
        state = t.get("state", "?")
        if state != "Success":
            print(f"  {state:8s} {t.get('fullTestPath', t.get('testDisplayName', '?'))}")
            for e in t.get("entries", [])[:3]:
                msg = (e.get("event", {}) or {}).get("message", "")
                if msg:
                    print(f"           {msg[:180]}")
    print(f"[auto] {succ} passed, {fail} failed, notRun={data.get('notRun', 0)} "
          f"in {time.time() - t0:.0f}s (report: {idx})")
    sys.exit(0 if (fail == 0 and succ > 0) else 1)


def cmd_build(a):
    if editor_running() and not a.force:
        _die(1, "editor is RUNNING — UBT needs it closed. Use `ue.py cycle` (auto-close) "
                "or close it and re-run. (--force skips this check.)")
    sys.exit(0 if build(a.target) else 1)


def cmd_editor(a):
    if a.action == "stop":
        print("stopped" if editor_stop() else "FAILED to stop editor")
    elif a.action == "start":
        if editor_running():
            _die(0, "editor already running")
        print("launching editor + waiting for bridge (~2-5 min)...")
        ok = editor_start()
        _die(0 if ok else 2, "bridge READY" if ok else "TIMEOUT waiting for bridge")
    elif a.action == "wait":
        deadline = time.time() + 360
        while time.time() < deadline:
            if bridge_alive(timeout=3):
                _die(0, "bridge READY")
            time.sleep(4)
        _die(2, "TIMEOUT waiting for bridge")


def cmd_cycle(a):
    """The full compile-verify loop: close editor -> build -> reopen -> [boot] -> [test]."""
    t0 = time.time()
    if editor_running():
        print("[cycle] stopping editor (force; unsaved editor state is lost)...")
        if not editor_stop():
            _die(1, "[cycle] could not stop the editor")
    print(f"[cycle] building {a.target}...")
    if not build(a.target):
        _die(1, "[cycle] BUILD FAILED — editor left closed")
    print("[cycle] launching editor + waiting for bridge...")
    if not editor_start():
        _die(2, "[cycle] editor up but bridge never registered")
    if a.boot:
        print("[cycle] booting new game...")
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", BOOT_PS1]
        if a.seed:
            cmd += ["-Seed", str(a.seed)]
        r = subprocess.run(cmd, text=True, capture_output=True, timeout=300)
        print((r.stdout or "").strip())
        if r.returncode != 0:
            _die(1, "[cycle] boot failed")
    if a.test:
        print("[cycle] running MO.Test.RunAll...")

        class _A:
            suite, timeout = "RunAll", 30.0
        try:
            cmd_test(_A)
        except SystemExit as e:
            print(f"[cycle] total {time.time() - t0:.0f}s")
            raise
    print(f"[cycle] READY in {time.time() - t0:.0f}s")


# =============================================================================

def main():
    p = argparse.ArgumentParser(prog="ue.py", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status", help="editor/bridge/MCP/PIE state in one shot").set_defaults(fn=cmd_status)

    s = sub.add_parser("run", help="run a console command via the bridge, correlated")
    s.add_argument("command")
    s.add_argument("--timeout", type=float, default=15)
    s.add_argument("--grep", help="regex filter for the game-log delta (e.g. MOTEST|MOQUERY)")
    s.add_argument("--logwait", type=float, default=4)
    s.set_defaults(fn=cmd_run)

    s = sub.add_parser("py", help="run editor python via the bridge (-c code | --file script.py)")
    g = s.add_mutually_exclusive_group(required=True)
    g.add_argument("-c", dest="code")
    g.add_argument("--file")
    s.add_argument("--timeout", type=float, default=20)
    s.add_argument("--grep")
    s.add_argument("--logwait", type=float, default=4)
    s.set_defaults(fn=cmd_py)

    s = sub.add_parser("seq", help="run a claude_seq multi-frame sequence file, wait for DONE")
    s.add_argument("file")
    s.add_argument("--timeout", type=float, default=60)
    s.set_defaults(fn=cmd_seq)

    s = sub.add_parser("boot", help="boot to a new game (wraps agent_boot_newgame.ps1)")
    s.add_argument("--seed", type=int)
    s.add_argument("--name")
    s.set_defaults(fn=cmd_boot)

    s = sub.add_parser("pie", help="begin/end PIE")
    s.add_argument("action", choices=["begin", "end"])
    s.set_defaults(fn=cmd_pie)

    s = sub.add_parser("test", help="run the regression suite, wait for + print the results file")
    s.add_argument("--suite", choices=["RunAll", "ValidateData"], default="RunAll")
    s.add_argument("--timeout", type=float, default=30)
    s.set_defaults(fn=cmd_test)

    sub.add_parser("results", help="print the last MOTestResults.txt").set_defaults(fn=cmd_results)

    s = sub.add_parser("logs", help="tail MO57.log")
    s.add_argument("-n", type=int, default=60)
    s.add_argument("--grep")
    s.set_defaults(fn=cmd_logs)

    s = sub.add_parser("mcp", help="raw MCP tool call: ue.py mcp dt list_rows --args '{...}'")
    s.add_argument("toolset", help="alias (dt|asset) or full toolset path")
    s.add_argument("tool")
    s.add_argument("--args", help="JSON arguments")
    s.set_defaults(fn=cmd_mcp)

    s = sub.add_parser("rows", help="DataTable row verbs with safe-authoring discipline")
    s.add_argument("action", choices=["list", "get", "set"])
    s.add_argument("table", help='refPath, e.g. "/MOFramework/Data/Recipes.Recipes"')
    s.add_argument("names", nargs="*", help="row names (get)")
    s.add_argument("--file", help='JSON file {"RowName": {field: value}} (set)')
    s.add_argument("--no-save", action="store_true")
    s.set_defaults(fn=cmd_rows)

    s = sub.add_parser("save", help="save an asset via MCP")
    s.add_argument("asset")
    s.set_defaults(fn=cmd_save)

    sub.add_parser("refresh-data",
                   help="invalidate item+recipe static caches (after MCP DataTable edits)"
                   ).set_defaults(fn=cmd_refresh_data)

    s = sub.add_parser("auto", help="run headless UE automation tests (editor must be closed); exit code = pass/fail")
    s.add_argument("--filter", default="MOFramework", help="Automation RunTests filter (default: MOFramework)")
    s.add_argument("--force", action="store_true")
    s.set_defaults(fn=cmd_auto)

    s = sub.add_parser("build", help="UBT build (refuses if editor is running)")
    s.add_argument("--target", default="MO57Editor")
    s.add_argument("--force", action="store_true")
    s.set_defaults(fn=cmd_build)

    s = sub.add_parser("editor", help="editor lifecycle")
    s.add_argument("action", choices=["start", "stop", "wait"])
    s.set_defaults(fn=cmd_editor)

    s = sub.add_parser("cycle", help="close editor -> build -> reopen -> [--boot] -> [--test]")
    s.add_argument("--target", default="MO57Editor")
    s.add_argument("--boot", action="store_true")
    s.add_argument("--seed", type=int)
    s.add_argument("--test", action="store_true")
    s.set_defaults(fn=cmd_cycle)

    a = p.parse_args()
    try:
        a.fn(a)
    except BrokenPipeError:
        sys.exit(0)
    except OSError as e:
        if e.errno in (22, 32):  # Windows broken-pipe flavors (head/Select -First)
            sys.exit(0)
        raise


if __name__ == "__main__":
    main()
