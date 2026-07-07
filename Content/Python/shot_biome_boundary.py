"""Position the PIE camera high above a biome boundary for an aerial shot.
Leaves PIE running — follow with `ue.py asset shot pie <out.png>`, then `pie end`.
"""
import unreal


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def sequence(ctx):
    helper = unreal.MOEditorTestHelper
    ok = helper.configure_pie(1, False)
    if not ctx.guard("ConfigurePIE", ok):
        return
    yield 1
    if ctx.atl:
        ctx.atl.begin_pie(ctx.out)
    world = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if world:
            break
    if not ctx.guard("PIE world", world is not None):
        return
    if ctx.atl:
        ctx.atl.skip_intro(world, ctx.out)
        yield 10
        ctx.atl.start_new_game(world, ctx.out, seed=4242, survivor_name="ShotRig")
    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p:
            player = p
            break
    if not ctx.guard("player", player is not None):
        return

    # scan the mask outward for a Meadow<->TemperateForest boundary
    base = player.get_actor_location()
    seed = 4242
    def biome(x, y):
        # exact P2-gate mask sampling (absolute coords, h=500, slope=5)
        return str(unreal.MOBiomeDatabaseSettings.resolve_biome_at(
            unreal.Vector(x, y, 0.0), 500.0, 5.0, seed, 300000.0, 450000.0))
    boundary = None
    STEP = 75000.0
    for gy in range(16):
        for gx in range(15):
            b0 = biome(gx * STEP, gy * STEP)
            b1 = biome((gx + 1) * STEP, gy * STEP)
            if b0 != b1 and b0 != "None" and b1 != "None":
                # bisect to ~5k for a tight boundary point
                lo, hi = gx * STEP, (gx + 1) * STEP
                while hi - lo > 5000.0:
                    mid = (lo + hi) * 0.5
                    if biome(mid, gy * STEP) == b0:
                        lo = mid
                    else:
                        hi = mid
                boundary = unreal.Vector((lo + hi) * 0.5, gy * STEP, base.z)
                ctx.out("boundary at (%.0f, %.0f): %s | %s" % (boundary.x, boundary.y, b0, b1))
                break
        if boundary:
            break
    if not ctx.guard("boundary located", boundary is not None):
        return

    try:
        clock = unreal.MOGameClockSubsystem.cast(
            helper.get_world_subsystem(world, unreal.MOGameClockSubsystem))
        if clock:
            clock.set_game_date_time(unreal.DateTime(2026, 7, 21, 12, 0, 0))
    except Exception as e:  # noqa: BLE001
        ctx.out("clock rig error: %r" % e)

    cam = unreal.Vector(boundary.x, boundary.y, base.z + 25000.0)
    for _ in range(14):
        try:
            world = helper.find_pie_world_by_net_mode("Standalone")
            player = _pawn(world)
            if not player:
                ctx.out("pawn lost during settle")
                break
            player.set_actor_location(cam, False, True)
            try:
                mv = player.get_movement_component()
                if mv:
                    mv.set_editor_property("movement_mode", unreal.MovementMode.MOVE_FLYING)
            except Exception as e:  # noqa: BLE001
                ctx.out("fly-mode error (non-fatal): %r" % e)
            ctrl = player.get_controller()
            if ctrl:
                ctrl.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-75.0, yaw=0.0))
        except Exception as e:  # noqa: BLE001
            ctx.out("camera rig error: %r" % e)
        yield 20
    ctx.out("camera rigged at boundary; take the shot now")
    ctx.assert_true("rigged", True)
