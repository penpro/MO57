"""nettest walk helpers (dev tooling): drive and observe a pawn across frames. Run through the bridge with `ue.py seq`.

Two roles, picked by the FILE the harness runs:
  nettest_walk.py     on the CLIENT: hold "move forward" every frame for WALK_SECONDS and log what the client sees
  nettest_observe.py  on the HOST:   log the SERVER's view of the joiner's pawn (player index 1) for the same stretch

Both write `[NETWALK] t=<epoch> role=<client|server> x= y= z= mode=<movement mode> speed=` lines into the game log
(unreal.log_warning), which Tools/ue_inst.py parses and aligns by time. The question they answer: while the client WALKS,
does it stay on the same ground the server has, or does it fall / hover / rubber-band?
"""
import re
import time

import unreal

WALK_SECONDS = 14.0
SAMPLE_EVERY = 0.4


def _log(role, pawn):
    loc = pawn.get_actor_location()
    cm = pawn.get_component_by_class(unreal.CharacterMovementComponent)
    # str(enum) is "<MovementMode.MOVE_WALKING: 1>" -- keep just the name or the harness's regex stops at the space
    m = re.search(r"MOVE_\w+", str(cm.movement_mode)) if cm else None
    mode = m.group(0) if m else "?"
    speed = pawn.get_velocity().length()
    unreal.log_warning("[NETWALK] t=%.3f role=%s x=%.0f y=%.0f z=%.1f mode=%s speed=%.0f"
                       % (time.time(), role, loc.x, loc.y, loc.z, mode, speed))


def sequence(ctx):
    role = "client"
    end = time.time() + WALK_SECONDS
    next_log = 0.0
    check_at, check_pos = time.time() + 2.0, None
    unreal.log_warning("[NETWALK] begin role=client")
    while time.time() < end:
        pawn = ctx.pawn
        if pawn:
            # one frame of "move forward" (X=strafe, Y=forward); the cheat is the same seam the keyboard uses
            unreal.SystemLibrary.execute_console_command(ctx.world, "MO.Test.Input Move 0 1")
            if time.time() >= next_log:
                _log(role, pawn)
                next_log = time.time() + SAMPLE_EVERY
            # A fresh random world can put the pawn facing a cliff or a rock. Every 2 s, if it has barely moved, turn 90
            # degrees: the test is about walking over ground, not about this seed's geometry.
            if time.time() >= check_at:
                loc = pawn.get_actor_location()
                if check_pos is not None and ((loc.x - check_pos[0]) ** 2 + (loc.y - check_pos[1]) ** 2) ** 0.5 < 150.0:
                    pc = unreal.GameplayStatics.get_player_controller(ctx.world, 0)
                    if pc:
                        rot = pc.get_control_rotation()
                        pc.set_control_rotation(unreal.Rotator(roll=rot.roll, pitch=rot.pitch, yaw=rot.yaw + 90.0))
                        unreal.log_warning("[NETWALK] blocked -> turning 90 degrees")
                check_pos = (loc.x, loc.y)
                check_at = time.time() + 2.0
        yield 1
    unreal.log_warning("[NETWALK] end role=client")
