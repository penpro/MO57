"""H23 gate: two harvesters harvest concurrently without clobbering each other.

    python Tools/ue.py seq Content/Python/test_harvest_concurrent.py --timeout 900

Before H23 the harvest subsystem held ONE global FMOHarvestContext. When a
second harvester called BeginHarvest it CANCELLED the first (global clobber): a
player mid-harvest would silently lose their harvest the instant a survivor
started gathering. H23 keys contexts by harvester (the inventory owner), so any
number of harvesters run at once.

This gate proves, on real world resource nodes with a real player pawn and a
real spawned survivor:
  (a) BOTH:      BeginHarvest on B leaves A's harvest in progress (the fix)
  (b) CONCURRENT: A and B are simultaneously in progress, each with its own progress
  (c) ISOLATION: cancelling A does NOT cancel B (per-harvester interrupt/cancel)
  (d) COMPLETE:   B still completes successfully after A is gone

The (a)/(c) checks are done in a single synchronous block — consecutive calls
with no `yield` between them, so no AI/movement tick can interleave and the
result is deterministic regardless of what the idle survivor does.
"""
import unreal


SEED = 6210


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _exec(world, cmd):
    unreal.SystemLibrary.execute_console_command(world, cmd)


def _harvest_subsys(world):
    o = unreal.MOEditorTestHelper.get_world_subsystem(world, unreal.MOHarvestSubsystem)
    return unreal.MOHarvestSubsystem.cast(o) if o else None


def _villagers(world, player):
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if a != player and "MetaHuman" in a.get_name():
            out.append(a)
    out.sort(key=lambda p: p.get_name())
    return out


def _action_ids_from_tags(tags):
    """Resource-node ACTION ids are carried on the ISM as 'Action_<Id>' tags
    (BeginHarvest looks these up in the resource-node definition via FindAction).
    This is a DIFFERENT namespace from GetHarvestRecipesForTags, which returns
    crafting-recipe ids and only coincides with actions for some node types."""
    out = []
    for t in tags:
        s = str(t)
        if s.startswith("Action_"):
            out.append(s[len("Action_"):])
    return out


def _find_harvest_node(world, hsub, pawn, ctx=None):
    """Find the first (ISM, instanceIndex, actionId) that BeginHarvest actually
    accepts for `pawn`. Self-validating: we PROBE each candidate with a real
    BeginHarvest (then immediately cancel) so we only return a node+action the
    subsystem will honor — no guessing whether the tag suffix matches the action
    id. Returns (None, None, None) if no loaded chunk has a harvestable ISM."""
    inv = pawn.get_component_by_class(unreal.MOInventoryComponent)
    n_actors = 0
    n_ism_inst = 0
    n_tagged = 0
    n_actiontagged = 0
    sample_tags = None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if not actor:
            continue
        n_actors += 1
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if not comp or comp.get_instance_count() == 0:
                continue
            n_ism_inst += 1
            tags = hsub.collect_target_tags(comp)
            if not tags or len(tags) == 0:
                continue
            n_tagged += 1
            if sample_tags is None:
                sample_tags = [str(t) for t in tags]
            actions = _action_ids_from_tags(tags)
            if not actions:
                continue
            n_actiontagged += 1
            for act in actions:
                name = unreal.Name(act)
                if hsub.begin_harvest(comp, 0, name, inv):
                    hsub.cancel_harvest(pawn)  # undo the probe
                    if ctx:
                        ctx.out("scan HIT (probed): %s action='%s' tags=%s"
                                % (comp.get_name(), act, [str(t) for t in tags]))
                    return comp, 0, name
    if ctx:
        ctx.out("scan MISS: actors=%d ISM+inst=%d tagged=%d actionTagged=%d sampleTags=%s"
                % (n_actors, n_ism_inst, n_tagged, n_actiontagged, sample_tags))
    return None, None, None


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="HarvestGate")

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

    hsub = _harvest_subsys(world)
    if not ctx.guard("harvest subsystem up", hsub is not None):
        return

    # ---- second harvester: spawn a survivor ----------------------------------
    _exec(world, "MO.Colony.SpawnSurvivor 300")
    yield 8
    world = helper.find_pie_world_by_net_mode("Standalone")
    vills = _villagers(world, _pawn(world))
    if not ctx.guard("survivor spawned (second harvester)", len(vills) >= 1):
        return
    survivor = vills[0]
    surv_inv = survivor.get_component_by_class(unreal.MOInventoryComponent)
    if not ctx.guard("survivor has an inventory", surv_inv is not None):
        return

    # ---- find a real harvestable node in the loaded world --------------------
    ism, idx, recipe = _find_harvest_node(world, hsub, player, ctx)
    if not ctx.guard("found a harvestable node in the loaded world", ism is not None):
        return
    ctx.out("harvest node: %s instance %d recipe '%s'" % (ism.get_name(), idx, str(recipe)))

    player_inv = player.get_component_by_class(unreal.MOInventoryComponent)
    surv_skills = survivor.get_component_by_class(unreal.MOSkillsComponent)

    # =========================================================================
    # THE H23 INVARIANT — one synchronous block, no `yield`, so no tick can
    # interleave and cancel a harvest out from under us.
    # =========================================================================
    began_a = hsub.begin_harvest(ism, idx, recipe, player_inv)
    a_in_progress = hsub.is_harvest_in_progress(player)

    # Under the OLD single-global-context code, THIS call cancels A.
    began_b = hsub.begin_harvest(ism, idx, recipe, surv_inv)

    a_still = hsub.is_harvest_in_progress(player)
    b_now = hsub.is_harvest_in_progress(survivor)
    a_prog = hsub.get_harvest_progress(player)
    b_prog = hsub.get_harvest_progress(survivor)

    # Isolation: cancel A, B must survive — still same frame.
    hsub.cancel_harvest(player)
    a_after_cancel = hsub.is_harvest_in_progress(player)
    b_after_cancel = hsub.is_harvest_in_progress(survivor)

    ctx.out("A began=%s inProg=%s | B began=%s | after B: A=%s B=%s (progA=%.3f progB=%.3f) | after cancelA: A=%s B=%s"
            % (began_a, a_in_progress, began_b, a_still, b_now, a_prog, b_prog, a_after_cancel, b_after_cancel))

    ctx.assert_true("A: player BeginHarvest succeeded", began_a)
    ctx.assert_true("A: player harvest is in progress before B starts", a_in_progress)
    ctx.assert_true("B: survivor BeginHarvest succeeded", began_b)
    ctx.assert_true("H23 CORE: player's harvest SURVIVES the survivor's BeginHarvest (was clobbered pre-fix)", a_still)
    ctx.assert_true("H23 CORE: survivor's harvest is ALSO in progress (concurrent)", b_now)
    ctx.assert_true("A and B report independent progress (both >= 0)", a_prog >= 0.0 and b_prog >= 0.0)
    ctx.assert_true("cancelling A leaves the player not-in-progress", not a_after_cancel)
    ctx.assert_true("H23 ISOLATION: cancelling A does NOT cancel B", b_after_cancel)

    # ---- B still completes on its own ----------------------------------------
    ctx.out("surv_skills present=%s" % (surv_skills is not None))
    # UE Python strips the leading 'b' from bool UPROPERTYs: bSuccess -> .success
    res_b = hsub.complete_harvest(surv_inv, surv_skills, True)
    ok_b = bool(res_b is not None and res_b.success)
    ctx.out("complete B: success=%s produced=%s" % (ok_b, str(res_b.produced_items) if res_b else "<none>"))
    ctx.assert_true("H23 COMPLETE: survivor's harvest completes successfully after A is gone", ok_b)

    ctx.out("H23 concurrent-harvest gate complete")
