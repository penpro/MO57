"""Repair migrated Megascans / Electric Dreams master materials so they COOK on UE 5.8 with Substrate on (2026-10-12).

The failure (a cook -- and an editor recompile -- abort with):
    Assertion failed: ShaderMapId.ContainsShaderType(ShaderType, kUniqueShaderPermutationId) [MaterialShader.cpp:2567]
    Material shader map <M> is missing expected shader type FAnisotropyPS.
      DIFF [bHasAnisotropyConnected]: Conservative Parameters=false vs Compilation Parameters=true
      (often also WorldPositionOffset / PixelDepthOffset / MayModifyMeshPosition true vs false)

Cause (engine): the master's front material is a Substrate "legacy conversion" node (MaterialExpressionSubstrateShadingModels)
whose "Single Shading Model" pin (its ShadingModel input) is wired to a CONSTANT MaterialExpressionShadingModel node.
UMaterialExpressionSubstrateShadingModels::GatherSubstrateMaterialInfo adds no shading model when that pin is connected, so the
FSubstrateMaterialInfo is "invalid" (IsValid() == ShadingModelField > 0) and UpdatePropertyConnectedMask skips the Substrate
overwrite of the property-connected mask. The "conservative" mask then comes from the pre-conversion root pins (stale: it still
says WPO/PDO connected and anisotropy NOT connected) while the real compile finds the anisotropy pin used -> the shader map was
built without FAnisotropyPS and the engine's consistency check fires.

Repair (content, no engine change): move the constant into the node's own `ShadingModelOverride` (the same single shading model,
set as the property) and disconnect the pin. Gather then adds that shading model, the info is valid, the connected mask is the
Substrate one, and both analyses agree. The compiled result is the same single shading model.

NOT repairable this way: a master whose shading model comes from MaterialAttributes (M_Foliage / MS_Foliage_Material_LATEST: the pin is
fed by a BreakMaterialAttributes, different shading models per instance function). Those stay unreferenced (see biome_palettes.py
HORNBEAM_COOKS).

Run in the editor:  python Tools/ue.py py --file Content/Python/fix_substrate_shading_model_pin.py
Idempotent. Prints a line per master ("FIXED" / "ok" / "SKIP ..."). Save happens here; recompile is NOT done (a still-broken master
would crash the editor) -- verify with a cook, or `recompile_material` on one master in a throwaway editor session.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ROOTS = ["/Game/Custom", "/Game/MSPresets", "/Game/Megascans", "/Game/Fab"]
PIN_NAME = "Single Shading Model"   # GetInputName(20) of UMaterialExpressionSubstrateShadingModels = the ShadingModel input

ar = unreal.AssetRegistryHelpers.get_asset_registry()
fixed = 0
for root in ROOTS:
    for a in ar.get_assets_by_path(root, recursive=True):
        if str(a.asset_class_path.asset_name) != "Material":
            continue
        path = str(a.package_name)
        name = path.split("/")[-1]
        m = unreal.load_asset(path)
        node = MEL.get_material_property_input_node(m, unreal.MaterialProperty.MP_FRONT_MATERIAL)
        if node is None or node.get_class().get_name() != "MaterialExpressionSubstrateShadingModels":
            continue
        ins = MEL.get_inputs_for_material_expression(m, node)
        src = ins[20] if len(ins) > 20 else None
        if src is None:
            unreal.log("SUBSTRATE-PIN ok      %s (shading model pin not connected)" % name)
            continue
        if src.get_class().get_name() != "MaterialExpressionShadingModel":
            unreal.log("SUBSTRATE-PIN SKIP    %s (pin fed by %s, per-instance shading model: not repairable)" % (name, src.get_class().get_name()))
            continue
        model = src.get_editor_property("shading_model")
        node.set_editor_property("shading_model_override", model)
        if not MEL.disconnect_material_expressions(node, PIN_NAME):
            unreal.log("SUBSTRATE-PIN SKIP    %s (disconnect of '%s' failed)" % (name, PIN_NAME))
            continue
        EAL.save_loaded_asset(m, False)
        fixed += 1
        unreal.log("SUBSTRATE-PIN FIXED   %s (single shading model = %s)" % (name, model))
unreal.log("SUBSTRATE-PIN done: %d master(s) repaired" % fixed)
