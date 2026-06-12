"""Compare BP_MOPlayerController's action asset refs vs the IMC's mapped actions + IA configs."""
import traceback
import unreal

out_path = r"C:\Users\penum\AppData\Local\Temp\claude\pc_actions.txt"
lines = []

try:
    pc_class = unreal.load_object(None, "/MOFramework/Characters/BP_MOPlayerController.BP_MOPlayerController_C")
    cdo = unreal.get_default_object(pc_class)
    props = [
        "inventory_action", "skills_action", "back_menu_action", "craft_action",
        "player_status_action", "build_action", "interact_action", "possess_action",
        "move_action", "look_action", "hustle_action", "journal_action",
    ]
    lines.append("--- BP_MOPlayerController CDO action refs ---")
    for p in props:
        try:
            v = cdo.get_editor_property(p)
            lines.append(f"  {p} = {v.get_path_name() if v else 'None'}")
        except Exception as e:
            lines.append(f"  {p}: <no such property>")

    lines.append("--- IA asset configs (/MOFramework/Input) ---")
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in ar.get_assets_by_path("/MOFramework/Input", recursive=True):
        if str(a.asset_class_path.asset_name) != "InputAction":
            continue
        ia = a.get_asset()
        if not ia:
            continue
        trig = ia.get_editor_property("triggers")
        vt = ia.get_editor_property("value_type")
        consume = None
        for cp in ("b_consume_input", "consume_input"):
            try:
                consume = ia.get_editor_property(cp)
                break
            except Exception:
                pass
        lines.append(f"  {a.asset_name}: value={vt} triggers={[type(t).__name__ for t in trig]} consume={consume} path={a.package_name}")
except Exception:
    lines.append("EXCEPTION:")
    lines.append(traceback.format_exc())

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log(f"[dump_pc_actions] wrote {len(lines)} lines")
