"""MO57 promo capture: the real game, on a fixed time step, in 4K, one shot at a time. No OS input: every camera and action is a console verb or Python inside the game.

    python Tools/promo/capture.py boot [--seed N] [--fps 96] [--res 3840x2160] [--sound]     a -game instance named "promo" in a new world, through the creator's Begin; waits for READY
    python Tools/promo/capture.py shot ID [--dry] [--still]                                   one shot from Tools/promo/shots.json: settles, records (MyScreen, window, NVENC), checks the frame count, cuts it
    python Tools/promo/capture.py all [--from ID]                                             every shot still missing a take
    python Tools/promo/capture.py cut ID                                                      the recording -> Saved/Promo/mezz/ID.mov (the sub-frames blended into a real 180 degree shutter)
    python Tools/promo/capture.py run "console line"                                          a console line to the instance
    python Tools/promo/capture.py where                                                       the pawn's location, the clock, the biome
    python Tools/promo/capture.py frames ID [out.png]                                         a sheet of a take's first, middle and last frames, to LOOK at
    python Tools/promo/capture.py bed NAME [--seconds 25] [--at X Y Z] [--clock H M] [--weather P]   the game's own sound where the pawn stands, from its master submix (needs an instance booted with --sound --fps 0): Saved/Promo/audio/game/NAME.wav
    python Tools/promo/capture.py scout NAME [--biome B | --island N | --ravine N | --lake N | --poi P | --at X Y Z] [--hour H] [--weather P] [--wait 45]   looks round a place from 6 views: Saved/Promo/scout/NAME_sheet.jpg + NAME.json (the spot)
    python Tools/promo/capture.py look NAME X Y Z TX TY TZ [--fov 50] [--hour H] [--weather P] [--wait 6] [--pawn]   one framing: Saved/Promo/scout/NAME.jpg
    python Tools/promo/capture.py fly X Y Z | walk                                            the pawn hidden and flying at a spot (the world builds round the pawn) | back on its feet
    python Tools/promo/capture.py stop                                                        the instance, gracefully

Why this way (Docs/Promo_Video.md section 3): 96 simulation steps a second, two of every four blended to one film frame at 24 fps, is a 180 degree shutter with real motion through the frame, and any hitch
in the machine cannot reach the picture. Movie Render Queue was the other candidate; it needs a level sequence asset and its plugin enabled for a world that is built at run time: kept in reserve.
Close the editor first: it starves a -game instance's vegetation (TRAPS). Claim the window in claude-codex-coop.md.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import ue  # noqa: E402
import ue_inst as ui  # noqa: E402

NAME = "promo"
SHOTS = os.path.join(HERE, "shots.json")
WORK = os.path.join(ROOT, "Saved", "Promo", "shots")
MEZZ = os.path.join(ROOT, "Saved", "Promo", "mezz")
MYSCREEN = r"D:\MyScreen\myscreen.py"
SEQ = "Content/Python/promo_shot.py"
SUB = 4          # simulation frames per film frame (96 / 24)
SHUTTER = 2      # of those, how many are blended: 180 degrees


def load_shots():
    with open(SHOTS, encoding="utf-8") as f:
        return json.load(f)


def console(line):
    ui.console(NAME, line)


def query(code, key, timeout=60):
    _, out = ui.call(NAME, ["py", "-c", code], timeout=timeout)
    rows = re.findall(r"^%s (.*)$" % re.escape(key), out, re.M)     # whole lines: the bridge also echoes the submitted code, which contains the key
    return rows[-1].strip() if rows else ("ERR " + out[-600:])


WHERE = r'''
import unreal
p = unreal.GameplayStatics.get_player_pawn(world, 0)
o = p.get_actor_location()
out("WHERE %.1f %.1f %.1f" % (o.x, o.y, o.z))
'''


def boot(a):
    w, h = (int(v) for v in a.res.lower().split("x"))
    extra = ("-usefixedtimestep", "-fps=%d" % a.fps) if a.fps else ()
    if a.sound:
        extra = tuple(extra) + ("-DeterministicAudio", "-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0")     # the mix runs and is metered without a speaker or the window having focus (the project mutes when unfocused)
    ui.start(NAME, ue.UPROJECT, ue.EDITOR_EXE, creator=True, res=(w, h), nosteam=True, sound=a.sound, extra=extra)
    if not ui.wait_ready(NAME, 600):
        sys.exit("FAIL: the instance did not boot")
    ui.call(NAME, ["py", "-c", "import agent_test_lib as atl; atl.skip_intro(world, out)"], timeout=30)
    off = ui.log_size(NAME)
    console("MO.NewGame %d" % a.seed)
    if not ui.wait_log(NAME, r"Character creator opened", since=off, timeout=900):
        sys.exit("FAIL: no character creator")
    time.sleep(8)
    console("MO.Test.ClickWidget BeginButton")
    ok = ui.wait_log(NAME, r"\[MOReadiness\] READY", since=off, timeout=900)     # from before the click: on the fixed step the world can be ready while the creator is still open
    print("READY" if ok else "no READY line; look at the log", flush=True)
    console("MO.Weather.Director 0")
    console("r.setres %dx%dwf" % (w, h))
    for line in QUALITY:
        console(line)
    print("BOOTED seed", a.seed, flush=True)


# what a player's Epic settings lack and a film can afford: nothing here changes what exists in the world, only how finely it is drawn
QUALITY = [
    "r.ScreenPercentage 100",
    "r.AntiAliasingMethod 4",
    "r.MotionBlurQuality 0",            # the shutter is real: the blended frames make the blur
    "r.VolumetricRenderTarget 0",       # at 4K the volumetric clouds' low-res buffer shows blocks (SteamArt/README.md)
    "r.Lumen.ScreenProbeGather.DownsampleFactor 8",
    "r.Shadow.Virtual.ResolutionLodBiasDirectional -1",
    "foliage.LODDistanceScale 2",
    "r.Nanite.MaxPixelsPerEdge 0.5",
    "r.Streaming.PoolSize 12000",
    "r.ViewDistanceScale 2",
    "t.MaxFPS 0",
    "r.VSync 0",
]


def session_dir():
    rec = os.path.join(r"D:\MyScreen", "recordings")
    dirs = [os.path.join(rec, d) for d in os.listdir(rec)]
    return max((d for d in dirs if os.path.isdir(d)), key=os.path.getmtime)


def shot(a):
    shots = load_shots()
    if a.id not in shots:
        sys.exit("no shot %s in shots.json" % a.id)
    job = dict(shots[a.id])
    job["id"] = a.id
    job.setdefault("fps", 96)
    film_frames = int(job.get("film_frames", 0))
    job["frames"] = film_frames * SUB
    os.makedirs(WORK, exist_ok=True)
    if a.still:
        job["frames"] = 3
        job["preroll"] = 2
        job["slate"] = False
        job["stills"] = [1]
        job["settle"] = job.get("settle_still", job.get("settle", 15))
    if a.draft:
        # the whole action, the real way, but a still only every N film frames: quick to look at before a take
        job["stills"] = list(range(0, job["frames"], SUB * a.draft))
        job["settle"] = min(float(job.get("settle", 15)), a.draft_settle)
    with open(os.path.join(WORK, "current_job.json"), "w", encoding="utf-8") as f:
        json.dump(job, f, indent=1)
    if a.dry:
        print(json.dumps(job, indent=1))
        return
    off = ui.log_size(NAME)
    recording = False
    png = not a.still and not a.norec and not a.recording and not a.draft and job.get("png", True)
    job["recording"] = bool(a.recording)
    if a.norec:
        job["png"] = False
    with open(os.path.join(WORK, "current_job.json"), "w", encoding="utf-8") as f:
        json.dump(job, f, indent=1)
    before = set(screenshots())
    if a.recording:
        r = subprocess.run([sys.executable, MYSCREEN, "start", "--name", a.id, "--window-exe", "UnrealEditor", "--fps", "240", "--cq", str(a.cq), "--no-cursor", "--max-minutes", "60", "--stall-restart", "0"], capture_output=True, text=True)
        print("MYSCREEN", r.stdout.strip()[-200:], flush=True)
        recording = True
    t0 = time.time()
    _, out = ui.call(NAME, ["seq", os.path.join(ROOT, SEQ), "--timeout", str(max(1800, job["frames"] * 2))], timeout=max(1800, job["frames"] * 2) + 60)
    print(out[-800:], flush=True)
    if png:
        collect(a.id, before, 2 * film_frames, time.time() - t0, off)
        cut_png(a.id)
    if recording:
        r = subprocess.run([sys.executable, MYSCREEN, "stop"], capture_output=True, text=True)
        print("MYSCREEN", r.stdout.strip()[-400:], flush=True)
        with open(os.path.join(WORK, a.id + ".take.json"), "w", encoding="utf-8") as f:
            json.dump({"session": session_dir(), "frames": job["frames"], "preroll": job.get("preroll", 96), "time": time.strftime("%Y-%m-%d %H:%M:%S"), "wall": time.time() - t0}, f, indent=1)
        cut(a.id)
    if a.draft:
        time.sleep(4)
        draft_sheet(a.id, before)
    if a.still:
        time.sleep(3)
        src = newest_shot()
        if src:
            dest = os.path.join(WORK, a.id + "_still.png")
            shutil.copy2(src, dest)
            print("STILL", dest)


SCREENSHOTS = os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor")
RAW = os.path.join(ROOT, "Saved", "Promo", "raw")


def draft_sheet(shot_id, before):
    """A draft's stills as one sheet: Saved/Promo/shots/ID_draft.jpg."""
    from PIL import Image
    now = sorted((f for f in screenshots() if f not in before), key=lambda f: int(re.sub(r"\D", "", f) or 0))
    W, H, cols = 800, 450, 3
    rows = (len(now) + cols - 1) // cols
    sheet = Image.new("RGB", (W * cols, H * max(rows, 1)))
    for i, f in enumerate(now):
        path = os.path.join(SCREENSHOTS, f)
        with Image.open(path) as im:
            sheet.paste(im.convert("RGB").resize((W, H)), ((i % cols) * W, (i // cols) * H))
        os.remove(path)
    out = os.path.join(WORK, shot_id + "_draft.jpg")
    sheet.save(out, quality=86)
    print("DRAFT", out, len(now), "stills", flush=True)


def screenshots():
    os.makedirs(SCREENSHOTS, exist_ok=True)
    return [f for f in os.listdir(SCREENSHOTS) if f.lower().endswith(".png")]


def newest_shot():
    files = [os.path.join(SCREENSHOTS, f) for f in screenshots()]
    return max(files, key=os.path.getmtime) if files else None


def lost_frames(off, found):
    """A screenshot now and then does not come back. The game's log says which frame each one was taken in, so the holes are known: take positions in the pattern (2 of every 4 simulation frames)."""
    rows = [int(m) for m in re.findall(r"\]\[\s*(\d+)\]LogCore: Display: Tracing Screenshot", ui.read_log_from(NAME, off))]
    rows = rows[-found:]
    pos, have = 0, []
    for i, f in enumerate(rows):
        if i:
            pos += (f - rows[i - 1]) % 1000
        have.append(pos // SUB * SHUTTER + pos % SUB)
    return have


def collect(shot_id, before, expected, wall, off=0):
    """The take's screenshots, in the order they were asked for, into Saved/Promo/raw/ID/s_#####.png. The sequence has ended when this runs, and the writes are synchronous, so a short wait is enough.
    A lost frame is filled from its neighbour (one film frame without its blend: the eye cannot see it) and reported; more than 2 lost is a failed take."""
    deadline = time.time() + 20
    last = -1
    while time.time() < deadline:
        now = [f for f in screenshots() if f not in before]
        if len(now) >= expected and len(now) == last:
            break
        last = len(now)
        time.sleep(3)
    now = sorted((f for f in screenshots() if f not in before), key=lambda f: int(re.sub(r"\D", "", f) or 0))
    dest = os.path.join(RAW, shot_id)
    shutil.rmtree(dest, ignore_errors=True)
    os.makedirs(dest)
    places = list(range(len(now)))
    filled = []
    if len(now) != expected and off is not None:
        places = lost_frames(off, len(now))
    for i, f in zip(places, now):
        shutil.move(os.path.join(SCREENSHOTS, f), os.path.join(dest, "s_%05d.png" % i))
    if len(now) != expected:
        for i in range(expected):
            path = os.path.join(dest, "s_%05d.png" % i)
            if not os.path.isfile(path):
                near = next((os.path.join(dest, "s_%05d.png" % j) for j in sorted(range(expected), key=lambda j: abs(j - i)) if os.path.isfile(os.path.join(dest, "s_%05d.png" % j))), None)
                if near:
                    shutil.copy2(near, path)
                    filled.append(i)
    ok = len(now) == expected or (len(filled) <= 2 and len(now) + len(filled) == expected)
    with open(os.path.join(WORK, shot_id + ".take.json"), "w", encoding="utf-8") as f:
        json.dump({"raw": dest, "frames": expected, "found": len(now), "filled": filled, "frames_ok": ok, "time": time.strftime("%Y-%m-%d %H:%M:%S"), "wall": wall}, f, indent=1)
    print("TAKE %s: %d of %d lossless frames%s, %.0f s" % (shot_id, len(now), expected, (", filled " + str(filled)) if filled else "", wall), flush=True)
    if not ok:
        print("WARNING: frames lost; retake %s" % shot_id, flush=True)


def cut_png(shot_id):
    """Lossless frames -> mezzanine: each pair blended (SHUTTER of SUB simulation frames: a real 180 degree shutter), ProRes 4444 at 24 fps."""
    with open(os.path.join(WORK, shot_id + ".take.json"), encoding="utf-8") as f:
        take = json.load(f)
    os.makedirs(MEZZ, exist_ok=True)
    out = os.path.join(MEZZ, shot_id + ".mov")
    vf = ("format=gbrpf32le,tmix=frames=%d:weights='%s',select='not(mod(n-%d\\,%d))',setpts=N/(24*TB),"
          "scale=in_range=pc:out_range=tv:out_color_matrix=bt709:flags=accurate_rnd+full_chroma_int,format=yuv444p10le") % (SHUTTER, " ".join(["1"] * SHUTTER), SHUTTER - 1, SHUTTER)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", "48", "-i", os.path.join(take["raw"], "s_%05d.png"), "-vf", vf, "-r", "24", "-fps_mode", "cfr", "-c:v", "prores_ks", "-profile:v", "4", "-qscale:v", "6", "-vendor", "apl0",
                    "-color_primaries", "bt709", "-color_trc", "iec61966-2-1", "-colorspace", "bt709", "-color_range", "tv", out], check=True)
    print("MEZZ", out, os.path.getsize(out) // (1024 * 1024), "MB", flush=True)


def probe_lumas(path):
    """Per-frame mean brightness of a recording, cheaply: scaled down, read as raw grey."""
    p = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-i", path, "-vf", "scale=32:18,format=gray", "-fps_mode", "passthrough", "-f", "rawvideo", "-"], stdout=subprocess.PIPE)
    data = p.stdout.read()
    p.wait()
    n = len(data) // (32 * 18)
    return [sum(data[i * 576:(i + 1) * 576]) / 576.0 for i in range(n)]


def find_slates(lumas):
    """The black-white-(preroll) and last black-white-black marks: frame indices of the end of the first white and the start of the last."""
    white = [i for i, v in enumerate(lumas) if v > 200]
    if len(white) < 2:
        return None, None
    first_end = white[0]
    while first_end + 1 < len(white) and white[first_end + 1 - white[0]] == first_end + 1:
        first_end += 1
    return first_end, white[-1]


def cut(shot_id):
    """Recording -> mezzanine: from the slate find frame 0, check the count, blend SHUTTER of every SUB frames, write ProRes 4444 at 24 fps."""
    with open(os.path.join(WORK, shot_id + ".take.json"), encoding="utf-8") as f:
        take = json.load(f)
    with open(os.path.join(take["session"], "manifest.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    segs = [os.path.join(take["session"], s["file"] if isinstance(s, dict) and "file" in s else s) for s in manifest.get("segments", [])] or \
           sorted(os.path.join(take["session"], f) for f in os.listdir(take["session"]) if f.endswith(".mkv"))
    if len(segs) != 1:
        print("WARNING: %d segments: the capture restarted; retake %s" % (len(segs), shot_id))
    src = segs[0]
    lumas = probe_lumas(src)
    end_first, start_last = find_slates(lumas)
    if end_first is None:
        sys.exit("no slate found in " + src)
    preroll = take["preroll"]
    first = end_first + 1 + preroll
    expected = take["frames"]
    got = start_last - first - 3     # the last mark's black is 3 frames before its white
    print("CUT %s: %d frames recorded; slate ends at %d; expected %d frames between the marks, found %d" % (shot_id, len(lumas), end_first, expected, got), flush=True)
    take["frames_found"] = got
    take["frames_ok"] = abs(got - expected) <= 1
    if not take["frames_ok"]:
        print("WARNING: frame count off by %d: dropped frames in the recording. Retake or lower the frame rate." % (got - expected))
    os.makedirs(MEZZ, exist_ok=True)
    out = os.path.join(MEZZ, shot_id + ".mov")
    weights = " ".join(["1"] * SHUTTER)
    vf = ("trim=start_frame=%d:end_frame=%d,setpts=N/(96*TB),format=yuv444p16le,tmix=frames=%d:weights='%s',select='not(mod(n-%d\\,%d))',setpts=N/(24*TB),format=yuv444p10le"
          % (first, first + expected, SHUTTER, weights, SHUTTER - 1, SUB))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-vf", vf, "-r", "24", "-fps_mode", "cfr", "-c:v", "prores_ks", "-profile:v", "4", "-qscale:v", "6", "-vendor", "apl0",
                    "-color_primaries", "bt709", "-color_trc", "iec61966-2-1", "-colorspace", "bt709", out], check=True)
    with open(os.path.join(WORK, shot_id + ".take.json"), "w", encoding="utf-8") as f:
        json.dump(take, f, indent=1)
    print("MEZZ", out, flush=True)


def frames_sheet(shot_id, out=None):
    src = os.path.join(MEZZ, shot_id + ".mov")
    out = out or os.path.join(WORK, shot_id + "_sheet.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-vf", "select='eq(n,0)+eq(n,trunc(prev_selected_n+1)*0)+not(mod(n,40))',scale=960:-2,tile=3x2", "-frames:v", "1", "-fps_mode", "vfr", out], check=True)
    print("SHEET", out)


BED = r'''
import unreal, os, time
lib = unreal.AudioMixerLibrary
if {stage!r} == "start":
    lib.start_recording_output(world, {seconds})
    out("BED recording")
else:
    lib.stop_recording_output(world, unreal.AudioRecordingExportType.WAV_FILE, {name!r}, {path!r})
    out("BED saved")
'''


def bed(a):
    """Record what the game plays, through its own mixer. The instance must run in real time with sound (boot --sound --fps 0, which adds -DeterministicAudio: the mix runs whether the window has focus or not, Docs/Audio.md)."""
    dest = os.path.join(ROOT, "Saved", "Promo", "audio", "game")
    os.makedirs(dest, exist_ok=True)
    if a.at:
        console("MO.Player.Teleport %s %s %s" % tuple(a.at))
        time.sleep(float(a.settle))
    if a.clock:
        console("MO.Clock.SetTime %s %s" % tuple(a.clock))
    if a.weather:
        console("MO.Weather.SetPreset " + a.weather)
    if a.look:
        console(a.look)
    time.sleep(3)
    ui.call(NAME, ["py", "-c", BED.format(stage="start", seconds=a.seconds, name=a.name, path=dest.replace("\\", "/"))], timeout=60)
    time.sleep(a.seconds)
    _, out = ui.call(NAME, ["py", "-c", BED.format(stage="stop", seconds=a.seconds, name=a.name, path=dest.replace("\\", "/"))], timeout=120)
    print(out[-300:], flush=True)
    time.sleep(2)
    path = os.path.join(dest, a.name + ".wav")
    if os.path.isfile(path):
        r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", path, "-af", "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True)
        print("BED", path, os.path.getsize(path), [ln.strip() for ln in r.stderr.splitlines() if " I:" in ln or "Peak:" in ln][-2:])
    else:
        print("BED FAILED: no file at", path)


def log_since(off):
    return ui.read_log_from(NAME, off)


def shoot_to(path):
    """A viewport screenshot (`shot`: no HUD) moved to path."""
    before = newest_shot()
    console("shot")
    for _ in range(40):
        time.sleep(0.5)
        now = newest_shot()
        if now and now != before:
            time.sleep(0.5)
            shutil.move(now, path)
            return True
    return False


def scout(a):
    """Go somewhere and look round it. Spots come from the game's own finders, so they exist in this seed."""
    os.makedirs(os.path.join(ROOT, "Saved", "Promo", "scout"), exist_ok=True)
    off = ui.log_size(NAME)
    spot = None
    if a.at:
        spot = [float(v) for v in a.at]
        console("MO.Player.Teleport %.0f %.0f %.0f" % tuple(spot))
    elif a.biome:
        console("MO.WorldGen.FindBiomeSpot %s 40 %sgo" % (a.biome, "rough " if a.rough else ""))
    elif a.poi:
        console("poi " + a.poi)
    elif a.island is not None or a.ravine is not None or a.lake is not None:
        finder, tag, idx = ("FindIsland", "Island", a.island) if a.island is not None else (("FindRavine", "Ravine", a.ravine) if a.ravine is not None else ("FindLake", "Lake", a.lake))
        console("MO.WorldGen.%s 600000" % finder)
        time.sleep(3)
        text = log_since(off)
        rows = re.findall(r"WORLDGEN %s #(\d+) [^\r\n]*?x=(-?\d+) y=(-?\d+)(?: top=(-?\d+))?" % tag, text)
        print("\n".join(ln.split("[MOQUERY] ")[-1].strip()[:170] for ln in text.splitlines() if ("WORLDGEN " + tag) in ln or ("WORLDGEN " + finder) in ln), flush=True)
        if not rows or idx >= len(rows):
            sys.exit("not found")
        r = rows[idx]
        z = float(r[3]) if r[3] else 30000.0
        spot = [float(r[1]), float(r[2]), z + 400.0]
        console("MO.Player.Teleport %.0f %.0f %.0f" % tuple(spot))
    if a.hour is not None:
        console("MO.Clock.SetTime %d %d" % (int(a.hour), int((a.hour % 1) * 60)))
    if a.weather:
        console("MO.Weather.SetPreset " + a.weather)
    time.sleep(a.wait)
    w = query(WHERE, "WHERE")
    print("WHERE", w, flush=True)
    x, y, z = [float(v) for v in w.split()]
    views = [(170, 0, 0, -8, 75, "eye0"), (170, 90, 0, -8, 75, "eye90"), (170, 180, 0, -8, 75, "eye180"), (170, 270, 0, -8, 75, "eye270"), (3500, 45, -5000, -24, 70, "high0"), (3500, 225, -5000, -24, 70, "high180")]
    import math
    from PIL import Image, ImageDraw
    frames = []
    for up, yaw, back, pitch, fov, label in views:
        fx, fy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        cx, cy, cz = x + fx * back, y + fy * back, z + up
        tx, ty, tz = cx + fx * 10000 * math.cos(math.radians(pitch)), cy + fy * 10000 * math.cos(math.radians(pitch)), cz + 10000 * math.sin(math.radians(pitch))
        console("MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f %d" % (cx, cy, cz, tx, ty, tz, fov))
        time.sleep(a.view_wait)
        path = os.path.join(ROOT, "Saved", "Promo", "scout", "%s_%s.png" % (a.name, label))
        if shoot_to(path):
            frames.append((label, path))
    console("MO.Cam.Reset")
    W, H = 960, 540
    sheet = Image.new("RGB", (W * 3, H * 2))
    for i, (label, path) in enumerate(frames):
        with Image.open(path) as im:
            sheet.paste(im.convert("RGB").resize((W, H)), ((i % 3) * W, (i // 3) * H))
        ImageDraw.Draw(sheet).text(((i % 3) * W + 8, (i // 3) * H + 8), label, fill=(255, 255, 0))
    out = os.path.join(ROOT, "Saved", "Promo", "scout", a.name + "_sheet.jpg")
    sheet.save(out, quality=88)
    with open(os.path.join(ROOT, "Saved", "Promo", "scout", a.name + ".json"), "w", encoding="utf-8") as f:
        json.dump({"pawn": [x, y, z], "hour": a.hour, "weather": a.weather}, f)
    print("SCOUT", out, flush=True)


FLY = r"""
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
pawn.set_actor_location(unreal.Vector(%f, %f, %f), False, True)
pawn.set_actor_hidden_in_game(True)
for a in pawn.get_attached_actors():
    a.set_actor_hidden_in_game(True)
out('FLOWN ok')
"""
WALK = r"""
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FALLING)
pawn.set_actor_hidden_in_game(False)
for a in pawn.get_attached_actors():
    a.set_actor_hidden_in_game(False)
out('WALK ok')
"""


def fly(x, y, z):
    """The pawn, hidden and flying, at this spot: the world (voxels, vegetation, the streaming) builds round the pawn."""
    print(query(FLY % (x, y, z), "FLOWN"), flush=True)


def look(a):
    """One framing, to judge it: the photo camera at a pose, a wait for the world to load round it, a still in Saved/Promo/scout/NAME.png (and a small .jpg to read)."""
    v = [float(x) for x in a.pose]
    if a.pawn:
        fly(v[0], v[1], v[2])
    if a.hour is not None:
        console("MO.Clock.SetTime %d %d" % (int(a.hour), int((a.hour % 1) * 60)))
    if a.weather:
        console("MO.Weather.SetPreset " + a.weather)
    console("MO.Cam.LookAt %.1f %.1f %.1f %.1f %.1f %.1f %.1f" % (v[0], v[1], v[2], v[3], v[4], v[5], a.fov))
    time.sleep(a.wait)
    d = os.path.join(ROOT, "Saved", "Promo", "scout")
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, a.name + ".png")
    if not shoot_to(path):
        sys.exit("no screenshot")
    from PIL import Image
    with Image.open(path) as im:
        im.convert("RGB").resize((1600, 900)).save(os.path.join(d, a.name + ".jpg"), quality=88)
    print("LOOK", os.path.join(d, a.name + ".jpg"), flush=True)


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="verb")
    fl = sp.add_parser("fly")
    fl.add_argument("x")
    fl.add_argument("y")
    fl.add_argument("z")
    sp.add_parser("walk")
    lk = sp.add_parser("look")
    lk.add_argument("name")
    lk.add_argument("pose", nargs=6, help="camera x y z, target x y z")
    lk.add_argument("--fov", type=float, default=50.0)
    lk.add_argument("--hour", type=float)
    lk.add_argument("--weather")
    lk.add_argument("--wait", type=float, default=6.0)
    lk.add_argument("--pawn", action="store_true", help="teleport the pawn to the camera first, so the world loads round it")
    so = sp.add_parser("scout")
    so.add_argument("name")
    so.add_argument("--biome")
    so.add_argument("--rough", action="store_true")
    so.add_argument("--poi")
    so.add_argument("--island", type=int)
    so.add_argument("--ravine", type=int)
    so.add_argument("--lake", type=int)
    so.add_argument("--at", nargs=3)
    so.add_argument("--hour", type=float)
    so.add_argument("--weather")
    so.add_argument("--wait", type=float, default=45.0)
    so.add_argument("--view-wait", dest="view_wait", type=float, default=4.0)
    be = sp.add_parser("bed")
    be.add_argument("name")
    be.add_argument("--seconds", type=float, default=25.0)
    be.add_argument("--at", nargs=3)
    be.add_argument("--clock", nargs=2)
    be.add_argument("--weather")
    be.add_argument("--look")
    be.add_argument("--settle", default="20")
    b = sp.add_parser("boot")
    b.add_argument("--seed", type=int, default=777)
    b.add_argument("--fps", type=int, default=96, help="0 = real time (scouting, audio)")
    b.add_argument("--res", default="3840x2160")
    b.add_argument("--sound", action="store_true")
    s = sp.add_parser("shot")
    s.add_argument("id")
    s.add_argument("--dry", action="store_true")
    s.add_argument("--still", action="store_true")
    s.add_argument("--norec", action="store_true")
    s.add_argument("--cq", type=int, default=12)
    s.add_argument("--draft", type=int, default=0, metavar="N", help="a still every N film frames instead of a take: Saved/Promo/shots/ID_draft.jpg")
    s.add_argument("--draft-settle", dest="draft_settle", type=float, default=8.0)
    s.add_argument("--recording", action="store_true", help="MyScreen's window recording instead of lossless screenshots (softer: section 3 of the book says why it is not the default)")
    sp.add_parser("stop")
    c = sp.add_parser("cut")
    c.add_argument("id")
    r = sp.add_parser("run")
    r.add_argument("line")
    sp.add_parser("where")
    f = sp.add_parser("frames")
    f.add_argument("id")
    f.add_argument("out", nargs="?")
    a = ap.parse_args()
    if a.verb == "boot":
        boot(a)
    elif a.verb == "look":
        look(a)
    elif a.verb == "fly":
        fly(float(a.x), float(a.y), float(a.z))
    elif a.verb == "walk":
        print(query(WALK, "WALK"))
    elif a.verb == "bed":
        bed(a)
    elif a.verb == "scout":
        scout(a)
    elif a.verb == "shot":
        shot(a)
    elif a.verb == "cut":
        (cut_png if os.path.isdir(os.path.join(RAW, a.id)) else cut)(a.id)
    elif a.verb == "run":
        console(a.line)
    elif a.verb == "where":
        print(query(WHERE, "WHERE"))
    elif a.verb == "frames":
        frames_sheet(a.id, a.out)
    elif a.verb == "stop":
        ui.stop(NAME)
    else:
        ap.print_help()


if __name__ == "__main__":
    main()
