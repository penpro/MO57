"""Agent test library — in-process game-flow drivers for autonomous testing.

Used through claude_bridge.py:  py:import agent_test_lib as atl; atl.<fn>(world, out)
All functions drive the REAL gameplay code paths (the same functions the UI
buttons call) — no synthetic input, no clicks, no screenshots. Seed-safe:
the new-game flow goes through the normal menu-map -> OpenLevel transition.

Grow this library with every new test need instead of adding click sequences.
"""
import unreal


# ---------------------------------------------------------------- core access

def pc(world):
    return unreal.GameplayStatics.get_player_controller(world, 0)


def pawn(world):
    return unreal.GameplayStatics.get_player_pawn(world, 0)


def gi_subsystem(world, name):
    """GameInstance subsystem instance, e.g. gi_subsystem(world, 'MOQuestSubsystem')."""
    gi = unreal.GameplayStatics.get_game_instance(world)
    for suffix in ("_0", "_1", "_2", ""):
        obj = unreal.find_object(gi, name + suffix)
        if obj:
            return obj
    return None


def world_subsystem(world, name):
    """World subsystem instance, e.g. world_subsystem(world, 'MOCraftingSubsystem')."""
    for suffix in ("_0", "_1", "_2", ""):
        obj = unreal.find_object(world, name + suffix)
        if obj:
            return obj
    return None


def component(world, comp_class):
    """Component on the player pawn, e.g. component(world, unreal.MOCraftingQueueComponent)."""
    p = pawn(world)
    return p.get_component_by_class(comp_class) if p else None


def ui_manager(world):
    p = pc(world)
    return p.get_component_by_class(unreal.MOUIManagerComponent) if p else None


# ---------------------------------------------------------------- boot ladder

def ensure_loading_level(out):
    """Editor-time: make sure LoadingLevel is the open map (the seed-safe boot origin)."""
    lvl = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if lvl.is_in_play_in_editor():
        out("[atl] PIE already running — call end_play first")
        return False
    cur = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if cur and cur.get_name() != "LoadingLevel":
        ok = lvl.load_level("/Game/Penumbra/Maps/LoadingLevel")
        out(f"[atl] loaded LoadingLevel={ok}")
    else:
        out("[atl] already on LoadingLevel")
    return True


def begin_pie(out):
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    out("[atl] begin_play requested")


def end_pie(out):
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    out("[atl] end_play requested")


def skip_intro(world, out):
    """Same function the any-key handler calls on the menu map."""
    p = pc(world)
    if not p:
        out("[atl] skip_intro: no PC")
        return
    p.skip_intro_video()
    out("[atl] SkipIntroVideo called")


def start_new_game(world, out, seed=0, survivor_name=""):
    """Drive the New Game panel's path: seed/name into settings, then the real
    StartNewGame() (normal OpenLevel transition — voxel/PCG seed flow intact)."""
    gus = unreal.GameUserSettings.get_game_user_settings()
    if seed:
        gus.set_editor_property("PendingWorldSeed", seed)
    if survivor_name:
        try:
            gus.set_editor_property("PendingSurvivorName", survivor_name)
        except Exception as e:
            out(f"[atl] PendingSurvivorName: {e}")
    gus.save_settings()
    p = pc(world)
    p.start_new_game()
    out(f"[atl] StartNewGame called (seed={seed or 'panel default'})")


def set_noon(world, out):
    unreal.SystemLibrary.execute_console_command(world, "MO.Clock.SetTime 12 0")
    out("[atl] noon set")


# ---------------------------------------------------------------- test helpers

def give(world, item_id, count, out):
    unreal.SystemLibrary.execute_console_command(world, f"MO.Player.GiveItem {item_id} {count}")
    out(f"[atl] gave {count}x {item_id}")


def quest_status(world, out):
    qs = gi_subsystem(world, "MOQuestSubsystem")
    out("[atl] active=" + str([str(q.get_editor_property("quest_id")) for q in qs.get_active_quests()]))
    out("[atl] completed=" + str([str(q) for q in qs.get_completed_quest_ids()]))
