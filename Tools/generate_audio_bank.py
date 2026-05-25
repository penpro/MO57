"""
Generate DT_MOSound.json from the forest sounds pack + existing music files.

Naming convention used:
  ENV_<Sub>_<Name>  ->  Ambient.<Sub>.<Name>
  SFX_<Sub>_<Name>  ->  SFX.<Sub>.<Name>

State-machine aliases (rows expected by EMOMusicState/EMOAmbientState):
  Ambient.Outdoor.Day   -> ENV_Day_ForestAmbience1
  Ambient.Outdoor.Night -> ENV_Night_ForestAmbience1
  Ambient.Water         -> ENV_Water_RiverFlow1
  Music.MainMenu        -> Intro01

Run:
  python Tools/generate_audio_bank.py
"""

import os
import json

ENV_DIR = r"D:\UEProjects\MO57\Plugins\MOFramework\Content\Audio\A001_ForestSounds\Audios\ENV"
SFX_DIR = r"D:\UEProjects\MO57\Plugins\MOFramework\Content\Audio\A001_ForestSounds\Audios\SFX"
OUTPUT = r"D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\DT_MOSound.json"

UE_PATH_FOREST = "/MOFramework/Audio/A001_ForestSounds/Audios"
UE_PATH_MUSIC_ROOT = "/MOFramework/Audio"


def make_row(name, sound_path, category, description=""):
    return {
        "Name": name,
        "Sound": sound_path,
        "Category": category,
        "VolumeMultiplier": 1.0,
        "PitchMultiplier": 1.0,
        "Description": description,
    }


def file_to_row(filename, subdir_in_pack, target_prefix, category):
    """
    Convert a pack file basename to a JSON row.

    filename:        'ENV_Air_AirDrop1' (no extension)
    subdir_in_pack:  'ENV' or 'SFX'  (for path building)
    target_prefix:   'Ambient' or 'SFX' (for row name)
    category:        UE enum string ('Ambient' or 'SFX')
    """
    parts = filename.split("_")
    # Replace first token with our category, dot-join the rest
    row_name = target_prefix + "." + ".".join(parts[1:])
    sound_path = f"{UE_PATH_FOREST}/{subdir_in_pack}/{filename}.{filename}"
    return make_row(row_name, sound_path, category)


rows = []

# ---------------------------------------------------------------
# Forest pack — ENV files become Ambient.* rows
# ---------------------------------------------------------------
for f in sorted(os.listdir(ENV_DIR)):
    if f.endswith(".uasset"):
        base = os.path.splitext(f)[0]
        rows.append(file_to_row(base, "ENV", "Ambient", "Ambient"))

# ---------------------------------------------------------------
# Forest pack — SFX files become SFX.* rows
# ---------------------------------------------------------------
for f in sorted(os.listdir(SFX_DIR)):
    if f.endswith(".uasset"):
        base = os.path.splitext(f)[0]
        rows.append(file_to_row(base, "SFX", "SFX", "SFX"))

# ---------------------------------------------------------------
# Existing music files (manually mapped — was Music01-04)
# ---------------------------------------------------------------
rows.append(
    make_row(
        "Music.MainMenu",
        f"{UE_PATH_MUSIC_ROOT}/Intro01.Intro01",
        "Music",
        "Default main menu / title track (Intro01)",
    )
)
rows.append(
    make_row(
        "Music.Exploration.Day",
        f"{UE_PATH_MUSIC_ROOT}/female_vocalization01.female_vocalization01",
        "Music",
        "Day exploration track (vocal-driven). Was Music02. Swap if intended for a different state.",
    )
)
rows.append(
    make_row(
        "Music.Exploration.Night",
        f"{UE_PATH_MUSIC_ROOT}/female_vocalization02.female_vocalization02",
        "Music",
        "Night exploration track (vocal-driven). Was Music03.",
    )
)
rows.append(
    make_row(
        "Music.Combat",
        f"{UE_PATH_MUSIC_ROOT}/flukes_drums01.flukes_drums01",
        "Music",
        "Combat track (drum-driven). Was Music04.",
    )
)

# ---------------------------------------------------------------
# State-machine aliases — duplicate rows that point to specific
# pack assets so the auto-play state machine finds them.
#
# Same Sound asset can be referenced by multiple rows. Aliasing
# this way (rather than renaming the canonical row) keeps the
# direct ID (e.g. Ambient.Day.ForestAmbience1) accessible AND
# the state name (Ambient.Outdoor.Day) wired up.
# ---------------------------------------------------------------
rows.append(
    make_row(
        "Ambient.Outdoor.Day",
        f"{UE_PATH_FOREST}/ENV/ENV_Day_ForestAmbience1.ENV_Day_ForestAmbience1",
        "Ambient",
        "Default day ambient (aliased to Day_ForestAmbience1)",
    )
)
rows.append(
    make_row(
        "Ambient.Outdoor.Night",
        f"{UE_PATH_FOREST}/ENV/ENV_Night_ForestAmbience1.ENV_Night_ForestAmbience1",
        "Ambient",
        "Default night ambient (aliased to Night_ForestAmbience1)",
    )
)
rows.append(
    make_row(
        "Ambient.Outdoor.Dusk",
        f"{UE_PATH_FOREST}/ENV/ENV_Day_CicadaSong1.ENV_Day_CicadaSong1",
        "Ambient",
        "Default dusk ambient (aliased to Day_CicadaSong1; swap when better dusk loop available)",
    )
)
rows.append(
    make_row(
        "Ambient.Water",
        f"{UE_PATH_FOREST}/ENV/ENV_Water_RiverFlow1.ENV_Water_RiverFlow1",
        "Ambient",
        "Default water ambient (aliased to Water_RiverFlow1)",
    )
)
# Note: Ambient.Cave / Ambient.Indoor have no good matches in this pack — leave empty so
# SetAmbientState(Cave/Indoor) is a silent no-op (logged Warning). Add when assets exist.

# ---------------------------------------------------------------
# Sort by Name for deterministic output and easy diffing
# ---------------------------------------------------------------
rows.sort(key=lambda r: r["Name"])

# Pretty-print with tab indent to match UE's exporter style
with open(OUTPUT, "w", encoding="utf-8") as f:
    json.dump(rows, f, indent="\t")
    f.write("\n")

print(f"Wrote {len(rows)} rows to {OUTPUT}")
print(f"  Ambient.* rows: {sum(1 for r in rows if r['Name'].startswith('Ambient.'))}")
print(f"  SFX.* rows:     {sum(1 for r in rows if r['Name'].startswith('SFX.'))}")
print(f"  Music.* rows:   {sum(1 for r in rows if r['Name'].startswith('Music.'))}")
