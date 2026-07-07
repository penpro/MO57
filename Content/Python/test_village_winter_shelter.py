"""V2.5 gate: cold villagers go home (winter survival AI).

    python Tools/ue.py seq Content/Python/test_village_winter_shelter.py --timeout 900

One housed villager pinned out in a winter night: body temp falls, the
colony shelter pass overrides the stay order and sends them home, the roof
(overhead-cover insulation) lets regulation rewarm them.
  (a) RELOCATE: villager ends up near its residence
  (b) RECOVER: body temp climbs back above the release threshold
"""
import unreal


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _clock(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOGameClockSubsystem)
    return unreal.MOGameClockSubsystem.cast(o) if o else None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _body_temp(pawn):
    v = pawn.get_component_by_class(unreal.MOVitalsComponent)
    return v.get_vital_signs().get_editor_property("body_temperature") if v else -1.0


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
        ctx.atl.start_new_game(world, ctx.out, seed=4242, survivor_name="ShelterGate")
    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            player = p
            break
    if not ctx.guard("player pawn possessed", player is not None):
        return

    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildLeanTo 400")
    yield 5
    _exec(world, "MO.Colony.SpawnSurvivor 250")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villager spawned (%d)" % len(vills), len(vills) >= 1):
        return
    v = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % v.get_name())
    yield 4
    house = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor):
        if act.get_recipe_id() == "BuildLeanTo":
            house = act
            break
    if not ctx.guard("house placed", house is not None):
        return
    _exec(world, "MO.Colony.AssignHouse %s %s" % (v.get_name(), house.get_name()))
    yield 3

    # pin the villager OUT in the open, then bring winter
    hub = house.get_actor_location()
    far = unreal.Vector(hub.x + 2500.0, hub.y, hub.z)
    ai = unreal.MOSurvivorController.cast(v.get_controller())
    if ai:
        ai.set_stay_at_location(far)
    _clock(world).set_game_date_time(unreal.DateTime(2027, 1, 15, 3, 0, 0))
    yield 10
    t0 = _body_temp(v)
    ctx.out("winter night begins: body=%.1fC, %.0fuu from home" % (
        t0, (v.get_actor_location() - hub).length()))

    _clock(world).set_time_scale(120.0)
    relocated = False
    recovered = False
    min_temp = t0
    for i in range(90):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        vills = _villagers(world, _pawn(world))
        if not vills:
            break
        v = vills[0]
        temp = _body_temp(v)
        min_temp = min(min_temp, temp)
        dist = (v.get_actor_location() - hub).length()
        if not relocated and dist < 500.0:
            relocated = True
            ctx.out("poll %d: RELOCATED home (%.0fuu, body %.1fC)" % (i, dist, temp))
        if relocated and temp >= 36.8:
            recovered = True
            ctx.out("poll %d: RECOVERED (body %.1fC)" % (i, temp))
            break
        if i % 15 == 14:
            ctx.out("poll %d: dist=%.0f body=%.1fC" % (i, dist, temp))
    _clock(world).set_time_scale(1.0)

    ctx.assert_true("COLD BITES: body temp dropped below the seek threshold (min %.1fC)" % min_temp,
                    min_temp < 36.2)
    ctx.assert_true("RELOCATE: cold villager went home", relocated)
    ctx.assert_true("RECOVER: warmed back up under the roof", recovered)
    ctx.out("V2.5 winter shelter gate complete")
