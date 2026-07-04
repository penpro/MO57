"""P3 gate: regen-on-edit — dig terrain, scatter must update (pipeline P3).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_terraform_p3.py --timeout 900

The destructible-world contract this gate defines:
  1. boot a seed, wait for scatter near the player
  2. pick a REAL scatter instance within terraform reach and dig there
     (ServerApplyTerraform — the true host path: voxel edit +
     RegisterModifiedZone + foliage sweep)
  3. assert the dug zone empties (inner circle -> 0), the wider circle
     thins, and NOTHING FLOATS (remaining nearby instances line-trace to
     ground), and the hole STAYS clear through further PCG regen churn.

Counts use get_instances_overlapping_sphere on the ISM BASE class (PCG may
create plain ISMs — HISM-only probes are blind, see AUTONOMOUS_TOOLING).
"""
import unreal


SEED = 4242
SCATTER_TAGS = ("MOResource_", "MOItem_", "MOBiome_")
INNER_R = 250.0     # dug zone core — must go to zero
OUTER_R = 500.0     # wider circle — must thin
DIG_COUNT = 3


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _scatter_comps(world):
    comps = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        try:
            cs = a.get_components_by_class(unreal.InstancedStaticMeshComponent)
        except Exception:
            continue
        for c in cs:
            tags = [str(t) for t in c.component_tags]
            if any(t.startswith(p) for t in tags for p in SCATTER_TAGS):
                comps.append(c)
    return comps


def _count_near(world, center, radius, sweepable_only=False):
    """Instance count near center. sweepable_only limits to decorative comps
    (terrain-mod AutoSweep tag "grass") — harvestable trees/rocks deliberately
    PERSIST through a dig as interaction targets, so the clear-asserts only
    apply to the sweepable class."""
    n = 0
    for c in _scatter_comps(world):
        if sweepable_only and not any(str(t) == "grass" for t in c.component_tags):
            continue
        try:
            n += len(c.get_instances_overlapping_sphere(center, radius, True))
        except Exception:
            pass
    return n


def _instance_loc(comp, index):
    r = comp.get_instance_transform(index, True)
    xf = r[1] if isinstance(r, tuple) else r
    return xf.translation


def _find_dig_target(world, pawn_loc):
    """Location of a real SWEEPABLE (grass-tagged) instance within reach."""
    best = None
    best_d = 1e12
    for c in _scatter_comps(world):
        if not any(str(t) == "grass" for t in c.component_tags):
            continue
        try:
            idx = c.get_instances_overlapping_sphere(pawn_loc, 2000.0, True)
        except Exception:
            continue
        for i in list(idx)[:8]:
            loc = _instance_loc(c, i)
            d = (loc - pawn_loc).length()
            if 300.0 < d < best_d:      # not directly underfoot
                best, best_d = loc, d
    return best


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="P3Probe")

    pawn = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOInventoryComponent):
            pawn = p
            break
    if not ctx.guard("player pawn possessed", pawn is not None):
        return

    # ---- wait for scatter near the player ---------------------------------
    target = None
    for i in range(36):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        pawn = _pawn(world)
        if not (world and pawn):
            continue
        target = _find_dig_target(world, pawn.get_actor_location())
        if target:
            break
    if not ctx.guard("sweepable grass instance within terraform reach", target is not None):
        return

    before_inner = _count_near(world, target, INNER_R, sweepable_only=True)
    before_outer = _count_near(world, target, OUTER_R, sweepable_only=True)
    ctx.out("dig target %s | before: inner=%d outer=%d" % (target, before_inner, before_outer))
    ctx.assert_true("something to remove at dig site", before_outer >= 1)

    # ---- dig (authoritative public API: voxel edit + zone + sweep) --------
    # TerraformAtLocation is BlueprintCallable (Server RPCs aren't visible to
    # Python) and, as of the P3 fix, registers worked ground itself — before
    # that it sculpted with NO zone, so scatter respawned in the crater.
    tc = pawn.get_component_by_class(unreal.MOTerraformingComponent)
    if not ctx.guard("pawn has terraforming component", tc is not None):
        return
    for _ in range(DIG_COUNT):
        ok_dig = tc.terraform_at_location(target, unreal.MOTerraformMode.DIG)
        ctx.out("dig applied=%s" % ok_dig)
        yield 5

    # ---- the zone must EMPTY and stay empty -------------------------------
    after_inner = -1
    after_outer = -1
    for i in range(24):                       # ~80s: sweep burst + regen churn
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        after_inner = _count_near(world, target, INNER_R, sweepable_only=True)
        after_outer = _count_near(world, target, OUTER_R, sweepable_only=True)
        if i % 6 == 5:
            ctx.out("poll %d: inner=%d outer=%d" % (i, after_inner, after_outer))
        if after_inner == 0 and i >= 6:       # hold for a few regen cycles
            break

    ctx.assert_true("dug core cleared of decorative cover (inner %d -> %d)" % (before_inner, after_inner),
                    after_inner == 0)
    ctx.assert_true("dug area thinned or held (outer %d -> %d)" % (before_outer, after_outer),
                    after_outer <= before_outer)

    # ---- regen respect: hole STAYS clear through further churn ------------
    for _ in range(9):                        # ~30s more of PCG re-executions
        yield 10
    world = helper.find_pie_world_by_net_mode("Standalone")
    persist_inner = _count_near(world, target, INNER_R, sweepable_only=True) if world else -1
    ctx.assert_true("hole stays clear after regen churn (inner=%d)" % persist_inner,
                    persist_inner == 0)

    # ---- nothing floats: survivors near the crater trace to ground --------
    floaters = 0
    checked = 0
    if world:
        for c in _scatter_comps(world):
            try:
                idx = c.get_instances_overlapping_sphere(target, OUTER_R * 1.6, True)
            except Exception:
                continue
            for i in list(idx)[:10]:
                loc = _instance_loc(c, i)
                hit = unreal.SystemLibrary.line_trace_single(
                    world, loc + unreal.Vector(0, 0, 100), loc - unreal.Vector(0, 0, 600),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                    unreal.DrawDebugTrace.NONE, True)
                checked += 1
                if not hit:
                    floaters += 1
                if checked >= 40:
                    break
            if checked >= 40:
                break
    ctx.assert_true("no floating instances near crater (%d/%d floated)" % (floaters, checked),
                    floaters == 0)
    ctx.out("P3 regen-on-edit gate complete")
