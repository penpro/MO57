"""Probe: wild survivors spawn CLOTHED (spawn-manager starter loadout).

    python Tools/ue.py seq Content/Python/probe_survivor_clothing.py --timeout 600

Force-spawns a Survivor through the REAL spawn-manager path (not the dev verb)
and asserts the pawn is wearing the configured starter set.
"""
import unreal


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


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
        ctx.atl.start_new_game(world, ctx.out, seed=777, survivor_name="ClothProbe")

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

    sm = helper.get_world_subsystem(world, unreal.MOSpawnManagerSubsystem)
    sm = unreal.MOSpawnManagerSubsystem.cast(sm) if sm else None
    if not ctx.guard("spawn manager up", sm is not None):
        return

    loc = player.get_actor_location()
    spawned = sm.force_spawn_at_location(
        unreal.MOSpawnCategory.SURVIVOR,
        unreal.Vector(loc.x + 500.0, loc.y, loc.z + 50.0))
    if not ctx.guard("survivor spawned via spawn manager", spawned is not None):
        return
    yield 10

    equip = spawned.get_component_by_class(unreal.MOEquipmentComponent)
    if not ctx.guard("survivor has equipment component", equip is not None):
        return
    items = equip.get_all_equipped_items()
    worn = [str(i.get_editor_property("item_definition_id")) for i in items
            if i.get_editor_property("item_definition_id") != "None"]
    ctx.out("survivor %s wearing: %s" % (spawned.get_name(), worn))
    ctx.assert_true("survivor spawned wearing 3 starter items (%d)" % len(worn),
                    len(worn) >= 3)
    ctx.out("clothed-survivor probe complete")
