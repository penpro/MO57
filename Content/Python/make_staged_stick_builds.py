"""The stick buildings go up stick by stick (Wes, Oct 24 2026: "i want to do the same thing with the stick walls as well, which are already just
seperate sticks, so they become visible in order as the build timer fills up so it looks like you're actually building the things").

    python Tools/ue.py py --file Content/Python/make_staged_stick_builds.py      (editor open, PIE NOT running)

For each stick Blueprint, its stick components (the Megascans stick mesh) in build order become AMOBuildableActor::StagedBuildParts, hidden
until the build reaches them (UnbuiltStagedPartStyle Hidden). The order: the main sticks first (the majority direction: a wall's uprights, a
floor's boards, a roof's rafters), along the panel, then the crossing rails bottom up. The engine Cube (collision) and empty components are left
out. Idempotent. Prints STAGED lines.
"""
import unreal

SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
EAL = unreal.EditorAssetLibrary
BPS = ["/MOFramework/BP_BuildableWallBase", "/MOFramework/BP_BuildableWallHalf", "/MOFramework/BP_BuildableFloorBase", "/MOFramework/BP_BuildableRoofBase"]
STICK_MESH = "SM_xflicih_tier_2"
lines = []

for path in BPS:
    bp = unreal.load_asset(path)
    sticks = []
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        o = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(SDS.k2_find_subobject_data_from_handle(h))
        if not isinstance(o, unreal.StaticMeshComponent):
            continue
        m = o.get_editor_property("static_mesh")
        if not m or m.get_name() != STICK_MESH:
            continue
        d = o.get_editor_property("relative_rotation").get_forward_vector()   # the stick runs along its mesh X
        sticks.append({"name": o.get_name().replace("_GEN_VARIABLE", ""), "loc": o.get_editor_property("relative_location"),
                       "dir": unreal.Vector(abs(d.x), abs(d.y), abs(d.z))})
    if not sticks:
        lines.append("%s: no sticks" % path)
        continue
    # The main direction: the one most sticks share (within ~25 deg).
    best = max(sticks, key=lambda s: sum(1 for t in sticks if s["dir"].dot(t["dir"]) > 0.9))
    mains = [s for s in sticks if s["dir"].dot(best["dir"]) > 0.9]
    rails = [s for s in sticks if s not in mains]
    # Along the panel: the horizontal axis the mains spread over most.
    xs = [s["loc"].x for s in mains]
    ys = [s["loc"].y for s in mains]
    along = "y" if (max(ys) - min(ys)) >= (max(xs) - min(xs)) else "x"
    mains.sort(key=lambda s: getattr(s["loc"], along))
    rails.sort(key=lambda s: (s["loc"].z, getattr(s["loc"], along)))
    order = [s["name"] for s in mains + rails]
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("staged_build_parts", [unreal.Name(n) for n in order])
    cdo.set_editor_property("unbuilt_staged_part_style", unreal.MOStagedPartStyle.HIDDEN)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_asset(path, only_if_is_dirty=False)
    check = unreal.get_default_object(unreal.load_asset(path).generated_class()).get_editor_property("staged_build_parts")
    lines.append("%s: %d mains along %s + %d rails -> %d staged: %s" % (path.split("/")[-1], len(mains), along, len(rails), len(check),
                                                                      ", ".join(str(n).replace("SM_xflicih_", "") for n in check)))
out("STAGED " + "\nSTAGED ".join(lines))
