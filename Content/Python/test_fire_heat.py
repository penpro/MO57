"""F1 gate: a lit campfire warms the air around it and dries wet clothes.

    python Tools/ue.py seq Content/Python/test_fire_heat.py --timeout 600

Places a REAL campfire station, then walks the whole chain:
  (a) UNLIT: fresh campfire (fuel required, none loaded) contributes nothing
  (b) LIT: fueled + activated -> heat delta near the fire, zero beyond radius
  (c) DRYING: a soaked pawn by the fire dries far faster than open-air decay
  (d) DEPLETED: heat dies with the fuel (the fire is a consumer, not a toggle)
"""
import unreal


SEED = 991


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _heat_at(world, loc):
    return unreal.MOWeatherBlueprintLibrary.get_local_heat_delta_at(world, loc)


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="FireGate")

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

    # ---- place a real campfire station ---------------------------------------
    _exec(world, "MO.Colony.PlaceBuilding BuildCampfire 400")
    yield 10
    fire = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCraftingStationActor):
        if act.get_recipe_id() == "BuildCampfire":
            fire = act
            break
    if not ctx.guard("campfire placed", fire is not None):
        return
    floc = fire.get_actor_location()
    near = unreal.Vector(floc.x + 100.0, floc.y, floc.z)
    far = unreal.Vector(floc.x + 2000.0, floc.y, floc.z)

    # (a) UNLIT: requires fuel, has none -> no warmth
    h_unlit = _heat_at(world, near)
    ctx.out("unlit heat near fire: %.2fC" % h_unlit)
    ctx.assert_true("UNLIT: cold hearth warms nobody (%.2f)" % h_unlit, h_unlit == 0.0)

    # (b) LIT: fuel + activate -> warmth near, nothing beyond the radius
    added = fire.add_fuel("Stick01", 5)
    fire.set_station_active(True)
    yield 5
    h_near = _heat_at(world, near)
    h_far = _heat_at(world, far)
    ctx.out("lit: fuel added %.0f, heat near %.2fC, heat far %.2fC" % (added, h_near, h_far))
    ctx.assert_true("LIT: warm near the fire (%.2f > 10)" % h_near, h_near > 10.0)
    ctx.assert_true("LIT: falloff reaches zero beyond radius (%.2f)" % h_far, h_far == 0.0)

    # (c) DRYING: soak the player, park them by the fire, and compare the dry
    # rate against the same window in open air. Same real duration, same pawn,
    # same weather — the only variable is the fire.
    vit = player.get_component_by_class(unreal.MOVitalsComponent)
    if not ctx.guard("player has vitals", vit is not None):
        return
    away = unreal.Vector(floc.x + 3000.0, floc.y, floc.z + 50.0)

    player.set_actor_location(away, False, False)
    vit.set_wetness_level(1.0)
    for _ in range(4):
        yield 10   # ~8s real: a few wetness polls
    wet_away = vit.get_wetness_level()
    dried_away = 1.0 - wet_away

    # Stand OUTSIDE the fire's collision hull (teleporting to 1m overlaps it
    # and depenetration shoves the pawn out past the heat radius — found by
    # instrumented polls 2026-07-07). 2.5m is still deep in the warmth.
    near_stand = unreal.Vector(floc.x + 250.0, floc.y, floc.z + 50.0)
    player.set_actor_location(near_stand, False, False)
    vit.set_wetness_level(1.0)
    for _ in range(4):
        yield 10
        ploc = player.get_actor_location()
        ctx.out("  near-fire poll: heat_at_pawn=%.2fC dist_to_fire=%.0fuu wet=%.3f fuel=%.1f active=%s"
                % (_heat_at(world, ploc), (ploc - floc).length(),
                   vit.get_wetness_level(), fire.get_editor_property("current_fuel"),
                   fire.is_station_active()))
    wet_fire = vit.get_wetness_level()
    dried_fire = 1.0 - wet_fire

    ctx.out("dried in open air: %.3f | dried by the fire: %.3f" % (dried_away, dried_fire))
    ctx.assert_true(
        "DRYING: fire dries clothes far faster (%.3f > 4x %.3f)" % (dried_fire, dried_away),
        dried_fire > 4.0 * max(dried_away, 0.001) and dried_fire > 0.05)

    # (d) DEPLETED: burn the fuel down -> the warmth dies with it.
    # 50 fuel at 0.1/s is realistic (~8 min); config-for-test: drain directly
    # and let the station's own tick notice the empty hearth.
    fire.set_editor_property("current_fuel", 0.05)
    for _ in range(12):
        yield 5
        if _heat_at(world, near) == 0.0:
            break
    h_dead = _heat_at(world, near)
    ctx.out("after fuel depletion: %.2fC" % h_dead)
    ctx.assert_true("DEPLETED: heat dies with the fuel (%.2f)" % h_dead, h_dead == 0.0)

    ctx.out("F1 fire-heat gate complete")
