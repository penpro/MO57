"""V0 co-op sub-gate: the villager is server-authoritative, the CLIENT sees the
result (pipeline V0 sub-gate, unblocked by S0).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_coop.py --timeout 1200
    (2-client listen-server PIE; restores nothing itself — run via a wrapper
     that ends PIE, or follow with `ue.py pie end` + configure_pie(1, False))

Flow mirrors mptest phases 0-3, then:
  4  HOST world (authority): found settlement, place basket + workbench,
     spawn/recruit/skill a villager, stock flint, assign the craft job
  5  CLIENT world assertions: the villager pawn REPLICATED (a second
     MetaHuman exists client-side) and the crafted output lands in the
     CLIENT's view of the communal basket (FastArray inventory replication).
"""
import unreal


SEED = 4242
RECIPE_OUT = "FlintFlake01"


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _metahumans(world):
    out = []
    try:
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
            if "MetaHuman" in a.get_name():
                out.append(a)
    except Exception:
        pass
    return out


def _basket_count(world, item_id):
    try:
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOContainerActor)
    except Exception:
        return -1
    for basket in actors:
        inv = basket.get_component_by_class(unreal.MOInventoryComponent)
        if inv:
            return inv.get_item_count_by_definition_id(item_id)
    return -1


def sequence(ctx):
    helper = unreal.MOEditorTestHelper

    ok = helper.configure_pie(2, True)
    if not ctx.guard("ConfigurePIE(2, listen) succeeded", ok):
        return
    yield 1
    if ctx.atl:
        ctx.atl.begin_pie(ctx.out)
    host = client = None
    for _ in range(90):
        yield 10
        host = helper.find_pie_world_by_net_mode("ListenServer")
        client = helper.find_pie_world_by_net_mode("Client")
        if host and client:
            break
    if not ctx.guard("both PIE worlds resolved", host and client):
        return
    if ctx.atl:
        ctx.atl.skip_intro(host, ctx.out)
        yield 10
        ctx.atl.start_new_game(host, ctx.out, seed=SEED, survivor_name="CoopHost")

    host_pawn = None
    for _ in range(60):
        yield 10
        host = helper.find_pie_world_by_net_mode("ListenServer")
        p = _pawn(host, 0) if host else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            host_pawn = p
            break
    if not ctx.guard("host pawn possessed", host_pawn is not None):
        return
    client = None
    for _ in range(60):
        yield 10
        client = helper.find_pie_world_by_net_mode("Client")
        p = _pawn(client, 0) if client else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            break
    if not ctx.guard("client pawn possessed (S0 join-spawn)", client is not None):
        return

    # ---- host-side V0 setup (all authority) --------------------------------
    _exec(host, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(host, "MO.Colony.PlaceBuilding BuildBasketContainer01 350")
    yield 5
    _exec(host, "MO.Colony.PlaceBuilding BuildWorkbench 700")
    yield 5
    _exec(host, "MO.Colony.SpawnSurvivor 250")
    yield 8
    host = helper.find_pie_world_by_net_mode("ListenServer")
    villager = None
    for p in _metahumans(host):
        if p != _pawn(host, 0) and p != _pawn(host, 1):
            villager = p
            break
    if not ctx.guard("host-side villager spawned", villager is not None):
        return
    _exec(host, "MO.Colony.Recruit %s" % villager.get_name())
    yield 4
    _exec(host, "MO.Colony.SetSkill %s Stoneworking 3" % villager.get_name())
    yield 3
    basket_host = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(host, unreal.MOContainerActor):
        basket_host = a
        break
    _exec(host, "MO.Colony.Stock %s Flint01 1" % basket_host.get_name())
    yield 4
    station = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(host, unreal.MOCraftingStationActor):
        station = a
        break
    _exec(host, "MO.Colony.AssignJob %s KnapFlintFlakes %s %s 1"
          % (villager.get_name(), station.get_name(), basket_host.get_name()))
    yield 5

    # Bring player 2 TO the settlement: S0 join-spawns roll independent
    # locations, so the client can start outside net relevancy of the host's
    # village — first run of this gate saw literally nothing replicate. The
    # teleport is the shortcut for "the second player walks over" (server-side
    # move of the client's authority pawn).
    p2 = _pawn(host, 1)
    if p2 and basket_host:
        loc = basket_host.get_actor_location()
        p2.set_actor_location(unreal.Vector(loc.x + 400.0, loc.y, loc.z + 200.0), False, True)
        ctx.out("player-2 pawn moved to the settlement (server-side)")
    for _ in range(9):                        # let relevancy + replication settle
        yield 10

    # ---- diagnostics: is the client even still CONNECTED? ------------------
    for line in helper.get_pie_worlds_summary().splitlines():
        ctx.out(line)
    host = helper.find_pie_world_by_net_mode("ListenServer")
    client = helper.find_pie_world_by_net_mode("Client")
    if host:
        for p in _metahumans(host):
            l = p.get_actor_location()
            ctx.out("HOST pawn %s @ (%.0f,%.0f,%.0f)" % (p.get_name(), l.x, l.y, l.z))
    if client:
        for p in _metahumans(client):
            l = p.get_actor_location()
            ctx.out("CLIENT pawn %s @ (%.0f,%.0f,%.0f)" % (p.get_name(), l.x, l.y, l.z))

    # ---- CLIENT-side truth --------------------------------------------------
    client = helper.find_pie_world_by_net_mode("Client")
    client_mh = len(_metahumans(client)) if client else 0
    ctx.assert_true("villager replicated to client (%d MetaHumans client-side)" % client_mh,
                    client_mh >= 3)   # client pawn + host proxy + villager

    flakes_client = -1
    for i in range(60):                       # ~3 min for the full labor loop
        yield 10
        client = helper.find_pie_world_by_net_mode("Client")
        if not client:
            break
        flakes_client = _basket_count(client, RECIPE_OUT)
        if i % 6 == 5:
            ctx.out("poll %d: CLIENT basket %s=%d" % (i, RECIPE_OUT, flakes_client))
        if flakes_client >= 1:
            break
    ctx.assert_true("client SEES the villager's work (%s=%d in client basket)" % (RECIPE_OUT, flakes_client),
                    flakes_client >= 1)
    ctx.out("V0 co-op sub-gate complete")
