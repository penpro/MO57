"""P2 gate: biome-driven voxel-surface scatter is real and regional (pipeline P2, #172).

A claude_seq sequence: run via

    python Tools/ue.py seq Content/Python/test_biomes_p2.py --timeout 900

Boots a fresh seed and asserts, from the LIVE world:
  1. MO Biome Spawner produced HISM components carrying MOBiome_<Id> tags.
  2. At least 2 distinct biomes materialized (the seeded mask makes regions).
  3. Density ordering is honest: TemperateForest tree instances outnumber
     Meadow tree instances (authored 190/ha vs 6/ha).

Per-tag counts stream as [seq] lines for the burn-down/tuning record.
"""
import unreal


SEED = 4242


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


def _biome_counts(world):
    """{tag: instance_count} for every MOBiome_-tagged ISM/HISM, plus tree counts.

    Search the ISM BASE class: PCG may create plain ISM components even when
    the descriptor asks for HISM (observed 2026-07-04 — tree/rock comps came
    back as ISM while grass got HISM), and get_components_by_class(HISM) does
    NOT return base-class components. An HISM-only probe is blind to half the
    scatter and cost several false debugging iterations.
    """
    counts = {}
    tree_by_biome = {}
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    for a in actors:
        try:
            comps = a.get_components_by_class(unreal.InstancedStaticMeshComponent)
        except Exception:
            continue
        for c in comps:
            tags = [str(t) for t in c.component_tags]
            biome = next((t for t in tags if t.startswith("MOBiome_")), None)
            if not biome:
                continue
            n = c.get_instance_count()
            counts[biome] = counts.get(biome, 0) + n
            if any(t == "MOResource_Tree" for t in tags):
                tree_by_biome[biome] = tree_by_biome.get(biome, 0) + n
    return counts, tree_by_biome


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="P2Probe")

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

    # PCG scatter generates as the voxel world settles; poll until biome
    # components exist (or timeout honestly).
    counts, trees = {}, {}
    for i in range(36):                       # ~2 min
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        if not world:
            break
        counts, trees = _biome_counts(world)
        if counts and i % 6 == 5:
            ctx.out("poll %d: %s" % (i, counts))
        if sum(counts.values()) > 200 and len(counts) >= 2:
            break

    for tag, n in sorted(counts.items()):
        ctx.out("BIOME %s instances=%d trees=%d" % (tag, n, trees.get(tag, 0)))

    ctx.assert_true("biome-tagged HISM instances exist (total=%d)" % sum(counts.values()),
                    sum(counts.values()) > 0)
    ctx.assert_true("at least 2 biomes materialized (%d)" % len(counts),
                    len(counts) >= 2)
    forest_trees = trees.get("MOBiome_TemperateForest", 0)
    meadow_trees = trees.get("MOBiome_Meadow", 0)
    ctx.assert_true("forest trees (%d) >> meadow trees (%d)" % (forest_trees, meadow_trees),
                    forest_trees > meadow_trees)
    ctx.out("P2 biome scatter gate complete")
