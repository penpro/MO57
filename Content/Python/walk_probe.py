"""A claude_seq sequence: how a walking pawn's legs move, frame by frame. Driven by Tools/check_ai_walk.py, which writes the job.

The job (Saved/Promo/shots/walk_probe.json): {"pawn": name suffix, "seconds": s, "goal": [x, y, z] or null (null: the player's own key is already held)}
Each simulation frame it reads the body's feet in the pawn's own frame. Walking, one foot passes the other twice a stride, so the difference of the feet along the pawn's forward axis swings through zero.
A pawn that slides on a held pose (Wes, Nov 6: "drag floating instead of walking") keeps that difference nearly still while its speed is real. Prints one line: WALKPROBE <json>.
"""
import json
import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
JOB = os.path.join(ROOT, "Saved", "Promo", "shots", "walk_probe.json")


def sequence(ctx):
    with open(JOB, encoding="utf-8") as f:
        job = json.load(f)
    pawn = next((a for a in unreal.GameplayStatics.get_all_actors_of_class(ctx.world, unreal.Pawn) if a.get_name().endswith(job["pawn"])), None)
    if pawn is None:
        ctx.out("WALKPROBE " + json.dumps({"error": "no pawn " + job["pawn"]}))
        return
    move = pawn.get_component_by_class(unreal.CharacterMovementComponent)
    if job.get("goal"):
        pawn.get_controller().set_stay_at_location(unreal.Vector(*job["goal"]))
    yield 30    # under way
    mesh = pawn.get_editor_property("mesh")
    t0 = unreal.GameplayStatics.get_time_seconds(ctx.world)
    start = pawn.get_actor_location()
    diffs, speeds, accels, times = [], [], [], []
    while unreal.GameplayStatics.get_time_seconds(ctx.world) - t0 < float(job["seconds"]):
        fwd = pawn.get_actor_forward_vector()
        l, r = mesh.get_socket_location("foot_l"), mesh.get_socket_location("foot_r")
        diffs.append((l.x - r.x) * fwd.x + (l.y - r.y) * fwd.y)
        speeds.append(pawn.get_velocity().length())
        accels.append(move.get_current_acceleration().length())
        times.append(unreal.GameplayStatics.get_time_seconds(ctx.world) - t0)
        yield 1
    # passes: the feet's difference crossing zero, with a little hysteresis so a tremble does not count
    passes, side = 0, 0
    for d in diffs:
        s = 1 if d > 5.0 else (-1 if d < -5.0 else 0)
        if s and side and s != side:
            passes += 1
        side = s or side
    span = times[-1] if times else 1.0
    end = pawn.get_actor_location()
    moving = [s for s in speeds if s > 20.0]
    ctx.out("WALKPROBE " + json.dumps({
        "pawn": pawn.get_name(), "frames": len(diffs), "sim_seconds": round(span, 2),
        "metres": round(((end.x - start.x) ** 2 + (end.y - start.y) ** 2) ** 0.5 / 100.0, 2),
        "speed": round(sum(moving) / max(len(moving), 1), 0), "accel": round(sum(accels) / max(len(accels), 1), 0),
        "foot_passes_per_s": round(passes / max(span, 0.01), 2), "feet_swing_cm": round(max(diffs) - min(diffs), 0) if diffs else 0,
    }))
