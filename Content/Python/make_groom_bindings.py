"""A groom binding for every MetaHuman library groom on a survivor face (step 4 of Tools/import_metahuman_grooms.py; Wes, Oct 25 2026: "grab all the
other hair styles from the metahumans pipeline and prepare them for use in the character customization screen").

A groom swapped onto a pawn at run time (UMOAppearanceSubsystem::ApplyGroomsToCharacter) is skinned to the face through a GroomBindingAsset whose
groom is the style and whose target is that face mesh; the subsystem finds it in the asset registry (the binding's Groom tag). Each library groom
was authored on one of the MetaHuman template heads (Tools/metahuman_groom_heads.json, read off the library's own bindings): that head is the
binding's SOURCE, so the engine transfers the groom from the head it was made on onto the survivor's (the MetaHuman Creator fits its grooms per
character the same way). Hair_S_Coil, Hair_M_TwistedBraids and Hair_S_BrushCut were authored on MH_Groom_Head (208 MB, not imported): their
bindings have no source and project straight onto the face.

    python Tools/ue.py py --file Content/Python/make_groom_bindings.py --timeout 1800           (editor open, NOT in PIE: creates and saves)
    GROOMBIND_VERIFY=1 ... the same file                                                         (a second call: the built root counts)

Env: GROOMBIND_FACE (default /Game/MetaHumans/MH_F_1/Face/SKM_MH_F_1_FaceMesh), GROOMBIND_OUT (default <face's character>/Grooms/Bindings).
Idempotent: a binding already right is left alone. GROOMBIND lines.
"""
import json
import os
import unreal

FACE = os.environ.pop("GROOMBIND_FACE", "/Game/MetaHumans/MH_F_1/Face/SKM_MH_F_1_FaceMesh")
OUT = os.environ.pop("GROOMBIND_OUT", FACE.rsplit("/", 2)[0] + "/Grooms/Bindings")
VERIFY = os.environ.pop("GROOMBIND_VERIFY", "") == "1"
LIBRARY = "/Game/MetaHumans/Common/Optional/Grooms"
CATEGORIES = ("Hair", "Beards", "Mustaches")
HEADS = json.load(open(os.path.join(unreal.Paths.project_dir(), "Tools", "metahuman_groom_heads.json"), encoding="utf-8"))

reg = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = lambda m: out("GROOMBIND " + m)

face = unreal.load_asset(FACE)
if face is None:
    raise RuntimeError("no face mesh " + FACE)
grooms = []
for cat in CATEGORIES:
    for ad in reg.get_assets_by_path(LIBRARY + "/GroomAssets/" + cat, recursive=True):
        if str(ad.asset_class_path.asset_name) == "GroomAsset":
            grooms.append((cat, str(ad.asset_name), str(ad.package_name)))
grooms.sort()

made, kept, bad = 0, 0, []
for cat, name, package in grooms:
    path = "%s/%s_Binding" % (OUT, name)
    if VERIFY:
        b = unreal.load_asset(path)
        infos = b.get_editor_property("group_infos") if b else []
        roots = [i.get_editor_property("ren_root_count") for i in infos]
        ok = b is not None and len(roots) > 0 and all(r > 0 for r in roots)
        if not ok:
            bad.append(name)
        log("%s %s: %d group(s), render roots %s" % ("OK " if ok else "BAD", name, len(roots), roots))
        continue
    groom = unreal.load_asset(package)
    head_name = HEADS.get(name)
    head = unreal.load_asset("%s/GroomMesh/%s" % (LIBRARY, head_name)) if head_name else None
    b = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if b and b.get_editor_property("groom") == groom and b.get_editor_property("target_skeletal_mesh") == face and \
            b.get_editor_property("source_skeletal_mesh") == head:
        kept += 1
        continue
    if b is None:
        b = tools.create_asset(name + "_Binding", OUT, unreal.GroomBindingAsset, unreal.GroomBindingFactory())
    b.set_editor_property("source_skeletal_mesh", head)
    b.set_editor_property("target_skeletal_mesh", face)
    b.set_editor_property("groom", groom)
    eal.save_loaded_asset(b, only_if_is_dirty=False)
    made += 1
    log("%s: %s -> %s (source %s)" % (path, name, FACE.split("/")[-1], head_name if head else "none: projected"))
if VERIFY:
    log("verify: %d binding(s), %d bad %s" % (len(grooms), len(bad), bad))
else:
    log("done: %d groom(s), %d binding(s) made or rebuilt, %d already right -> %s" % (len(grooms), made, kept, OUT))
