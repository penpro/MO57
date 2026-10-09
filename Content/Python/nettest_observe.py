"""Host-side observer for `nettest walk`: logs the SERVER's view of the joiner's pawn (see nettest_walk.py)."""
import importlib
import time

import unreal

import nettest_walk
importlib.reload(nettest_walk)  # the bridge process caches modules; pick up edits without restarting the game
from nettest_walk import SAMPLE_EVERY, WALK_SECONDS, _log


def sequence(ctx):
    end = time.time() + WALK_SECONDS + 2.0
    next_log = 0.0
    unreal.log_warning("[NETWALK] begin role=server")
    while time.time() < end:
        if time.time() >= next_log:
            pawn = unreal.GameplayStatics.get_player_pawn(ctx.world, 1)
            if pawn:
                _log("server", pawn)
            next_log = time.time() + SAMPLE_EVERY
        yield 1
    unreal.log_warning("[NETWALK] end role=server")
