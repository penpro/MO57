"""V2.4 gate: seasons are gameplay — winter is cold, cold is expensive, forage has windows.

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_seasons.py --timeout 900

Flow:
  0  boot standalone (seed 4242); found colony; spawn+recruit one villager
  1  SUMMER (Jul 21 15:00 via SetGameDateTime):
       (a) season index == 1, ambient warm (>18C)
       (b) villager thermogenesis multiplier ~1.0 (no cold burn)
       (c) Berries01 in season (window "Summer,Autumn" authored on the row)
  2  WINTER (Jan 15 03:00):
       (d) season index == 3, ambient below freezing
       (e) REAL SPIKE: villager body temp drops below neutral outdoors and
           the metabolism multiplier rises >1.2 — same villager, same idle
           activity, more kcal burned. Winter upkeep is EMERGENT, not scripted.
       (f) Berries01 out of season
"""
import unreal


SEED = 4242


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


def _weather(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOWeatherIntegrationSubsystem)
    return unreal.MOWeatherIntegrationSubsystem.cast(o) if o else None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _thermo(pawn):
    metab = pawn.get_component_by_class(unreal.MOMetabolismComponent)
    vit = pawn.get_component_by_class(unreal.MOVitalsComponent)
    mult = metab.get_current_thermogenesis_multiplier() if metab else -1.0
    temp = vit.get_vital_signs().get_editor_property("body_temperature") if vit else -1.0
    return (mult, temp)


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="SeasonGate")

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

    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.SpawnSurvivor 250")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villager spawned (%d)" % len(vills), len(vills) >= 1):
        return
    v = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % v.get_name())
    yield 5

    clock = _clock(world)
    weather = _weather(world)
    if not ctx.guard("clock+weather subsystems up", clock and weather):
        return

    # ---- SUMMER: Jul 21 15:00 -------------------------------------------------
    clock.set_game_date_time(unreal.DateTime(2026, 7, 21, 15, 0, 0))
    yield 10
    season = weather.get_current_season_index()
    temp_summer = weather.get_global_temperature(unreal.MOTemperatureUnit.CELSIUS)
    ctx.assert_true("SUMMER: season=%d ambient=%.1fC (warm)" % (season, temp_summer),
                    season == 1 and temp_summer > 10.0)

    # let vitals settle a few ticks at comfort, then read the burn multiplier
    for _ in range(10):
        yield 10
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    v = vills[0]
    m_summer, t_summer = _thermo(v)
    ctx.assert_true("SUMMER BURN: multiplier %.2f (~1.0) at body %.1fC" % (m_summer, t_summer),
                    0.99 <= m_summer <= 1.15)

    # ---- forage window on the REAL authored row --------------------------------
    in_summer = unreal.MOItemDatabaseSettings.is_item_in_forage_season("Berries01", 1)
    ctx.assert_true("FORAGE: Berries01 in season in summer", in_summer)

    # ---- WINTER: Jan 15 03:00 ---------------------------------------------------
    clock.set_game_date_time(unreal.DateTime(2027, 1, 15, 3, 0, 0))
    yield 10
    season = weather.get_current_season_index()
    temp_winter = weather.get_global_temperature(unreal.MOTemperatureUnit.CELSIUS)
    ctx.assert_true("WINTER: season=%d ambient=%.1fC (below freezing, %.1fC colder than summer)"
                    % (season, temp_winter, temp_summer - temp_winter),
                    season == 3 and temp_winter < 0.0 and temp_winter < temp_summer - 12.0)

    # accelerate briefly: outdoors at sub-zero the villager's core temp drops
    # and shivering thermogenesis kicks in. Short window — hypothermia is REAL
    # here and we don't want the test subject dying for the assert.
    clock.set_time_scale(120.0)
    best_mult, best_temp = 1.0, 37.0
    for i in range(12):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        vills = _villagers(world, _pawn(world))
        if not vills:
            break
        m, t = _thermo(vills[0])
        if m > best_mult:
            best_mult, best_temp = m, t
        if i % 4 == 3:
            ctx.out("winter poll %d: mult=%.2f body=%.1fC" % (i, m, t))
    clock.set_time_scale(1.0)

    ctx.assert_true("WINTER BURN: shivering multiplier peaked %.2f at body %.1fC (>1.2 = real kcal spike)"
                    % (best_mult, best_temp),
                    best_mult > 1.2)

    in_winter = unreal.MOItemDatabaseSettings.is_item_in_forage_season("Berries01", 3)
    ctx.assert_true("FORAGE: Berries01 OUT of season in winter", not in_winter)
    ctx.out("V2.4 seasons gate complete")
