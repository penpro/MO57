"""Persistence gate: a station-targeted survivor job survives a save/reload
with its target actor rehydrated (H39 residual).

    python Tools/ue.py seq Content/Python/test_persist_jobrefs.py --timeout 900

A RefuelStation job holds its station/storage as TWeakObjectPtr (not
serialized) plus a persisted GUID. Before the fix, reload restored the GUID
but left the weak ptr null, so the job stalled silently. This gate:
  (a) enqueues a RefuelStation job (live target valid)
  (b) MO.Save.SaveAs -> MO.Save.LoadFrom (real disk round-trip, in-place)
  (c) asserts the restored job STILL has its type (H39 core) AND its target
      actor resolves to a live actor (H39 residual — null pre-fix)
"""
import unreal


SEED = 8080
SLOT = "JobRefGate01"


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _guid_key(pawn):
    ident = pawn.get_component_by_class(unreal.MOIdentityComponent)
    if not ident:
        return None
    g = ident.get_guid()
    try:
        return g.to_string()
    except Exception:
        return g.export_text()


def _jobqueue(pawn):
    return pawn.get_component_by_class(unreal.MOSurvivorJobQueueComponent)


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="JobRefGate")

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

    # ---- settlement: campfire + basket + one recruited survivor -------------
    _exec(world, "MO.Colony.Found JobRefstead 30000")
    yield 5
    _exec(world, "MO.Colony.PlaceBuilding BuildCampfire 500")
    yield 8
    _exec(world, "MO.Colony.PlaceBuilding BuildBasketContainer01 900")
    yield 8
    fire = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOCraftingStationActor):
        if act.get_recipe_id() == "BuildCampfire":
            fire = act
            break
    if not ctx.guard("campfire placed", fire is not None):
        return
    basket = None
    for act in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MOContainerActor):
        basket = act
        break
    if not ctx.guard("basket placed", basket is not None):
        return
    _exec(world, "MO.Colony.Stock %s Firewood01 12" % basket.get_name())
    yield 5

    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 6
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("villager spawned", len(vills) >= 1):
        return
    v = vills[0]
    _exec(world, "MO.Colony.Recruit %s" % v.get_name())
    yield 5
    vkey = _guid_key(v)

    # ---- enqueue a RefuelStation job directly (deterministic target) --------
    jq = _jobqueue(v)
    if not ctx.guard("survivor has a job queue", jq is not None):
        return
    jq.enqueue_refuel_job(fire, basket, "Firewood01", 5)
    yield 4

    pre_target = jq.get_current_job_target_actor()
    pre_job = jq.get_current_job()
    pre_type = str(pre_job.get_editor_property("job_type"))
    ctx.out("pre-save: job=%s, target=%s" % (
        pre_type, pre_target.get_name() if pre_target else "None"))
    ctx.assert_true("SETUP: refuel job enqueued with a live target",
                    pre_target is not None and "REFUEL_STATION" in pre_type)

    # ---- real disk save/load round-trip -------------------------------------
    _exec(world, "MO.Save.SaveAs %s" % SLOT)
    yield 15
    _exec(world, "MO.Save.LoadFrom %s" % SLOT)

    # The restored job starts executing immediately and (if its target
    # rehydrated) runs to completion in a few seconds. Poll TIGHTLY to catch it
    # mid-flight: seeing the refuel job with a non-null target after the reload
    # is the proof. A null target (pre-fix) would fail the job at validation,
    # never resolving. Completion-with-refuel is accepted as backup evidence
    # (a refuel job can only complete by resolving its station + storage).
    survived = False
    resolved = False
    for _ in range(50):
        yield 3
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            continue
        v2 = None
        for cand in _villagers(world, _pawn(world)):
            if _guid_key(cand) == vkey:
                v2 = cand
                break
        if not v2:
            continue
        jq2 = _jobqueue(v2)
        if not jq2:
            continue
        cj = jq2.get_current_job()
        jt = str(cj.get_editor_property("job_type"))
        if "REFUEL_STATION" in jt:
            survived = True
            tgt = jq2.get_current_job_target_actor()
            if tgt is not None:
                resolved = True
                ctx.out("post-load: restored refuel job, target rehydrated -> %s" % tgt.get_name())
                break

    ctx.assert_true("H39 CORE: the refuel job survived the reload", survived)
    ctx.assert_true("H39 RESIDUAL: the job's target actor rehydrated from its GUID (null pre-fix)",
                    resolved)

    _exec(world, "MO.Save.Delete %s" % SLOT)
    ctx.out("persist job-refs gate complete")
