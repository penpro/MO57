"""The building pieces (Nov 2 2026; Wes: "take the wattle walls we have and add moss dobbing for insulation", "wooden 'framing' where some
branches snap to the edges of the wall panels", "wooden floors that don't have anything in between them and the ground get wooden supports",
"cobblestone foundations and walls", "thatch to the roof wattle framing", "doors", "hardwood framing and cobble and dob", "hide lean to and
tent and wattle and thatch lean to ... remember to have the assets 'pop in' as building goes on").

One table, read by three places:
  Tools/blender_buildings_blender.py   builds each piece's parts (GEOMETRY names the builder) into Saved/BlenderBuildings/<Piece>/
  Content/Python/import_blender_buildings.py   imports them, makes BP_Piece_<Piece> (AMOBuildPieceActor) and the recipe row
  Docs/Building.md section 7           the same pieces in prose

Frames (AMOBuildPieceActor's header): a "wall" piece in the stick wall's frame (its panel centred at X -50, Y +-50, Z +-50), a "roof" in the
stick roof's (eave X 0 Z -50, ridge X -100 Z +50), a "floor" centred with its top at Z +5, a "shelter" standing on its pivot at the ground.

Costs are real quantities for the real size and times are real work at a builder's pace (Principle 11; the building skill doubles the time
and spoils a quarter of the material with none, FMOBuildSkillPenalty). Item weights: StraightBranch01 0.35 kg (a pole), Stick01 0.4 kg (a green
rod), Branch01 1 kg, Log01 15 kg (a 1 m section), Plank01 4 kg, Hardwood01 3 kg, Mud01 4 kg, Clay01 3 kg, DriedGrass01 0.3 kg (a bundle),
ScrapedHide01 1.5 kg, Moss01 0.5 kg (an armful, new), FieldStone01 12 kg (a building stone, new).
"""

# The tiling surfaces every piece is made of (Blender bakes each once, seamless: T_Surf_<Name>_*; MI_Surf_<Name> in the editor). tile_cm: the
# size one repeat of the texture covers on a piece. Each primitive's tint (the mesh's second UV channel: light, weather; Nov 3) moves it
# within light (the base colour's multiplier at tint 0 .. 1; 0.5 is the bake) and toward weather (the colour it goes with age, times its own
# brightness: bark silvers, moss yellows, hide darkens with smoke) by up to weather_amount. M_Surf_Base does it in the game, surf_mat in the
# Blender previews.
SURFACES = {
    "Bark":   dict(tile_cm=60.0, light=(0.72, 1.28), weather=(1.10, 1.08, 1.02), weather_amount=0.75),   # a pole's bark: grey-brown, fine fissures along it, lenticels across
    "Rod":    dict(tile_cm=40.0, light=(0.68, 1.32), weather=(1.00, 0.95, 0.80), weather_amount=0.65),   # hazel / willow rods: smooth red-brown bark, pale lenticels
    "Hewn":   dict(tile_cm=100.0, light=(0.80, 1.20), weather=(1.08, 1.06, 1.02), weather_amount=0.70),  # axe-hewn oak: scalloped facets across, grain along
    "Plank":  dict(tile_cm=100.0, light=(0.78, 1.22), weather=(1.08, 1.06, 1.02), weather_amount=0.70),  # riven oak boards
    "Moss":   dict(tile_cm=50.0, light=(0.70, 1.30), weather=(1.45, 1.15, 0.45), weather_amount=0.70),   # packed moss: green clumps, brown dead stems
    "Daub":   dict(tile_cm=100.0, light=(0.85, 1.15), weather=(1.15, 1.10, 1.02), weather_amount=0.50),  # clay daub with chopped straw, hand-smoothed, hairline cracks
    "Thatch": dict(tile_cm=60.0, light=(0.78, 1.22), weather=(0.95, 0.95, 0.95), weather_amount=0.70),   # dried grass / reed laid down the slope
    "Hide":   dict(tile_cm=120.0, light=(0.75, 1.20), weather=(0.62, 0.52, 0.42), weather_amount=0.60),  # scraped, smoked hide
    "Mortar": dict(tile_cm=80.0, light=(0.85, 1.15), weather=(1.05, 1.05, 1.05), weather_amount=0.40),   # earth mortar, gritty
    "Cord":   dict(tile_cm=10.0, light=(0.80, 1.20), weather=(1.05, 1.02, 0.95), weather_amount=0.50),   # lashing cord / rawhide thong
}

# Building stones from the scans already in the project (ISM parts): the river cobble and the rounded cobble at building-stone size.
STONES = {
    "RiverCobble": "/Game/Fab/Megascans/3D/Beach_Rock_wd5mbhd/Medium/wd5mbhd_tier_2/StaticMeshes/wd5mbhd_tier_2.wd5mbhd_tier_2",
    "Rounded": "/Game/Fab/Megascans/3D/Rounded_Cobble_wffnehvs/Medium/wffnehvs_tier_2/StaticMeshes/wffnehvs_tier_2.wffnehvs_tier_2",
    "BeachRock": "/Game/Fab/Megascans/3D/Beach_Rock_wc2hbfr/Medium/wc2hbfr_tier_2/StaticMeshes/wc2hbfr_tier_2.wc2hbfr_tier_2",
    # Nov 3 (natural variation): seven field stones of the small-stones pack (10-18 cm scans, so near their own size in a wall) and a scatter
    # rock; and the pebble pack's seven for the pinning stones knocked into the joints
    **{"Field%d" % i: "/Game/Fab/Megascans/3D/Small_Stones_Pack_vcljbb1iw/Medium/vcljbb1iw_tier_2/StaticMeshes/vcljbb1iw_LOD0_TIER2_00%d."
                      "vcljbb1iw_LOD0_TIER2_00%d" % (i, i) for i in range(7)},
    "Scatter": "/Game/Fab/Megascans/3D/Military_Trenches_Scatter_Rock_S_03_ydzsbbr/Medium/SM_ydzsbbr_tier_2/StaticMeshes/SM_ydzsbbr_tier_2.SM_ydzsbbr_tier_2",
    **{"Pebble%d" % i: "/Game/Fab/Megascans/3D/Stones_and_Pebbles_Pack_ukhkbediw/Medium/ukhkbediw_tier_2/StaticMeshes/ukhkbediw_LOD0_TIER2_00%d."
                       "ukhkbediw_LOD0_TIER2_00%d" % (i, i) for i in range(7)},
}
WALLING = [k for k in STONES if not k.startswith("Pebble")]   # the stones a course is built of
PINNING = [k for k in STONES if k.startswith("Pebble")]       # the small stones wedged into its joints

WALL_SNAP = dict(center=(-50.0, 0.0, 0.0), half=(5.0, 50.0, 50.0))
ROOF_SNAP = dict(center=(-50.0, 0.0, 0.0), half=(53.5, 50.0, 53.5))
FLOOR_SNAP = dict(center=(0.0, 0.0, 0.0), half=(50.0, 50.0, 5.0))


def part(item, qty, weight=1):
    return {"ItemDefinitionId": item, "Quantity": qty, "Weight": weight}


PIECES = {
    # ---- Wattle and daub (the stick tier) ---------------------------------------------------------------------------------------------------
    "WattleWall": dict(
        frame="wall", geometry="wattle_wall", display="Wattle Wall", recipe="BuildWattleWall",
        description="Five stakes driven in a row and green rods woven between them, in front of one and behind the next, packed down course by "
                    "course. A wattle panel stands by itself and keeps out animals, but the wind blows through it: daub it with moss or clay.",
        parts=[part("StraightBranch01", 5, 2), part("Stick01", 24, 1)], seconds=2400, skill=("Woodworking", 1),
        snap_wall=True, stack_z=50.0, tags=["Wall", "Wattle"], rain_pass=0.6, wind_pass=0.45,
        colliders=[dict(center=(-50.0, 0.0, 0.0), half=(4.0, 50.0, 50.0))], snap=WALL_SNAP),
    "MossDaub": dict(
        frame="wall", geometry="moss_daub", display="Moss Daubing", recipe="DaubWallMoss",
        description="Moss pressed into the weave from both sides and packed hard, the way log and wattle walls were chinked in the north: it "
                    "stops the draught that blows through a bare wattle wall. Goes on a wattle or stick wall.",
        parts=[part("Moss01", 12, 1)], seconds=1500, skill=("Woodworking", 0),
        attach="Face", host_tags=["Wattle"], tags=["Daub"], rain_pass=0.4, wind_pass=0.05,
        colliders=[dict(center=(-50.0, 0.0, 0.0), half=(6.0, 50.0, 50.0))], snap=WALL_SNAP),
    "ClayDaub": dict(
        frame="wall", geometry="clay_daub", display="Clay Daubing", recipe="DaubWallClay",
        description="Wet earth and clay trodden together with chopped grass, thrown onto the weave by the handful and pressed in from both "
                    "sides. Dried, it is a tight wall: no draught, and it does not burn the way bare wattle does. Goes on a wattle or stick wall.",
        parts=[part("Mud01", 10, 2), part("Clay01", 4, 2), part("DriedGrass01", 4, 1)], seconds=2700, skill=("Woodworking", 1),
        attach="Face", host_tags=["Wattle"], tags=["Daub"], rain_pass=0.2, wind_pass=0.0,
        colliders=[dict(center=(-50.0, 0.0, 0.0), half=(7.0, 50.0, 50.0))], snap=WALL_SNAP),

    # ---- Frame members that snap to a panel's edges -----------------------------------------------------------------------------------------
    "PolePost": dict(
        frame="member", geometry="pole_post", display="Pole Post", recipe="BuildPolePost",
        description="A straight pole set upright at the end of a wall panel and lashed to it: the frame that holds a run of wattle straight. "
                    "Snaps to a wall's side.",
        parts=[part("StraightBranch01", 1, 2), part("Cordage01", 2, 1)], seconds=600, skill=("Woodworking", 0),
        attach="Edge", member_axis="Vertical", host_tags=["Wall"], tags=["Frame"],
        colliders=[dict(center=(0.0, 0.0, 0.0), half=(3.5, 3.5, 50.0))], snap=dict(center=(0.0, 0.0, 0.0), half=(3.5, 3.5, 50.0))),
    "PoleRail": dict(
        frame="member", geometry="pole_rail", display="Pole Rail", recipe="BuildPoleRail",
        description="A pole lashed along the top or the foot of a wall panel, or under the edge of a floor: it ties the frame together. Snaps to "
                    "a wall's top or bottom edge, or under a floor's edge.",
        parts=[part("StraightBranch01", 1, 2), part("Cordage01", 2, 1)], seconds=600, skill=("Woodworking", 0),
        attach="Edge", member_axis="Horizontal", host_tags=["Wall", "Floor"], tags=["Frame"],
        colliders=[dict(center=(0.0, 0.0, 0.0), half=(3.0, 50.0, 3.0))], snap=dict(center=(0.0, 0.0, 0.0), half=(3.0, 50.0, 3.0))),
    "TimberPost": dict(
        frame="member", geometry="timber_post", display="Timber Post", recipe="BuildTimberPost",
        description="An oak post hewn square with the axe from a log section, pegged into the frame: the timber frame's upright. Snaps to a "
                    "wall's side.",
        parts=[part("Log01", 1, 3), part("Hardwood01", 1, 1)], seconds=3600, skill=("Woodworking", 3), tools=["Axe"],
        attach="Edge", member_axis="Vertical", host_tags=["Wall"], tags=["Frame", "Timber"],
        colliders=[dict(center=(0.0, 0.0, 0.0), half=(7.0, 7.0, 50.0))], snap=dict(center=(0.0, 0.0, 0.0), half=(7.0, 7.0, 50.0))),
    "TimberBeam": dict(
        frame="member", geometry="timber_beam", display="Timber Beam", recipe="BuildTimberBeam",
        description="A hewn oak beam: a sill under a wall, a plate along its top, a joist under a floor's edge. Snaps to a wall's top or bottom "
                    "edge, or under a floor's edge.",
        parts=[part("Log01", 1, 3), part("Hardwood01", 1, 1)], seconds=3600, skill=("Woodworking", 3), tools=["Axe"],
        attach="Edge", member_axis="Horizontal", host_tags=["Wall", "Floor"], tags=["Frame", "Timber"],
        colliders=[dict(center=(0.0, 0.0, 0.0), half=(7.0, 50.0, 7.0))], snap=dict(center=(0.0, 0.0, 0.0), half=(7.0, 50.0, 7.0))),
    "TimberBrace": dict(
        frame="wall", geometry="timber_brace", display="Timber Brace", recipe="BuildTimberBrace",
        description="A hewn oak brace set corner to corner across a wall panel and pegged at both ends: it keeps the frame from racking. Goes on "
                    "a wall.",
        parts=[part("Hardwood01", 4, 2)], seconds=2400, skill=("Woodworking", 3), tools=["Axe"],
        attach="Face", host_tags=["Wall"], tags=["Frame", "Timber"], rain_pass=1.0, wind_pass=1.0,
        colliders=[], snap=dict(center=(-50.0, 0.0, 0.0), half=(5.0, 50.0, 50.0))),

    # ---- Stone -------------------------------------------------------------------------------------------------------------------------------
    "CobbleFoundation": dict(
        frame="floor", geometry="cobble_foundation", display="Cobble Foundation", recipe="BuildCobbleFoundation",
        description="A footing of field stones laid in earth mortar, 40 cm deep, its top levelled with flat stones and rammed earth: it keeps a "
                    "building's sills off the wet ground. Walls stand on its edges; foundations join side by side.",
        parts=[part("FieldStone01", 55, 3), part("Mud01", 15, 1)], seconds=14400, skill=("Stoneworking", 2),
        tags=["Floor", "Foundation", "Cobble"],
        colliders=[dict(center=(0.0, 0.0, -15.0), half=(50.0, 50.0, 20.0))], snap=dict(center=(0.0, 0.0, -15.0), half=(50.0, 50.0, 20.0))),
    "CobbleWall": dict(
        frame="wall", geometry="cobble_wall", display="Cobble Wall", recipe="BuildCobbleWall",
        description="Field stones laid in courses in earth mortar, the faces chosen flat outward and the heart filled with rubble, 40 cm thick. "
                    "Tight, fireproof and slow: a day's hard work for a metre of it.",
        parts=[part("FieldStone01", 55, 3), part("Mud01", 15, 1)], seconds=14400, skill=("Stoneworking", 2),
        snap_wall=True, stack_z=50.0, tags=["Wall", "Cobble"], rain_pass=0.0, wind_pass=0.0,
        colliders=[dict(center=(-50.0, 0.0, 0.0), half=(20.0, 50.0, 50.0))], snap=dict(center=(-50.0, 0.0, 0.0), half=(20.0, 50.0, 50.0))),
    "CobbleInfill": dict(
        frame="wall", geometry="cobble_infill", display="Cobble and Daub Infill", recipe="BuildCobbleInfill",
        description="A thin panel of small field stones bedded in clay daub, the daub smoothed over the faces: the infill of a timber frame "
                    "(frame it with timber posts and beams).",
        parts=[part("FieldStone01", 18, 3), part("Mud01", 8, 2), part("Clay01", 3, 1), part("DriedGrass01", 3, 1)], seconds=7200,
        skill=("Stoneworking", 2), snap_wall=True, stack_z=50.0, tags=["Wall", "Cobble", "Infill"], rain_pass=0.1, wind_pass=0.0,
        colliders=[dict(center=(-50.0, 0.0, 0.0), half=(7.0, 50.0, 50.0))], snap=dict(center=(-50.0, 0.0, 0.0), half=(7.0, 50.0, 50.0))),

    # ---- Roofs and floors --------------------------------------------------------------------------------------------------------------------
    "Thatch": dict(
        frame="roof", geometry="thatch", display="Grass Thatch", recipe="ThatchRoof",
        description="Bundles of dried grass laid butt-down in courses from the eaves up, each course tied to the battens and covering the "
                    "last, a hand thick: rain runs down the stems and off. Goes on a stick roof.",
        parts=[part("DriedGrass01", 40, 2), part("Stick01", 4, 1), part("Cordage01", 6, 1)], seconds=3600, skill=("Woodworking", 1),
        attach="Face", host_tags=["Roof"], tags=["Thatch"], rain_pass=0.0, wind_pass=0.05,
        colliders=[dict(center=(-36.6, 0.0, 13.4), rot=(-45.0, 0.0, 0.0), half=(75.0, 53.0, 9.0))], snap=ROOF_SNAP),
    "PlankFloor": dict(
        frame="floor", geometry="plank_floor", display="Plank Floor", recipe="BuildPlankFloor",
        description="Riven oak boards pegged across two joists. Where the ground falls away under it, hewn posts carry it down to the ground.",
        parts=[part("Plank01", 5, 2), part("Log01", 1, 2), part("Hardwood01", 1, 1)], seconds=3600, skill=("Woodworking", 2), tools=["Axe"],
        tags=["Floor", "Timber"],
        colliders=[dict(center=(0.0, 0.0, 0.0), half=(50.0, 50.0, 5.0))], snap=FLOOR_SNAP,
        supports=dict(points=[(-43.0, -43.0, -5.0), (43.0, -43.0, -5.0), (-43.0, 43.0, -5.0), (43.0, 43.0, -5.0)],
                      mesh="TimberSupport", item="Log01", cm_per_item=100.0, lashing="Hardwood01", seconds=2400)),

    # ---- Doors -------------------------------------------------------------------------------------------------------------------------------
    "WattleDoor": dict(
        frame="wall2", geometry="wattle_door", display="Wattle Door", recipe="BuildWattleDoor",
        description="A doorway two panels high: two posts, a lintel and a sill, and a wattle hurdle hung on rawhide loops that swings inward. "
                    "Stands in a run of walls like a wall panel; E opens and closes it.",
        parts=[part("StraightBranch01", 8, 2), part("Stick01", 30, 1), part("Branch01", 2, 2), part("ScrapedHide01", 1, 1), part("Cordage01", 4, 1)],
        seconds=4800, skill=("Woodworking", 1), snap_wall=True, stack_z=100.0, tags=["Wall", "Door", "Wattle"], rain_pass=0.6, wind_pass=0.4,
        door=dict(hinge=(-50.0, -40.0, 0.0), open_yaw=-100.0, leaf=["LeafStakes", "LeafWeave"]),
        colliders=[dict(center=(-50.0, -46.0, 50.0), half=(5.0, 4.0, 100.0)), dict(center=(-50.0, 46.0, 50.0), half=(5.0, 4.0, 100.0)),
                   dict(center=(-50.0, 0.0, 144.0), half=(5.0, 50.0, 6.0)),
                   dict(center=(-50.0, 0.0, 44.0), half=(3.0, 38.0, 89.0), door=True)],
        snap=dict(center=(-50.0, 0.0, 50.0), half=(5.0, 50.0, 100.0))),
    "PlankDoor": dict(
        frame="wall2", geometry="plank_door", display="Plank Door", recipe="BuildPlankDoor",
        description="A doorway two panels high in hewn oak -- posts, lintel and sill pegged together -- and a door of riven planks on two "
                    "battens, hung on wooden pintles. E opens and closes it.",
        parts=[part("Log01", 2, 3), part("Plank01", 4, 2), part("Hardwood01", 3, 1)], seconds=10800, skill=("Woodworking", 3), tools=["Axe"],
        snap_wall=True, stack_z=100.0, tags=["Wall", "Door", "Timber"], rain_pass=0.0, wind_pass=0.05,
        door=dict(hinge=(-50.0, -38.0, 0.0), open_yaw=-100.0, leaf=["Leaf", "Battens", "Pegs"]),
        colliders=[dict(center=(-50.0, -44.0, 50.0), half=(7.0, 6.0, 100.0)), dict(center=(-50.0, 44.0, 50.0), half=(7.0, 6.0, 100.0)),
                   dict(center=(-50.0, 0.0, 142.0), half=(7.0, 50.0, 8.0)),
                   dict(center=(-50.0, 0.0, 44.0), half=(2.5, 37.0, 89.0), door=True)],
        snap=dict(center=(-50.0, 0.0, 50.0), half=(7.0, 50.0, 100.0))),

    # ---- Shelters (stand on their own) ------------------------------------------------------------------------------------------------------
    "ThatchLeanTo": dict(
        frame="shelter", geometry="thatch_leanto", display="Lean-To Shelter", recipe="BuildLeanTo",
        description="Two forked uprights and a ridge pole, rafters slanting back to the ground, a lattice of rods across them and grass thatch "
                    "laid in courses from the bottom up: a dry place to sleep out of the wind, open at the front to the fire.",
        parts=[part("Branch01", 2, 2), part("StraightBranch01", 6, 2), part("Stick01", 10, 1), part("DriedGrass01", 50, 2), part("Cordage01", 6, 1)],
        seconds=5400, skill=("Woodworking", 0), tags=["Shelter", "Thatch"], rain_pass=0.0, wind_pass=0.1,
        colliders=[dict(center=(-94.0, 0.0, 81.0), rot=(40.0, 0.0, 0.0), half=(117.0, 118.0, 9.0)),
                   dict(center=(0.0, -105.0, 75.0), half=(5.0, 5.0, 75.0)), dict(center=(0.0, 105.0, 75.0), half=(5.0, 5.0, 75.0))],
        snap=dict(center=(-85.0, 0.0, 75.0), half=(95.0, 115.0, 75.0))),
    "HideLeanTo": dict(
        frame="shelter", geometry="hide_leanto", display="Hide Lean-To", recipe="BuildHideLeanTo",
        description="Forked uprights, a ridge pole and slanting rafters with scraped hides lashed over them: quick to raise where hides are "
                    "plentiful, and they shed rain completely.",
        parts=[part("Branch01", 2, 2), part("StraightBranch01", 6, 2), part("ScrapedHide01", 3, 2), part("Cordage01", 10, 1)],
        seconds=3600, skill=("Leatherworking", 0), tags=["Shelter", "Hide"], rain_pass=0.0, wind_pass=0.05,
        colliders=[dict(center=(-92.0, 0.0, 78.0), rot=(40.0, 0.0, 0.0), half=(117.0, 110.0, 4.0)),
                   dict(center=(0.0, -100.0, 75.0), half=(5.0, 5.0, 75.0)), dict(center=(0.0, 100.0, 75.0), half=(5.0, 5.0, 75.0))],
        snap=dict(center=(-85.0, 0.0, 75.0), half=(95.0, 110.0, 75.0))),
    "HideTent": dict(
        frame="shelter", geometry="hide_tent", display="Hide Tent", recipe="BuildHideTent",
        description="Eight poles tied at the top and spread in a circle three metres across, covered with scraped hides sewn into a cone and "
                    "pegged down at the foot, a smoke hole at the top and a flap for a door: a home that can be taken down and carried.",
        parts=[part("StraightBranch01", 16, 2), part("ScrapedHide01", 8, 3), part("Cordage01", 14, 1), part("Stick01", 12, 1)],
        seconds=21600, skill=("Leatherworking", 1), tags=["Shelter", "Hide"], rain_pass=0.05, wind_pass=0.03,
        colliders="tent", snap=dict(center=(0.0, 0.0, 135.0), half=(160.0, 160.0, 135.0))),
}

# The new materials the pieces need (their rows: import_blender_buildings.ITEM_ROWS).
NEW_ITEMS = ["Moss01", "FieldStone01"]
