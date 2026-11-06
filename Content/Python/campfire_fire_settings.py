"""The campfires' flames, smoke and light (Wes, Nov 5 2026, on the promo's fire: "turn down the smoke effects a LOT, it's fucking massive and a giant wide cone, and make it slower").

    python Tools/ue.py py --file Content/Python/campfire_fire_settings.py      (editor open, NOT in PIE: it saves; run Content/Python/make_smoke_wisp.py first)

Before: one M5 pack system, NFire_03 at full size, whose smoke emitter is over 10 m wide, and a 5000 cd light that bleached the logs and the grass at dusk. The pack's systems have no size or smoke parameter to dial (the _BP
variants' User.Smoke did not turn it off in the test), and scaled down their smoke reads as a ball of cotton. So the flames are the bonfire loop's thin flame at 0.26, about 80 cm over the logs, and the smoke is apart from it:
a thin strip (M_MO_SmokeWisp) on AMOCraftingStationActor, whose speed and density are the material's parameters. Numbers from real lit campfires in a game, Nov 5.
Idempotent. Output lines start with CAMPFIRE.
"""
import unreal

FLAME = "/Game/M5VFXVOL2/Niagara/Reference/Fireloop/2_Nbonefirethin_pt"
SMOKE = "/Game/Materials/FX/M_MO_SmokeWisp"
FLAME_SCALE = 0.26
SMOKE_SIZE = unreal.Vector2D(120.0, 380.0)
SMOKE_BASE = 40.0
LIGHT = 1500.0
BLUEPRINTS = ("/MOFramework/BP_Campfire", "/MOFramework/BP_Campfire_StoneRing", "/MOFramework/BP_SurvivorCampfire")

eal = unreal.EditorAssetLibrary

for path in BLUEPRINTS:
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("active_particle_system", unreal.load_asset(FLAME))
    cdo.set_editor_property("active_particle_scale", FLAME_SCALE)
    cdo.set_editor_property("active_smoke_material", unreal.load_asset(SMOKE))
    cdo.set_editor_property("active_smoke_size", SMOKE_SIZE)
    cdo.set_editor_property("active_smoke_base", SMOKE_BASE)
    cdo.set_editor_property("light_base_intensity", LIGHT)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = eal.save_asset(path, only_if_is_dirty=False)
    cdo = unreal.get_default_object(bp.generated_class())
    out("CAMPFIRE %s: flame %s x%.2f, smoke %s %s at %.0f cm, light %.0f, saved %s" % (path.rsplit("/", 1)[-1], cdo.get_editor_property("active_particle_system").get_name(), cdo.get_editor_property("active_particle_scale"),
                                                                                       cdo.get_editor_property("active_smoke_material").get_name(), cdo.get_editor_property("active_smoke_size"), cdo.get_editor_property("active_smoke_base"),
                                                                                       cdo.get_editor_property("light_base_intensity"), saved))
