"""MO57 promo audio: the game's own sound assets out to WAV, in the editor, for the mix. Run with the editor up and nothing playing:

    python Tools/ue.py py --file Tools/promo/audio/export_game_sounds.py

Writes Saved/Promo/audio/game/oneshots/<asset>.wav for the sound effects the film puts on the picture and Saved/Promo/audio/game/music/<asset>.wav for the game's music. A read-only export: nothing in the project changes.
"""
import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(ROOT, "Saved", "Promo", "audio", "game")
SFX = "/MOFramework/Audio/A001_ForestSounds/Audios/SFX"
ONESHOTS = ["SFX_Footsteps_GrassTread1", "SFX_Footsteps_GrassTread2", "SFX_Footsteps_LeafCrunch1", "SFX_Footsteps_LeafCrunch2", "SFX_Footsteps_StoneStep1", "SFX_Footsteps_StoneStep2",
            "SFX_Footsteps_SandTrail1", "SFX_Footsteps_SandTrail2"]
MUSIC = ["/MOFramework/Audio/Intro01", "/MOFramework/Audio/Cedar_Smoke_Psalm", "/MOFramework/Audio/female_vocalization01", "/MOFramework/Audio/female_vocalization02", "/MOFramework/Audio/flukes_drums01"]


def export(path, dest):
    asset = unreal.load_asset(path)
    if not asset:
        print("EXPORT missing", path)
        return False
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = dest
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.SoundExporterWAV()
    ok = unreal.Exporter.run_asset_export_task(task)
    print("EXPORT", "ok" if ok else "FAILED", path, dest)
    return ok


os.makedirs(os.path.join(OUT, "oneshots"), exist_ok=True)
os.makedirs(os.path.join(OUT, "music"), exist_ok=True)
for name in ONESHOTS:
    export(SFX + "/" + name, os.path.join(OUT, "oneshots", name + ".wav"))
for path in MUSIC:
    export(path, os.path.join(OUT, "music", path.rsplit("/", 1)[1] + ".wav"))
