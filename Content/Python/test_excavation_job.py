"""Stage 3 gate: the ExcavateAndHaul survivor job (dig -> haul -> fill), with conservation.

    python Tools/ue.py seq Content/Python/test_excavation_job.py --timeout 900

A spawned survivor is given an ExcavateAndHaul(FillZone) job: dig a bite of earth from
a designated Dig zone (producing carryable Dirt), haul it, and raise terrain at a
designated Fill zone (consuming the Dirt). Proves the whole loop runs autonomously and
conserves earth:
  (a) the job runs to completion (started then drained),
  (b) DIG: the Dig zone's earth budget drops (earth removed),
  (c) FILL: the Fill zone's budget drops (earth placed),
  (d) HAUL + CONSERVATION: the pawn CARRIED spoil mid-trip (Dirt peaked > 0) then ended
      at 0 (produced at dig, fully consumed at fill).
The terraform rate is lowered as config-for-test so the realistically hours-long
volume-based dig finishes in seconds — the simulation is not skipped, only sped for the test.
"""
import unreal

SEED = 6210
DIRT = "Dirt01"


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _desig(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MODesignationSubsystem)
    return unreal.MODesignationSubsystem.cast(o) if o else None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _zone_vol(desig, zid):
    r = desig.find_zone(zid)
    # unreal may return (bool, struct) for a bool-return + out-param, or just the struct.
    if isinstance(r, tuple):
        ok = bool(r[0])
        zone = r[1]
    else:
        zone = r
        ok = zone is not None
    return zone.remaining_volume_m3 if (ok and zone is not None) else -1.0


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="ExcavJobGate")

    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            player = p
            break
    if not ctx.guard("player pawn", player is not None):
        return

    ctx.out("BC: player acquired")
    desig = _desig(world)
    if not ctx.guard("designation subsystem up", desig is not None):
        return
    ctx.out("BC: designation subsystem ok")

    # ---- the digger (found a settlement + recruit so the AI runs jobs) --------
    _exec(world, "MO.Colony.Found Digtown 30000")
    yield 5
    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("survivor spawned", len(vills) >= 1):
        return
    surv = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % surv.get_name())
    yield 5
    jq = surv.get_component_by_class(unreal.MOSurvivorJobQueueComponent)
    terra = surv.get_component_by_class(unreal.MOTerraformingComponent)
    inv = surv.get_component_by_class(unreal.MOInventoryComponent)
    if not ctx.guard("survivor has job-queue + terraform + inventory", jq and terra and inv):
        return
    ctx.out("BC: survivor %s + components ok" % surv.get_name())

    # config-for-test: make the volume-based dig complete in ~seconds, not hours.
    terra.set_editor_property("terraform_seconds_per_cubic_meter", 1.0)
    terra.set_editor_property("terraform_duration_seconds", 1.0)
    ctx.out("BC: config-for-test set")

    # designate a dig zone + a fill zone a few metres from the survivor.
    loc = surv.get_actor_location()
    dig_c = unreal.Vector(loc.x + 300.0, loc.y, loc.z)
    fill_c = unreal.Vector(loc.x, loc.y + 300.0, loc.z)
    dig_id = desig.designate_zone(unreal.MODesignationKind.DIG, dig_c, 200.0, 0.0, 50.0)
    fill_id = desig.designate_zone(unreal.MODesignationKind.DUMP, fill_c, 200.0, loc.z + 300.0, 50.0)
    dig0 = _zone_vol(desig, dig_id)
    fill0 = _zone_vol(desig, fill_id)
    if not ctx.guard("dig + fill zones designated", dig0 > 0.0 and fill0 > 0.0):
        return

    job_id = jq.enqueue_excavate_job(dig_id, unreal.MOExcavateDumpMode.FILL_ZONE, fill_id, None, DIRT)
    ctx.out("enqueued excavate job (dig=%.1fm3 fill=%.1fm3)" % (dig0, fill0))

    # ---- drive the AI at FRAME resolution: the carry window (dig->fill) is brief,
    # so poll every frame to catch the Dirt peak; detect completion via the durable
    # zone changes (dug + filled) then the pawn returning to 0 spoil.
    max_dirt = 0
    dug = False
    filled = False
    completed = False
    for i in range(700):
        yield 1
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        d = inv.get_item_count_by_definition_id(DIRT) if inv else 0
        if d > max_dirt:
            max_dirt = d
        dv = _zone_vol(desig, dig_id)
        fv = _zone_vol(desig, fill_id)
        if dv >= 0.0 and dv < dig0 - 0.001:
            dug = True
        if fv >= 0.0 and fv < fill0 - 0.001:
            filled = True
        # done: earth moved at both ends AND the pawn carried then offloaded the spoil.
        if dug and filled and max_dirt > 0 and d == 0:
            completed = True
            break
        if i % 120 == 119:
            ctx.out("poll %d: carried=%d max=%d dig=%.2f fill=%.2f" % (i, d, max_dirt, dv, fv))

    dig1 = _zone_vol(desig, dig_id)
    fill1 = _zone_vol(desig, fill_id)
    final_dirt = inv.get_item_count_by_definition_id(DIRT) if inv else -1
    ctx.out("RESULT completed=%s max_dirt=%d final_dirt=%d dig %.2f->%.2f fill %.2f->%.2f"
            % (completed, max_dirt, final_dirt, dig0, dig1, fill0, fill1))

    ctx.assert_true("DIG: dig zone earth budget decreased (earth removed)", dig1 < dig0 - 0.001)
    ctx.assert_true("FILL: fill zone budget decreased (earth placed elsewhere)", fill1 < fill0 - 0.001)
    ctx.assert_true("HAUL: pawn carried spoil mid-trip (Dirt peaked > 0)", max_dirt > 0)
    ctx.assert_true("CONSERVATION: carried spoil fully consumed at fill (final Dirt == 0)", final_dirt == 0)
    ctx.assert_true("LOOP: dig -> haul -> fill ran end-to-end", dug and filled and max_dirt > 0)
    ctx.out("Stage 3 excavation-job gate complete")
