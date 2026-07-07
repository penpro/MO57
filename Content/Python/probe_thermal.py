"""Diagnostic: why doesn't a pawn cool at -10C? Reads the whole thermal chain."""
import unreal


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
        ctx.atl.start_new_game(world, ctx.out, seed=4242, survivor_name="ThermalProbe")
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

    wsub = unreal.MOWeatherIntegrationSubsystem.cast(
        helper.get_world_subsystem(world, unreal.MOWeatherIntegrationSubsystem))
    clock = unreal.MOGameClockSubsystem.cast(
        helper.get_world_subsystem(world, unreal.MOGameClockSubsystem))
    clock.set_game_date_time(unreal.DateTime(2027, 1, 15, 3, 0, 0))
    yield 10

    loc = player.get_actor_location()
    C = unreal.MOTemperatureUnit.CELSIUS
    ctx.out("provider=%s" % wsub.has_weather_provider())
    ctx.out("global=%.1f atLoc=%.1f feelsLike=%.1f" % (
        wsub.get_global_temperature(C),
        wsub.get_temperature_at_location(loc, C),
        wsub.get_feels_like_temperature(loc, C)))
    vit = player.get_component_by_class(unreal.MOVitalsComponent)
    ctx.out("envEnabled=%s insulation=%.2f tickInterval known" % (
        vit.get_editor_property("enable_environmental_temperature"),
        vit.get_editor_property("default_insulation_factor")))
    # villager comparison: why does the AI pawn not cool?
    unreal.SystemLibrary.execute_console_command(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    unreal.SystemLibrary.execute_console_command(world, "MO.Colony.SpawnSurvivor 250")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vill = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != _pawn(world) and "MetaHuman" in a.get_name():
            vill = a
            break
    if vill:
        vv = vill.get_component_by_class(unreal.MOVitalsComponent)
        if vv:
            ctx.out("VILLAGER envEnabled=%s insulation=%.2f compTickEnabled=%s class=%s" % (
                vv.get_editor_property("enable_environmental_temperature"),
                vv.get_editor_property("default_insulation_factor"),
                vv.is_component_tick_enabled(), vill.get_class().get_name()))
            ctx.out("PLAYER compTickEnabled=%s class=%s" % (
                vit.is_component_tick_enabled(), player.get_class().get_name()))
        else:
            ctx.out("VILLAGER has NO vitals component (class=%s)" % vill.get_class().get_name())
    else:
        ctx.out("no villager found")

    clock.set_time_scale(120.0)
    for i in range(10):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world)
        if not p:
            break
        v = p.get_component_by_class(unreal.MOVitalsComponent)
        body = v.get_vital_signs().get_editor_property("body_temperature")
        fl = wsub.get_feels_like_temperature(p.get_actor_location(), C)
        vbody, vfl = -1.0, 0.0
        if vill:
            vv = vill.get_component_by_class(unreal.MOVitalsComponent)
            if vv:
                vbody = vv.get_vital_signs().get_editor_property("body_temperature")
                vfl = wsub.get_feels_like_temperature(vill.get_actor_location(), C)
        ctx.out("poll %d: PLAYER body=%.2f fl=%.1f | VILLAGER body=%.2f fl=%.1f" % (
            i, body, fl, vbody, vfl))
    clock.set_time_scale(1.0)
    ctx.assert_true("probe complete", True)
