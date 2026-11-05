"""MO57 promo capture: one shot, driven frame by frame in a -game instance (a claude_seq sequence). Tools/promo/capture.py writes the job and reads the log; run it from there.

The instance runs on a fixed time step (-usefixedtimestep -fps=96), so a frame of simulation is always 1/96 s however long the GPU takes over it: no stutter, ever, and the world's
loading has more wall-clock time per second of film than a player would give it. Every frame the camera is set from the job's keys (a smooth path through key poses, the keys'
own timing eased at both ends); a slate marks the first and last frames so the recording can be cut to the exact frame (a window recording does not know the game's frame numbers).

The job file: Saved/Promo/shots/current_job.json
    id, frames (simulation frames in the shot), preroll (frames held on the first pose before the slate's end), keys: [{t: 0..1, pos: [x,y,z], look: [x,y,z], fov: deg, focus: cm or null}, ...]
    clock: [h, m] and timescale (the game clock), weather (a preset name), director (false: the weather stays), settle (seconds of wall-clock on the first pose before rolling)
    pawn: {hide: bool, at: [x,y,z] (teleport first), under_camera: bool (keep it under the camera so the world loads round the lens), base, item, ...}
    follow: {offset: [forward, right, up] cm in the pawn's yaw frame, look: [forward, right, up]}: a camera that goes where the pawn goes, smoothed
    before: [console lines], actions: [[frame, "console line"], ...], fstop, exposure ({bias} or {fixed: EV100}), handheld (cm), slate (bool), stills: [frame, ...]: `shot`s written at these frames
    showui (the pawn's own view and HUD, no photo camera)
Log lines for the driver: [seq:promo_shot] PROMO ...
"""
import json
import math
import os
import time

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
JOB = os.path.join(ROOT, "Saved", "Promo", "shots", "current_job.json")


def catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return [0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3) for a, b, c, d in zip(p0, p1, p2, p3)]


def ease(t, amount=1.0):
    """Eased through the middle of the move: a slow start and stop; amount 0 = linear (a move already under way when the cut comes)."""
    e = t * t * (3 - 2 * t)
    return t * (1 - amount) + e * amount


def sample(keys, t, field):
    """A value at t along the keys' spline."""
    n = len(keys)
    if n == 1:
        return keys[0][field]
    i = 0
    while i < n - 2 and t > keys[i + 1]["t"]:
        i += 1
    span = max(keys[i + 1]["t"] - keys[i]["t"], 1e-6)
    u = min(max((t - keys[i]["t"]) / span, 0.0), 1.0)
    a = keys[max(i - 1, 0)][field]
    b = keys[i][field]
    c = keys[i + 1][field]
    d = keys[min(i + 2, n - 1)][field]
    return catmull(a, b, c, d, u)


def scalar(keys, t, field, default):
    if field not in keys[0]:
        return default
    n = len(keys)
    i = 0
    while i < n - 2 and t > keys[i + 1]["t"]:
        i += 1
    if n == 1:
        return keys[0].get(field, default)
    span = max(keys[i + 1]["t"] - keys[i]["t"], 1e-6)
    u = min(max((t - keys[i]["t"]) / span, 0.0), 1.0)
    return keys[i].get(field, default) * (1 - u) + keys[i + 1].get(field, default) * u


def look_rot(pos, look):
    d = [look[0] - pos[0], look[1] - pos[1], look[2] - pos[2]]
    yaw = math.degrees(math.atan2(d[1], d[0]))
    pitch = math.degrees(math.atan2(d[2], math.hypot(d[0], d[1])))
    return pitch, yaw


def handheld(frame, amount, fps):
    """A person's slow sway, smooth sums of sines: nothing the eye reads as a pattern. Degrees and cm."""
    t = frame / float(fps)
    return (amount * (0.55 * math.sin(t * 1.13 + 1.2) + 0.3 * math.sin(t * 2.71 + 0.4) + 0.15 * math.sin(t * 6.3)),
            amount * (0.5 * math.sin(t * 0.91 + 3.3) + 0.35 * math.sin(t * 2.33 + 2.1)),
            amount * (0.5 * math.sin(t * 1.37 + 5.1) + 0.3 * math.sin(t * 3.11)))


def pp_of(cam):
    return cam.camera_component.get_editor_property("post_process_settings")


def apply_pp(cam, fov=None, focus=None, fstop=None, bias=None, gain=None):
    comp = cam.camera_component
    if fov is not None:
        comp.set_editor_property("field_of_view", fov)
    pp = pp_of(cam)
    if focus is not None and focus > 0:
        pp.set_editor_property("override_depth_of_field_focal_distance", True)
        pp.set_editor_property("depth_of_field_focal_distance", float(focus))
        pp.set_editor_property("override_depth_of_field_fstop", True)
        pp.set_editor_property("depth_of_field_fstop", float(fstop or 4.0))
        pp.set_editor_property("override_depth_of_field_sensor_width", True)
        pp.set_editor_property("depth_of_field_sensor_width", 36.0)
    if bias is not None:
        pp.set_editor_property("override_auto_exposure_bias", True)
        pp.set_editor_property("auto_exposure_bias", float(bias))
    if gain is not None:
        pp.set_editor_property("override_color_gain", True)
        pp.set_editor_property("color_gain", unreal.Vector4(gain, gain, gain, 1.0))
    comp.set_editor_property("post_process_settings", pp)
    comp.set_editor_property("post_process_blend_weight", 1.0)


def sequence(ctx):
    with open(JOB, encoding="utf-8") as f:
        job = json.load(f)
    run = lambda c: unreal.SystemLibrary.execute_console_command(ctx.world, c)
    ctx.out("PROMO begin %s" % job["id"])
    if job.get("director") is False:
        run("MO.Weather.Director 0")
    if job.get("weather"):
        run("MO.Weather.SetPreset " + job["weather"])
    if job.get("clock"):
        run("MO.Clock.SetTime %d %d" % (job["clock"][0], job["clock"][1]))
    if "timescale" in job:
        run("MO.Clock.SetTimeScale %s" % job["timescale"])
    pawn_cfg = job.get("pawn", {})
    if pawn_cfg.get("at"):
        a = pawn_cfg["at"]
        run("MO.Player.Teleport %.1f %.1f %.1f" % (a[0], a[1], a[2]))
    yield 2
    for line in job.get("before", []):
        run(line)
        yield 1
    pawn = ctx.pawn
    if pawn and pawn_cfg.get("hide"):
        pawn.set_actor_hidden_in_game(True)
    keys = job.get("keys") or []
    follow = job.get("follow")
    use_camera = not job.get("showui")
    fps = job.get("fps", 96)
    frames = int(job["frames"])
    cam = None
    pos0 = None
    if use_camera:
        if follow:
            loc = pawn.get_actor_location()
            pos0, look0 = _follow_pose(pawn, follow)
        else:
            pos0, look0 = sample(keys, 0.0, "pos"), sample(keys, 0.0, "look")
        fov0 = scalar(keys, 0.0, "fov", 50.0) if keys else follow.get("fov", 50.0)
        run("MO.Cam.LookAt %.2f %.2f %.2f %.2f %.2f %.2f %.3f" % (pos0[0], pos0[1], pos0[2], look0[0], look0[1], look0[2], fov0))
        yield 2
        cam = ctx.pc.get_view_target()
        ctx.out("PROMO camera %s" % cam.get_name())
    expo = job.get("exposure") or {}
    if cam:
        apply_pp(cam, fov=fov0, focus=scalar(keys, 0.0, "focus", None) if keys else follow.get("focus"), fstop=job.get("fstop"), bias=expo.get("bias"))
    # settle: hold the first pose in wall-clock time until the world round it has loaded
    t0 = time.time()
    settle = float(job.get("settle", 20.0))
    while time.time() - t0 < settle:
        yield 5
    ctx.out("PROMO settled %.0fs fps=%.1f" % (time.time() - t0, 0.0))

    # the slate: black, then two white frames: the first frame after the second white one + preroll is the shot's frame 0
    if cam and job.get("slate", True):
        apply_pp(cam, gain=0.0)
        yield 6
        apply_pp(cam, gain=40.0)
        yield 2
        apply_pp(cam, gain=1.0)
    preroll = int(job.get("preroll", 96))
    yield preroll

    t_real = time.time()
    amount = float(job.get("handheld", 0.0))
    actions = {}
    for f, line in job.get("actions", []):
        actions.setdefault(int(f), []).append(line)
    stills = set(job.get("stills", []))
    eased = float(job.get("ease", 1.0))
    smooth = None
    ctx.out("PROMO rolling %d frames" % frames)
    for i in range(frames):
        for line in actions.get(i, []):
            run(line)
        if cam:
            t = ease(i / max(frames - 1, 1), eased)
            if follow:
                p, l = _follow_pose(ctx.pawn, follow)
                k = float(follow.get("smooth", 0.06))
                smooth = (p, l) if smooth is None else ([a + (b - a) * k for a, b in zip(smooth[0], p)], [a + (b - a) * k for a, b in zip(smooth[1], l)])
                pos, look = smooth
                fov = follow.get("fov", 50.0)
                focus = follow.get("focus")
            else:
                pos, look = sample(keys, t, "pos"), sample(keys, t, "look")
                fov = scalar(keys, t, "fov", 50.0)
                focus = scalar(keys, t, "focus", None)
            pitch, yaw = look_rot(pos, look)
            if amount:
                dp, dy, dz = handheld(i, amount, fps)
                pitch += dp * 0.08
                yaw += dy * 0.08
                pos = [pos[0], pos[1], pos[2] + dz * 0.3]
            cam.set_actor_location_and_rotation(unreal.Vector(*pos), unreal.Rotator(0.0, pitch, yaw), False, True) if False else _place(cam, pos, pitch, yaw)
            if focus or len(keys) > 1:
                apply_pp(cam, fov=fov, focus=focus, fstop=job.get("fstop"))
            if pawn_cfg.get("under_camera"):
                ctx.pawn.set_actor_location(unreal.Vector(pos[0], pos[1], pos[2] + float(pawn_cfg.get("under_offset", 0.0))), False, True)
        if i in stills:
            run("shot")
        yield 1
    elapsed = time.time() - t_real
    if cam and job.get("slate", True):
        apply_pp(cam, gain=0.0)
        yield 3
        apply_pp(cam, gain=40.0)
        yield 2
        apply_pp(cam, gain=0.0)
    ctx.out("PROMO done %s frames=%d real=%.1fs (%.1f fps)" % (job["id"], frames, elapsed, frames / max(elapsed, 0.001)))
    yield 2
    if cam:
        apply_pp(cam, gain=1.0)


def _place(cam, pos, pitch, yaw):
    rot = unreal.Rotator()
    rot.pitch = pitch
    rot.yaw = yaw
    rot.roll = 0.0
    cam.set_actor_location_and_rotation(unreal.Vector(pos[0], pos[1], pos[2]), rot, False, True)


def _follow_pose(pawn, follow):
    o = pawn.get_actor_location()
    yaw = math.radians(pawn.get_actor_rotation().yaw)
    f, r = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))

    def at(v):
        return [o.x + f[0] * v[0] + r[0] * v[1], o.y + f[1] * v[0] + r[1] * v[1], o.z + v[2]]

    return at(follow["offset"]), at(follow.get("look", [0, 0, 60]))
