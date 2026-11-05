"""Run the promo kit's Blender builds on DARKTOWER's GPU (so this PC's stays quiet for whoever holds the Unreal window). CLAUDE.md § The second PC.

    python Tools/promo/darktower.py sync                       the kit's scripts and fonts to D:/MO57Promo (Blender 5.2 is already a portable folder there)
    python Tools/promo/darktower.py run NAME SCRIPT '<json>'   a scheduled task (it outlives the SSH session): Tools/promo/kit/SCRIPT with that config; frames in D:/MO57Promo/out/NAME
    python Tools/promo/darktower.py state NAME                 the task's state, frames written so far, the log's last lines
    python Tools/promo/darktower.py wait NAME [SECONDS]        returns when the task stopped (a foreground wait, for one job you just started)
    python Tools/promo/darktower.py pull NAME [DEST]           scp the frames back (default Saved/Promo/kit/NAME)

The config's "out" is filled in; "blend": true saves the built scene beside the frames. Blender here runs without a window; nothing is written outside D:/MO57Promo on that PC.
Started by hand, nothing polls: `wait` is what a session runs in the background when it wants a nudge.
"""
import json
import os
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
REMOTE = "D:/MO57Promo"
REMOTE_WIN = r"D:\MO57Promo"
BLENDER = REMOTE_WIN + r"\Blender 5.2\blender.exe"


def ssh(cmd, check=True, timeout=300):
    r = subprocess.run(["ssh", "darktower", cmd], capture_output=True, text=True, timeout=timeout)
    if check and r.returncode:
        sys.exit("ssh failed: " + r.stderr[-800:])
    return r.stdout


def sync():
    ssh(r"New-Item -ItemType Directory -Force %s\kit,%s\fonts,%s\jobs,%s\logs,%s\out | Out-Null" % ((REMOTE_WIN,) * 5))
    kit = os.path.join(HERE, "kit")
    files = [os.path.join(kit, f) for f in os.listdir(kit) if f.endswith((".py", ".json"))]
    subprocess.run(["scp", "-q"] + files + ["darktower:%s/kit/" % REMOTE], check=True)
    fonts = os.path.join(ROOT, "Saved", "Promo", "fonts")
    subprocess.run(["scp", "-q"] + [os.path.join(fonts, f) for f in os.listdir(fonts)] + ["darktower:%s/fonts/" % REMOTE], check=True)
    print("SYNC", len(files), "scripts")


def run(name, script, cfg):
    cfg = dict(cfg)
    cfg["out"] = "%s/out/%s" % (REMOTE, name)
    if cfg.get("blend") is True:
        cfg["blend"] = "%s/out/%s/%s.blend" % (REMOTE, name, name)
    with tempfile.TemporaryDirectory() as td:
        path = os.path.join(td, name + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(cfg, f)
        subprocess.run(["scp", "-q", path, "darktower:%s/jobs/%s.json" % (REMOTE, name)], check=True)
    ps = r'''
$cmd = '/c set "MO57_PROMO_FONTS={root}\fonts" && "{blender}" -b --factory-startup --python "{root}\kit\{script}" -- "{root}\jobs\{name}.json" > "{root}\logs\{name}.log" 2>&1'
$action = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument $cmd -WorkingDirectory '{root}'
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 12) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
Unregister-ScheduledTask -TaskName 'MO57Promo_{name}' -Confirm:$false -ErrorAction SilentlyContinue
Register-ScheduledTask -TaskName 'MO57Promo_{name}' -Action $action -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName 'MO57Promo_{name}'
Start-Sleep -Seconds 2
(Get-ScheduledTask -TaskName 'MO57Promo_{name}').State
'''.format(root=REMOTE_WIN, blender=BLENDER, script=script, name=name)
    print("RUN", name, ssh(ps).strip())


def state(name):
    ps = r'''
(Get-ScheduledTask -TaskName 'MO57Promo_{name}' -ErrorAction SilentlyContinue).State
(Get-ChildItem '{root}\out\{name}' -Filter *.png -ErrorAction SilentlyContinue | Measure-Object).Count
Get-Content '{root}\logs\{name}.log' -Tail 3 -ErrorAction SilentlyContinue
'''.format(root=REMOTE_WIN, name=name)
    return ssh(ps, check=False).strip()


def wait(name, seconds):
    t0 = time.time()
    while time.time() - t0 < seconds:
        text = state(name)
        if not text.startswith("Running"):
            print(text[-1500:])
            return
        time.sleep(20)
    print("STILL RUNNING", state(name)[-400:])


def pull(name, dest):
    dest = dest or os.path.join(ROOT, "Saved", "Promo", "kit", name)
    os.makedirs(dest, exist_ok=True)
    subprocess.run(["scp", "-q", "-r", "darktower:%s/out/%s/*" % (REMOTE, name), dest + "/"], check=False)
    print("PULLED", dest, len(os.listdir(dest)))


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    if a[0] == "sync":
        sync()
    elif a[0] == "run":
        run(a[1], a[2], json.loads(a[3]) if len(a) > 3 else {})
    elif a[0] == "state":
        print(state(a[1]))
    elif a[0] == "wait":
        wait(a[1], float(a[2]) if len(a) > 2 else 3600)
    elif a[0] == "pull":
        pull(a[1], a[2] if len(a) > 2 else None)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
