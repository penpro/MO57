"""Stage 4 gate: the colony's excavation dispatch (RunExcavationPass) -- villagers pick up the designated dig zones by themselves.

    python Tools/ue.py seq Content/Python/test_excavation_dispatch.py --timeout 900

Two recruited villagers, two dig zones and one fill zone inside the settlement, plus a third dig zone OUTSIDE it. Nobody is
given a job by hand; the colony upkeep tick (RunUpkeepTick, which also runs on its own 10 s timer) must:
  (a) DISPATCH: give each villager an ExcavateAndHaul trip, on DIFFERENT dig zones, both filling the designated fill zone,
  (b) ONE WORKER PER ZONE: a second upkeep tick while the trips run adds nothing (the claimed set holds),
  (c) SETTLEMENT SCOPE: the dig zone outside the settlement is never worked,
  (d) THE LOOP: both inside dig zones lose earth, the fill zone gains it, and both villagers end carrying no spoil (conservation).
Config-for-test: the terraform rate is raised so the realistically hours-long volume-based bite takes seconds; the simulation runs,
only faster. Pure decision cases (default targets, full fill zones, containers) are the headless MOFramework.Colony.Excavation.*.
"""
import unreal

SEED = 6211
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


def _villagers(world, player):
    out = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn) if a != player and "MetaHuman" in a.get_name()]
    out.sort(key=lambda p: p.get_name())
    return out


def _zone_vol(desig, zid):
    r = desig.find_zone(zid)
    if isinstance(r, tuple):
        ok, zone = bool(r[0]), r[1]
    else:
        zone, ok = r, r is not None
    return zone.remaining_volume_m3 if (ok and zone is not None) else -1.0


def _excavate_jobs(jq):
    return [j for j in jq.get_all_jobs() if j.job_type == unreal.MOSurvivorJobType.EXCAVATE_AND_HAUL]


def _same(a, b):
    return unreal.GuidLibrary.equal_equal_guid_guid(a, b)


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="ExcavDispatchGate")
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

    # ---- a settlement (300 m) and two recruited villagers ---------------------------------------------------------------
    _exec(world, "MO.Colony.Found Digtown 30000")
    yield 5
    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 8
    _exec(world, "MO.Colony.SpawnSurvivor 500")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("two survivors spawned", len(vills) >= 2):
        return
    vills = vills[:2]
    for v in vills:
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 5
    jqs = [v.get_component_by_class(unreal.MOSurvivorJobQueueComponent) for v in vills]
    invs = [v.get_component_by_class(unreal.MOInventoryComponent) for v in vills]
    terras = [v.get_component_by_class(unreal.MOTerraformingComponent) for v in vills]
    if not ctx.guard("both villagers have job queue + inventory + terraform", all(jqs) and all(invs) and all(terras)):
        return
    for jq in jqs:
        jq.cancel_all_jobs()
    for t in terras:   # config-for-test: seconds, not hours, per bite
        t.set_editor_property("terraform_seconds_per_cubic_meter", 1.0)
        t.set_editor_property("terraform_duration_seconds", 1.0)

    # ---- zones: two digs + one fill inside, one dig far outside --------------------------------------------------------
    loc = player.get_actor_location()
    dig_a = desig.designate_zone(unreal.MODesignationKind.DIG, unreal.Vector(loc.x + 400.0, loc.y, loc.z), 200.0, 0.0, 50.0)
    dig_b = desig.designate_zone(unreal.MODesignationKind.DIG, unreal.Vector(loc.x - 400.0, loc.y, loc.z), 200.0, 0.0, 50.0)
    fill = desig.designate_zone(unreal.MODesignationKind.DUMP, unreal.Vector(loc.x, loc.y + 400.0, loc.z), 200.0, loc.z + 300.0, 50.0)
    dig_out = desig.designate_zone(unreal.MODesignationKind.DIG, unreal.Vector(loc.x + 60000.0, loc.y, loc.z), 200.0, 0.0, 50.0)
    v0 = {k: _zone_vol(desig, z) for k, z in (("a", dig_a), ("b", dig_b), ("fill", fill), ("out", dig_out))}
    if not ctx.guard("zones designated", all(x > 0.0 for x in v0.values())):
        return
    ctx.out("zones: %s" % v0)

    # ---- (a) dispatch, (b) no doubling, (c) settlement scope -------------------------------------------------------------
    colony.run_upkeep_tick()
    yield 2
    jobs = [_excavate_jobs(jq) for jq in jqs]
    ctx.out("after upkeep 1: %s" % [[(str(j.dig_zone_id)[:8], str(j.excavate_dump_mode)) for j in js] for js in jobs])
    got = [js[0] for js in jobs if js]
    ctx.assert_true("DISPATCH: both villagers were given an excavation trip by the colony", len(got) == 2)
    if len(got) == 2:
        ctx.assert_true("DISPATCH: the two trips dig DIFFERENT zones", not _same(got[0].dig_zone_id, got[1].dig_zone_id))
        ctx.assert_true("DISPATCH: both trips are the inside zones and fill the designated fill zone",
                        all((_same(j.dig_zone_id, dig_a) or _same(j.dig_zone_id, dig_b)) and _same(j.dump_zone_id, fill) for j in got))
    colony.run_upkeep_tick()
    yield 2
    counts = [len(_excavate_jobs(jq)) for jq in jqs]
    ctx.assert_true("ONE WORKER PER ZONE: a second upkeep tick adds no trip while the first ones run (%s)" % counts, all(c <= 1 for c in counts))
    ctx.assert_true("SETTLEMENT SCOPE: the dig zone 600 m outside the settlement is not assigned",
                    not any(_same(j.dig_zone_id, dig_out) for jq in jqs for j in _excavate_jobs(jq)))

    # ---- (d) the loop runs; poll at frame resolution for the carry peaks ------------------------------------------------
    peaks = [0, 0]
    for i in range(1500):
        yield 1
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        for k in range(2):
            peaks[k] = max(peaks[k], invs[k].get_item_count_by_definition_id(DIRT))
        va, vb, vf = _zone_vol(desig, dig_a), _zone_vol(desig, dig_b), _zone_vol(desig, fill)
        carried = [invs[k].get_item_count_by_definition_id(DIRT) for k in range(2)]
        if va < v0["a"] - 0.001 and vb < v0["b"] - 0.001 and vf < v0["fill"] - 0.001 and min(peaks) > 0 and max(carried) == 0:
            break
        if i % 300 == 299:
            ctx.out("poll %d: a=%.2f b=%.2f fill=%.2f carried=%s peaks=%s" % (i, va, vb, vf, carried, peaks))
    v1 = {k: _zone_vol(desig, z) for k, z in (("a", dig_a), ("b", dig_b), ("fill", fill), ("out", dig_out))}
    final = [invs[k].get_item_count_by_definition_id(DIRT) for k in range(2)]
    ctx.out("RESULT zones %s -> %s, carried peaks %s, final carried %s" % (v0, v1, peaks, final))
    ctx.assert_true("LOOP: both inside dig zones lost earth", v1["a"] < v0["a"] - 0.001 and v1["b"] < v0["b"] - 0.001)
    ctx.assert_true("LOOP: the fill zone took earth", v1["fill"] < v0["fill"] - 0.001)
    ctx.assert_true("HAUL: both villagers carried spoil mid-trip", min(peaks) > 0)
    ctx.assert_true("CONSERVATION: both ended carrying no spoil", max(final) == 0)
    ctx.assert_true("SETTLEMENT SCOPE: the outside dig zone is untouched", abs(v1["out"] - v0["out"]) < 0.001)
    ctx.out("Stage 4 excavation-dispatch gate complete")
