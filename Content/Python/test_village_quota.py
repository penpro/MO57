"""V2.1 gate: standing orders — villagers fill quotas unattended (pipeline V2).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_quota.py --timeout 1200

Flow:
  0  boot standalone (seed 4242)
  1  found settlement; place basket + workbench; spawn/recruit/skill TWO
     villagers; stock 2x Flint01
  2  MO.Colony.SetQuota FlintFlake01 KnapFlintFlakes 8  — and NOTHING else:
     no AssignJob, no UI. The quota pass on the upkeep tick must notice the
     deficit, hand standing orders to idle villagers, and the V0 job flow
     does the real labor (2 crafts x 4 flakes = quota exactly met when the
     flint runs out).
  3  assert >= 8 FlintFlake01 in communal storage, unattended.
"""
import unreal


SEED = 4242
RECIPE_OUT = "FlintFlake01"
QUOTA = 8


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="QuotaGate")

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

    # ---- setup --------------------------------------------------------------
    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildBasketContainer01 350")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildWorkbench 700")
    yield 5
    for _ in range(2):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("2 villagers spawned (%d)" % len(vills), len(vills) >= 2):
        return
    for v in vills[:2]:
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 4
        _exec(world, "MO.Colony.SetSkill %s Stoneworking 3" % v.get_name())
        yield 2
    basket = _find_one(world, unreal.MOContainerActor)
    _exec(world, "MO.Colony.Stock %s Flint01 2" % basket.get_name())
    yield 5

    # ---- the standing order, and then HANDS OFF -----------------------------
    _exec(world, "MO.Colony.SetQuota %s KnapFlintFlakes %d" % (RECIPE_OUT, QUOTA))
    yield 5

    flakes = -1
    for i in range(90):                       # ~5 min: 2 sequential/parallel craft loops
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        flakes = _basket_count(world, RECIPE_OUT)
        if i % 9 == 8:
            ctx.out("poll %d: basket %s=%d" % (i, RECIPE_OUT, flakes))
            _exec(world, "MO.Colony.Status")
        if flakes >= QUOTA:
            break
    ctx.assert_true("quota filled UNATTENDED (%s=%d, target %d)" % (RECIPE_OUT, flakes, QUOTA),
                    flakes >= QUOTA)
    ctx.out("V2.1 quota gate complete")
