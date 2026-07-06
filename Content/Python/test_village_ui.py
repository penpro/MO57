"""V1 gate (b): the V0 craft flow driven THROUGH the colony UI (pipeline V1, #170).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_ui.py --timeout 1200

Flow:
  0  boot standalone (seed 4242) -> pawn
  1  found settlement; place workbench + basket; spawn/recruit/skill a villager;
     stock 1x Flint01
  2  MO.Colony.UI  -> colony overview (C++-built tree) on the Menu layer
  3  MO.Test.FindWidget ColonyAssignJob  (harness sees the button)
  4  MO.Test.ClickWidget ColonyAssignJob_0 -> EnqueueCraftJob fires through the
     REAL CommonUI click path (SimulateClick + guards)
  5  assert the CraftAtStation job landed on the villager's queue, then the
     full V0 outcome: FlintFlake01 deposited into the basket
"""
import unreal


SEED = 4242
STORAGE_RECIPE = "BuildBasketContainer01"
STATION_RECIPE = "BuildWorkbench"
RECIPE_OUT = "FlintFlake01"


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _find_one(world, cls):
    try:
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
        return actors[0] if actors else None
    except Exception:
        return None


def _villager(world, player):
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            return a
    return None


def _basket_count(world, item_id):
    basket = _find_one(world, unreal.MOContainerActor)
    if not basket:
        return -1
    inv = basket.get_component_by_class(unreal.MOInventoryComponent)
    return inv.get_item_count_by_definition_id(item_id) if inv else -1


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="UIGate")

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

    # ---- V0 setup ----------------------------------------------------------
    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding %s 350" % STORAGE_RECIPE)
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding %s 700" % STATION_RECIPE)
    yield 5
    _exec(world, "MO.Colony.SpawnSurvivor 250")
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vill = _villager(world, _pawn(world))
    if not ctx.guard("villager spawned", vill is not None):
        return
    _exec(world, "MO.Colony.Recruit %s" % vill.get_name())
    yield 5
    _exec(world, "MO.Colony.SetSkill %s Stoneworking 3" % vill.get_name())
    yield 3
    basket = _find_one(world, unreal.MOContainerActor)
    _exec(world, "MO.Colony.Stock %s Flint01 1" % basket.get_name())
    yield 5

    # ---- open the overview and drive the flow through UI ------------------
    _exec(world, "MO.Colony.UI")
    yield 10
    _exec(world, "MO.Test.FindWidget ColonyAssignJob")
    yield 5
    _exec(world, "MO.Test.ClickWidget ColonyAssignJob_0")
    yield 2

    # Informational peek: the job can already be mid-flight (active entries
    # move through the queue fast); the BINDING assert is the outcome below —
    # flakes in the basket REQUIRE the whole enqueue->withdraw->craft->deposit
    # chain to have run from this click.
    world = helper.find_pie_world_by_net_mode("Standalone")
    vill = _villager(world, _pawn(world))
    jq = vill.get_component_by_class(unreal.MOSurvivorJobQueueComponent) if vill else None
    jobs = jq.get_all_jobs() if jq else []
    ctx.out("queue right after click: %d job(s)" % len(jobs))

    # ---- the full V0 outcome, UI-initiated ---------------------------------
    flakes = -1
    for i in range(60):                       # ~3 min: walk + withdraw + 15s craft + deposit
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        flakes = _basket_count(world, RECIPE_OUT)
        if i % 6 == 5:
            ctx.out("poll %d: basket %s=%d" % (i, RECIPE_OUT, flakes))
        if flakes >= 1:
            break
    ctx.assert_true("UI-assigned craft completed (%s=%d in basket)" % (RECIPE_OUT, flakes),
                    flakes >= 1)
    ctx.out("V1 UI gate complete")
