"""The main-menu backdrop (LoadingLevel): the camera flies far above where the PCG hierarchy generates, so the coast read as bare materials. Two data changes, saved with the level:

  * the level's PCG component generates much farther (every bounded radius x RADIUS_SCALE = 4.5 after two rounds of "farther" through the component's own override: the game level keeps the engine defaults),
  * the flyby keeps a lower clearance (1500 cm instead of 7000: close enough that the generated layers fill the frame).

    python Tools/ue.py py --file Content/Python/menu_backdrop_range.py        (editor open, not in PIE, no `inst` running)
"""
import unreal

RADIUS_SCALE = 4.5
CLEARANCE_CM = 1500.0
FIELDS = ("radius400", "radius800", "radius1600", "radius3200", "radius6400", "radius12800", "radius25600", "radius51200", "radius102400", "radius204800")   # radius_unbounded stays

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Penumbra/Maps/LoadingLevel")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
done = []
for a in eas.get_all_level_actors():
    cn = a.get_class().get_name()
    if cn == "PCGVolume":
        for c in a.get_components_by_class(unreal.ActorComponent):
            if c.get_class().get_name() != "PCGComponent":
                continue
            r = c.get_editor_property("generation_radii")
            for k in FIELDS:
                f = r.get_editor_property(k)
                base = 2.0 * float(k[len("radius"):])   # the engine default is twice the grid size: absolute, so a second run does not scale again
                f.set_editor_property("default", base * RADIUS_SCALE)
                r.set_editor_property(k, f)
            c.set_editor_property("generation_radii", r)
            c.set_editor_property("override_generation_radii", True)
            done.append("pcg x%.1f" % RADIUS_SCALE)
    elif cn == "MOMenuFlyby":
        a.set_editor_property("clearance_cm", CLEARANCE_CM)
        done.append("flyby clearance %.0f" % CLEARANCE_CM)
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log("MENURANGE %s" % done)
