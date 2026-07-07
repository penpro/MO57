"""Diagnostic: why does a quota job stall at x480? Samples the assigned
villager's movement + medical gates live."""
import unreal


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


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
        ctx.atl.start_new_game(world, ctx.out, seed=4242, survivor_name="StallProbe")
    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            player = p
            break
    if not ctx.guard("player", player is not None):
        return

    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildBasketContainer01 350")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildWorkbench 700")
    yield 5
    # DISTINCT offsets — identical distances stack three buildings into one
    # overlapping collision hull that pens the villagers (2026-07-07).
    for dist in (450, 800, 1150):
        _exec(world, "MO.Colony.PlaceBuilding BuildLeanTo %d" % dist)
        yield 4
    for _ in range(5):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 4
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)
             if a != _pawn(world) and "MetaHuman" in a.get_name()]
    if not ctx.guard("villager", len(vills) >= 1):
        return
    houses = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor)
              if a.get_recipe_id() == "BuildLeanTo"]
    for idx, vv in enumerate(vills[:5]):
        _exec(world, "MO.Colony.Recruit %s" % vv.get_name())
        yield 3
        _exec(world, "MO.Colony.SetSkill %s Stoneworking 3" % vv.get_name())
        yield 2
        if houses:
            _exec(world, "MO.Colony.AssignHouse %s %s" % (vv.get_name(), houses[idx // 2 % len(houses)].get_name()))
            yield 2
        inv2 = vv.get_component_by_class(unreal.MOInventoryComponent)
        eq2 = vv.get_component_by_class(unreal.MOEquipmentComponent)
        if inv2 and eq2:
            for item_id, slot_name in [("LeatherTunic01", "CHEST"), ("LeatherTrousers01", "LEGS"), ("LeatherBoots01", "FEET")]:
                g = unreal.GuidLibrary.new_guid()
                inv2.add_item_by_guid(g, item_id, 1)
                slot = getattr(unreal.MOEquipmentSlot, slot_name, None)
                if slot is not None:
                    eq2.equip_from_inventory(inv2, g, slot)
    v = vills[0]
    basket = next(iter(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOContainerActor)), None)
    _exec(world, "MO.Colony.Stock %s Flint01 6" % basket.get_name())
    yield 4

    # x480 FIRST (the failing regime), then the quota
    clock = unreal.MOGameClockSubsystem.cast(
        helper.get_world_subsystem(world, unreal.MOGameClockSubsystem))
    clock.set_time_scale(480.0)
    _exec(world, "MO.Colony.SetQuota FlintFlake01 KnapFlintFlakes 24")
    yield 5

    for i in range(90):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        vills = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn)
                 if a != _pawn(world) and "MetaHuman" in a.get_name()]
        if not vills:
            continue
        v = vills[0]
        vel = v.get_velocity().length()
        vit = v.get_component_by_class(unreal.MOVitalsComponent)
        men = v.get_component_by_class(unreal.MOMentalStateComponent)
        body = vit.get_vital_signs().get_editor_property("body_temperature") if vit else -1
        cons, fat = "?", -1.0
        if men:
            ms = men.get_editor_property("mental_state") if hasattr(men, "get_editor_property") else None
            try:
                ms = men.get_mental_state()
                cons = str(ms.get_editor_property("consciousness_level"))
                fat = ms.get_editor_property("morale_fatigue")
            except Exception:
                pass
        can_move = "?"
        try:
            can_move = v.can_move()
        except Exception:
            pass
        basket_flakes = -1
        inv = basket.get_component_by_class(unreal.MOInventoryComponent)
        if inv:
            basket_flakes = inv.get_item_count_by_definition_id("FlintFlake01")
        # report ALL villagers' velocity so the ASSIGNED one is visible
        vels = ",".join("%.0f" % vv.get_velocity().length() for vv in vills[:5])
        if i % 6 == 0 or basket_flakes > 0:
            ctx.out("p%d vels=[%s] body0=%.1f canMove=%s flakes=%d"
                    % (i, vels, body, can_move, basket_flakes))
        if basket_flakes >= 4:
            ctx.out("CRAFT COMPLETED at poll %d — no stall in this run" % i)
            break
    clock.set_time_scale(1.0)
    ctx.assert_true("probe complete", True)
