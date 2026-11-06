"""The campfires' flames and smoke (Wes, Nov 5 2026, on the promo's fire: "turn down the smoke effects a LOT, it's fucking massive and a giant wide cone, and make it slower").

    python Tools/ue.py py --file Content/Python/campfire_fire_settings.py      (editor open, NOT in PIE: it saves)

Before: one M5 pack system, NFire_03 at full size, whose smoke emitter is over 10 m wide. The pack's systems have no size or smoke parameter to dial (the _BP variants' User.Smoke did not
turn it off in the test), so the flames and the smoke are now two systems on AMOCraftingStationActor, each with its own scale, the smoke on a slower clock. The numbers came from a row of real
lit campfires in a game (Nov 5): the bonfire loop's thin flame at 0.26 stands about 80 cm over the logs, and the pack's fine smoke at 0.04 is a thin wisp that fades in about 2-3 m.
Idempotent. Output lines start with CAMPFIRE.
"""
import unreal

FLAME = "/Game/M5VFXVOL2/Niagara/Reference/Fireloop/2_Nbonefirethin_pt"
SMOKE = "/Game/M5VFXVOL2/Niagara/Reference/Smoke/0_6_Nsmoke_pt"
FLAME_SCALE = 0.26
SMOKE_SCALE = 0.04
SMOKE_HEIGHT = 35.0     # starts in the flames: its first frames are lit red, which only reads inside a fire
SMOKE_DILATION = 0.6
BLUEPRINTS = ("/MOFramework/BP_Campfire", "/MOFramework/BP_Campfire_StoneRing", "/MOFramework/BP_SurvivorCampfire")

eal = unreal.EditorAssetLibrary

for path in BLUEPRINTS:
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("active_particle_system", unreal.load_asset(FLAME))
    cdo.set_editor_property("active_particle_scale", FLAME_SCALE)
    cdo.set_editor_property("active_smoke_system", unreal.load_asset(SMOKE))
    cdo.set_editor_property("active_smoke_scale", SMOKE_SCALE)
    cdo.set_editor_property("active_smoke_height", SMOKE_HEIGHT)
    cdo.set_editor_property("active_smoke_time_dilation", SMOKE_DILATION)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = eal.save_asset(path, only_if_is_dirty=False)
    cdo = unreal.get_default_object(bp.generated_class())
    out("CAMPFIRE %s: flame %s x%.2f, smoke %s x%.2f at %.0f cm, clock %.2f, saved %s" % (path.rsplit("/", 1)[-1], cdo.get_editor_property("active_particle_system").get_name(), cdo.get_editor_property("active_particle_scale"),
                                                                                         cdo.get_editor_property("active_smoke_system").get_name(), cdo.get_editor_property("active_smoke_scale"), cdo.get_editor_property("active_smoke_height"),
                                                                                         cdo.get_editor_property("active_smoke_time_dilation"), saved))
