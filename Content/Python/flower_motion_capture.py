"""BR23-10: are the flowers blurry only while the camera moves, and does the anti-aliasing decide it? (Wes, Oct 23-24 2026: "a few of these
little flowers are blurry", "purple flowers ... weird and blurry in other places".)

A claude_seq sequence, run in PIE on the report's world seed (FLOWERCAP_VIEW below holds its spot, camera and time):
  1. the pawn to the report's spot at its time of day, time frozen, then a wait for the ground cover to generate;
  2. for each configuration (FLOWERCAP_CONFIGS, "name=cvar value;cvar value"): his camera (LookAt.View of the report) held still for
     HOLD_FRAMES (TSR converges), a strafe to the camera's right at STEP_CM per frame for PAN_FRAMES (a walk at 60 fps), a viewport screenshot
     IN motion (`shot`, the frame as displayed), then the end pose held for HOLD_FRAMES and a second screenshot (converged);
  3. logs [seq:...] FLOWERCAP lines with each screenshot's path, frames and seconds per frame.
Score the screenshots with Tools/flower_sharpness.py (edge contrast of the flowers' purple pixels, or a whole crop with --all).

    python Tools/ue.py boot --seed 10496                     (the report's world seed)
    python Tools/ue.py py -c "import os; os.environ['FLOWERCAP_CONFIGS'] = '...'; os.environ['FLOWERCAP_VIEW'] = '...'"
    python Tools/ue.py seq Content/Python/flower_motion_capture.py
"""
import glob
import math
import os
import time

import unreal

# A report's view: "x y z | cam x y z | pitch yaw | hh mm" in FLOWERCAP_VIEW (default: 34096019, seed 10496); 89300267 (seed 3014, the
# rainforest's palette flowers) is "963482 -2803102 22272 | 963393 -2802984 22387 | -45.0 -22.1 | 8 28".
_view = [part.split() for part in os.environ.get("FLOWERCAP_VIEW",
         "3238678 -1486421 24511 | 3238696 -1486232 24545 | -14.8 -71.9 | 17 46").split("|")]
SPOT = unreal.Vector(*(float(v) for v in _view[0]))
CAM = unreal.Vector(*(float(v) for v in _view[1]))
PITCH, YAW = (float(v) for v in _view[2])
HOUR, MINUTE = (int(v) for v in _view[3])
FOV = 90
HOLD_FRAMES = 90
PAN_FRAMES = 45
STEP_CM = 3.0
SHOTS = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots", "WindowsEditor")
CONFIGS = os.environ.get("FLOWERCAP_CONFIGS", "tsr=r.AntiAliasingMethod 4|taa=r.AntiAliasingMethod 2")


def sequence(ctx):
    w = ctx.world
    run = lambda c: unreal.SystemLibrary.execute_console_command(ctx.world, c)
    ctx.pawn.set_actor_location(SPOT, False, True)
    run("MO.Clock.SetTime %d %d" % (HOUR, MINUTE))
    run("MO.Clock.SetTimeScale 0")
    t0 = time.time()
    while time.time() - t0 < 45.0:   # streaming + the near ground-cover tier round the new spot
        yield 10
    p, y = math.radians(PITCH), math.radians(YAW)
    fwd = unreal.Vector(math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))
    right = unreal.Vector(-math.sin(y), math.cos(y), 0.0)

    def look(pos):
        tgt = pos + fwd * 1000.0
        run("MO.Cam.LookAt %.1f %.1f %.1f %.1f %.1f %.1f %d" % (pos.x, pos.y, pos.z, tgt.x, tgt.y, tgt.z, FOV))

    def newest():
        files = glob.glob(os.path.join(SHOTS, "*.png"))
        return max(files, key=os.path.getmtime) if files else None

    for spec in CONFIGS.split("|"):
        name, cvars = spec.split("=", 1)
        for cv in cvars.split(";"):
            run(cv.strip())
        look(CAM)
        yield HOLD_FRAMES
        t = time.time()
        for i in range(PAN_FRAMES):
            look(CAM + right * (STEP_CM * (i + 1)))
            yield 1
        spf = (time.time() - t) / PAN_FRAMES
        before = newest()
        run("shot")
        yield 3
        moving = newest()
        yield HOLD_FRAMES
        run("shot")
        yield 3
        still = newest()
        ctx.out("FLOWERCAP %s moving=%s still=%s (%.3f s per frame while panning; the previous shot was %s)" % (name, moving, still, spf, before))
    run("MO.Cam.Reset")
    run("r.AntiAliasingMethod 4")
