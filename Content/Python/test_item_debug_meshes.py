"""Debug-mesh gate: a dropped item is VISIBLE in the world (rule 1).

    python Tools/ue.py seq Content/Python/test_item_debug_meshes.py --timeout 600

Gives the player one item of each debug-meshed type, drops them, and asserts
the spawned world items carry the SM_GBI_* graybox meshes. Ends with a PIE
shot for the eyeball check.
"""
import unreal


ITEMS = ["Flint01", "Berries01"]   # MATERIAL + CONSUMABLE — two shape families


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def sequence(ctx):
    helper = unreal.MOEditorTestHelper
    ok = helper.configure_pie(1, False)
    if not ctx.guard("ConfigurePIE", ok):
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
    if not ctx.guard("PIE world", world is not None):
        return
    if ctx.atl:
        ctx.atl.skip_intro(world, ctx.out)
        yield 10
        ctx.atl.start_new_game(world, ctx.out, seed=4242, survivor_name="MeshGate")
    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            player = p
            break
    if not ctx.guard("player pawn", player is not None):
        return

    inv = player.get_component_by_class(unreal.MOInventoryComponent)
    dropped = []
    for i, item_id in enumerate(ITEMS):
        guid = unreal.GuidLibrary.new_guid()
        gave = inv.add_item_by_guid(guid, item_id, 1)
        loc = player.get_actor_location() + unreal.Vector(150.0 + i * 80.0, 0.0, 20.0)
        actor = inv.drop_item_by_guid(guid, loc, unreal.Rotator(0.0, 0.0, 0.0))
        dropped.append((item_id, actor))
        ctx.out("dropped %s gave=%s actor=%s" % (item_id, gave, actor.get_name() if actor else None))
        yield 5

    for item_id, actor in dropped:
        mesh_name = ""
        if actor:
            smc = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh = smc.static_mesh if smc else None
            mesh_name = mesh.get_name() if mesh else ""
        ctx.assert_true("%s world item renders a debug mesh (%s)" % (item_id, mesh_name),
                        mesh_name.startswith("SM_GBI_"))
    ctx.out("item debug-mesh gate complete")
