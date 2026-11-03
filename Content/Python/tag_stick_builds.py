"""The stick buildings as building pieces (Nov 2 2026): what each one is for the pieces that attach to it (PieceTags: moss or clay daub goes on a
"Wattle" wall, thatch on a "Roof"), and what the gaps between their sticks let through (RainPass / WindPass, read by the shelter rays:
Wes, "add moss dobbing for insulation" -- a bare stick wall is draughty until it is daubed, a stick roof leaks until it is thatched). The stick
floor grows posts down to the ground (AMOBuildPieceActor supports: "wooden floors that don't have anything in between them and the ground get
wooden supports"), so it is reparented onto AMOBuildPieceActor and keeps its own sticks.

    python Tools/ue.py py --file Content/Python/tag_stick_builds.py      (editor open, PIE NOT running)

Idempotent. Prints TAGGED lines.
"""
import unreal

EAL = unreal.EditorAssetLibrary
BEL = unreal.BlueprintEditorLibrary
SPECS = {
    "/MOFramework/BP_BuildableWallBase": dict(tags=["Wall", "Wattle"], rain=0.6, wind=0.5),
    "/MOFramework/BP_BuildableWallHalf": dict(tags=["Wall", "Wattle", "Half"], rain=0.6, wind=0.5),
    "/MOFramework/BP_BuildableRoofBase": dict(tags=["Roof"], rain=0.85, wind=0.6),
    "/MOFramework/BP_BuildableRoofPeak45": dict(tags=["RoofPeak"], rain=0.85, wind=0.6),
    "/MOFramework/BP_BuildableFloorBase": dict(tags=["Floor"], rain=0.7, wind=0.6, supports=dict(
        mesh="/Game/BlenderBuildings/Supports/SM_PoleSupport", points=[(-44.0, -44.0, -5.0), (44.0, -44.0, -5.0), (-44.0, 44.0, -5.0), (44.0, 44.0, -5.0)],
        item="StraightBranch01", cm_per_item=150.0, lashing="Cordage01", seconds=900.0)),
}
lines = []
for path, spec in SPECS.items():
    bp = unreal.load_asset(path)
    sup = spec.get("supports")
    if sup and not isinstance(unreal.get_default_object(bp.generated_class()), unreal.MOBuildPieceActor):
        BEL.reparent_blueprint(bp, unreal.MOBuildPieceActor.static_class())
    cdo = unreal.get_default_object(bp.generated_class())
    staged_before = len(cdo.get_editor_property("staged_build_parts"))
    cdo.set_editor_property("piece_tags", spec["tags"])
    cdo.set_editor_property("rain_pass", spec["rain"])
    cdo.set_editor_property("wind_pass", spec["wind"])
    if sup:
        cdo.set_editor_property("grows_supports", True)
        cdo.set_editor_property("support_mesh", unreal.load_asset(sup["mesh"]))
        cdo.set_editor_property("support_points", [unreal.Vector(*p) for p in sup["points"]])
        cdo.set_editor_property("support_item", sup["item"])
        cdo.set_editor_property("support_cm_per_item", sup["cm_per_item"])
        cdo.set_editor_property("support_lashing_item", sup["lashing"])
        cdo.set_editor_property("support_seconds_per_post", sup["seconds"])
    BEL.compile_blueprint(bp)
    EAL.save_asset(path, only_if_is_dirty=False)
    check = unreal.get_default_object(unreal.load_asset(path).generated_class())
    lines.append("%s: tags %s rain %.2f wind %.2f staged %d -> %d%s" % (
        path.split("/")[-1], [str(t) for t in check.get_editor_property("piece_tags")], check.get_editor_property("rain_pass"),
        check.get_editor_property("wind_pass"), staged_before, len(check.get_editor_property("staged_build_parts")),
        ", supports %d points" % len(check.get_editor_property("support_points")) if sup else ""))
out("TAGGED " + "\nTAGGED ".join(lines))
