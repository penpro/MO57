"""Stage 1 gate for terraform excavation (unit 3): the parameterized earth-move
   primitive + conservation / volume->item math.

    python Tools/ue.py seq Content/Python/test_excavation_primitive.py --timeout 600

TerraformAtLocationEx applies a bounded Dig/Raise at an explicit location with an
explicit radius+strength (NOT the shared Config brush) and returns the earth
VOLUME moved (m^3). This gate proves:
  (a) a Dig and a Raise each apply and return a positive moved volume,
  (b) CONSERVATION by construction — equal brushes move equal volume, so digging V
      at A and raising V at B balances the earth,
  (c) the moved volume is the earth the sculpt REALLY moves: an independent integral of the Voxel plugin's rule (Radius/10 x Strength cm
      per column, smooth falloff 0.5) AND the ground measured before/after a dig (centre drop = Radius/10 x Strength). Until Oct 14 2026
      this asserted pi r^2 x Strength x 1 m -- the books, not the terrain -- which was 7-15x the earth that moved,
  (d) the static ComputeMovedVolumeCubicMeters == the instance return (single source),
  (e) SpoilItemsForVolume == floor(volume * itemsPerCubicMeter) (the dig-produce /
      fill-consume mapping), and
  (f) an unsupported mode (Flatten) returns 0 from the Ex primitive (Flatten
      decomposes into paired Dig/Raise at the job layer, not here).
"""
import unreal
import math

SEED = 6210


def _pawn(world, idx=0):
    try:
        return unreal.GameplayStatics.get_player_pawn(world, idx)
    except Exception:
        return None


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
        ctx.atl.start_new_game(world, ctx.out, seed=SEED, survivor_name="ExcavGate")

    player = None
    for _ in range(60):
        yield 10
        world = helper.find_pie_world_by_net_mode("Standalone")
        p = _pawn(world) if world else None
        if p and p.get_component_by_class(unreal.MOTerraformingComponent):
            player = p
            break
    if not ctx.guard("player pawn with terraform component", player is not None):
        return

    comp = player.get_component_by_class(unreal.MOTerraformingComponent)
    # Let the world settle so the shared height sculpt actor is resolved at BeginPlay.
    yield 20

    radius = 250.0
    strength = 0.1
    items_per_m3 = comp.get_editor_property("excavation_items_per_cubic_meter")

    loc = player.get_actor_location()
    loc_a = unreal.Vector(loc.x + 400.0, loc.y, loc.z)               # clear of the pawn's own capsule
    loc_b = unreal.Vector(loc.x + 400.0 + radius * 4.0, loc.y, loc.z)

    def ground(at):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(at.x, at.y, at.z + 2000.0), unreal.Vector(at.x, at.y, at.z - 2000.0),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [player], unreal.DrawDebugTrace.NONE, True)
        return hit.to_tuple()[4].z if hit else None   # ImpactPoint.z

    z_before = ground(loc_a)
    dig_vol = comp.terraform_at_location_ex(loc_a, unreal.MOTerraformMode.DIG, radius, strength)
    yield 2
    raise_vol = comp.terraform_at_location_ex(loc_b, unreal.MOTerraformMode.RAISE, radius, strength)
    yield 60   # the sculpt applies asynchronously; collision rebuilds after it
    z_after = ground(loc_a)

    # Independent re-derivation of the plugin's SculptHeight rule: Delta(r) = R/10 * S * smoothstep falloff (full to R/2), integrated.
    def falloff(d):
        x = 1.0 if d <= radius * 0.5 else max(0.0, 1.0 - (d - radius * 0.5) / (radius * 0.5))
        return x * x * (3 - 2 * x)
    rings = 400
    expected_vol = sum(radius / 10.0 * strength * falloff((i + 0.5) * radius / rings) * math.pi * (((i + 1) * radius / rings) ** 2 - (i * radius / rings) ** 2)
                       for i in range(rings)) / 1.0e6
    static_vol = unreal.MOTerraformingComponent.compute_moved_volume_cubic_meters(radius, strength)
    items_from_dig = unreal.MOTerraformingComponent.spoil_items_for_volume(dig_vol, items_per_m3)
    expected_items = int(math.floor(dig_vol * items_per_m3))

    flat_vol = comp.terraform_at_location_ex(loc_a, unreal.MOTerraformMode.FLATTEN, radius, strength)

    ctx.out("dig_vol=%.4f raise_vol=%.4f expected=%.4f static=%.4f items=%d (per_m3=%.1f) flat=%.4f ground %s -> %s"
            % (dig_vol, raise_vol, expected_vol, static_vol, items_from_dig, items_per_m3, flat_vol, z_before, z_after))

    ctx.assert_true("Dig returns a positive moved volume (primitive applied end-to-end)", dig_vol > 0.0)
    ctx.assert_true("Raise returns a positive moved volume", raise_vol > 0.0)
    ctx.assert_true("CONSERVATION: equal brushes move equal volume (dig == raise)",
                    abs(dig_vol - raise_vol) < 1e-4)
    ctx.assert_true("moved volume matches an independent integral of the plugin's sculpt rule (within 2 %)",
                    abs(dig_vol - expected_vol) < 0.02 * expected_vol)
    drop = (z_before - z_after) if (z_before is not None and z_after is not None) else None
    ctx.assert_true("THE GROUND: the dig really lowered the centre by Radius/10 x Strength = %.1f cm (measured %s)" % (radius / 10.0 * strength, drop),
                    drop is not None and abs(drop - radius / 10.0 * strength) < 0.35 * radius / 10.0 * strength)
    ctx.assert_true("static ComputeMovedVolumeCubicMeters == instance return (single source of truth)",
                    abs(static_vol - dig_vol) < 1e-4)
    ctx.assert_true("SpoilItemsForVolume == floor(volume * itemsPerCubicMeter), and > 0",
                    items_from_dig == expected_items and items_from_dig > 0)
    ctx.assert_true("unsupported mode (Flatten) returns 0 from the Ex primitive", flat_vol == 0.0)

    ctx.out("Stage 1 excavation-primitive gate complete")
