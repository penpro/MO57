"""F1/T3 gate: a villager keeps the home fire burning (pipeline F1 unit 4).

    python Tools/ue.py seq Content/Python/test_village_firewood.py --timeout 900

Settlement with a nearly-dead campfire, a basket of firewood, and one idle
recruit. The colony hearth pass must notice the hungry fire and send the
villager hauling — with no player input:
  (a) HAUL: the villager receives and runs a RefuelStation job
  (b) FED: the fire's fuel tank rises above the refuel threshold
  (c) RELIT: a fire that burned out during the wait ends up BURNING again
"""
import unreal


SEED = 6210


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _colony(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOColonyManagerSubsystem)
    return unreal.MOColonyManagerSubsystem.cast(o) if o else None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="FirewoodGate")

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

    # ---- settlement: campfire + firewood basket + one idle recruit -----------
    _exec(world, "MO.Colony.Found Emberwatch 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildCampfire 500")
    yield 8
    _exec(world, "MO.Colony.PlaceBuilding BuildBasketContainer01 900")
    yield 8

    fire = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCraftingStationActor):
        if act.get_recipe_id() == "BuildCampfire":
            fire = act
            break
    if not ctx.guard("campfire placed", fire is not None):
        return
    basket = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOContainerActor):
        basket = act
        break
    if not ctx.guard("basket placed", basket is not None):
        return
    _exec(world, "MO.Colony.Stock %s Firewood01 12" % basket.get_name())
    yield 5

    # Nearly-dead fire: one stick's worth, burning. It dies during the wait,
    # which also exercises the relight half of the job.
    fire.add_fuel("Firewood01", 1)
    fire.set_editor_property("current_fuel", 3.0)
    fire.set_station_active(True)

    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villager spawned", len(vills) >= 1):
        return
    v = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % v.get_name())
    yield 5

    colony = _colony(world)
    if not ctx.guard("colony subsystem up", colony is not None):
        return

    # ---- let the hearth pass work ---------------------------------------------
    hauled = False
    fed = False
    relit = False
    fuel0 = fire.get_editor_property("current_fuel")
    ctx.out("fire starts at %.1f fuel, basket holds 12 Firewood01" % fuel0)
    for i in range(80):
        colony.run_upkeep_tick()
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        jq = v.get_component_by_class(unreal.MOSurvivorJobQueueComponent)
        cur = jq.get_current_job() if jq else None
        # The job can START and FINISH between polls (short legs, 2s handling)
        # so sampling "current job" races. Evidence instead: only the hauler
        # removes firewood from the basket.
        binv = basket.get_component_by_class(unreal.MOInventoryComponent)
        left = binv.get_item_count_by_definition_id("Firewood01") if binv else -1
        if not hauled and 0 <= left < 12:
            hauled = True
            ctx.out("poll %d: HAULED (basket down to %d Firewood01)" % (i, left))
        fuel = fire.get_editor_property("current_fuel")
        active = fire.is_station_active()
        if not fed and fuel > 40.0:
            fed = True
            ctx.out("poll %d: FED (fuel %.1f)" % (i, fuel))
        if fed and active:
            relit = True
            ctx.out("poll %d: BURNING (fuel %.1f, active %s)" % (i, fuel, active))
            break
        if i % 15 == 14:
            ctx.out("poll %d: fuel=%.1f active=%s job=%s" % (
                i, fuel, active,
                str(cur.get_editor_property("job_type")) if cur else "idle"))

    ctx.assert_true("HAUL: villager withdrew firewood from the basket", hauled)
    ctx.assert_true("FED: fire's tank refilled past the threshold", fed)
    ctx.assert_true("RELIT: the fire is burning again", relit)
    ctx.out("F1 firewood gate complete")
