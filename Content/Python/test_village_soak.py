"""V2 MILESTONE soak gate: the settlement HOLDS (pipeline V2 milestone).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_soak.py --timeout 1800

Five villagers, three accelerated game days, ZERO player intervention after
setup. The colony must:
  (1) keep all five alive and out of starvation (real food from communal
      storage through the real metabolism),
  (2) keep everyone home (inside the settlement radius),
  (3) produce per the standing order (quota fills through the real V0
      craft loop: withdraw mats -> real 15s crafts at the workbench ->
      deposit output),
  (4) hold mood above the misery floor (housed + fed).

Setup: June dates (summer — winter survival is the V2.5 card), basket +
workbench + 3 lean-tos (capacity 2 each), food + flint stocked, quota
FlintFlake01=24 (6 crafts x 4 flakes), then HANDS OFF at x480 for ~3
game days (~9 real minutes).
"""
import unreal


SEED = 4242
RECIPE_OUT = "FlintFlake01"
QUOTA = 24
VILLAGERS = 5
SOAK_POLLS = 250          # ~9 real min at x480 ~= 3 game days


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


def _colony(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOColonyManagerSubsystem)
    return unreal.MOColonyManagerSubsystem.cast(o) if o else None


def _find_one(world, cls):
    try:
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
        return actors[0] if actors else None
    except Exception:
        return None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _basket_count(world, item_id):
    basket = _find_one(world, unreal.MOContainerActor)
    if not basket:
        return -1
    inv = basket.get_component_by_class(unreal.MOInventoryComponent)
    return inv.get_item_count_by_definition_id(item_id) if inv else -1


def _food_calories(name):
    try:
        r = unreal.MOItemDatabaseSettings.get_item_definition(name)
    except Exception:
        return 0.0
    row = r[1] if isinstance(r, tuple) else r
    ok = r[0] if isinstance(r, tuple) else bool(r)
    return float(row.nutrition.calories) if (ok and row) else 0.0


def _find_best_food():
    """Highest-calorie edible in DT_Items — 3 days x 5 villagers is ~30k kcal
    and the basket should not need 600 apples to hold it."""
    best, best_cal = None, 50.0
    table = None
    try:
        table = unreal.get_default_object(unreal.MOItemDatabaseSettings).get_editor_property("item_definitions_data_table")
    except Exception:
        table = None
    if table:
        try:
            for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table):
                cal = _food_calories(name)
                if cal > best_cal:
                    best, best_cal = str(name), cal
        except Exception:
            pass
    if best:
        return best, best_cal
    for cand in ("CookedMeat01", "RawMeat01", "Berries01", "Apple01"):
        cal = _food_calories(cand)
        if cal > 50.0:
            return cand, cal
    return None, 0.0


def _guid_of(pawn):
    ident = pawn.get_component_by_class(unreal.MOIdentityComponent)
    return ident.get_guid() if ident else None


def _guid_key(pawn):
    """VALUE key for identity sets — str() on a wrapped struct is the wrapper
    ADDRESS, unique per call, so it never matches across polls."""
    g = _guid_of(pawn)
    if not g:
        return None
    try:
        return g.to_string()
    except Exception:
        return g.export_text()


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="SoakGate")

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

    # ---- build the settlement ------------------------------------------------
    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildBasketContainer01 350")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildWorkbench 700")
    yield 5
    for _ in range(3):
        _exec(world, "MO.Colony.PlaceBuilding BuildLeanTo 500")
        yield 4
    for _ in range(VILLAGERS):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("%d villagers spawned (%d)" % (VILLAGERS, len(vills)), len(vills) >= VILLAGERS):
        return
    vills = vills[:VILLAGERS]
    for v in vills:
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 3
        _exec(world, "MO.Colony.SetSkill %s Stoneworking 3" % v.get_name())
        yield 2

    # houses: 3 lean-tos x capacity 2 covers all five
    houses = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor):
        if a.get_recipe_id() == "BuildLeanTo":
            houses.append(a)
    if not ctx.guard("3 lean-tos placed (%d)" % len(houses), len(houses) >= 3):
        return
    for i, v in enumerate(vills):
        _exec(world, "MO.Colony.AssignHouse %s %s" % (v.get_name(), houses[i // 2].get_name()))
        yield 2

    # identity: the FIVE RECRUITS are the test subjects — wild wanderers may
    # drift into the area during the soak and must not pollute the verdicts.
    recruit_guids = set()
    for v in vills:
        k = _guid_key(v)
        if k:
            recruit_guids.add(k)
    if not ctx.guard("5 recruit GUIDs captured (%d)" % len(recruit_guids), len(recruit_guids) == VILLAGERS):
        return

    # stores: provision by CALORIES (5 villagers x 3 days x ~2000 kcal + 30%)
    food, food_cal = _find_best_food()
    if not ctx.guard("edible item found (%s, %.0f kcal)" % (food, food_cal), food is not None):
        return
    need_items = int((VILLAGERS * 3 * 2000.0 * 1.3) / food_cal) + 1
    basket = _find_one(world, unreal.MOContainerActor)
    _exec(world, "MO.Colony.Stock %s %s %d" % (basket.get_name(), food, need_items))
    yield 4
    _exec(world, "MO.Colony.Stock %s Flint01 6" % basket.get_name())
    yield 4

    # the standing order — then HANDS OFF
    _exec(world, "MO.Colony.SetQuota %s KnapFlintFlakes %d" % (RECIPE_OUT, QUOTA))
    yield 5

    colony = _colony(world)
    center = _colony(world).get_settlement().get_editor_property("center")
    radius = _colony(world).get_settlement().get_editor_property("radius")
    ctx.out("soak start: food=%s x%d (%.0f kcal each), flint=6, quota=%d, radius=%.0f"
            % (food, need_items, food_cal, QUOTA, radius))

    # ---- 3 game days, unattended ----------------------------------------------
    _clock(world).set_time_scale(480.0)
    day0 = _clock(world).get_game_date_time()
    flakes = -1
    for i in range(SOAK_POLLS):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        if i % 25 == 24:
            vills_now = _villagers(world, _pawn(world))
            flakes = _basket_count(world, RECIPE_OUT)
            fud = _basket_count(world, food)
            ctx.out("poll %d: villagers=%d flakes=%d food=%d" % (i, len(vills_now), flakes, fud))
            _exec(world, "MO.Colony.Status")
    _clock(world).set_time_scale(1.0)

    # ---- verdicts ---------------------------------------------------------------
    world = helper.find_pie_world_by_net_mode("Standalone")
    colony = _colony(world)
    vills_end = [v for v in _villagers(world, _pawn(world))
                 if _guid_key(v) in recruit_guids]
    ctx.assert_true("ALIVE: %d/%d RECRUITS present after 3 days" % (len(vills_end), VILLAGERS),
                    len(vills_end) >= VILLAGERS)

    starving = 0
    away = 0
    low_mood = 0
    for v in vills_end[:VILLAGERS]:
        metab = v.get_component_by_class(unreal.MOMetabolismComponent)
        if metab and metab.is_starving():
            starving += 1
        d = v.get_actor_location() - center
        if (d.x * d.x + d.y * d.y) > radius * radius:
            away += 1
        g = _guid_of(v)
        if g and colony.get_villager_mood(g) < 0.25:
            low_mood += 1
    ctx.assert_true("FED: nobody starving (%d starving)" % starving, starving == 0)
    ctx.assert_true("HOME: nobody left the settlement (%d away)" % away, away == 0)
    ctx.assert_true("MOOD: nobody in misery (%d below 0.25)" % low_mood, low_mood == 0)

    flakes = _basket_count(world, RECIPE_OUT)
    ctx.assert_true("PRODUCING: quota filled unattended (%s=%d, target %d)"
                    % (RECIPE_OUT, flakes, QUOTA), flakes >= QUOTA)
    ctx.out("V2 MILESTONE soak complete — the settlement holds")
