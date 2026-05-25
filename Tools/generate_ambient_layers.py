"""
Generate DT_AmbientLayers.json — semantic event groups version.

Layered ambient design (per game audio best practice):

  BASE LAYERS (continuous, simultaneous loops): "the roar of the place"
    Always-on bed sounds — wind, general forest ambience, insect drone.

  EVENT GROUPS (semantic categories with cooldowns + dominance):
    Each group represents a CATEGORY of event sounds (Birds, Insects,
    Wildlife, Wood/Plant, Wind-gusts). Playing one member of a group
    cools down the entire group — so "BirdsChirping1" prevents
    "BirdsChirping2" from firing for the cooldown period. Forces variety.

    One group is "dominant" at a time, picked randomly every 60-120s.
    Dominant group has heavy weight; others have low background weight.
    Creates phases — birds dominate for a while, then insects, then wildlife.

  Per-fire jitter:
    Volume: 25-100% of nominal (75% swing)
    Pitch:  ±15% (0.85-1.15)
    Same asset firing twice sounds like nearby vs distant.

Output goes to Plugins/MOFramework/Content/Data/DT_AmbientLayers.json
"""

import json

OUTPUT = r"D:\UEProjects\MO57\Plugins\MOFramework\Content\Data\DT_AmbientLayers.json"


def aid(short_name):
    return f"Ambient.{short_name}"


def group(name, audio_ids, cooldown=25.0, dominant_weight=5.0, background_weight=1.0):
    return {
        "GroupName": name,
        "AudioIds": audio_ids,
        "CooldownSeconds": cooldown,
        "DominantWeight": dominant_weight,
        "BackgroundWeight": background_weight,
    }


def base_layer(short_name, volume=1.0, delay_min=0.0, delay_max=0.0):
    """
    A single base layer slot.

    Volume hierarchy (multiplied together):
      FMOAmbientLayerConfig::BaseLayerVolume  (global to the state)
        * volume                              (per-layer, this arg, default 1.0)
        * FMOAudioBankRow::VolumeMultiplier   (per-asset, in DT_MOSound)

    Two playback modes:
      - delay_min == delay_max == 0:   continuous loop (SoundWave bLooping).
                                       Use for long bed sounds (forest bed,
                                       cricket drone, steady wind, river flow).
      - delay_max > 0:                 play once, wait RandRange(min,max), repeat.
                                       Use for short loops that would sound
                                       repetitive back-to-back (breeze swells,
                                       light rustles, occasional gusts).
    """
    return {
        "AudioId": aid(short_name),
        "Volume": float(volume),
        "DelayBetweenLoopsMin": float(delay_min),
        "DelayBetweenLoopsMax": float(delay_max),
    }


# =============================================================================
# OUTDOOR_DAY
# =============================================================================
# Base layers: 4 simultaneous loops creating the daytime "bed"
# Event groups: Birds, Insects, Wildlife, Plants, Wind
outdoor_day = {
    "BaseLayers": [
        # ForestAmbience1 is empty/broken — use the 2 variant. Continuous.
        # 2.0x volume so it sits prominent in the day bed.
        base_layer("Day.ForestAmbience2", volume=2.0),
        base_layer("Day.InsectsBuzz1"),
        base_layer("Day.DappledWind1"),
        # GentleBreeze2 is a short loop — give it 5-10s gaps between iterations
        # so it doesn't sound like a tape on repeat.
        base_layer("Air.GentleBreeze2", delay_min=5.0, delay_max=10.0),
    ],
    "BaseLayerVolume": 1.0,
    "EventGroups": [
        group("Birds",
              [aid("Day.BirdsChirping1"), aid("Day.BirdsChirping2"),
               aid("Animal.BirdSongVaried1"), aid("Animal.BirdSongVaried2"),
               aid("Day.WoodpeckerTap1"), aid("Day.WoodpeckerTap2")],
              cooldown=35.0, dominant_weight=6.0, background_weight=1.0),
        group("Insects",
              [aid("Animal.BeeHiveBuzz1"), aid("Animal.BeeHiveBuzz2"),
               aid("Day.ButterflyFlutter1"), aid("Day.ButterflyFlutter2"),
               aid("Animal.ButterflyLand1"), aid("Animal.ButterflyLand2")],
              cooldown=30.0, dominant_weight=5.0, background_weight=1.2),
        group("Wildlife",
              [aid("Day.SquirrelScurry1"), aid("Day.SquirrelScurry2"),
               aid("Animal.DeerHoofDrum1"), aid("Animal.DeerHoofDrum2"),
               aid("Animal.DeerRustle1"), aid("Animal.DeerRustle2"),
               aid("Animal.HedgehogSnuffle1"), aid("Animal.HedgehogSnuffle2"),
               aid("Animal.FoxCall1"), aid("Animal.FoxCall2")],
              cooldown=45.0, dominant_weight=4.0, background_weight=0.8),
        group("Plants",
              [aid("Day.LeafRustle1"), aid("Day.LeafRustle2"),
               aid("Day.MossyGroundShift1"), aid("Day.MossyGroundShift2"),
               aid("Day.SunbeamRustle1"), aid("Day.SunbeamRustle2"),
               aid("Day.MushroomSporeWind1"), aid("Day.MushroomSporeWind2")],
              cooldown=20.0, dominant_weight=4.0, background_weight=1.5),
        group("Wind",
              [aid("Air.SwirlingLeaves1"), aid("Air.SwirlingLeaves2"),
               aid("Air.TreeSway1"), aid("Air.TreeSway2"),
               aid("Air.HighCanopyWind1"), aid("Air.HighCanopyWind2"),
               aid("Day.TreeCreak1"), aid("Day.TreeCreak2")],
              cooldown=25.0, dominant_weight=4.0, background_weight=1.2),
    ],
    "EventIntervalMin": 5.0,
    "EventIntervalMax": 18.0,
    "EventVolume": 0.85,
    "VolumeJitterMin": 0.25,
    "VolumeJitterMax": 1.0,
    "PitchJitterMin": 0.85,
    "PitchJitterMax": 1.15,
    "DominanceShiftIntervalMin": 60.0,
    "DominanceShiftIntervalMax": 120.0,
}

# =============================================================================
# OUTDOOR_NIGHT — quieter, eerier, sparser activity
# =============================================================================
outdoor_night = {
    "BaseLayers": [
        base_layer("Night.ForestAmbience1"),
        base_layer("Night.CricketChirp1"),
        base_layer("Night.ForestBreeze1"),
        base_layer("Night.MoonlitAmbience1"),
    ],
    "BaseLayerVolume": 0.9,
    "EventGroups": [
        group("Owls",
              [aid("Night.OwlHoot1"), aid("Night.OwlHoot2"),
               aid("Night.OwlWings1"), aid("Night.OwlWings2")],
              cooldown=50.0, dominant_weight=6.0, background_weight=1.0),
        group("Frogs",
              [aid("Night.FrogsCroak1"), aid("Night.FrogsCroak2")],
              cooldown=30.0, dominant_weight=4.0, background_weight=1.5),
        group("Insects",
              [aid("Night.CicadaChorus1"), aid("Night.CicadaChorus2"),
               aid("Night.FireflyWhisper1"), aid("Night.FireflyWhisper2")],
              cooldown=25.0, dominant_weight=5.0, background_weight=1.2),
        group("Wildlife",
              [aid("Night.DistantWolfHowl1"), aid("Night.DistantWolfHowl2"),
               aid("Night.BatEchoes1"), aid("Night.BatEchoes2"),
               aid("Animal.BatFlutter1"), aid("Animal.BatFlutter2"),
               aid("Animal.FoxCall1"), aid("Animal.FoxCall2")],
              cooldown=60.0, dominant_weight=4.0, background_weight=0.6),
        group("Plants",
              [aid("Night.PineNeedleRustle1"), aid("Night.PineNeedleRustle2"),
               aid("Night.GlowMushroomDrip1"), aid("Night.GlowMushroomDrip2"),
               aid("Night.MushroomGlowAmbience1"), aid("Night.MushroomGlowAmbience2"),
               aid("Night.TwigCrack1"), aid("Night.TwigCrack2")],
              cooldown=20.0, dominant_weight=3.0, background_weight=1.0),
    ],
    "EventIntervalMin": 7.0,  # sparser at night
    "EventIntervalMax": 25.0,
    "EventVolume": 0.75,  # quieter at night
    "VolumeJitterMin": 0.2,
    "VolumeJitterMax": 1.0,
    "PitchJitterMin": 0.85,
    "PitchJitterMax": 1.15,
    "DominanceShiftIntervalMin": 60.0,
    "DominanceShiftIntervalMax": 150.0,
}

# =============================================================================
# OUTDOOR_DUSK — transition
# =============================================================================
outdoor_dusk = {
    "BaseLayers": [
        base_layer("Day.CicadaSong1"),
        base_layer("Day.DappledWind1"),
        base_layer("Air.ForestBreath1"),
    ],
    "BaseLayerVolume": 0.9,
    "EventGroups": [
        group("Birds",  # last day calls
              [aid("Day.BirdsChirping2"), aid("Animal.BirdSongVaried2")],
              cooldown=40.0, dominant_weight=4.0, background_weight=1.0),
        group("EmergingNight",  # night creatures coming online
              [aid("Night.FrogsCroak1"), aid("Night.OwlHoot1"),
               aid("Animal.BatFlutter1"), aid("Animal.BatFlutter2")],
              cooldown=35.0, dominant_weight=5.0, background_weight=1.5),
        group("Wind",
              [aid("Air.SwirlingLeaves1"), aid("Air.GentleBreeze2"),
               aid("Day.LeafRustle2")],
              cooldown=20.0, dominant_weight=3.0, background_weight=1.2),
        group("Wildlife",
              [aid("Animal.FoxCall1"), aid("Animal.FoxCall2"),
               aid("Day.SquirrelScurry2")],
              cooldown=50.0, dominant_weight=3.0, background_weight=0.7),
    ],
    "EventIntervalMin": 6.0,
    "EventIntervalMax": 20.0,
    "EventVolume": 0.8,
    "VolumeJitterMin": 0.25,
    "VolumeJitterMax": 1.0,
    "PitchJitterMin": 0.85,
    "PitchJitterMax": 1.15,
    "DominanceShiftIntervalMin": 45.0,
    "DominanceShiftIntervalMax": 100.0,
}

# =============================================================================
# WATER — proximity-based
# =============================================================================
water = {
    "BaseLayers": [
        base_layer("Water.RiverFlow1"),
        base_layer("Water.StreamTrickle1"),
        base_layer("Water.RiverbankWind1"),
    ],
    "BaseLayerVolume": 0.9,
    "EventGroups": [
        group("Surface",
              [aid("Water.BrookGurgle1"), aid("Water.BrookGurgle2"),
               aid("Water.PondRipple1"), aid("Water.PondRipple2")],
              cooldown=15.0, dominant_weight=5.0, background_weight=2.0),
        group("Drips",
              [aid("Water.WaterDrip1"), aid("Water.WaterDrip2"),
               aid("Water.HollowDrip1"), aid("Water.HollowDrip2")],
              cooldown=18.0, dominant_weight=4.0, background_weight=1.5),
        group("Falls",
              [aid("Water.LargeWaterfall1"), aid("Water.LargeWaterfall2"),
               aid("Water.SmallWaterfall1"), aid("Water.SmallWaterfall2"),
               aid("Water.DistantCascade1"), aid("Water.DistantCascade2"),
               aid("Water.FallMistySpray1"), aid("Water.FallMistySpray2")],
              cooldown=40.0, dominant_weight=4.0, background_weight=0.8),
        group("Drift",
              [aid("Water.LeafBoatFloat1"), aid("Water.LeafBoatFloat2"),
               aid("Water.StreamCross1"), aid("Water.StreamCross2")],
              cooldown=30.0, dominant_weight=3.0, background_weight=1.0),
    ],
    "EventIntervalMin": 4.0,
    "EventIntervalMax": 14.0,
    "EventVolume": 0.85,
    "VolumeJitterMin": 0.3,
    "VolumeJitterMax": 1.0,
    "PitchJitterMin": 0.88,
    "PitchJitterMax": 1.12,  # water sounds tolerate less pitch shift
    "DominanceShiftIntervalMin": 60.0,
    "DominanceShiftIntervalMax": 120.0,
}


# =============================================================================
# Build DataTable rows
# =============================================================================

rows = [
    {"Name": "Ambient.Outdoor.Day", **outdoor_day},
    {"Name": "Ambient.Outdoor.Night", **outdoor_night},
    {"Name": "Ambient.Outdoor.Dusk", **outdoor_dusk},
    {"Name": "Ambient.Water", **water},
]

with open(OUTPUT, "w", encoding="utf-8") as f:
    json.dump(rows, f, indent="\t")
    f.write("\n")

print(f"Wrote {len(rows)} layer configs to {OUTPUT}")
for r in rows:
    total_events = sum(len(g["AudioIds"]) for g in r["EventGroups"])
    print(f"  {r['Name']}: {len(r['BaseLayers'])} base / "
          f"{len(r['EventGroups'])} groups / "
          f"{total_events} total event variants")
