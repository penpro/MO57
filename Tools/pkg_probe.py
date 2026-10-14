"""pkg_probe -- run the PACKAGED Development build once with a list of console commands, wait for a marker, and print the log lines you ask for.

    python Tools/pkg_probe.py --cmds "MO.NewGame 1029024224" "MO.Test.AfterReady MO.Delay 25 MO.PCG.MeshSurvey 60" \
                              --until "MOQUERY\\] MESH SURVEY" --grep "MOReadiness\\] (READY|holding|released)" "MOQUERY" --timeout 240

Why: every packaged measurement (load timing, census, perf) needs the same launch / wait / stop / read cycle, and -ExecCmds splits on commas, so the
commands are passed as a list and joined here. The game's own log is the only output (the packaged build has no editor bridge).

Rules baked in:
  * refuses to start when another MO57.exe is running (it would own the log and the GPU, and perf numbers would be meaningless);
  * stops ONLY the process it launched (by pid, graceful WM_CLOSE first, then /F on that pid) -- never by image name;
  * the log is removed before the launch, so a stale verdict is never read;
  * --keep-log copies the log to Saved/Logs/pkg_probe/<label>.log for later comparison.
Exit code: 0 marker seen, 1 timeout / crash, 2 environment (no build, another game running).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EXE = os.path.join(ROOT, "Saved", "StagedBuilds_DevTest", "Windows", "MO57.exe")
LOG = os.path.join(ROOT, "Saved", "StagedBuilds_DevTest", "Windows", "MO57", "Saved", "Logs", "MO57.log")
KEEP_DIR = os.path.join(ROOT, "Saved", "Logs", "pkg_probe")
BASE_ARGS = "-NoSteam -NoQualityBenchmark -windowed -WinX=0 -WinY=0"
CRASH_RE = re.compile(r"Fatal error|Unhandled Exception|Assertion failed|=== Critical error ===")


# --shipdiag: the MO57ShipDiag target (Source/MO57ShipDiag.Target.cs) -- Shipping code, but MOFramework's UE_LOG reaches the log and -ExecCmds works.
# Staged by copying a Shipping package (its cook must match the code) to Saved/StagedBuilds_ShipDiag and dropping the exe next to the game's.
# Shipping keeps its saves, config and log under %LOCALAPPDATA%/MO57/Saved (the player's own settings: backed up and restored around the run).
SHIPDIAG_EXE = os.path.join(ROOT, "Saved", "StagedBuilds_ShipDiag", "Windows", "MO57", "Binaries", "Win64", "MO57ShipDiag-Win64-Shipping.exe")
SHIPDIAG_SAVED = os.path.join(os.environ.get("LOCALAPPDATA", ""), "MO57", "Saved")
SHIPDIAG_LOG = os.path.join(SHIPDIAG_SAVED, "Logs", "MO57ShipDiag.log")
IMAGE = "MO57.exe"


def running_games():
    out = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {IMAGE}", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    return [ln for ln in out.splitlines() if ln.startswith(f'"{IMAGE}"')]


def read_log():
    try:
        with open(LOG, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def child_pids(pid):
    """Processes whose parent is `pid` (the real game under the staged launcher)."""
    ps = f"Get-CimInstance Win32_Process -Filter 'ParentProcessId={int(pid)}' | ForEach-Object {{ $_.ProcessId }}"
    out = subprocess.run(["powershell.exe", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout
    return [int(x) for x in out.split() if x.strip().isdigit()]


def stop_child(child):
    if child and any(f'"{child}"' in ln for ln in running_games()):
        subprocess.run(["taskkill", "/PID", str(child), "/T", "/F"], capture_output=True)


def stop(pid):
    """The staged MO57.exe is a LAUNCHER: the game is its child (MO57/Binaries/Win64/MO57.exe). Stopping the launcher alone leaves the game running, holding the log, the GPU and
    the staging directory (the next BuildCookRun then fails with Error_FailedToDeleteStagingDirectory). So the whole tree of the pid we launched goes, never anything matched by name."""
    subprocess.run(["taskkill", "/PID", str(pid), "/T"], capture_output=True)
    for _ in range(20):
        if not running_games():
            return
        time.sleep(0.5)
    subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cmds", nargs="+", required=True, help="console commands, in order (joined into -ExecCmds)")
    ap.add_argument("--until", default=r"\[MOReadiness\] READY after", help="regex: stop once a log line matches it")
    ap.add_argument("--settle", type=float, default=2.0, help="seconds to keep running after the marker (lets the last lines flush)")
    ap.add_argument("--grep", nargs="*", default=[r"\[MOReadiness\] (READY|holding|released|vegetation stage capped)"], help="regexes of log lines to print")
    ap.add_argument("--timeout", type=float, default=240.0)
    ap.add_argument("--extra", default="", help="extra command-line arguments")
    ap.add_argument("--res", default="960x540", help="window size WxH (perf runs: say which; the default suits functional probes)")
    ap.add_argument("--keep-log", default="", help="label: copy the log to Saved/Logs/pkg_probe/<label>.log")
    ap.add_argument("--package", action="store_true", help="re-package the Development build first (RunUAT BuildCookRun; the editor must be closed)")
    ap.add_argument("--shipdiag", action="store_true", help="run the MO57ShipDiag Shipping executable instead of the Development package (see SHIPDIAG_EXE)")
    a = ap.parse_args()

    global EXE, LOG, IMAGE
    settings_backup = None
    if a.shipdiag:
        EXE, LOG, IMAGE = SHIPDIAG_EXE, SHIPDIAG_LOG, os.path.basename(SHIPDIAG_EXE)
        a.extra = f'{a.extra} -abslog="{SHIPDIAG_LOG}"'
        user_settings = os.path.join(SHIPDIAG_SAVED, "Config", "Windows", "GameUserSettings.ini")
        if os.path.exists(user_settings):
            settings_backup = (user_settings, open(user_settings, "rb").read())
    try:
        return run(a)
    finally:
        if settings_backup:
            with open(settings_backup[0], "wb") as f:
                f.write(settings_backup[1])
            print("[pkg_probe] restored", settings_backup[0])


def run(a):
    if a.package:
        sys.path.insert(0, HERE)
        import ue_inst   # the one packaging recipe (Saved/Logs/package.log)
        if not ue_inst._package_build():
            return 1
    if not os.path.exists(EXE):
        print("[pkg_probe] no packaged build at", EXE)
        return 2
    if running_games():
        print("[pkg_probe] another MO57.exe is running -- refusing (it would own the log and the GPU)")
        return 2
    if os.path.exists(LOG):
        os.remove(LOG)
    cmds = ",".join(c.replace(",", " ") for c in a.cmds)
    rx, ry = (int(v) for v in a.res.lower().split("x"))
    cmdline = f'"{EXE}" {BASE_ARGS} -ResX={rx} -ResY={ry} {a.extra} -ExecCmds="{cmds}"'
    flags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    proc = subprocess.Popen(cmdline, creationflags=flags, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    until = re.compile(a.until)
    t0 = time.time()
    child = proc.pid if a.shipdiag else None   # the diag exe IS the game (no launcher)
    for _ in range(0 if a.shipdiag else 20):   # the launcher starts the game within a second or two
        kids = child_pids(proc.pid)
        if kids:
            child = kids[0]
            break
        time.sleep(0.5)
    print(f"[pkg_probe] launcher pid {proc.pid}, game pid {child}: -ExecCmds=\"{cmds}\"", flush=True)
    verdict = 1
    while time.time() - t0 < a.timeout:
        if proc.poll() is not None:
            print(f"[pkg_probe] the game exited by itself after {time.time() - t0:.0f}s (code {proc.returncode})")
            verdict = 0 if until.search(read_log()) else 1
            break
        log = read_log()
        if CRASH_RE.search(log):
            print("[pkg_probe] crash marker in the log")
            break
        if until.search(log):
            time.sleep(a.settle)
            verdict = 0
            break
        time.sleep(1.0)
    else:
        print(f"[pkg_probe] TIMEOUT after {a.timeout:.0f}s waiting for /{a.until}/")
    if proc.poll() is None:
        stop(proc.pid)
    stop_child(child)   # an orphaned game (its launcher already gone) is not reached by the tree kill
    log = read_log()
    pats = [re.compile(p) for p in a.grep]
    for ln in log.splitlines():
        if any(p.search(ln) for p in pats):
            print(ln[:400])
    for m in CRASH_RE.finditer(log):
        print("[pkg_probe] CRASH:", log[m.start():m.start() + 300].replace("\n", " | "))
        verdict = 1
        break
    if a.keep_log:
        os.makedirs(KEEP_DIR, exist_ok=True)
        dst = os.path.join(KEEP_DIR, a.keep_log + ".log")
        shutil.copyfile(LOG, dst)
        print("[pkg_probe] log kept:", dst)
    print(f"[pkg_probe] {'OK' if verdict == 0 else 'FAILED'} in {time.time() - t0:.0f}s")
    return verdict


if __name__ == "__main__":
    sys.exit(main())
