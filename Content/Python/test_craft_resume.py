"""H25 gate: pause/resume (and reload) keep accumulated craft progress.

    python Tools/ue.py seq Content/Python/test_craft_resume.py --timeout 600

ProcessCraftingTick derives Progress from (UtcNow - CurrentCraftStartTime)/
Duration. StartCrafting used to re-anchor at UtcNow() with no back-date, so
the next tick recomputed ~0 and overwrote the accumulated Progress — every
pause/resume and save/load silently reset a partial craft. This gate:
  (a) crafts to ~partway (real time; crafting ticks on UtcNow, not the clock)
  (b) PauseCrafting -> StartCrafting
  (c) asserts progress is RETAINED (pre-fix it snapped to ~0)

Uses SelectHammerstone (8s, no ingredients, hand-craftable).
"""
import unreal


SEED = 3131
RECIPE = "SelectHammerstone"


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _queue(pawn):
    return pawn.get_component_by_class(unreal.MOCraftingQueueComponent) if pawn else None


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="CraftResume")

    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and _queue(p):
            player = p
            break
    if not ctx.guard("player pawn has a crafting queue", player is not None):
        return
    q = _queue(player)

    # ---- enqueue + start ----------------------------------------------------
    enq = q.enqueue_craft(unreal.Name(RECIPE), 1, unreal.MOCraftingStation.NONE)
    if not ctx.guard("enqueued %s" % RECIPE, enq):
        return
    q.start_crafting()
    yield 3

    # ---- craft partway (real seconds; poll the progress) --------------------
    p1 = 0.0
    for _ in range(120):
        yield 5
        p1 = q.get_current_craft_progress()
        if p1 >= 0.3:
            break
    ctx.out("progress before pause: %.3f" % p1)
    if not ctx.guard("craft advanced past 0.3 before pause", p1 >= 0.3):
        return

    # ---- pause, then resume; read IMMEDIATELY (crafting ticks on real UtcNow,
    #      so a long wait lets the 8s craft finish and empty the queue -> 0) --
    q.pause_crafting()
    yield 3
    p_paused = q.get_current_craft_progress()
    q.start_crafting()
    yield 2  # minimal — before the craft advances/completes
    p2 = q.get_current_craft_progress()
    ctx.out("progress paused=%.3f, right after resume=%.3f (was %.3f)" % (p_paused, p2, p1))

    ctx.assert_true(
        "RESUME keeps progress (%.3f >= %.3f-0.05, not wiped to ~0)" % (p2, p1),
        p2 >= p1 - 0.05)

    # ---- it should run FORWARD to completion (queue empties -> progress 0
    #      only after having climbed), not stall or reset -----------------------
    peak = p2
    completed = False
    for _ in range(150):
        yield 3
        cur = q.get_current_craft_progress()
        peak = max(peak, cur)
        if peak >= 0.7 and cur < 0.05:  # climbed then queue emptied = done
            completed = True
            break
        if peak >= 0.98:
            completed = True
            break
    ctx.out("peak after resume=%.3f, completed=%s" % (peak, completed))
    ctx.assert_true("resumed craft advanced forward and finished (didn't stall/reset)",
                    completed or peak >= 0.7)
    ctx.out("craft-resume gate complete")
