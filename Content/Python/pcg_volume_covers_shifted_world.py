"""The game level's PCG volume must cover the whole island wherever the island lies (2026-10-26, the world origin).

A new world is generated shifted so its START is the world's (0,0) (FMOWorldGenParams::Origin, AMOGameMode::ChooseNewWorldOrigin), so the island's
centre is anywhere up to WorldHalfSizeCm (60 km) off (0,0) on each axis and its far shore up to 120 km out. Runtime PCG plants only inside the
volume's brush (Docs/PCG.md section 1), which was +-65 km around (0,0): the far side of a shifted island would stay bare. This sets the brush to
+-HALF_EXTENT_CM in X and Y (absolute, so a second run changes nothing) and keeps it centred on (0,0). Runtime generation only plants around the
players, so a larger volume costs nothing. UMOWorldSeedSubsystem logs an Error at world start when a volume does not cover the island.

    python Tools/ue.py py --file Content/Python/pcg_volume_covers_shifted_world.py        (editor open, not in PIE)
"""
import unreal

MAP = "/Game/VoxelExamples/PCGScattering/MOPCGScattering"
HALF_EXTENT_CM = 12500000.0   # 125 km: the island's far shore (60 km of origin + 60 km of half-size) and a margin

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
done = []
for a in eas.get_all_level_actors():
    if a.get_class().get_name() != "PCGVolume":
        continue
    origin, extent = a.get_actor_bounds(False)
    scale = a.get_actor_scale3d()
    before = "centre (%.0f, %.0f) half-extent (%.0f, %.0f) km, scale (%.3f, %.3f, %.3f)" % (
        origin.x / 1e5, origin.y / 1e5, extent.x / 1e5, extent.y / 1e5, scale.x, scale.y, scale.z)
    new_scale = unreal.Vector(scale.x * HALF_EXTENT_CM / extent.x, scale.y * HALF_EXTENT_CM / extent.y, scale.z)
    a.modify()   # without it the level package is not dirty and save_current_level writes nothing (found Oct 26)
    a.set_actor_scale3d(new_scale)
    loc = a.get_actor_location()
    a.set_actor_location(unreal.Vector(loc.x - origin.x, loc.y - origin.y, loc.z), False, False)   # the brush centred on (0,0)
    origin2, extent2 = a.get_actor_bounds(False)
    done.append("%s: %s -> centre (%.1f, %.1f) half-extent (%.1f, %.1f) km" % (
        a.get_name(), before, origin2.x / 1e5, origin2.y / 1e5, extent2.x / 1e5, extent2.y / 1e5))
if not done:
    unreal.log_error("PCGVOLUME no PCGVolume in %s" % MAP)
else:
    unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log_warning("PCGVOLUME %s" % done)
