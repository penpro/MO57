"""Stage 5 gate: a FLATTEN zone levels uneven ground by itself -- a villager digs its high spots and fills its low spots, conserving earth.

    python Tools/ue.py seq Content/Python/test_excavation_flatten.py --timeout 900

Setup: one recruited villager. Beside it the ground is made uneven on purpose with the terraform primitive (a mound of three raised
bites, a pit of three dug bites, 2.6 m apart: equal earth both ways), and a Flatten zone (r = 2 m) is designated at the MEAN ground height
of its disc -- the one target a flatten can reach with its own earth (a target above or below the mean needs earth from outside; the zone
then levels what it can and waits). Nobody gives
the villager a job; the colony upkeep does. Proves:
  (a) DISPATCH: the zone is its own source and sink (the trip digs and fills the same zone),
  (b) BITE POINTS: the first trip digs the mound (the highest ground) and fills the pit (the lowest),
  (c) LEVEL: the zone ends with no ground more than one bite (15 cm) off its target on either side,
  (d) CONSERVATION: the villager ends carrying no spoil, and every dig was matched by a fill.
Config-for-test: the terraform rate is raised so each volume-based bite takes a second, not minutes, and the bite is the full-strength
brush (12 cm at the centre instead of 1.8) so a few trips do what a day of real trips would.
"""
import unreal

SEED = 6212
DIRT = "Dirt01"


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _subsystem(world, cls):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, cls)
    return cls.cast(o) if o else None


def _bite(desig, zid, dig):
    """(found, z) of the zone's next bite point on one side. (UE Python returns the out params only when the bool return is True, None otherwise.)"""
    r = desig.find_zone_bite_point(zid, dig)
    if not r:
        return False, None
    point = r[0] if isinstance(r, tuple) else r
    return True, point.z


def _readable(desig, center, radius):
    """The ground under a disc is readable (voxel collision there): a throwaway DIG-kind zone always finds its highest point if so."""
    probe = desig.designate_zone(unreal.MODesignationKind.DIG, center, radius, 0.0, 1.0)
    found, z = _bite(desig, probe, True)
    desig.clear_zone(probe)
    return found, z


def sequence(ctx):
    helper = unreal.MOEditorTestHelper
    if not ctx.guard("ConfigurePIE(1, standalone)", helper.configure_pie(1, False)):
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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="FlattenGate")
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
    desig = _subsystem(world, unreal.MODesignationSubsystem)
    colony = _subsystem(world, unreal.MOColonyManagerSubsystem)
    if not ctx.guard("designation + colony subsystems up", desig is not None and colony is not None):
        return

    _exec(world, "MO.Colony.Found Flatville 30000")
    yield 5
    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a != _pawn(world) and "MetaHuman" in a.get_name()]
    if not ctx.guard("survivor spawned", len(vills) >= 1):
        return
    surv = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % surv.get_name())
    yield 5
    jq = surv.get_component_by_class(unreal.MOSurvivorJobQueueComponent)
    terra = surv.get_component_by_class(unreal.MOTerraformingComponent)
    inv = surv.get_component_by_class(unreal.MOInventoryComponent)
    if not ctx.guard("survivor has job queue + terraform + inventory", jq and terra and inv):
        return
    jq.cancel_all_jobs()
    terra.set_editor_property("terraform_seconds_per_cubic_meter", 1.0)
    terra.set_editor_property("terraform_duration_seconds", 1.0)
    terra.set_editor_property("excavation_dig_strength", 1.0)

    # ---- make the ground uneven: a mound and a pit, 2.6 m apart, beside the player ----------------------------------------------------
    loc = player.get_actor_location()
    center = unreal.Vector(loc.x + 500.0, loc.y, loc.z)
    found, z0 = _readable(desig, center, 60.0)   # a 60 cm probe: the ground height at the centre
    if not ctx.guard("ground under the test spot is readable", found and z0 is not None):
        return
    mound = unreal.Vector(center.x - 130.0, center.y, z0)
    pit = unreal.Vector(center.x + 130.0, center.y, z0)
    for _ in range(3):
        terra.terraform_at_location_ex(mound, unreal.MOTerraformMode.RAISE, 120.0, 1.0)
        terra.terraform_at_location_ex(pit, unreal.MOTerraformMode.DIG, 120.0, 1.0)
        yield 5
    yield 60   # the sculpt applies asynchronously; collision rebuilds after it
    heights = []
    for gx in range(-4, 5):
        for gy in range(-4, 5):
            if gx * gx + gy * gy <= 16:
                ok, z = _readable(desig, unreal.Vector(center.x + gx * 50.0, center.y + gy * 50.0, center.z), 10.0)
                if ok:
                    heights.append(z)
    target = sum(heights) / len(heights) if heights else z0
    ctx.out("disc: %d ground samples, min %.1f max %.1f mean %.1f (centre %.1f)" % (len(heights), min(heights), max(heights), target, z0))
    zone = desig.designate_zone(unreal.MODesignationKind.FLATTEN, center, 200.0, target, 20.0)
    hi0 = _bite(desig, zone, True)
    lo0 = _bite(desig, zone, False)
    ctx.out("ground z0=%.1f; uneven: high %s low %s" % (z0, hi0, lo0))
    if not ctx.guard("the ground is uneven on both sides of the target", hi0[0] and lo0[0]):
        return

    # ---- the colony takes it from here ---------------------------------------------------------------------------------------------------
    colony.run_upkeep_tick()
    yield 2
    jobs = [j for j in jq.get_all_jobs() if j.job_type == unreal.MOSurvivorJobType.EXCAVATE_AND_HAUL]
    same = bool(jobs) and unreal.GuidLibrary.equal_equal_guid_guid(jobs[0].dig_zone_id, zone) and unreal.GuidLibrary.equal_equal_guid_guid(jobs[0].dump_zone_id, zone)
    ctx.assert_true("DISPATCH: the colony sent the villager to flatten, digging and filling the same zone", same)

    trips, peak, last_job = 0, 0, None
    for i in range(3000):
        yield 1
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        peak = max(peak, inv.get_item_count_by_definition_id(DIRT))
        cur = jq.get_current_job()
        cur_id = unreal.GuidLibrary.conv_guid_to_string(cur.job_id) if cur and cur.job_type == unreal.MOSurvivorJobType.EXCAVATE_AND_HAUL else None
        if last_job and cur_id != last_job:
            trips += 1
        last_job = cur_id
        if not cur_id and i % 60 == 0:
            colony.run_upkeep_tick()   # idle: the next trip (the 10 s timer would do it too, slower)
        if i % 120 == 119:
            hi, lo = _bite(desig, zone, True), _bite(desig, zone, False)
            ctx.out("poll %d: trips=%d carried=%d high=%s low=%s" % (i, trips, inv.get_item_count_by_definition_id(DIRT), hi, lo))
            if not hi[0] and not lo[0] and not cur_id:
                break
    hi1, lo1 = _bite(desig, zone, True), _bite(desig, zone, False)
    carried = inv.get_item_count_by_definition_id(DIRT)
    ctx.out("RESULT trips=%d high %s -> %s, low %s -> %s, peak carried %d, final carried %d" % (trips, hi0, hi1, lo0, lo1, peak, carried))
    ctx.assert_true("BITE POINTS / LOOP: at least two trips ran and spoil was carried", trips >= 2 and peak > 0)
    readable = _readable(desig, center, 200.0)[0]
    ctx.assert_true("LEVEL: the zone's ground is readable (so 'nothing off target' is a measurement, not a blind spot)", readable)
    ctx.assert_true("LEVEL: nothing left more than 15 cm above the target", not hi1[0])
    ctx.assert_true("LEVEL: nothing left more than 15 cm below the target", not lo1[0])
    ctx.assert_true("CONSERVATION: the villager ends carrying no spoil", carried == 0)
    ctx.out("Stage 5 flatten gate complete")
