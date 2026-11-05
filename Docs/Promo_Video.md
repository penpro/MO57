# The promo video

> **What this is:** the production book of MO57's promo film: concept, script, shot-by-shot storyboard, how every picture and sound is made, and how to re-use the kit for the next one.
> **Current state (2026-11-05):** in production. Script, storyboard, Blender kit, score, mix and edit pipeline are built and run on stand-in pictures; the game footage waits for the Unreal window (section 7 says exactly where it stands).
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
| **A fixed time step, recorded** | `-usefixedtimestep -fps=96`: every engine frame is 1/96 s of the world however long the GPU takes over it; a Python sequence (`promo_shot.py`) sets the camera each frame; MyScreen records the window (NVENC, 4:4:4); `capture.py cut` keeps two of every four frames and blends them | a dropped recording frame would break the count: slates at both ends find frame 0 and check the count | **chosen** |

Why it wins: nothing about the machine can reach the picture, so no stutter is possible. Blending 2 of 4 frames at 96 Hz is an exact 180 degree shutter at 24 fps, true motion blur from real sub-frames (the engine's own motion blur is off), and it lifts an 8-bit recording to about 9 bits before the grade. TSR and Lumen see 96 frames per second of film, so they converge far better than at 24. And the world keeps loading in wall-clock time while film time crawls, which is exactly what beats the old captures' bare terrain and half-grown vegetation (`Docs/DECISIONS.md`, Oct 13: "all fucky looking with bare materials on the terrain and weird ground spawn stuff all over").

Rules each shot obeys (from `Docs/TRAPS.md`): the editor closed (it starves a -game instance's vegetation); the world's second READY line before anything; `r.VolumetricRenderTarget 0` at 4K (`SteamArt/README.md`); the view is a photo camera with the HUD off (`shot` and a window recording with `MO.Cam.LookAt` draw no UMG unless `showui` is set); `settle` seconds on the first pose so PCG finishes round the lens; stills from every take are looked at before it counts. PCG follows the local player's pawn and camera (`Docs/PCG.md`), so the pawn travels under a flying camera (`pawn.under_camera`). Cameras are verbs and in-game Python, never OS input.

Sound is recorded separately in real time, because a fixed step and audio do not mix: `capture.py bed <name>` records the master submix in place.

## 4. Script and storyboard

The film's words, in order:

> THEY SENT TRILLIONS. / NOBODY REMEMBERS WHO. // YOU WAKE WITH NOTHING. / ONLY WHAT YOUR HANDS REMEMBER. // ONE SEED. / A WHOLE WORLD. // NO FAST TRAVEL. / NO INSTANT ANYTHING. / NO HEALTH REGEN. / YOU WILL NEED HELP. // FIND SURVIVORS. / PLAY ANY OF THEM. / OR BRING YOUR FRIENDS. // METAVERSE: ORIGINS

Lower thirds name each place with what the capture really was: the biome, then seed, clock and weather from the capture log. End card: `EARLY ACCESS ON STEAM`, `SOLO OR CO-OP`, and the line saying everything after the opening is the game itself, captured from the current development build, time-lapse where marked.

Acts: The seed (bars 0-5, CG, a low mass and glass tones); Wake (5-10, a thin felted piano over wind and surf); One seed, a whole world (10-18, a pulse builds and breaks open at bar 16 on the floating islands); Nothing is given (18-26, drums under each flat statement); You will need help (26-31, the theme in full); Title (31-36, the hit, the sun rising behind the stone letters, D major at last).

This table is `python Tools/promo/film.py table`; `film.py` is the source. Shots marked (game) are subject to scouting: what looks best in v13 decides the exact place, and what fails the bar is replaced (section 7 keeps the record).

| # | Time | Dur | Shot | On screen | Camera | How it is got | Text |
|---|---|---|---|---|---|---|---|
| 1 | 0:00.00 | 7.50 | S01_void (cg) | A stream of stone pods falling past a planet's dark limb; the sun rises over it; one pod turns through focus | almost still, a slow creep forward | kit: build_coldopen.py void | THEY SENT TRILLIONS. |
| 2 | 0:07.50 | 6.67 | S02_entry (cg) | One pod, nose first, the stone burning; plasma streaming back, sparks | rides alongside, buffeted more and more | kit: build_coldopen.py entry | NOBODY REMEMBERS WHO. |
| 3 | 0:14.17 | 2.50 | S03_streak (cg) | The dusk side of the planet from high up: thin lines falling all over the cloud deck, one passes the lens | fixed, a little shake | kit: build_coldopen.py streak |  |
| 4 | 0:16.67 | 1.67 | B01_black (black) | Black |  |  |  |
| 5 | 0:18.33 | 5.00 | G01_pod (game) | The open stone pod on the shore at first light, its hatch beside it, the sea behind | slow push in at knee height | start site of the chosen seed, pawn hidden; dawn | YOU WAKE WITH NOTHING. |
| 6 | 0:23.33 | 5.00 | G02_survivor (game) | The survivor stands by the pod, her back to us, looking inland; bare skin and rough ground; hair moving | slow arc round her at shoulder height | the pawn idle; photo camera |  |
| 7 | 0:28.33 | 5.00 | G03_feet (game) | Low: bare feet walking through long grass towards the first trees | tracks beside at shin height, grass passing the lens | the pawn walking by MO.Test.InjectInput; camera follows the pawn | ONLY WHAT YOUR HANDS REMEMBER. |
| 8 | 0:33.33 | 5.00 | G04_forest (game) | Temperate forest from above at golden hour, rolling and endless | drone, slow flight forward over the canopy | photo camera, pawn under the path; PCG primed | ONE SEED. *Temperate forest* |
| 9 | 0:38.33 | 5.00 | G05_alpine (game) | Bare granite crags and snow, cloud shadow crossing | slow lateral slide | photo camera | A WHOLE WORLD. *Alpine rock* |
| 10 | 0:43.33 | 3.33 | G06_rainforest (game) | Inside the rainforest: layered ferns, broad leaves, shafts of light | low dolly between trunks | photo camera at the floor | *Rainforest* |
| 11 | 0:46.67 | 3.33 | G07_coast (game) | The sea on a rocky strand, waves running in, sea stacks | slow push out over the water | photo camera; Oceanology ocean | *Nordic strand* |
| 12 | 0:50.00 | 3.33 | G08_ravine (game) | Down into a ravine: a 50-100 m slot of ledges and crags, light from above | tips over the edge and sinks | photo camera; generator v13 | *Ravine* |
| 13 | 0:53.33 | 6.67 | G09_islands (game) | Floating islands torn from the rock, forests on their tops, over the land below | slow rise and turn to reveal, the score breaks open here | photo camera; generator v13; the pawn near the first island | *Floating islands* |
| 14 | 1:00.00 | 3.33 | G10_trek (game) | One small figure crossing a long hillside at dusk | long lens, nearly locked off | pawn walking; photo camera far away | NO FAST TRAVEL. |
| 15 | 1:03.33 | 3.33 | G11_climb (game) | The survivor climbing a rock face, hands and feet on the stone | beside and below her, tilting up with her | the real climb on a cliff (Tools/check_cliff_climb.py's setup) |  |
| 16 | 1:06.67 | 1.67 | G12_swim (game) | Swimming a crawl across a lake | low at the water, tracking | the real swim |  |
| 17 | 1:08.33 | 3.33 | G13_dig (game) | A pit opening in the hillside: the voxel ground bitten away dig by dig, earth left beside it | low and close, locked off | the real timed dig (time-lapse tag) | NO INSTANT ANYTHING. *TIME-LAPSE* |
| 18 | 1:11.67 | 3.33 | G14_wall (game) | A wattle wall going up rod by rod, then its daub | slow push | the real building system's stages (time-lapse tag) | *TIME-LAPSE* |
| 19 | 1:15.00 | 1.67 | G15_fire (game) | The stone-ring campfire catching at dusk | close, low | a lit fire (MO.Fire) |  |
| 20 | 1:16.67 | 3.33 | G16_wolf (game) | A wolf comes through the trees, head low | low, in front of it | the wolf's real hunt | NO HEALTH REGEN. |
| 21 | 1:20.00 | 3.33 | G17_wound (game) | The body panel: real wounds by body part, bleeding, what they stop you doing | the screen itself | the real medical panel (shot showui) |  |
| 22 | 1:23.33 | 3.33 | G18_cave (game) | A torch going down into a dark cave; the rock lit only by it | behind her, moving with her | the real torch and cave | YOU WILL NEED HELP. |
| 23 | 1:26.67 | 5.00 | G19_survivor_meet (game) | A stranger at the edge of a clearing, turning towards the camera | slow arc | a real survivor near the start | FIND SURVIVORS. |
| 24 | 1:31.67 | 5.00 | G20_fire_group (game) | Night. Several survivors round a fire; firelight on faces | slow lateral, the fire in front | real recruited survivors, night clock |  |
| 25 | 1:36.67 | 3.33 | G21_possess (game) | The possession menu: pick another body, and you are them | the screen, then behind the new body | the real menu (shot showui), MO.Possess | PLAY ANY OF THEM. |
| 26 | 1:40.00 | 3.33 | G22_coop (game) | Two players on a ridge, two real game processes in one world | wide, slow push | a real LAN co-op session (ue.py inst) | OR BRING YOUR FRIENDS. |
| 27 | 1:43.33 | 10.00 | S04_title (cg) | METAVERSE, ORIGINS in stone, the sun rising behind it | slow push through drifting dust | kit: build_title.py |  |
| 28 | 1:53.33 | 6.67 | S05_end (cg) | End card: the wordmark, status line, honesty line | still, slow fade | kit: build_endcard.py |  |

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

(Updated as the work goes.)

- Built and run: `film.py`, the Blender kit (final CG and type rendering on DARKTOWER as batches `final_cg` and `final_cards`, then `darktower.py pull` into `Saved/Promo/kit/`), the score, the mix (-14.2 LUFS, -1.2 dBTP on stand-in beds), the grade LUTs, `edit.py` on slates with type.
- Written, not yet run: `capture.py`, `promo_shot.py` (they need the Unreal window, coop CL-0361), `audio/export_game_sounds.py` (needs the editor).
- Not yet: game footage, recorded beds, the final master.

Known constraints found while planning the shots: the bases Jalena and Zuri are topless in game (F49), so the film's women are the femme base, the men Harvey and Trey in their briefs; wide aerials show bare clearings where far vegetation thins (`MO_WorldLab/Saved/IslandsRavines/r9_plants/`), so island shots look up from low ground rather than across the land; there is no work animation (F54): a survivor stands still while a bar fills, so the film shows what the work leaves behind, not the hands doing it.

### Wes: check by eye and ear

Filled in when the film is delivered.
