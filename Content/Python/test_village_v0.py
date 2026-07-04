"""V0 fun-gate: ONE villager runs ONE REAL crafting job (pipeline card V0, #170).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_v0.py --timeout 900

Flow (all via MO.Colony.* dev verbs -- deferred console bodies, real game code):
  0  1-player standalone PIE -> menu -> new game (seed 4242) -> pawn possessed
  1  place a COMPLETED BuildBasketContainer01 (storage) + BuildWorkbench (station)
  2  spawn + force-recruit a survivor; give it Stoneworking 3
  3  stock the basket with 1x Flint01
  4  AssignJob: CraftAtStation KnapFlintFlakes (station=workbench, storage=basket)
  5  poll: the survivor must walk to the basket, withdraw the flint, walk to
     the bench, run the REAL crafting queue (15s craft), walk back, and
     deposit FlintFlake01 x4 -- assert the basket ends with >=1 FlintFlake01
     and 0 Flint01.

The job cycle is observable via [MOQUERY] COLONY Status dumps along the way.
NOTE: unfocused PIE runs ~3 fps -- yield 10 ~= 3 s wall time.
"""
import unreal


SEED = 4242
RECIPE = "KnapFlintFlakes"          # Flint01 x1 -> FlintFlake01 x4, 15s, Stoneworking 3
INGREDIENT = "Flint01"
OUTPUT = "FlintFlake01"
STORAGE_RECIPE = "BuildBasketContainer01"   # ContainerSlotCount=2 -> AMOContainerActor
STATION_RECIPE = "BuildWorkbench"           # ProvidedStationType=Workbench -> AMOCraftingStationActor


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


def _container_count(world, item_id):
    """Item count of item_id in the (single) placed container, or -1."""
    container = _find_one(world, unreal.MOContainerActor)
    if not container:
        return -1
    inv = container.get_component_by_class(unreal.MOInventoryComponent)
    if not inv:
        return -1
    try:
        return inv.get_item_count_by_definition_id(item_id)
    except Exception:
        return -1


def sequence(ctx):
    helper = unreal.MOEditorTestHelper

    # ---- Phase 0: boot a 1-player standalone game -------------------------
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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="V0Host")

    pawn = None
    for _ in range(60):                       # world gen takes a while
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            pawn = p
            break
    if not ctx.guard("player pawn possessed", pawn is not None):
        return

    # ---- Phase 1: place completed storage + station -----------------------
    _exec(world, "MO.Colony.PlaceBuilding %s 350" % STORAGE_RECIPE)
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding %s 700" % STATION_RECIPE)
    yield 5
    world = helper.find_pie_world_by_net_mode("Standalone")
    storage = _find_one(world, unreal.MOContainerActor)
    station = _find_one(world, unreal.MOCraftingStationActor)
    ctx.assert_true("storage container placed", storage is not None)
    ctx.assert_true("crafting station placed", station is not None)
    if not (storage and station):
        return

    # ---- Phase 2: spawn + recruit + skill the villager ---------------------
    _exec(world, "MO.Colony.SpawnSurvivor 200")
    yield 5
    _exec(world, "MO.Colony.Recruit MetaHuman")
    yield 5
    _exec(world, "MO.Colony.SetSkill MetaHuman Stoneworking 3")
    yield 5

    # ---- Phase 3: stock the basket -----------------------------------------
    _exec(world, "MO.Colony.Stock %s %s 1" % (storage.get_name(), INGREDIENT))
    yield 5
    flint_before = _container_count(world, INGREDIENT)
    ctx.assert_true("basket stocked with %s" % INGREDIENT, flint_before >= 1)

    # ---- Phase 4: assign the craft job --------------------------------------
    _exec(world, "MO.Colony.AssignJob MetaHuman %s %s %s 1"
          % (RECIPE, station.get_name(), storage.get_name()))
    yield 5
    _exec(world, "MO.Colony.Status")

    # ---- Phase 5: watch the villager do REAL work ---------------------------
    # walk + 2s withdraw + walk + 15s craft + walk + 2s deposit; generous
    # budget for slow unfocused PIE and nav detours.
    output_count = -1
    for i in range(60):                       # ~3 min wall time
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        output_count = _container_count(world, OUTPUT)
        if i % 6 == 5:
            _exec(world, "MO.Colony.Status")
            ctx.out("poll %d: basket %s=%d" % (i, OUTPUT, output_count))
        if output_count >= 1:
            break

    ctx.assert_true("villager crafted %s into storage (count=%d)" % (OUTPUT, output_count),
                    output_count >= 1)
    ctx.assert_true("ingredient consumed from storage",
                    _container_count(world, INGREDIENT) == 0)
    _exec(world, "MO.Colony.Status")
    ctx.out("V0 fun-gate complete")
