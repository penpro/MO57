"""Stage 2 gate: excavation designation subsystem CRUD + IMOSaveDomain round-trip.

    python Tools/ue.py seq Content/Python/test_designation_persist.py --timeout 600

Proves UMODesignationSubsystem creates/queries dig & dump zones (sphere: center +
radius + earth budget), auto-estimates a budget when none is given, and that its
save domain round-trips: capture -> clear -> apply restores every zone with its
kind, radius and budget intact. (Zones are matched by their distinct radii rather
than GUID, since str() on a wrapped struct is the wrapper address, not the value.)
"""
import unreal

SEED = 6210


def _designation_subsys(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MODesignationSubsystem)
    return unreal.MODesignationSubsystem.cast(o) if o else None


def sequence(ctx):
    helper = unreal.MOEditorTestHelper
    ok = helper.configure_pie(1, False)
    if not ctx.guard("ConfigurePIE(1, standalone)", ok):
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
    if not ctx.guard("PIE world up", world is not None):
        return
    if ctx.atl:
        ctx.atl.skip_intro(world, ctx.out)
        yield 10
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="DesigGate")

    # Wait for the game world to be fully up (player possessed => OnWorldBeginPlay ran,
    # so the subsystem registered its save domain).
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if world and unreal.GameplayStatics.get_player_pawn(world, 0):
            break

    sub = _designation_subsys(world)
    if not ctx.guard("designation subsystem up", sub is not None):
        return

    sub.clear_all_zones()  # start clean

    sub.designate_zone(unreal.MODesignationKind.DIG, unreal.Vector(1000, 0, 200), 300.0, 0.0, 12.5)
    sub.designate_zone(unreal.MODesignationKind.DUMP, unreal.Vector(-500, 400, 150), 250.0, 180.0, 8.0)
    sub.designate_zone(unreal.MODesignationKind.DIG, unreal.Vector(0, -800, 100), 200.0, 0.0, 0.0)  # auto-volume

    ctx.assert_true("three zones designated", sub.get_zone_count() == 3)

    zones0 = sub.get_zones()
    auto = [z for z in zones0 if abs(z.radius - 200.0) < 0.5]
    ctx.assert_true("auto-volume zone (r=200) got a positive earth budget",
                    len(auto) == 1 and auto[0].remaining_volume_m3 > 0.0)

    # --- save-domain round trip: capture -> clear -> apply ---
    save_data = sub.build_save_data()
    cleared = sub.clear_all_zones()
    ctx.assert_true("clear removed all three", cleared == 3 and sub.get_zone_count() == 0)

    sub.apply_save_data_authority(save_data)
    ctx.assert_true("apply restored all three zones", sub.get_zone_count() == 3)

    zones = sub.get_zones()
    n_dig = sum(1 for z in zones if z.kind == unreal.MODesignationKind.DIG)
    n_dump = sum(1 for z in zones if z.kind == unreal.MODesignationKind.DUMP)
    ctx.assert_true("restored kinds: 2 Dig + 1 Dump", n_dig == 2 and n_dump == 1)

    try:
        radii = [round(z.radius, 1) for z in zones]
        vols = [round(z.remaining_volume_m3, 3) for z in zones]
        ctx.out("restored radii=%s vols=%s" % (radii, vols))
        r300 = [z for z in zones if abs(z.radius - 300.0) < 0.5]
        ok_r300 = len(r300) == 1 and abs(r300[0].remaining_volume_m3 - 12.5) < 0.01
        ctx.assert_true("the r=300 dig zone restored with budget 12.5 intact", ok_r300)
    except Exception as e:
        ctx.out("r300 block EXCEPTION: %r" % (e,))
        ctx.assert_true("the r=300 dig zone restored with budget 12.5 intact", False)

    # ConsumeZoneVolume removes a zone when its budget is exhausted.
    try:
        dump = [z for z in zones if z.kind == unreal.MODesignationKind.DUMP]
        if dump:
            remaining = sub.consume_zone_volume(dump[0].zone_id, 1000.0)  # over-consume
            ctx.out("consume remaining=%.3f count=%d" % (remaining, sub.get_zone_count()))
            ctx.assert_true("over-consuming a zone's budget removes it",
                            remaining == 0.0 and sub.get_zone_count() == 2)
    except Exception as e:
        ctx.out("consume block EXCEPTION: %r" % (e,))
        ctx.assert_true("over-consuming a zone's budget removes it", False)

    sub.clear_all_zones()
    ctx.out("Stage 2 designation-persist gate complete")
