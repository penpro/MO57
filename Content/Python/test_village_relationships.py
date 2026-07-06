"""V2.3 gate: relationships grow from REAL shared time; standing gates recruitment.

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_relationships.py --timeout 1200

Flow:
  0  boot standalone (seed 4242)
  1  found settlement; spawn THREE survivors:
       A, B  -> recruited, pinned together (the co-located pair)
       W     -> stays WILD (unrecruited), held near the pair (the stranger)
  2  gate check BEFORE time passes: W cannot be recruited (standing 0)
  3  x480 clock ~26 game hours (phase 1: bonds grow)
  4  asserts:
       (a) BOND: A->B Strength past FriendThreshold, type auto-set Friend
       (b) STANDING: W's colony standing >= threshold, recruit gate now OPEN
  5  teleport B far + pin (phase 2, ~13 game hours apart)
  6  assert (c) DRIFT: A->B Strength strictly below its phase-1 peak
  7  MO.Colony.Marry A B -> assert (d) Spouse both ways, strength >= 0.6
     (marriage is the V2.3 DATA MODEL; courtship sim is V2.5)
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


def _rel(pawn, other):
    """(type, strength) of pawn's bond toward other."""
    hist = pawn.get_component_by_class(unreal.MOCharacterHistoryComponent)
    g = _guid(other)
    if not hist or not g:
        return (None, 0.0)
    r = hist.get_relationship(g)
    return (r.get_editor_property("relationship_type"),
            r.get_editor_property("strength"))


def _pin(pawn, loc):
    ai = pawn.get_controller()
    sc = unreal.MOSurvivorController.cast(ai) if ai else None
    if sc:
        sc.set_stay_at_location(loc)
    return sc is not None


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="BondGate")

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
    for _ in range(3):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("3 villagers spawned (%d)" % len(vills), len(vills) >= 3):
        return
    a, b, w = vills[0], vills[1], vills[2]
    for v in (a, b):
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 3
    # W stays WILD — the stranger whose standing must be EARNED.

    # Hub: pin A and B together; W is wild (its wander AI may not take Stay
    # orders) so we hold it in the circle by nudging it back each poll.
    hub = a.get_actor_location()
    _pin(a, hub)
    _pin(b, unreal.Vector(hub.x + 300.0, hub.y, hub.z))
    w.set_actor_location(unreal.Vector(hub.x, hub.y + 300.0, hub.z), False, True)

    colony = _colony(world)
    if not ctx.guard("colony subsystem up", colony is not None):
        return

    # ---- gate closed at standing 0 ------------------------------------------
    ctx.assert_true("GATE CLOSED: stranger W cannot recruit at standing %.3f"
                    % colony.get_colony_standing(w),
                    not colony.can_recruit_by_standing(w))
    ab0 = _rel(a, b)
    ctx.out("start: A->B=%s W_standing=%.3f" % (str(ab0), colony.get_colony_standing(w)))

    # ---- phase 1: ~26 game hours together ------------------------------------
    _clock(world).set_time_scale(480.0)
    for i in range(100):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        vills = _villagers(world, _pawn(world))
        if len(vills) >= 3:
            a, b, w = vills[0], vills[1], vills[2]
            # hold the wild stranger in the circle (position-only; the PASS
            # measures real co-location, we're just defeating wander AI)
            hub = a.get_actor_location()
            d = w.get_actor_location() - hub
            if (d.x * d.x + d.y * d.y) > 1000.0 * 1000.0:
                w.set_actor_location(unreal.Vector(hub.x, hub.y + 300.0, hub.z), False, True)
        if i % 20 == 19:
            ctx.out("poll %d: A->B=%s W_standing=%.3f" % (
                i, str(_rel(a, b)), _colony(world).get_colony_standing(w)))

    world = helper.find_pie_world_by_net_mode("Standalone")
    colony = _colony(world)
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villagers still present (%d)" % len(vills), len(vills) >= 3):
        return
    a, b, w = vills[0], vills[1], vills[2]

    ab1 = _rel(a, b)
    ctx.assert_true("BOND: A->B grew to %.3f (friend threshold 0.35)" % ab1[1],
                    ab1[1] >= 0.35)
    ctx.assert_true("BOND TYPE: A->B auto-typed %s" % str(ab1[0]),
                    ab1[0] == unreal.RelationshipType.FRIEND)
    w_standing = colony.get_colony_standing(w)
    ctx.assert_true("STANDING: stranger W earned %.3f >= 0.15 and gate is OPEN" % w_standing,
                    w_standing >= 0.15 and colony.can_recruit_by_standing(w))

    # ---- phase 2: separate B ~13 game hours ----------------------------------
    far = unreal.Vector(hub.x + 6000.0, hub.y + 4000.0, hub.z + 500.0)
    b.set_actor_location(far, False, True)
    _pin(b, far)
    for i in range(50):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
    _clock(world).set_time_scale(1.0)

    vills = _villagers(world, _pawn(world))
    a, b, w = vills[0], vills[1], vills[2]
    ab2 = _rel(a, b)
    ctx.assert_true("DRIFT: apart bond faded %.4f -> %.4f" % (ab1[1], ab2[1]),
                    ab2[1] < ab1[1])

    # ---- marriage (data model) ------------------------------------------------
    _exec(world, "MO.Colony.Marry %s %s" % (a.get_name(), b.get_name()))
    yield 5
    ab3 = _rel(a, b)
    ba3 = _rel(b, a)
    ctx.assert_true("MARRIAGE: Spouse both ways (A->B=%s %.2f, B->A=%s %.2f)"
                    % (str(ab3[0]), ab3[1], str(ba3[0]), ba3[1]),
                    ab3[0] == unreal.RelationshipType.SPOUSE
                    and ba3[0] == unreal.RelationshipType.SPOUSE
                    and ab3[1] >= 0.6 and ba3[1] >= 0.6)
    ctx.out("V2.3 relationships gate complete")
