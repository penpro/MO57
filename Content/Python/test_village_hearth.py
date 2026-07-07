"""F1 gate: a cold villager walks to the lit fire (pipeline F1 unit 3).

    python Tools/ue.py seq Content/Python/test_village_hearth.py --timeout 900

One HOMELESS recruit (no residence assigned — the fire is their only
recourse), one lit campfire, one winter night at speed:
  (a) COLD BITES: exposed villager's body temp falls below the seek threshold
  (b) TO THE FIRE: the shelter pass relocates them beside the campfire
  (c) WARMING: body temp climbs at the hearth (heat delta is real, not decor)
"""
import unreal


SEED = 5150


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
    vit = pawn.get_component_by_class(unreal.MOVitalsComponent)
    return vit.get_vital_signs().body_temperature if vit else 37.0


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="HearthGate")

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

    # ---- settlement: a lit campfire and a homeless recruit -------------------
    _exec(world, "MO.Colony.Found Hearthstead 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildCampfire 500")
    yield 8
    fire = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCraftingStationActor):
        if act.get_recipe_id() == "BuildCampfire":
            fire = act
            break
    if not ctx.guard("campfire placed", fire is not None):
        return
    fire.add_fuel("Firewood01", 10)   # 100 fuel @ 0.1/s = a long winter night
    fire.set_station_active(True)
    floc = fire.get_actor_location()

    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villager spawned", len(vills) >= 1):
        return
    v = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % v.get_name())
    yield 3
    # NO AssignHouse — homeless on purpose. Strand them in the open, away
    # from the fire and from canopy (roof-luck flaked the seasons gate once).
    away = unreal.Vector(floc.x + 2500.0, floc.y + 500.0, floc.z + 50.0)
    ai = unreal.MOSurvivorController.cast(v.get_controller())
    if ai:
        ai.set_stay_at_location(away)

    # ---- winter night at speed ------------------------------------------------
    _clock(world).set_game_date_time(unreal.DateTime(2027, 1, 15, 3, 0, 0))
    yield 10
    t0 = _body_temp(v)
    ctx.out("winter night: body=%.1fC, %.0fuu from the fire" % (
        t0, (v.get_actor_location() - floc).length()))
    _clock(world).set_time_scale(120.0)

    relocated = False
    min_temp = t0
    temp_at_fire_first = None
    warmed = False
    for i in range(110):
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
        dist = (v.get_actor_location() - floc).length()
        if not relocated and dist < 500.0:
            relocated = True
            temp_at_fire_first = temp
            ctx.out("poll %d: AT THE FIRE (%.0fuu, body %.1fC)" % (i, dist, temp))
        if relocated and temp >= min(temp_at_fire_first + 0.5, 36.8):
            warmed = True
            ctx.out("poll %d: WARMING (body %.1fC)" % (i, temp))
            break
        if i % 20 == 19:
            ctx.out("poll %d: dist=%.0f body=%.1fC" % (i, dist, temp))
    _clock(world).set_time_scale(1.0)

    ctx.assert_true("COLD BITES: body dropped below seek threshold (min %.1fC)" % min_temp,
                    min_temp < 36.2)
    ctx.assert_true("TO THE FIRE: homeless villager relocated to the lit hearth", relocated)
    ctx.assert_true("WARMING: body temp climbed at the fire", warmed)
    ctx.out("F1 hearth gate complete")
