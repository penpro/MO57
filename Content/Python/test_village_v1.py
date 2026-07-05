"""V1 gate (a+d): the settlement loop — real food, real mood, real save (pipeline V1, #170).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_v1.py --timeout 1200

Flow:
  0  boot standalone (seed 4242) -> pawn
  1  MO.Colony.Found at the pawn; place communal basket + BuildLeanTo dwelling
  2  spawn + recruit 3 villagers; house EXACTLY ONE of them
  3  stock the basket with real food (id discovered from DT_Items nutrition)
  4  accelerate the clock (x480 ~= 3 real minutes per game day) and let the
     upkeep tick run: villagers EAT (basket count drops via the real
     metabolism path) and the unhoused villagers' mood decays while the
     housed one's holds
  5  save -> mutate (evict the housed villager) -> load -> assert the
     settlement, residency and mood survived the round-trip

NOTE: probes use the ISM/base-class + [MOQUERY] patterns per AUTONOMOUS_TOOLING.
"""
import unreal


SEED = 4242
STORAGE_RECIPE = "BuildBasketContainer01"
HOUSE_RECIPE = "BuildLeanTo"
FOOD_STOCK = 14
SAVE_SLOT = "v1gate"


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


def _colony(world):
    # No SubsystemBlueprintLibrary in this build's py surface — use the helper.
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOColonyManagerSubsystem)
    return unreal.MOColonyManagerSubsystem.cast(o) if o else None


def _clock(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOGameClockSubsystem)
    return unreal.MOGameClockSubsystem.cast(o) if o else None


def _persistence(world):
    o = unreal.MOEditorTestHelper.get_game_instance_subsystem(world, unreal.MOPersistenceSubsystem)
    return unreal.MOPersistenceSubsystem.cast(o) if o else None


def _villagers(world, player):
    """Non-player MetaHuman pawns, stable order."""
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _guid_of(pawn):
    ident = pawn.get_component_by_class(unreal.MOIdentityComponent)
    return ident.get_guid() if ident else None


def _probe_food(name):
    try:
        r = unreal.MOItemDatabaseSettings.get_item_definition(name)
    except Exception:
        return False
    row = r[1] if isinstance(r, tuple) else r
    ok = r[0] if isinstance(r, tuple) else bool(r)
    return bool(ok and row and row.nutrition.calories > 50.0)


def _find_food_item():
    """First DT_Items row with real calories. EditorAssetLibrary is BLOCKED in
    play mode — reach the table through the runtime settings CDO instead."""
    table = None
    try:
        table = unreal.get_default_object(unreal.MOItemDatabaseSettings).get_editor_property("item_definitions_data_table")
    except Exception:
        table = None
    if table:
        try:
            for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table):
                if _probe_food(name):
                    return str(name)
        except Exception:
            pass
    for cand in ("CookedMeat01", "RawMeat01", "Berries01", "WildBerries01",
                 "Apple01", "Bread01", "Fish01", "CookedFish01"):
        if _probe_food(cand):
            return cand
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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="V1Host")

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

    # ---- settlement + buildings -------------------------------------------
    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding %s 350" % STORAGE_RECIPE)
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding %s 650" % HOUSE_RECIPE)
    yield 5
    world = helper.find_pie_world_by_net_mode("Standalone")
    basket = _find_one(world, unreal.MOContainerActor)
    ctx.assert_true("communal basket placed", basket is not None)
    house = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor):
        if a.get_recipe_id() == HOUSE_RECIPE:
            house = a
            break
    ctx.assert_true("lean-to dwelling placed", house is not None)
    if not (basket and house):
        return

    # ---- villagers ---------------------------------------------------------
    for _ in range(3):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    ctx.assert_true("3 villagers spawned (%d)" % len(vills), len(vills) >= 3)
    if len(vills) < 3:
        return
    for v in vills[:3]:
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 4
    housed, unhoused_a, unhoused_b = vills[0], vills[1], vills[2]
    _exec(world, "MO.Colony.AssignHouse %s %s" % (housed.get_name(), house.get_name()))
    yield 5

    # ---- food --------------------------------------------------------------
    food = _find_food_item()
    if not ctx.guard("edible item found in DT_Items (%s)" % food, food is not None):
        return
    _exec(world, "MO.Colony.Stock %s %s %d" % (basket.get_name(), food, FOOD_STOCK))
    yield 5
    food_before = _basket_count(world, food)
    ctx.assert_true("basket stocked (%d %s)" % (food_before, food), food_before >= FOOD_STOCK)

    # ---- accelerated day ----------------------------------------------------
    _clock(world).set_time_scale(480.0)
    ctx.out("time scale x480 — one game day in ~3 real minutes")
    housed_guid, ua_guid = _guid_of(housed), _guid_of(unhoused_a)
    food_after = food_before
    mood_housed = mood_unhoused = -1.0
    for i in range(66):                       # ~3.7 real min ~= 1.2 game days
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        colony = _colony(world)
        food_after = _basket_count(world, food)
        mood_housed = colony.get_villager_mood(housed_guid)
        mood_unhoused = colony.get_villager_mood(ua_guid)
        if i % 6 == 5:
            ctx.out("poll %d: food=%d moodH=%.2f moodU=%.2f" % (i, food_after, mood_housed, mood_unhoused))
            _exec(world, "MO.Colony.Status")
        if food_after < food_before and (mood_housed - mood_unhoused) >= 0.15:
            break
    _clock(world).set_time_scale(1.0)

    ctx.assert_true("villagers ate REAL food (basket %d -> %d)" % (food_before, food_after),
                    food_after < food_before)
    ctx.assert_true("unhoused mood decayed below housed (H=%.2f U=%.2f)" % (mood_housed, mood_unhoused),
                    (mood_housed - mood_unhoused) >= 0.15)

    # ---- save / mutate / load round-trip ------------------------------------
    persistence = _persistence(world)
    if not ctx.guard("persistence subsystem", persistence is not None):
        return
    saved = persistence.save_world_to_slot(SAVE_SLOT)
    ctx.assert_true("settlement saved", bool(saved))
    colony = _colony(world)
    colony.clear_residence(housed_guid)
    ctx.assert_true("mutation applied (evicted)", not colony.has_residence(housed_guid))
    loaded = persistence.load_world_from_slot(SAVE_SLOT)
    ctx.assert_true("settlement loaded", bool(loaded))
    for _ in range(6):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if world:
            break
    colony = _colony(world)
    name = str(colony.get_settlement().display_name)
    ctx.assert_true("settlement name survived ('%s')" % name, "FirstLanding" in name)
    ctx.assert_true("residency survived the round-trip", colony.has_residence(housed_guid))
    mood_back = colony.get_villager_mood(housed_guid)
    ctx.assert_true("mood survived (%.2f)" % mood_back, abs(mood_back - mood_housed) < 0.25)
    ctx.out("V1 settlement-loop gate complete")
