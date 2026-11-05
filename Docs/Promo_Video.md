# The promo video

> **What this is:** the production book of MO57's promo film: concept, script, shot-by-shot storyboard, how every picture and sound is made, and how to re-use the kit for the next one.
> **Current state (2026-11-05):** first cut delivered to Wes: 2:00, 4K, all game shots captured on generator v13 from the real game, -14.0 LUFS with the game's own sound. Whether it looks and sounds right is Wes's call (section 7 has the list and says where it falls short).
> **Entry points:** `Tools/promo/film.py` (the film as data), `Tools/promo/capture.py` + `Content/Python/promo_shot.py` (footage), `Tools/promo/kit/` (Blender), `Tools/promo/audio/` (score and mix), `Tools/promo/edit.py` (assembly, grade, masters). Everything heavy lands in `Saved/Promo/` (not in git).
> **Related:** `Docs/VISION.md`, `Docs/PROJECT_STATUS.md`, `Docs/Audio.md`, `SteamArt/README.md` (the wordmark and the Field Ledger palette), `Art/References/ImageTo3D/2026-10-18-clone-egg/README.md` (the pod's story).

## 1. The honesty rule

Every gameplay shot is the actual game doing what it does today. Blender and generated material is only for what is plainly not gameplay: the three shots of the pods falling from space, the titles, the type. No AI video, no faked mechanics. A mechanic whose real duration is longer than its shot carries a TIME-LAPSE tag on screen. The end card says what was captured and how. A shot that does not reach the bar is re-shot or cut, not kept.

## 2. Concept

**Two sentences:** A stone pod falls out of the sky and a person wakes up beside it with nothing. Everything after that is earned, in a world nobody has seen.

The lore comes from the pod's own brief: an ancient civilisation sent trillions of rocklike pods across the universe, each carrying a human clone (`Art/References/ImageTo3D/2026-10-18-clone-egg/README.md`), and the colonists' skills feel like remembering (`Docs/VISION.md`). The film teases that and does not explain it. The middle of the film states the first pillar flatly (Principle 11: no fast travel, no instant builds, no health regen) over shots of what that means. That is the hook: most trailers promise less friction, this one promises more.

**Form:** text on screen, no narrator. A synthetic voice would be the cheapest-sounding thing in the film; large type over silence is what the genre does best. 2:00, 72 BPM, cut on the beat. At 72 BPM and 24 fps a beat is exactly 20 frames, so every cut, card and hit falls on a whole frame.

**Look:** Field Ledger's parchment and teal for type (the Steam wordmark's palette, Sitka), a filmic grade that leaves skin and foliage believable, real motion blur from the shutter (section 3), HUD off except where the HUD is the point (the wound panel, the possession menu).

## 3. How the footage is got, and why

Three ways to get pictures out of the game were weighed:

| Way | Image | Trouble | Call |
|---|---|---|---|
| Record a window in real time (MyScreen) | whatever the machine holds at 4K: TSR and Lumen get few frames per second of film | any stutter is in the film for good | no, not alone |
| Movie Render Queue | the best in principle: temporal and spatial samples | the plugin is not enabled in the project; it renders a level sequence, and this world exists only at run time after a new game (voxels, PCG, the seed); it would take a sequence asset per shot with world-space keys, and MRQ's runtime subsystem has never met this world | kept in reserve |
| A fixed time step, recorded | `-usefixedtimestep -fps=96` with MyScreen on the window (NVENC 4:4:4), slates to find frame 0 | tried first on the islands shot: the chain worked (the count within 2 of 640), but next to a lossless screenshot of the same pose the recording is visibly softer, rock grain smeared (NVENC at cq 12 averaged about 120 kB a frame) | dropped |
| **A fixed time step, lossless** | the same 96 Hz step, a Python sequence (`promo_shot.py`) setting the camera each frame, and the game's own screenshot (`shot`, a PNG straight from the final buffer) of two of every four simulation frames; each pair blended into one film frame, then ProRes 4444 at 24 fps | about 1 s per screenshot, so a 5 s shot takes about 4 minutes: the fixed step means that costs nothing but time. About 1 screenshot in 2,000 does not come back: the game's log says which, `capture.py` fills it from its neighbour (one film frame without its blend) and reports it | **chosen** |

Why it wins: nothing about the machine can reach the picture, so no stutter is possible and no codec touches the source. Blending 2 of 4 frames at 96 Hz is an exact 180 degree shutter at 24 fps, true motion blur from real sub-frames (the engine's own motion blur is off). TSR and Lumen see 96 frames per second of film, so they converge far better than at 24. And the world keeps loading in wall-clock time while film time crawls, which is exactly what beats the old captures' bare terrain and half-grown vegetation (`Docs/DECISIONS.md`, Oct 13: "all fucky looking with bare materials on the terrain and weird ground spawn stuff all over").

**The one exception is G22, the co-op shot:** two real game processes in a LAN session, recorded live from the host (`Tools/promo/coop_take.py`). A client's moves are stamped in real time, so a host held back by screenshots would rubber-band it. It holds about 26 fps at 1440p at the game's own settings with the client throttled, resampled to 24 and scaled up: softer than every other game shot, and it does not pretend otherwise.

Rules each shot obeys (from `Docs/TRAPS.md`): the editor closed (it starves a -game instance's vegetation); the world's second READY line before anything; `r.VolumetricRenderTarget 0` at 4K (`SteamArt/README.md`); the view is a photo camera with the HUD off (`shot` and a window recording with `MO.Cam.LookAt` draw no UMG unless `showui` is set); `settle` seconds on the first pose so PCG finishes round the lens; stills from every take are looked at before it counts. PCG follows the local player's pawn and camera (`Docs/PCG.md`), so the pawn travels under a flying camera (`pawn.under_camera`). Cameras are verbs and in-game Python, never OS input.

Sound is recorded separately in real time, because a fixed step and audio do not mix: `capture.py bed <name>` records the master submix in place (`Tools/promo/audio/record_beds.sh` is the whole set). The sound instance overrides the project's mute-when-unfocused on its command line (`-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0`); the Python name of the recorder is `unreal.AudioMixerLibrary`.

What bit the capture, so the next one doesn't re-learn it:

- `MO.Test.InjectInput`'s seconds are wall-clock, so on a slow fixed step hold a key with `9999` and let it go with the job's `after`. A draft's `after` lets go of a climb too: the next take then hangs still on the rock unless it holds the key again.
- `TaskStop` on a command that launched an instance kills the instance: run the boot in the foreground.
- The photo camera is one actor across shots: an exposure override stays until the next job sets it (`promo_shot.py` now always does).
- Teleport into the far world and the ground may not be there yet: she fell through a lake's bed and a cave's floor. Arrive, wait, then roll; `MO.Water.GoToLake` picks the nearest lake to where the pawn already is.
- The edit lost picture frames silently, three ways: `overlay=eof_action=pass` drops a frame where a card ends inside a shot, `-r 24` resampled away others, and a `split` to a null sink cost the CG shots their last frames. Each slipped every later cut against the music. Now: `eof_action=repeat`, `-fps_mode passthrough`, no stray split, 16 spare frames at the head of a graph, and `render_shot` reads the count back and stops if it is wrong. `check_motion.py --sources` matches every graded frame to its source: all moving shots are exact; the test cannot tell near-still shots (the title, the panel) from their neighbours, and for those the exact frame count is the check.
- Look at every take before it counts. The review sheet caught a take of the sky, a one-frame-short take, a smear of leaves through the lens and a frozen climber, all of which had "worked". For what a still cannot show, `python Tools/promo/check_motion.py` lists frozen frames and jumps in a finished cut.


## 4. Script and storyboard

The film's words, in order:

> THEY SENT TRILLIONS. / NOBODY REMEMBERS WHO. // YOU WAKE WITH NOTHING. / ONLY WHAT YOUR HANDS REMEMBER. // ONE SEED. / A WHOLE WORLD. // NO FAST TRAVEL. / NO INSTANT ANYTHING. / NO HEALTH REGEN. / YOU WILL NEED HELP. // FIND SURVIVORS. / PLAY ANY OF THEM. / OR BRING YOUR FRIENDS. // METAVERSE: ORIGINS

Lower thirds name each place with what the capture really was: the biome, then seed, clock and weather from the capture log. End card: `EARLY ACCESS ON STEAM`, `SOLO OR CO-OP`, and the line saying everything after the opening is the game itself, captured from the current development build, time-lapse where marked.

Acts: The seed (bars 0-5, CG, a low mass and glass tones); Wake (5-10, a thin felted piano over wind and surf); One seed, a whole world (10-18, a pulse builds and breaks open at bar 16 on the floating islands); Nothing is given (18-26, drums under each flat statement); You will need help (26-31, the theme in full); Title (31-36, the hit, the sun rising behind the stone letters, D major at last).

This table is `python Tools/promo/film.py table`; `film.py` is the source; every camera and action is in `Tools/promo/shots.json`. What v13 and scouting changed from the first storyboard is in section 7.

| # | Time | Dur | Shot | On screen | Camera | How it is got | Text |
|---|---|---|---|---|---|---|---|
| 1 | 0:00.00 | 7.50 | S01_void (cg) | A stream of stone pods falling past a planet's dark limb; the sun rises over it; one pod turns through focus | almost still, a slow creep forward | kit: build_coldopen.py void | THEY SENT TRILLIONS. |
| 2 | 0:07.50 | 6.67 | S02_entry (cg) | One pod, nose first, the stone burning; plasma streaming back, sparks | rides alongside, buffeted more and more | kit: build_coldopen.py entry | NOBODY REMEMBERS WHO. |
| 3 | 0:14.17 | 2.50 | S03_streak (cg) | The dusk side of the planet from high up: thin lines falling all over the cloud deck, one passes the lens | fixed, a little shake | kit: build_coldopen.py streak |  |
| 4 | 0:16.67 | 1.67 | B01_black (black) | Black |  |  |  |
| 5 | 0:18.33 | 5.00 | G01_pod (game) | The open stone pod in the grass at first light, its hatch beside it; a crag and a floating island behind | slow push in at knee height, past the plants | start site of the chosen seed, pawn hidden; dawn | YOU WAKE WITH NOTHING. |
| 6 | 0:23.33 | 5.00 | G02_survivor (game) | The survivor by the pod, her back to us, head and shoulders; the sea, then the sun and a floating island beyond her | arc round her at shoulder height | the pawn idle; photo camera |  |
| 7 | 0:28.33 | 5.00 | G03_feet (game) | Low: bare feet walking through long grass towards the first trees | tracks beside at shin height, grass passing the lens | the pawn walking by MO.Test.InjectInput; camera follows the pawn | ONLY WHAT YOUR HANDS REMEMBER. |
| 8 | 0:33.33 | 5.00 | G04_forest (game) | Temperate forest from above at golden hour, rolling and endless | drone, slow flight forward over the canopy | photo camera, pawn under the path; PCG primed | ONE SEED. *Temperate forest* |
| 9 | 0:38.33 | 5.00 | G05_sea (game) | The open sea from a low strand in the early light, swell running in over the shallows | slow drift down towards the water | photo camera; Oceanology ocean | A WHOLE WORLD. *The open sea* |
| 10 | 0:43.33 | 3.33 | G06_rainforest (game) | Inside the rainforest: ferns and trunks, dappled light | low dolly through the ferns | photo camera at the floor | *Rainforest* |
| 11 | 0:46.67 | 3.33 | G07_coast (game) | A rock strand at evening, tide pools, a mountain with a floating island over it | slow push | photo camera | *Nordic strand* |
| 12 | 0:50.00 | 3.33 | G08_ravine (game) | On the floor of a ravine: a 50-100 m slot of rock, the sky a strip above | glides along it | photo camera; generator v13 | *Ravine* |
| 13 | 0:53.33 | 6.67 | G09_islands (game) | Floating islands torn from the rock, forests on their tops, over the land below | slow rise and turn to reveal, the score breaks open here | photo camera; generator v13; the pawn near the first island | *Floating islands* |
| 14 | 1:00.00 | 3.33 | G10_trek (game) | One small figure crossing a dark rock strand under a pink evening sky | long lens, a slow pan with her | pawn walking (MO.Test.InjectInput); photo camera 60 m away | NO FAST TRAVEL. |
| 15 | 1:03.33 | 3.33 | G11_climb (game) | The survivor on a mossy rock face, hands and feet finding holds | above and behind, moving with her | the real climb on a cliff, W held, a jump to grab (Tools/check_cliff_climb.py's way) |  |
| 16 | 1:06.67 | 1.67 | G12_swim (game) | Swimming seen from under the water, the surface a mirror above her | under the surface, tracking | the real swim in an upland lake |  |
| 17 | 1:08.33 | 6.67 | G14_wall (game) | A wattle wall going up: stakes, the rods woven in, then the moss daub layer by layer | slow push | the real pieces and stages (MO.Test.PlacePiece, MO.Test.PieceProgress); the game's time for this is far longer (time-lapse tag) | NO INSTANT ANYTHING. *TIME-LAPSE* |
| 18 | 1:15.00 | 1.67 | G15_fire (game) | A stone-ring campfire burning at dusk, in the grass | close, low | a lit fire (MO.Fire.SpawnLit) |  |
| 19 | 1:16.67 | 3.33 | G16_wolf (game) | A wolf comes through the trees, head low, towards the lens | long lens, panning with it | a real wolf hunting the hidden pawn (MO.Test.SpawnCreature) | NO HEALTH REGEN. |
| 20 | 1:20.00 | 3.33 | G17_wound (game) | The Wounds tab: what that wolf's bites did, part by part, bleeding in ml/s | the screen itself | the real status panel after real bites (shot showui) |  |
| 21 | 1:23.33 | 3.33 | G18_cave (game) | A figure walking into the black of a cave tunnel, lit only by her own torch | behind her, moving with her | the real torch and a generated cave (MO.WorldGen.FindCave) | YOU WILL NEED HELP. |
| 22 | 1:26.67 | 5.00 | G19_survivor_meet (game) | A stranger's face in evening light, her campfire's smoke drifting through | long lens, holds on her | a wild survivor the world spawned at her own campfire | FIND SURVIVORS. |
| 23 | 1:31.67 | 5.00 | G20_fire_group (game) | Night under an aurora: five survivors round a fire, smoke rising past the treeline | slow lateral slide, wide | real recruited survivors and a real fire; standing them in a ring is staging; night clock |  |
| 24 | 1:36.67 | 3.33 | G21_possess (game) | The Characters menu: Select, and you are someone else, walking into the dawn | the screen, then behind the new body | the real menu and a real click (shot showui, MO.Test.ClickWidget) | PLAY ANY OF THEM. |
| 25 | 1:40.00 | 3.33 | G22_coop (game) | Two players on a ridge, two real game processes in one world | wide, slow push | a real LAN co-op session (ue.py inst) | OR BRING YOUR FRIENDS. |
| 26 | 1:43.33 | 10.00 | S04_title (cg) | METAVERSE, ORIGINS in stone, the sun rising behind it | slow push through drifting dust | kit: build_title.py |  |
| 27 | 1:53.33 | 6.67 | S05_end (cg) | End card: the wordmark, status line, honesty line | still, slow fade | kit: build_endcard.py |  |

## 5. The kit: re-using it for the next film

Everything is a script that takes JSON, so new text or timing is a re-render, never a re-model. Blender 5.2, headless. Fonts: `python Tools/promo/kit/make_fonts.py` first (static instances of Windows' Sitka and Bahnschrift, the Steam wordmark's face; not tracked; when Wes answers F14 point `make_fonts.py` at Bitter and IBM Plex and re-render).

| Piece | Script | Makes |
|---|---|---|
| Shared | `kit/kit_common.py` | render setup, palette, node helper, the pod's granite, type layout with optical kerning |
| Hero prop | `kit/kit_pod.py` | the clone pod, closed: a stone egg 2.2 m long, charred nose that can glow (`heat_node`), hatch seam with a teal inlay (`inlay_node`); any detail level. The game's own open pod is `/Game/ImageTo3D/CloneEggPod01` |
| Hero set | `kit/kit_space.py` | stars, a planet with no lights on its night side, a thin atmosphere shell, a sun that rises over the limb, a render-time glow |
| Cold open | `kit/build_coldopen.py {"shot": "void" or "entry" or "streak"}` | three CG shots |
| Title | `kit/build_title.py {"line1", "line2", "frames_total", "hit", "logo_only"}` | METAVERSE / ORIGINS in pale cut stone and teal inlay; `logo_only` renders it on transparency over anything |
| Type | `kit/build_textcard.py {"style": "fade" or "slam" or "lower" or "end", ...}` | RGBA frames: a line that fades in and tracks out; one that lands hard on a beat; a lower third; an end card's lines |
| Overlays | `kit/build_overlay.py {"kind": "flare" or "embers" or "dust"}` | RGBA frames: a sweep of anamorphic light for a cut, rising embers, a burst of dust for an impact |
| All the cards of a film | `render_cards.py` | every card in `film.py`; `--stills` for one frame each, `--scale`, `--only` |
| The other PC | `darktower.py sync / run NAME SCRIPT JSON / wait / pull` | a kit job as a scheduled task on DARKTOWER's GPU (Blender 5.2 is a portable folder in `D:\MO57Promo`, copied over from here, nothing installed) |

Render a test first: `{"scale": 0.25, "samples": 48, "still": [60, 150]}`. Frames land as `f_####.png` in `Saved/Promo/kit/<id>/`.

To make the next film: copy `film.py`'s structure (bars, shots, cards), write the score against the same grid, run `render_cards.py`, then `edit.py --animatic` to check it before any capture.

## 6. Sound

Score (`audio/score.py`, `audio/instruments.py`, `audio/dsp.py`): synthesised note by note, numpy only, no samples. D minor, a falling cell (A G F) and a rising answer (A C F); it ends on the falling cell resolved in D major. Written on the film's bar grid; stems in `Saved/Promo/audio/stems/`. No music-generation model is on either PC (searched Nov 4: no MusicGen, ACE-Step, Stable Audio or soundfonts; DARKTOWER's ComfyUI has no audio checkpoints), and one would be an unapproved download.

Mix (`audio/mix.py`): music, the game's own sound under each shot (beds recorded from the game's master submix, one-shots from its bank), sound design on the cuts. Mastered to about -14 LUFS, -1 dBTP with a static gain after a limiter, measured with ffmpeg's `ebur128`. The report names every stand-in, so nothing ships silently fake. The game has its own music too (`/MOFramework/Audio/`: Intro01, Cedar_Smoke_Psalm, female_vocalization01 / 02, flukes_drums01): `mix.py --music file.wav` lays any track under the same mix.

## 7. Where it stands

**Delivered Nov 5:** `Saved/Promo/out/MO57_Promo_4K_master_hevc10.mp4`, `MO57_Promo_1080p.mp4`, `contact_sheet.png` (not in git; lossless takes in `Saved/Promo/raw/`, mezzanines in `Saved/Promo/mezz/`, graded shots in `Saved/Promo/graded/`). Captured on master f5b240e1, generator v13, seed 777. Mix: -14.0 LUFS, -1.2 dBTP, LRA 10.5, no stand-in beds (`Saved/Promo/audio/mix_report.json`).

**What is not the game:** the first three shots and the title and end card are Blender renders. The score is synthesised. Sound design on the cuts (the pods' roar, the air moves, the UI ticks) is synthesised.

**Staged, though real:** the five survivors were stood in a ring round a fire (G20) and two recruits were given their bases and names by test verbs. The wall (G14) runs the real pieces and stages, but its progress is set by `MO.Test.PieceProgress` at a pace far faster than the game's, under a TIME-LAPSE tag. The wounds on the panel (G17) are from a real wolf biting the real pawn; she later bled out, which is why the later shots use other survivors and a fresh world.

**What v13 and scouting changed from the first storyboard:**

- G05 Alpine rock became the open sea. At eye level AlpineRock's plants are black shards at seed 777 and its snow is flat white (`Saved/Promo/scout/alpine_sheet.jpg`); a distant view did not earn the shot.
- G13, the dig, is cut. An open-ground dig is 0.88 m3 spread over a 2.5 m radius: 26 of them read as nothing, and `MO.Test.TerraformRPC` aimed some far under the ground (Z -1422, -2027). The timed dig found no target on the ravine floor. A pit in the film would have needed faking, so its time went to the wall. Destructible voxel terrain is in the game and not in this film.
- G08 is on the ravine floor looking along it: from the rim a ravine reads as a ditch.
- G09, the floating islands, needed no working round: golden hour on seed 777's first island is the film's best shot, and they show on the horizon of G01, G02, G04 and G07 too.
- G12 went under water when the first test framed it: the surface as a mirror above her.
- G18 is a generated tunnel at -1.9 EV: at the game's own exposure its torch blows the rock out.
- G19 is a wild survivor the world spawned at her own campfire. G20 went wide because close up the idle poses and the underwear look wrong; wide, the game's aurora and the fire make the shot.

### Wes: check by eye and ear

No agent has heard the film, and whether it looks right is yours. Nothing below is called verified.

**By ear:**

1. The whole score: synthesised, judged only by levels and a spectrogram (fork F68: the game's tracks are the free alternative, `mix.py --music`).
2. The bed levels: every bed is normalised, so an event in one (a bird, a gust) may stick out. The lake bed is very quiet at source (-60 LUFS; water sounds were muted on Oct 24, your call) and lifted a long way.
3. Footsteps on G03 are timed by eye to the heel strikes.

**By eye, the shots I am least sure of:**

1. S02_entry, the CG I reworked last: the first version read as an egg in orange wrap, so it was rendered again with a narrow comet tail (`{"bow_a": 0.54, "flame_gain": 5.5, "core": 0.7, "heat": 0.42, "streak": 6.5}` in `build_coldopen.py`). Still the CG shot to look at hardest; another pass is 80 minutes on DARKTOWER.
2. S01 and S04: the planet's atmosphere line is very white.
3. G08 ravine: the voxel rock looks chewed close up.
4. G15 fire: the campfire's logs look low-poly this close, and the flames are the old effect.
5. G20: smoke is heavy, and the ring is staged.
6. G21 and G22: everyone is in underwear, because that is what the game gives them today. Harvey goes reddish in the low sun in G22, and G22 is the one soft shot.
7. G17: the panel is real and honest, but it is a plain UI on screen for 3 s, with the bar through the text (N-315).
8. G21: the game's HUD reticle fades out over about a third of a second after the click, faint at the top of the frame.
9. The flare sweeps at bars 16, 18, 26 and 31 (`edit.py` `FLARE_AT`, `FLARE_OPACITY`).
10. Type: Windows' Sitka until F69.

**Does the film fall short?** Yes, in these places: there is no digging, no real crafting or work animation (F54 is still open) and no combat in it, because none of them yet look good on camera; every character is in underwear; the CG is good, not AAA; G22 is softer than the rest. Forks F68 to F71 in `Docs/DECISIONS.md` are yours.
