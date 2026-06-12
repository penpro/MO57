"""Dump IMC default key mappings (UE5.7 DefaultKeyMappings) + the PC's PawnControlContext."""
import traceback
import unreal

out_path = r"C:\Users\penum\AppData\Local\Temp\claude\imc_dump.txt"
lines = []

try:
    # Which context does the player controller actually use?
    pc_class = unreal.load_object(None, "/MOFramework/Characters/BP_MOPlayerController.BP_MOPlayerController_C")
    if pc_class:
        cdo = unreal.get_default_object(pc_class)
        for prop in ("pawn_control_context", "PawnControlContext"):
            try:
                val = cdo.get_editor_property(prop)
                lines.append(f"BP_MOPlayerController.{prop} = {val}")
                break
            except Exception as e:
                lines.append(f"  ({prop} read failed: {e})")
    else:
        lines.append("BP_MOPlayerController_C failed to load")

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for root in ("/Game/Input", "/MOFramework/Input"):
        for a in ar.get_assets_by_path(root, recursive=True):
            if str(a.asset_class_path.asset_name) != "InputMappingContext":
                continue
            imc = a.get_asset()
            if not imc:
                continue
            lines.append(f"=== {a.package_name} ===")
            try:
                dkm = imc.get_editor_property("default_key_mappings")
                lines.append(f"  DefaultKeyMappings type: {type(dkm).__name__}")
                # Introspect: find iterable mapping data
                handled = False
                for sub in ("mappings", "key_mappings", "action_mappings"):
                    try:
                        arr = dkm.get_editor_property(sub)
                        for m in arr:
                            lines.append(f"  [{sub}] {m}")
                        handled = True
                        break
                    except Exception:
                        continue
                if not handled:
                    attrs = [n for n in dir(dkm) if not n.startswith("_")]
                    lines.append(f"  attrs: {attrs}")
                    lines.append(f"  repr: {dkm}")
            except Exception as e:
                lines.append(f"  default_key_mappings failed: {e}")
except Exception:
    lines.append("EXCEPTION:")
    lines.append(traceback.format_exc())

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log(f"[dump_input_mappings] wrote {len(lines)} lines")
