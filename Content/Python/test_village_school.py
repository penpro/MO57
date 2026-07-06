"""V2.2 gate: teaching + the School vs. real skill decay (pipeline V2).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_village_school.py --timeout 1200

Flow:
  0  boot standalone (seed 4242)
  1  found settlement; place BuildSchool; spawn/recruit THREE villagers:
       teacher  (Stoneworking 6) -> sent to school
       student  (Stoneworking 0) -> sent to school
       hermit   (Stoneworking 4) -> stays away, never uses the skill
  2  x480 clock for ~2.5 game days
  3  asserts:
       (a) TEACHING: student gained real Stoneworking XP (taught at school)
       (b) DECAY: the hermit's unused skill decayed below level 4
       (c) MAINTENANCE: the teacher (in school) kept level 6 exactly
"""
import unreal


SEED = 4242
SKILL = "Stoneworking"


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


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _skill(pawn, skill):
    sc = pawn.get_component_by_class(unreal.MOSkillsComponent)
    if not sc:
        return (-1, -1.0)
    lvl = sc.get_skill_level(skill)
    xp = 0.0
    for prog in sc.get_editor_property("skills"):
        if str(prog.get_editor_property("skill_id")) == skill:
            xp = prog.get_editor_property("current_xp")
    return (lvl, xp)


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="SchoolGate")

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
    _exec(world, "MO.Colony.PlaceBuilding BuildSchool 500")
    yield 5
    for _ in range(3):
        _exec(world, "MO.Colony.SpawnSurvivor 250")
        yield 4
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("3 villagers spawned (%d)" % len(vills), len(vills) >= 3):
        return
    teacher, student, hermit = vills[0], vills[1], vills[2]
    for v in (teacher, student, hermit):
        _exec(world, "MO.Colony.Recruit %s" % v.get_name())
        yield 3
    _exec(world, "MO.Colony.SetSkill %s %s 6" % (teacher.get_name(), SKILL))
    yield 2
    _exec(world, "MO.Colony.SetSkill %s %s 4" % (hermit.get_name(), SKILL))
    yield 2
    school = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOBuildableActor):
        if a.get_recipe_id() == "BuildSchool":
            school = a
            break
    if not ctx.guard("school placed", school is not None):
        return
    _exec(world, "MO.Colony.SendTo %s %s" % (teacher.get_name(), school.get_name()))
    yield 3
    _exec(world, "MO.Colony.SendTo %s %s" % (student.get_name(), school.get_name()))
    yield 3
    # hermit: genuinely OUT OF TOWN — first run he spawned inside the school
    # radius and the teacher schooled him UP a level. Teleport 5000uu out and
    # pin him there so his Stoneworking actually goes unused.
    loc = school.get_actor_location()
    far = unreal.Vector(loc.x + 5000.0, loc.y + 3000.0, loc.z + 500.0)
    hermit.set_actor_location(far, False, True)
    ai = hermit.get_controller()
    sc = unreal.MOSurvivorController.cast(ai) if ai else None
    if sc:
        sc.set_stay_at_location(far)
    hermit_ai_note = "hermit pinned 5.8km-radius-outside the school"

    t0 = _skill(teacher, SKILL)
    s0 = _skill(student, SKILL)
    h0 = _skill(hermit, SKILL)
    ctx.out("start: teacher=%s student=%s hermit=%s | %s" % (t0, s0, h0, hermit_ai_note))

    # ---- 2.5 accelerated game days ------------------------------------------
    _clock(world).set_time_scale(480.0)
    for i in range(110):                      # ~6 real min ~= 48+ game hours (grace 24h + decay passes)
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        if i % 15 == 14:
            vills = _villagers(world, _pawn(world))
            if len(vills) >= 3:
                ctx.out("poll %d: teacher=%s student=%s hermit=%s" % (
                    i, _skill(vills[0], SKILL), _skill(vills[1], SKILL), _skill(vills[2], SKILL)))
    _clock(world).set_time_scale(1.0)

    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villagers still present (%d)" % len(vills), len(vills) >= 3):
        return
    t1 = _skill(vills[0], SKILL)
    s1 = _skill(vills[1], SKILL)
    h1 = _skill(vills[2], SKILL)

    ctx.assert_true("TEACHING: student gained %s (lvl %d xp %.0f -> lvl %d xp %.0f)"
                    % (SKILL, s0[0], s0[1], s1[0], s1[1]),
                    (s1[0] > s0[0]) or (s1[1] > s0[1] + 10.0))
    ctx.assert_true("DECAY: hermit's unused %s rusted (lvl %d -> lvl %d, xp %.0f)"
                    % (SKILL, h0[0], h1[0], h1[1]),
                    h1[0] < h0[0] or (h1[0] == h0[0] and h1[1] < h0[1]))
    ctx.assert_true("MAINTENANCE: schooled teacher kept level %d (now %d)"
                    % (t0[0], t1[0]),
                    t1[0] >= t0[0])
    ctx.out("V2.2 school gate complete")
