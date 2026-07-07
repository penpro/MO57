"""V2.5 gate: the dynasty — courtship, marriage, a REAL child (pipeline V2.5).

    python Tools/ue.py seq Content/Python/test_village_dynasty.py --timeout 1500

Two villagers, one house, accelerated clocks (courtship/gestation configs
shortened via EditAnywhere properties — the shipped defaults stay realistic;
this is config-for-test, not sim-skip):
  (a) ROMANCE: mutual bond crosses the threshold -> Romantic both ways
  (b) MARRIAGE: romance holds through the courtship clock -> Spouse
  (c) CONCEPTION: married + co-housed -> pregnancy appears
  (d) BIRTH: gestation completes -> a THIRD villager exists, with
      Parent/Child bonds to both parents
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


def _colony(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOColonyManagerSubsystem)
    return unreal.MOColonyManagerSubsystem.cast(o) if o else None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _guid(pawn):
    ident = pawn.get_component_by_class(unreal.MOIdentityComponent)
    return ident.get_guid() if ident else None


def _guid_key(pawn):
    g = _guid(pawn)
    if not g:
        return None
    try:
        return g.to_string()
    except Exception:
        return g.export_text()


def _rel(pawn, other):
    hist = pawn.get_component_by_class(unreal.MOCharacterHistoryComponent)
    g = _guid(other)
    if not hist or not g:
        return (None, 0.0)
    r = hist.get_relationship(g)
    return (r.get_editor_property("relationship_type"), r.get_editor_property("strength"))


def _pin(pawn, loc):
    ai = pawn.get_controller()
    sc = unreal.MOSurvivorController.cast(ai) if ai else None
    if sc:
        sc.set_stay_at_location(loc)


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="DynastyGate")

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

    # ---- settlement: one house, two villagers, both residents ---------------
    _exec(world, "MO.Colony.Found FirstLanding 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildLeanTo 450")
    yield 5
    for _ in range(2):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("2 villagers spawned (%d)" % len(vills), len(vills) >= 2):
        return
    a, b = vills[0], vills[1]
    for v in (a, b):
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 3
    house = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor):
        if act.get_recipe_id() == "BuildLeanTo":
            house = act
            break
    if not ctx.guard("house placed", house is not None):
        return
    for v in (a, b):
        _exec(world, "MO.Colony.AssignHouse %s %s" % (v.get_name(), house.get_name()))
        yield 2
    hub = house.get_actor_location()
    _pin(a, unreal.Vector(hub.x + 150.0, hub.y, hub.z))
    _pin(b, unreal.Vector(hub.x - 150.0, hub.y, hub.z))

    # ---- accelerate the family clocks (config-for-test; defaults realistic) --
    colony = _colony(world)
    if not ctx.guard("colony subsystem up", colony is not None):
        return
    colony.set_editor_property("marry_after_romantic_game_hours", 2.0)
    colony.set_editor_property("gestation_game_hours", 6.0)
    colony.set_editor_property("conception_chance_per_game_day", 50.0)   # certain per pass
    colony.set_editor_property("romance_mutual_strength_threshold", 0.35)
    ctx.out("family clocks shortened for the gate (romance 0.35, marry 2h, gestation 6h)")

    key_a, key_b = _guid_key(a), _guid_key(b)
    # Wild survivors wander into the MetaHuman count — a BIRTH is a pawn with
    # a NEW guid appearing AFTER a pregnancy was observed, nothing else.
    known_keys = {_guid_key(v) for v in _villagers(world, _pawn(world))}
    known_keys.discard(None)
    child = None
    _clock(world).set_time_scale(480.0)
    stage = {"romantic": False, "married": False, "pregnant": False, "born": False}
    for i in range(260):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        vills = _villagers(world, _pawn(world))
        if len(vills) < 2:
            continue
        a = next((v for v in vills if _guid_key(v) == key_a), vills[0])
        b = next((v for v in vills if _guid_key(v) == key_b), vills[1])
        t_ab, s_ab = _rel(a, b)
        if not stage["romantic"] and t_ab == unreal.RelationshipType.ROMANTIC:
            stage["romantic"] = True
            ctx.out("poll %d: ROMANTIC (strength %.2f)" % (i, s_ab))
        if not stage["married"] and t_ab == unreal.RelationshipType.SPOUSE:
            stage["married"] = True
            ctx.out("poll %d: MARRIED" % i)
        colony = _colony(world)
        if not stage["pregnant"] and colony.get_pregnancy_count() > 0:
            stage["pregnant"] = True
            ctx.out("poll %d: EXPECTING" % i)
        if not stage["pregnant"]:
            # pre-pregnancy strangers join the known set, never count as births
            for v in vills:
                known_keys.add(_guid_key(v))
        elif not stage["born"]:
            newcomers = [v for v in vills if _guid_key(v) not in known_keys]
            if newcomers:
                child = newcomers[0]
                stage["born"] = True
                ctx.out("poll %d: BIRTH — %s" % (i, child.get_name()))
                break
        if i % 25 == 24:
            ctx.out("poll %d: rel=%s strength=%.2f pregnancies=%d villagers=%d"
                    % (i, str(t_ab), s_ab, colony.get_pregnancy_count(), len(vills)))
    _clock(world).set_time_scale(1.0)

    ctx.assert_true("ROMANCE formed", stage["romantic"])
    ctx.assert_true("MARRIAGE followed the courtship clock", stage["married"])
    ctx.assert_true("CONCEPTION while married + co-housed", stage["pregnant"])
    ctx.assert_true("BIRTH: a real third villager exists", stage["born"])

    # parentage: the newborn carries Parent bonds toward both parents.
    if child:
        pa, _sa = _rel(child, a)
        pb, _sb = _rel(child, b)
        ctx.assert_true("child holds Parent bonds to both (%s, %s)" % (str(pa), str(pb)),
                        pa == unreal.RelationshipType.PARENT and pb == unreal.RelationshipType.PARENT)
    ctx.out("V2.5 dynasty gate complete")
