"""MO57 promo: the co-op shot. Two real game processes in one LAN session, recorded live from the host: a fixed step cannot be used here, because a client's moves are stamped in real time and a host held back by
lossless screenshots would rubber-band it. So this one shot is a real-time window recording (Windows Graphics Capture and NVENC at constant QP), softer than the others, and says so in the book.

    python Tools/promo/coop_take.py [--seed 777] [--keep] [--retake]     Saved/Promo/mezz/G22_coop.mov
    python Tools/promo/coop_take.py --stop

The host films; both pawns walk by real injected input, each from its own process. Close the editor and every other instance first.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
sys.path.insert(0, HERE)
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402
import capture  # noqa: E402

HOST, CLIENT = "promo", "promoc"
SECONDS = 80 / 24.0
# two figures walking away from the lens towards the low sun, side by side, seen from behind (positions on seed 777's start meadow)
A = (700.0, 120.0, 900.0)
B = (880.0, 90.0, 900.0)
YAW = 268.0
JOB = {
    "id": "G22_coop", "clock": [17, 50], "director": False, "settle": 1, "preroll": 24, "fps": 60, "recording": True, "png": False, "slate": True, "realtime_seconds": SECONDS + 1.0, "ease": 0.5,
    "pawn": {"hide": False},
    "frames": 100000,
    "keys": [{"t": 0.0, "pos": [800, 700, 850], "look": [790, -900, 640], "fov": 30}, {"t": 1.0, "pos": [800, 480, 835], "look": [790, -900, 640], "fov": 30}],
    "after": ["MO.Test.InjectInput IA_MOMove 0 0 0.1"],
}


def wait_state(name, want, timeout=300):
    deadline = time.time() + timeout
    while time.time() < deadline:
        ui.console(name, "MO.State")
        off = ui.log_size(name)
        time.sleep(3)
        text = ui.read_log_from(name, max(off - 4000, 0))
        m = ui.STATE_RE.findall(text)
        if m and m[-1][0] == want and m[-1][2].lower() == "true":
            return True
        time.sleep(3)
    return False


def q(name, code, key, timeout=60):
    _, out = ui.call(name, ["py", "-c", code], timeout=timeout)
    rows = re.findall(r"^%s (.*)$" % key, out, re.M)
    return rows[-1].strip() if rows else "ERR " + out[-400:]


def boot(seed):
    ui.start(HOST, ue.UPROJECT, ue.EDITOR_EXE, creator=False, res=(3840, 2160), nosteam=True)
    ui.start(CLIENT, ue.UPROJECT, ue.EDITOR_EXE, creator=False, res=(960, 540), nosteam=True, pos=(40, 40))
    for n in (HOST, CLIENT):
        if not ui.wait_ready(n, 600):
            sys.exit("no boot: " + n)
        ui.call(n, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    time.sleep(2)
    off = ui.log_size(HOST)
    ui.console(HOST, "MO.Session.Host PromoCoop 4 %d" % seed)
    m = ui.wait_log(HOST, r"CreateSession succeeded|Already in a session|FAILED", since=off, timeout=60)
    print("HOST", m.group(0) if m else "no result", flush=True)
    off = ui.log_size(HOST)
    print("HOST READY", bool(ui.wait_log(HOST, r"\[MOReadiness\] READY", since=off, timeout=600)), flush=True)
    found, index = ui.find_session(CLIENT, "PromoCoop")
    print("FOUND", found, index, flush=True)
    off = ui.log_size(CLIENT)
    ui.console(CLIENT, "MO.Session.Join %s" % index)
    print("CLIENT READY", bool(ui.wait_log(CLIENT, r"\[MOReadiness\] READY", since=off, timeout=600)), flush=True)
    time.sleep(10)


PLACE = r"""
a = %r; b = %r; yaw = %f
rot = unreal.Rotator(); rot.yaw = yaw
p0 = unreal.GameplayStatics.get_player_pawn(world, 0)
p1 = unreal.GameplayStatics.get_player_pawn(world, 1)
unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(rot)
p0.set_actor_location_and_rotation(unreal.Vector(*a), rot, False, True)
if p1:
    p1.set_actor_location_and_rotation(unreal.Vector(*b), rot, False, True)
out('PLACED %%s %%s' %% (p0.get_name(), p1.get_name() if p1 else None))
"""


def host_hwnd():
    """The host's own window: MyScreen's exe match would take whichever instance is on top, so the capture is pointed at the HWND of the process this script launched."""
    import ctypes
    from ctypes import wintypes
    pid = ui.read_pid(HOST)
    user32 = ctypes.windll.user32
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def each(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            r = wintypes.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(r))
            found.append((r.right * r.bottom, hwnd))
        return True

    user32.EnumWindows(each, 0)
    return max(found)[1] if found else None


def record(out_path, cq=8, fps=120):
    hwnd = host_hwnd()
    if not hwnd:
        sys.exit("no host window")
    src = "gfxcapture=hwnd=%d:max_framerate=%d:capture_cursor=0:width=2560:height=1440:resize_mode=scale_aspect:scale_mode=bicubic" % (hwnd, fps)
    return subprocess.Popen(["ffmpeg", "-y", "-hide_banner", "-nostats", "-loglevel", "warning", "-filter_complex", src, "-c:v", "hevc_nvenc", "-preset", "p6", "-tune", "hq", "-rc", "constqp", "-qp", str(cq),
                             "-rgb_mode", "yuv444", "-profile:v", "rext", "-g", str(fps), "-bf", "0", "-fps_mode", "passthrough", out_path], stdin=subprocess.PIPE)


def take():
    # live, so it has to hold frame rate: 1440p at the game's own settings, not the film's; the client (a second process on this GPU) throttled and small
    for line in ("MO.Weather.Director 0", "r.setres 2560x1440wf", "r.MotionBlurQuality 0", "r.VSync 0", "t.MaxFPS 0", "MO.HUD.Mode off", "MO.Test.Base Harvey", "MO.Clock.SetTime 17 50"):
        ui.console(HOST, line)
    for line in ("MO.HUD.Mode off", "t.MaxFPS 20", "r.ScreenPercentage 50", "sg.ViewDistanceQuality 0", "sg.FoliageQuality 0", "sg.ShadowQuality 0", "sg.GlobalIlluminationQuality 0", "sg.ReflectionQuality 0"):
        ui.console(CLIENT, line)
    print(q(HOST, PLACE % (A, B, YAW), "PLACED"), flush=True)
    rot = "rot = unreal.Rotator(); rot.yaw = %f; unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(rot); out('TURNED ok')" % YAW
    print(q(CLIENT, rot, "TURNED"), flush=True)
    time.sleep(20)
    os.makedirs(capture.WORK, exist_ok=True)
    with open(os.path.join(capture.WORK, "current_job.json"), "w", encoding="utf-8") as f:
        json.dump(JOB, f, indent=1)
    src = os.path.join(capture.WORK, "G22_coop_raw.mkv")
    ui.console(HOST, "MO.Cam.LookAt %d %d %d %d %d %d %d" % (800, 700, 850, 790, -900, 640, 30))
    rec = record(src)
    time.sleep(1)
    # both walk from here on: each process holds its own key
    for n in (HOST, CLIENT):
        ui.console(n, "MO.Test.InjectInput IA_MOMove 0 1 40")
    _, out = ui.call(HOST, ["seq", os.path.join(ROOT, capture.SEQ), "--timeout", "120"], timeout=180)
    print(out[-400:], flush=True)
    ui.console(CLIENT, "MO.Test.InjectInput IA_MOMove 0 0 0.1")
    rec.communicate(b"q", timeout=30)
    lumas = capture.probe_lumas(src)
    end_first, _ = capture.find_slates(lumas)
    whites = [i for i, v in enumerate(lumas) if v > 200]
    last_start = whites[-1]
    while last_start - 1 in whites:
        last_start -= 1
    first = end_first + 1 + JOB["preroll"]
    print("RECORDING %d frames, slates %s..%s, rolling %d frames in %.1f s" % (len(lumas), end_first, last_start, last_start - first - 3, SECONDS + 1.0), flush=True)
    out_path = os.path.join(capture.MEZZ, "G22_coop.mov")
    vf = ("trim=start_frame=%d:end_frame=%d,setpts=PTS-STARTPTS,fps=24,scale=in_range=pc:out_range=tv:out_color_matrix=bt709:flags=accurate_rnd+full_chroma_int,format=yuv444p10le" % (first, last_start - 3))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-vf", vf, "-frames:v", "80", "-c:v", "prores_ks", "-profile:v", "4", "-qscale:v", "6", "-vendor", "apl0", "-color_primaries", "bt709", "-color_trc", "iec61966-2-1", "-colorspace", "bt709", "-color_range", "tv", out_path], check=True)
    print("MEZZ", out_path, flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--retake", action="store_true", help="the session is already up")
    ap.add_argument("--stop", action="store_true")
    a = ap.parse_args()
    if a.stop:
        ui.stop(CLIENT)
        ui.stop(HOST)
        return
    if not a.retake:
        boot(a.seed)
    take()
    if not a.keep:
        ui.stop(CLIENT)
        ui.stop(HOST)


if __name__ == "__main__":
    main()
