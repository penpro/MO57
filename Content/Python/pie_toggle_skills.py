"""Bisect test: call UIManager toggles directly on the live PIE player controller."""
import traceback
import unreal

out_path = r"C:\Users\penum\AppData\Local\Temp\claude\pie_toggle.txt"
lines = []

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_game_world()
    lines.append(f"game world = {world.get_name() if world else None}")
    if world:
        pcs = unreal.GameplayStatics.get_player_controller(world, 0)
        lines.append(f"PC = {pcs.get_name() if pcs else None} class={pcs.get_class().get_name() if pcs else ''}")
        if pcs:
            comp = pcs.get_component_by_class(unreal.MOUIManagerComponent)
            lines.append(f"UIManager = {comp.get_name() if comp else None}")
            if comp:
                comp.toggle_skills_panel()
                lines.append("called toggle_skills_panel()")
except Exception:
    lines.append("EXCEPTION:")
    lines.append(traceback.format_exc())

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log(f"[pie_toggle] {lines}")
