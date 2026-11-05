"""The MO57 promo film as data: one source of truth for the edit, the score, the sound mix, the kit's text and the storyboard table.

    python Tools/promo/film.py table      the storyboard as Markdown (Docs/Promo_Video.md section 4 is this table, pasted)
    python Tools/promo/film.py cards      the kit jobs for every text card and lower third (Tools/promo/render_cards.py runs them)
    python Tools/promo/film.py check      bars add up, every shot on the grid

Time: 72 BPM in 4/4 at 24 fps, so one beat is exactly 20 frames and one bar 80 (3.333 s). A position is a bar number, a float: 10.5 = the middle of bar 10.
To re-cut: change the bars here, then re-run edit.py and score.py; the score's hits and the mix's cues follow the same numbers.

kinds: "cg" = kit render, never passed off as the game; "game" = footage of the game (Tools/promo/capture.py's shot of the same id); "black" = nothing.
"""
import json
import sys
from dataclasses import asdict, dataclass, field

BPM = 72
FPS = 24
FRAMES_PER_BEAT = 20
FRAMES_PER_BAR = 80


def frame(bar):
    return int(round(bar * FRAMES_PER_BAR))


def seconds(bar):
    return bar * FRAMES_PER_BAR / FPS


@dataclass
class Shot:
    id: str
    bar: float            # where it starts in the film
    bars: float           # how long it runs
    kind: str             # cg | game | black
    what: str             # what must be on screen
    camera: str = ""      # the move
    capture: str = ""     # how it is got
    lower: str = ""       # lower third: the place's name (a game shot's second line comes from its capture log: seed, clock, weather)
    sfx: tuple = ()       # sound cues at this shot's start, by name (Tools/promo/audio/mix.py)
    timelapse: bool = False   # shows a small TIME-LAPSE tag: the game's real duration is longer than the shot
    grade: str = "neutral"    # per-shot grade hint: dawn | day | gold | night | storm | cave | neutral


@dataclass
class Card:
    id: str
    bar: float
    bars: float
    text: str
    style: str = "fade"   # fade: over the picture, tracks out; slam: on a beat, hard in; black: on black between shots
    size: float = 0.13    # the type's em against the picture's height
    kind: str = "display"
    tracking: float = 0.10


ACTS = [
    ("The seed", 0, 5, "CG. Drone and silence. The pods come down."),
    ("Wake", 5, 10, "The real game from here on. A thin piano over wind and surf."),
    ("One seed, a whole world", 10, 18, "Pulse enters, builds, breaks open on the floating islands at bar 16."),
    ("Nothing is given", 18, 26, "Drums. The simulation pillar, stated flatly over what it means."),
    ("You will need help", 26, 31, "The theme in full. Survivors, possession, co-op."),
    ("Title", 31, 36, "The hit, the 3D title, the end card."),
]

SHOTS = [
    # ---- The seed (CG) ----
    Shot("S01_void", 0, 2.25, "cg", "A stream of stone pods falling past a planet's dark limb; the sun rises over it; one pod turns through focus",
         "almost still, a slow creep forward", "kit: build_coldopen.py void", sfx=("deep_drone",)),
    Shot("S02_entry", 2.25, 2.0, "cg", "One pod, nose first, the stone burning; plasma streaming back, sparks",
         "rides alongside, buffeted more and more", "kit: build_coldopen.py entry", sfx=("entry_rumble",)),
    Shot("S03_streak", 4.25, 0.75, "cg", "The dusk side of the planet from high up: thin lines falling all over the cloud deck, one passes the lens",
         "fixed, a little shake", "kit: build_coldopen.py streak", sfx=("streak_pass",)),
    # ---- Wake ----
    Shot("B01_black", 5.0, 0.5, "black", "Black", sfx=("impact_ground",)),
    Shot("G01_pod", 5.5, 1.5, "game", "The open stone pod on the shore at first light, its hatch beside it, the sea behind", "slow push in at knee height", "start site of the chosen seed, pawn hidden; dawn",
         sfx=("surf_bed",), grade="dawn"),
    Shot("G02_survivor", 7.0, 1.5, "game", "The survivor stands by the pod, her back to us, looking inland; bare skin and rough ground; hair moving", "slow arc round her at shoulder height", "the pawn idle; photo camera",
         grade="dawn"),
    Shot("G03_feet", 8.5, 1.5, "game", "Low: bare feet walking through long grass towards the first trees", "tracks beside at shin height, grass passing the lens", "the pawn walking by MO.Test.InjectInput; camera follows the pawn",
         sfx=("footsteps_grass",), grade="dawn"),
    # ---- One seed, a whole world ----
    Shot("G04_forest", 10.0, 1.5, "game", "Temperate forest from above at golden hour, rolling and endless", "drone, slow flight forward over the canopy", "photo camera, pawn under the path; PCG primed",
         lower="Temperate forest", grade="gold"),
    Shot("G05_alpine", 11.5, 1.5, "game", "Bare granite crags and snow, cloud shadow crossing", "slow lateral slide", "photo camera", lower="Alpine rock", grade="day"),
    Shot("G06_rainforest", 13.0, 1.0, "game", "Inside the rainforest: layered ferns, broad leaves, shafts of light", "low dolly between trunks", "photo camera at the floor", lower="Rainforest", grade="day"),
    Shot("G07_coast", 14.0, 1.0, "game", "The sea on a rocky strand, waves running in, sea stacks", "slow push out over the water", "photo camera; Oceanology ocean", lower="Nordic strand",
         sfx=("wave_crash",), grade="day"),
    Shot("G08_ravine", 15.0, 1.0, "game", "Down into a ravine: a 50-100 m slot of ledges and crags, light from above", "tips over the edge and sinks", "photo camera; generator v13", lower="Ravine", grade="day"),
    Shot("G09_islands", 16.0, 2.0, "game", "Floating islands torn from the rock, forests on their tops, over the land below", "slow rise and turn to reveal, the score breaks open here", "photo camera; generator v13; the pawn near the first island",
         lower="Floating islands", sfx=("hit_big",), grade="gold"),
    # ---- Nothing is given ----
    Shot("G10_trek", 18.0, 1.0, "game", "One small figure crossing a long hillside at dusk", "long lens, nearly locked off", "pawn walking; photo camera far away", sfx=("drums_in",), grade="gold"),
    Shot("G11_climb", 19.0, 1.0, "game", "The survivor climbing a rock face, hands and feet on the stone", "beside and below her, tilting up with her", "the real climb on a cliff (Tools/check_cliff_climb.py's setup)", grade="day"),
    Shot("G12_swim", 20.0, 0.5, "game", "Swimming a crawl across a lake", "low at the water, tracking", "the real swim", grade="day"),
    Shot("G13_dig", 20.5, 1.0, "game", "A pit opening in the hillside: the voxel ground bitten away dig by dig, earth left beside it", "low and close, locked off", "the real timed dig (time-lapse tag)", timelapse=True, grade="day"),
    Shot("G14_wall", 21.5, 1.0, "game", "A wattle wall going up rod by rod, then its daub", "slow push", "the real building system's stages (time-lapse tag)", timelapse=True, grade="day"),
    Shot("G15_fire", 22.5, 0.5, "game", "The stone-ring campfire catching at dusk", "close, low", "a lit fire (MO.Fire)", sfx=("fire_catch",), grade="gold"),
    Shot("G16_wolf", 23.0, 1.0, "game", "A wolf comes through the trees, head low", "low, in front of it", "the wolf's real hunt", grade="day"),
    Shot("G17_wound", 24.0, 1.0, "game", "The body panel: real wounds by body part, bleeding, what they stop you doing", "the screen itself", "the real medical panel (shot showui)", sfx=("ui_tick",), grade="neutral"),
    Shot("G18_cave", 25.0, 1.0, "game", "A torch going down into a dark cave; the rock lit only by it", "behind her, moving with her", "the real torch and cave", grade="cave"),
    # ---- You will need help ----
    Shot("G19_survivor_meet", 26.0, 1.5, "game", "A stranger at the edge of a clearing, turning towards the camera", "slow arc", "a real survivor near the start", sfx=("theme_in",), grade="gold"),
    Shot("G20_fire_group", 27.5, 1.5, "game", "Night. Several survivors round a fire; firelight on faces", "slow lateral, the fire in front", "real recruited survivors, night clock", sfx=("fire_bed",), grade="night"),
    Shot("G21_possess", 29.0, 1.0, "game", "The possession menu: pick another body, and you are them", "the screen, then behind the new body", "the real menu (shot showui), MO.Possess", sfx=("ui_tick",), grade="neutral"),
    Shot("G22_coop", 30.0, 1.0, "game", "Two players on a ridge, two real game processes in one world", "wide, slow push", "a real LAN co-op session (ue.py inst)", grade="gold"),
    # ---- Title ----
    Shot("S04_title", 31.0, 3.0, "cg", "METAVERSE, ORIGINS in stone, the sun rising behind it", "slow push through drifting dust", "kit: build_title.py", sfx=("hit_title",)),
    Shot("S05_end", 34.0, 2.0, "cg", "End card: the wordmark, status line, honesty line", "still, slow fade", "kit: build_endcard.py"),
]

CARDS = [
    Card("T01_trillions", 0.5, 1.5, "THEY SENT TRILLIONS."),
    Card("T02_who", 2.5, 1.5, "NOBODY REMEMBERS WHO."),
    Card("T03_nothing", 5.75, 1.25, "YOU WAKE WITH NOTHING."),
    Card("T04_hands", 8.75, 1.25, "ONLY WHAT YOUR HANDS REMEMBER.", size=0.105),
    Card("T05_seed", 10.25, 1.25, "ONE SEED."),
    Card("T06_world", 12.0, 1.0, "A WHOLE WORLD."),
    Card("T07_fast", 18.0, 1.0, "NO FAST TRAVEL.", style="slam", size=0.15),
    Card("T08_instant", 20.5, 1.0, "NO INSTANT ANYTHING.", style="slam", size=0.14),
    Card("T09_regen", 23.0, 1.0, "NO HEALTH REGEN.", style="slam", size=0.15),
    Card("T10_help", 25.0, 1.0, "YOU WILL NEED HELP.", style="slam", size=0.14),
    Card("T11_find", 26.25, 1.0, "FIND SURVIVORS."),
    Card("T12_any", 29.0, 1.0, "PLAY ANY OF THEM."),
    Card("T13_friends", 30.0, 1.0, "OR BRING YOUR FRIENDS."),
]


def check():
    last = 0.0
    bad = 0
    for s in SHOTS:
        if abs(s.bar - last) > 1e-6:
            print("GAP or overlap before", s.id, "at bar", s.bar, "(previous ended", last, ")")
            bad += 1
        last = s.bar + s.bars
        if abs(frame(s.bar) - s.bar * FRAMES_PER_BAR) > 1e-6:
            print("off the frame grid:", s.id)
            bad += 1
    print("film: %d shots, %d cards, %.2f s, %d frames" % (len(SHOTS), len(CARDS), seconds(last), frame(last)))
    return bad == 0


def table():
    out = ["| # | Time | Dur | Shot | On screen | Camera | How it is got | Text |", "|---|---|---|---|---|---|---|---|"]
    for i, s in enumerate(SHOTS, 1):
        texts = [c.text for c in CARDS if s.bar <= c.bar < s.bar + s.bars]
        if s.lower:
            texts.append("*" + s.lower + "*")
        if s.timelapse:
            texts.append("*TIME-LAPSE*")
        t = seconds(s.bar)
        out.append("| %d | %d:%05.2f | %.2f | %s (%s) | %s | %s | %s | %s |" % (i, t // 60, t % 60, seconds(s.bars), s.id, s.kind, s.what, s.camera, s.capture, " ".join(texts)))
    return "\n".join(out)


def dump():
    return {"bpm": BPM, "fps": FPS, "shots": [asdict(s) for s in SHOTS], "cards": [asdict(c) for c in CARDS], "acts": ACTS}


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "check"
    if what == "table":
        print(table())
    elif what == "json":
        print(json.dumps(dump(), indent=1))
    else:
        sys.exit(0 if check() else 1)
