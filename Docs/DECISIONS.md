# Decisions: Wes's binding calls and the forks waiting for him

> **What this is:** the one place for design decisions. **Needs Wes** (open forks) is first; then the calls Wes made, newest first, each
> with his words (verbatim where recorded), what was built and the commit. Gathered on 2026-10-17 from agent memory (wes-calls-oct15/16/17,
> directive-oct13, terraform-excavation-design, dev_status, the terrain / water / co-op / UI memories), the handoffs, night reports,
> bug-report trackers and the polish backlog.
> **Rules:** a decided call is not re-asked ("Do not repeatedly ask about choices already answered"); a new fork is logged here with a
> recommendation instead of being built blind; when Wes answers, move the fork to the dated list with his words and what was built.
> **Related:** `Docs/BACKLOG.md` (the work that follows from these), `Docs/PROGRESSION_ROADMAP.md` (progression design authority),
> `Docs/VISION.md`, `Docs/ENGINEERING_PRINCIPLES.md`.

## Needs Wes (open forks)

| # | Fork | Options (recommendation first) | Brief | Logged |
|---|---|---|---|---|
| F1 | Survival meters: one source of truth | **A** derive Hunger / Thirst / Energy from the metabolism (one realistic source); B clock-scaled counters; C keep arcade counters. Also: whether Energy models sleep debt. Pacing itself was decided Oct 15 | `Docs/Survival_Single_Source_Proposal.md` | Oct 12-13 |
| F2 | Body temperature model, the rest | Single node SHIPPED Oct 19 as the interim (Wes). Still to decide: **core + shell** (vasoconstriction, cold-water shock), clo values per item (+ wet multiplier, a DT_Items migration), shivering burns glucose | `Docs/Thermal_Model_Reproduction.md` | Oct 11-19 |
| F3 | Do removed things regrow? | slow regrowth on game time (litterfall weeks, saplings years) vs permanent (today) | `Docs/PCG.md` §7 | Oct 13 |
| F4 | Performance target | choose the reference resolution / quality and the target hardware tier before tuning scalability (the 4090 floor is ~11 ms at Epic) | `Docs/Performance.md` §1 | Oct 11-13 |
| F5 | Shore reeds | a Fab / Megascans download vs Electric Dreams plants (Taro, ferns; no reed asset there) | `Docs/Water.md` §5 | Oct 11 |
| F7 | Building cancel / refund | queue path (full refund + world drops) vs ghost-menu path (skill-partial); one rule for both | `Docs/Building.md` | Jul 20 |
| F8 | Excavation stage 6 UI | the designation tool + "where do you want to dump the contents?" popup (designated sites / fill inventory / fill nearby inventory). Wes said on Oct 15 the earlier question wasn't clear: **re-ask plainly with a mock-up**. Recommended default flatten target: the zone's MEAN height | `Docs/Terraform_Excavation_Plan.md` | Jul 13 / Oct 14 |
| F10 | Heat ladder values + missing kilns | author the drafted station / recipe temperatures (campfire 700, forge 1,300, bloomery 1,250; copper 1,100, iron bloom 1,200 ...) and add pit kiln (900) + updraft kiln (1,100) stations; waits also on Codex's fuel-to-temperature rule | `Docs/PROGRESSION_ROADMAP.md` §4 | Oct 17 |
| F11 | Engine patch #1 upstream | post the ready report (hair strands vs Single Layer Water depth) on the Epic forum + bug form (a public post) | `Docs/EnginePatches/0001-upstream-report.md` | Oct 16 |
| F12 | Reeds along narrow rivers | `ShoreFactor` samples one 25 m ring: rings at 5 / 12 / 25 m add reeds along rivers at ~3x shore-tier samples (load time + look change) | `Docs/PCG.md` | Oct 16 |
| F13 | Default clock time scale | real time (1.0, today: weather and day barely change in a session) vs faster (e.g. 24 = a day per hour) | `Docs/Weather_And_Time.md` §1 | Oct 10 |
| F14 | Downloads waiting for a yes | Epic's official "Made with Unreal Engine" logo (splash card); the UI kit's OFL fonts (Bitter, IBM Plex Sans / Mono); a tropical plant pack; beach packs (shells, dune grass, driftwood); more sand sets; Megascans beyond the migrated ED slice | `Docs/UI.md` §6, `Docs/PCG.md` | Oct 10-14 |
| F15 | Character gender | where gender comes from (the creator sets everything else). Oct 26: the creator shows Body type: Femme with Masc coming soon (Wes); still open: whether the identity's free-text Gender follows the body type | `Docs/UI.md`, wes-calls-oct15 | Oct 15 |
| F16 | Play time per era | the February intent was ~1,000 h single-player (Early Paleolithic 0-50 h ... Iron Age 950+ h); not re-approved | `Docs/PROGRESSION_ROADMAP.md` §2 | Feb / Oct 17 |
| F17 | Chariot and horseshoe without draft animals | keep, gate behind domestic horses, or cut | `Docs/PROGRESSION_ROADMAP.md` §3 | Oct 17 |
| F18 | "Attempt anything" | on top of inspection precursors (built Oct 17): may an undiscovered recipe be tried at a penalty (x3 time, poor quality, chance to waste), success teaching it? Recommended Oct 14 as the rule | `Docs/PROGRESSION_ROADMAP.md` §6 | Oct 14 |
| F19 | Ground for colonists far from every player | recruited colonists working away from players have no collision invoker and fall / get rescued: an invoker only while a job is active (collision chunks per colonist) vs hold / freeze until ground exists | `Docs/Coop_And_Networking.md` §3 | Oct 7-8 |
| F20 | Eye adaptation speed | UDS adapts 3 EV/s up, 1 EV/s down; real dark adaptation takes minutes. Slower down = more faithful but changes every shade transition | `Docs/Light_Sources.md` | Oct 17 |
| F21 | Bland distant mountains | after a measured negative shader result: mountain-scale relief that survives far LODs / a clearer Clear_Skies atmosphere / a lower or temperature-driven snow line / an artist's far texture; also the flat-white sunlit snow at 1 km (SnowColor vs exposure) | `Docs/Terrain_Generation_Exploration.md` To-do | Oct 12-15 |
| F22 | Water track beyond drinking | W2 freshwater springs as world features, W3 rain collector, W4 pawn water hauling (W1 drinking built Oct 15) | `Docs/Archive/Fable5_Autonomous_Pipeline.md` | Jul 7 |
| F26 | How long studying an item takes | Every build so far studied an item in **1 s**: the inspection widget's `DebugInspectionDuration` (1.0) overrode the code's 15 s. The study is now the server's (`MO.Inspect.Seconds`, kept at 1 s). **A** keep 1 s; **B** 15 s for everything (the code's old design value); **C** by the item: a few seconds for a stick or a stone, longer for a made thing or an ore whose lesson is deeper (a column on the item row). Discovery runs through inspection (PROGRESSION_ROADMAP rule 4), so this sets the pace of the whole tech tree. **Recommended: C**, about 3 s for raw materials up to 15 s for a finished tool or a metal, with A until the column is filled | `Docs/BugReports_2026-10-18.md` | Oct 18 |
| F27 | Cliff bands that read as a staircase | Wes's joiner report BR18-18 ("goffy tran jan"): a 42 deg AlpineRock flank cut into a flight of even horizontal steps with dark risers. That is the generator's v8 cliff bands (`MOWorldGen.cpp`, the crag terraces: steps of `CragAmplitudeM` 9 m x VerticalScale on slopes 0.30-0.65, in soft patches): every riser the same height, so a big flank shows a regular stair. **A** keep; **B** irregular bands -- each band's height from a hash of its index (about 0.5-1.6 x the step) and the patches rarer (`CragEdge` 0.6 -> ~0.68), generator v12; **C** drop the terraces and leave cliffs to F9's composition. B and C change the steep ground of EVERY saved world (the generator contract, MOWorldGen.h: version bump + golden checksum). **Recommended: B**, bundled with the next save-breaking generator change so worlds change once | `Docs/BugReports_2026-10-18.md`, `Docs/Terrain_Generation_Exploration.md` | Oct 18 |
| F28 | Teaching what the bark container is for | Wes (Oct 18): "im not sure what we're supposed to fill it with? like hide glue and resin?". It is the first water vessel (about 1 L: fill it at a river or lake, boil by dropping fire-heated stones in, drink), and its item description says so, but no tutorial step uses it (the chain ends campfire -> inspect an item -> possession). It is 14 x 13 x 12 cm in the world: true to a 1 L folded-bark vessel, so the size stays. **A** add a "Safe water" tutorial step after the campfire: make a bark container, fill it, boil it with heated stones, drink (the hints name each action); B a hint only, the first time the player stands at water carrying an empty container; C leave it to the description | `Docs/Water_And_Illness.md` §First hours to safe water; BR18-29 | Oct 18 |
| F29 | How a pawn sleeps (nobody can today) | Energy only recovers by sleeping (`UMOSurvivalStatsComponent`: `Energy.RegenRate = 0`, "only recovers by sleeping"), the Exhausted moodle says "sleep", but no player or survivor action sleeps: every pawn runs down for good (BACKLOG N-065). Real sleep is ~8 h, and the clock runs at real time (F13), so sleeping in real time is a session. **A** a timed Sleep action (ground / bedding / shelter quality sets the rate) that the PAWN does while the player possesses another colonist (possess-any-pawn is the design's answer to tedium; a lone pawn's player watches a short summary or plays on the menu clock), plus survivors sleeping as a colony job at night; B sleep fast-forwards that pawn's needs (a skip: against Principle 11); C energy regenerates while resting (no sleep) | `Docs/Medical_And_Survival.md` §3, F1, F13 | Oct 18 |
| F34 | Does handling items take time? | Moving items is instant everywhere today: one item or Take all / Store all between the pack and a container (`EMOInventoryOp::Transfer` / `TransferAll`), and Loot all on the Nearby tab (already the "sweep pickup": every ground item in reach, at once). A timer on the batch alone would make Take all slower than clicking each item, so N-092's "combined timer" needs a per-item time first. **A** handling takes real time by bulk and weight (about 0.5 s + 0.1 s per kg an item, a batch the sum on the pawn's timed action, interruptible; the principles' Zomboid example); **B** keep it instant (a UI shuffle, like stacking); **C** only the ground costs time (bending to pick up or lay down: E, Loot all), container to container stays instant | `Docs/ENGINEERING_PRINCIPLES.md` (tedium), BACKLOG N-092 | Oct 20 |
| F35 | How much a novice's treatment does | A treatment's quality is `clamp(skill / 100, 0.1, 1) + (skill - minimum) x QualitySkillScaling / 100` (`UMOAnatomyComponent::ApplyResolvedTreatment`), on skills that run to 100: an unskilled person's bandage (Medicine 0) cuts the bleeding by 0.7 x 0.1 = **7 %**, and even Medicine 10 only ~9 %; a tourniquet anyone ties stops 10 %. Real first aid is the other way round: direct pressure and a dressing, or a tourniquet, work in untrained hands (that is why they are taught to everyone); skill shows in the hard procedures (suture, reducing a dislocation). Every player and the new colony medic start near 0. **A** a floor by the treatment's own minimum skill: a row whose `MinimumSkillLevel` is 0 or 1 (bandage, tourniquet, clean, painkiller) starts at quality ~0.6 and reaches 1.0 by Medicine 30; harder rows keep today's curve; **B** one curve for all, quality 0.4 at skill 0 to 1.0 at 50; **C** keep. **Recommended: A** | `Docs/Medical_And_Survival.md` §7 | Oct 20 |
| F36 | A first-aid dressing before cloth (N-062) | MakeBandage needs WovenCloth01, which needs flax or wool (no producer yet), so for the whole stone age nobody can dress a wound -- and the colony medic (Oct 20) has nothing to carry. Before cloth people used pads of soft plant fibre or inner bark bound with cordage, sphagnum moss, and hide strips. **A** a new item, a fibre dressing (bark fibre pad + cordage, both made today), with its own treatment row a little weaker than the cloth bandage (bleed 0.5 against 0.7, infection 0.1 against 0.3: absorbent, not clean); **B** the same recipe making the existing Bandage01 (no new item; a cloth bandage in name only); **C** a moss dressing (needs a moss resource on forest floors and bogs first). **Recommended: A** (then C when moss exists) | `Docs/PROGRESSION_ROADMAP.md` section 3 (realism checks) | Oct 20 |
| F37 | How a HOSTED new world gets its creation settings (N-198) | The New Game panel now sets a world's shape, pace and start; the Host Game panel (camp name, max players) starts a new world with the defaults. **A** a "Host for co-op" toggle (+ camp name / max players) on the New Game panel: one place creates a world, hosting is how it opens; the Host panel keeps hosting a SAVED world. B: the same knobs and cards repeated on the Host panel. C: leave hosted new worlds at the defaults | `Docs/World_Creation.md` | Oct 22 |
| F41 | A horse (the rest of the animal purchase) | Wes bought PROTOFACTOR's ANIMALS FULL PACK (30 species; the dated list, Oct 26); it has no horse. **Recommended:** AnimalDev "Classic Horse" ($39.99) or another realistic rigged horse with walk / trot / canter / gallop cycles, when riding or draught animals come up | `AI_And_Colony.md` §1, BACKLOG N-221 | Oct 26 |
| F42 | How long crops take to grow | Real growing seasons on the game clock (barley ~90-100 days, flax ~100, peas ~70, emmer sown in autumn ~8 months), scaled by a world-creation **Growing** pace like crafting and building. At the default clock (real time, F13) a barley crop is ~3 months of play. **A** real seasons + the Growing pace (default 1) and settle F13 (24x = a crop in ~4 days of play); **B** compress crops alone (game-y: refused by Principle 11 unless chosen as a world rule). **Recommended: A** | `Docs/Farming_And_Husbandry.md` §2 | Oct 26 |
| F43 | How an animal becomes domestic | **A** captured young (a piglet, a lamb) tame through days of handling and feeding in a pen; adults stay wild (flight distance); animals born in the pen are tame; domestic traits (docile, more milk / wool) over generations. **B** any animal tamed by a feeding meter (game-y). **Recommended: A** | `Docs/Farming_And_Husbandry.md` §3 | Oct 26 |
| F44 | The first crops | **A** the Neolithic founder crops by biome: barley, emmer wheat, flax, pea (temperate / grassland), broomcorn millet (dry / cold), taro (rainforest); wild stands of each to gather seed from first. **B** a generic "grain" + "vegetable". **Recommended: A** (flax also unblocks cloth: FlaxFiber01 has no producer) | `Docs/Farming_And_Husbandry.md` §2 | Oct 26 |
| F23 | Rejected Steam art | `SteamArt/` (~15 MB LFS, pushed): delete only if Wes says so | `SteamArt/README.md` | Oct 13 |

## Answered 2026-10-17 (knowledge consolidation; Wes's words)

| # | Question | Wes | What it means now |
|---|---|---|---|
| N1 | CLAUDE.md said "after every successful compile: git add -A && commit && push"; the STOP table and global Law 6 said commit only when asked | "commit always, more backups and documentation is better, don't ask anymore just be diligent about documentation" | Commit (and push) after every completed, verified unit without asking; keep docs current in the same commit. In the shared tree stage explicit paths, never `git add -A`, never another agent's files. Overrides global Law 6 for MO57. |
| N2 | "Planned Plugins: Oceanology, Ultra Dynamic Weather" | "installed" | Both are installed and in use (`Docs/TECHNICAL_REFERENCE.md` § Plugins). |
| N3 | PowerShell `Get-Content` recipes vs "never round-trip file content through PowerShell" | "powershell as backup since the tools aren't bulletproof, whatever keeps work moving along" | bash / `ue.py` first; PowerShell recipes kept as the fallback in `Docs/TOOLING.md`; still never WRITE file content through PowerShell. |
| N4 | "source build" (global CLAUDE.md, AGENTS.md, bible) vs "launcher install" | "we're currently on 5.8.3" | UE 5.8.3 launcher install (CL 58210709, `InstalledBuild.txt`); engine fixes are staged, not applied. |
| N5 | "prompt the user to close the editor" vs `ue.py cycle` closing it | "go ahead and close when you need unless i explicitly say otherwise" | Close the editor when a build needs it (`ue.py editor stop`: graceful, refuses with unsaved packages); never force-kill by name; announce the window in the coop log. |
| N6 | Which DataTable authoring path is canonical (CSV / `ue.py rows set` / JSON skill) | "we need a master progression document or something, i think we started off a bit blind and kept having to adjust, we're much further along now so we need a proper roadmap that dictates design" | `Docs/PROGRESSION_ROADMAP.md` is the design authority; DataTables implement it. Mechanics (Claude's call, noted): live tables are edited with `ue.py rows set` (readback-verified); the CSV / sqlite utility stays for tables that still have a live CSV; the `*_JSON.json` mirrors are stale and the `ue-datatable-json` skill says so. |
| N7 | Codex's `Docs/agent/` ownership vs one doc per system | "as proposed" | `Docs/agent/` stays Codex's working state; finished knowledge lands in the system doc (`Docs/INDEX.md`). |
| N8 | Rotating the append-only coop log | "i think it's safe to just drop an fyi in the collab you guys can fight it out without involving me" | FYI posted (CL-0048); old entries move verbatim to `Docs/Archive/coop/`. |
| N9 | PATCH_NOTES.md frozen since June 30 | "so long as you can extract steam update patch notes from git commits we don't need it, might just need to note when the last steam patch was every time i ask for steam patch notes so you have a date/time though" | Docs/Archive/PATCH_NOTES.md archived; Steam patch notes are generated from `git log` since the last recorded Steam patch, and each request records its date / time / commit in `Docs/STEAM_RELEASES.md`. |

## Decisions, newest first

### 2026-10-26
- **Body shape: a milder thin end, build sliders, muscle** (Wes: "thin is too thin in that pic, dial it back about 10%" / "set the low end
  to maybe 10% of the current range"; "can we adjust any body parameters independently? breasts, butt, waist, etc? and can we blend
  between the fat levels?"; "are there any morphs for muscle mass?"): the thinnest body is 90 % of the earlier one (waist 59.8 cm, was
  56.8), still at 8 % body fat; bust, hips and butt, waist and thighs are build sliders (-1..1) chosen at character creation; muscle
  follows the simulated `MuscleMass` (20 / 30 / 40 kg). Weights are continuous in body fat and muscle, so the body changes as slowly as
  the metabolism does; the half-way bodies measure within 0.3 cm of a straight blend, so no in-between shapes. Built in MO_Tools
  (penpro/MO_Tools 92cc982); in game: BACKLOG N-226.
- **The MO_Tools lab plays** (Wes: "give me sliders in a menu in the mo toolkit project ... three levels"): L_Menu -> Characters with
  the slider panel. Voxel / PCG look-tuning goes in its own lab project (Wes: "maybe that should be it's own project???"): the Voxel
  plugin would weigh down the character lab.
- **F41: the Animals Full Pack** (Wes: "i added the animal pack to the project"): PROTOFACTOR's ANIMALS FULL PACK at
  `Content/AnimalsFullPack/` (30 species, 2.1 GB), local only like the other large packs (gitignored; each machine copies it, nothing is
  edited inside it). Wiring the land species is DARKTOWER's branch `darktower/animals` (request
  `D:\ClaudeMeetClaude\inbox-for-darktower\2026-10-26-animals-full-pack.md`). F41 stays open for a horse alone.
- **F40: leave it as is** (Wes: "Leave it as is for now then", after asking whether a source engine means compiling the engine every build):
  hair physics stays within 5 km of the origin (F39 D); the drafted Epic report waits for Wes. Removed from the open forks.
- **Creative worlds build and craft instantly** (Wes: "in creative mode make it so building happens instantly, no build time etc"; "also
  add crafting items to creative mode", read as the same for crafting): `FMOWorldRules::IsInstant`; the Principle 11 exception (CLAUDE.md).
- **A building mode beside Explore and Terraform** (Wes: "make a 'building' mode [next to] the terraforming and exploration mode where we can
  see the build ghosts better"): `Docs/Building.md`. Claude's small calls, noted: B is the mode's key (enter + menu; B with the menu open
  leaves; B with it closed reopens it; B while placing cancels back to the menu); Back closes the menu and keeps the mode, Back with nothing
  open leaves it; T and B exclude each other; the plans keep the Oct 24 dimmer tint (by eye).
- **Climbing and vaulting** (Wes: "we need to implement climbing of cliffs / trees and we need a vault mechanic to hop / pull up on things"):
  `Docs/Movement_And_Traversal.md`. Claude's small calls, noted: Jump is the one verb (vault / hop up / pull-up / grab), crouch lets go and
  is used up by it, a grab from the ground needs the input pushing at the face, built things are not climbable, climbing costs 3 stamina / s.
- **The survivor is "femme"; "masc" is a coming-soon customization** (Wes: "make the existing character 'femme' and mark 'masc' as a
  customization coming soon"). Settles the creator side of F15 for now.
- **Tool effectiveness ladder** (Claude's small call, noted Oct 26, found because a stone sickle could not harvest): stone 0.6-0.75,
  copper 0.8, bronze 0.85, iron 1.0 (`PROGRESSION_ROADMAP.md` § Tool capabilities). Wes may retune any value.
- **The first crop is wild grain** (Claude's small call, Oct 26): the existing Wild Grain item sown on purpose, the pre-domestic step that
  needs no new seed item; the founder crops by biome wait on F44.
- **Farming, animal husbandry and more animals** (Wes: "let's get farming, animal husbandry, and some other animals going. We will need
  horses, pigs, cows, etc plus bears, rabbits, birds, chickens, I'm sure there's more"): in progress (BACKLOG).
- **F39 option B tried and blocked** (Wes: "Try F39 option B ... on its own branch, and measure it"): UnrealBuildTool refuses a project copy
  of the Groom plugin because engine plugins (Interchange, MetaHumanSDKEditor) depend on it -- a hard-coded rule. No branch was pushed (the
  copy cannot build); the patch is staged as `Docs/EnginePatches/0003-hair-simulation-relative-transform.patch`. Next step: fork F40.
- **F39 hair physics: D** (Wes: "Yes, build F39 option D: hair physics on near the world origin, off far out"). Built: `MO.Hair.Simulation 2`
  (default), a groom simulates while every coordinate is within `MO.Hair.SimulationRadiusKm` 5 of the origin (`Docs/UI.md` §3). The
  HairStrands precision patch stays staged for expeditions (`Docs/Engine_Patches.md` #3).
- **The hair's copper look is the scene, not a bug** (Wes: "the hair is super shiny and gold now?", "at a distance it looks really odd ...
  LOD?"): the LOD is still forced to 0 and the strands shade the same at any distance (measured in open sky; a "black past 10 km" reading
  was the survivor hovering inside a mountain). Whether the default brown should be darker: Wes, by eye (BACKLOG).
- **A new world starts at the world's origin** (Wes: "can we have the landfall script the picks the starting location shift the entire world
  to 0,0 so we're always starting at an origin point?", then "build the world-origin shift here: offset in FMOWorldGenParams, spawn at (0,0),
  saved per world, synced to co-op, old saves at 0, with the equivalence test"). Built: `Docs/World_Creation.md` §2 (the origin),
  `Docs/Terrain_Generation_Exploration.md` § World space. A world saved before keeps (0,0).
- **Precision far from the origin: shift the start, fix the rest case by case** (Wes asked how big worlds avoid it). UE5's large world
  coordinates already make positions doubles; what jitters is float math inside systems (the hair simulation, Niagara, physics, shaders
  on world position). A floating origin (rebasing the world as the player moves) does not fit a listen-server co-op where players are far
  apart. So: the colony lives near (0,0), and a system that still jitters on a far expedition is fixed in that system (the foliage wind's
  precision fix, `Docs/PCG.md` §9, is the model).

### 2026-10-25
- **Hair physics off by default** (Claude, in place pending F39; numbered F38 at first, which the Oct 24 equippable-items fork already had): the simulated strands jitter far from the world origin -- an engine bug
  (`Docs/Engine_Patches.md` #3). Wes saw it stop with physics off ("they just stopped"); the bend-damping floor tried first was reverted.
- **Hair length is worth a customization option later** (Wes: "keep note of the stuff the makes the hair longer, that might be handy for
  more customization"): `Docs/UI.md` §3.
- **The creator camera must see the face** (Wes: "there needs to be a 'can see face' check or something, and that there isn't shit blocking
  the face"): BACKLOG N-213.
- **The creator offers the whole MetaHuman groom library** ("grab all the other hair styles from the metahumans pipeline and prepare them for
  use in the character customization screen"): 38 hair, 16 beards / goatees, 16 mustaches, from the 5.7 install's Core Data (no download).
  Claude, small calls (noted): beards and mustaches are offered whatever the survivor (gender is unset, F15); "Bald" / "None" first in each
  picker; Randomize picks a hair style but takes facial hair off; eyebrows / eyelashes / peach fuzz not offered yet (the library has 18 / 6 / 1).
- **A click on the world must not close the creator; click-drag orbits the camera round the survivor** ("i'd like to be able to click drag
  the world to pivot the camera around the character to be able to look at the back/sides/etc"). Claude added mouse-wheel zoom (110-420 cm).
- **No bald gap while a style loads** ("it takes like 5-10 seconds for hair changes to take effect, until it updates they look bald"): the old
  groom stays on until the new one can draw.
- **Dark Ruins cliff compositions are off** (BR25-16; Wes's second verdict, after "a ball": "a disaster ... floating WAY above ground"). Rows
  disabled, assets kept; the single Nordic cliff ("these cliffs look pretty good") stays in the crags.
- **Western cliffs are a POI: bands of 5-10 faces in a line along the hillside** (BR25-19). New placer layout `bClusterLine`; row
  `WC_WesternCliffBand`. The rare single faces of the PCG species stay.
- **TRELLIS crags smaller (x0.4-0.8), 2.5x as many, two-sided** (BR25-15: Wes offered "close up the holes and just tile the textures" or "scale
  them down and make more of them in clusters": both, the holes by two-sided rather than a mesh repair).
- **The PN meadow flowers render classic, not Nanite** (BR23-10; partly reverses Oct 14's "palette foliage on Nanite", which answered Wes's
  "these little dry grass assets are LOD switching which means they're not nanite enabled"). Measured in PIE: under Nanite the flowers' own
  material writes wrong motion vectors, so motion blur and TSR smeared them ("a few of these little flowers are blurry", "look good here ... but
  weird and blurry in other places": the good ones were the native scatter's classic lupines). Their LOD steps cross-fade instead
  (MA_Flowers' Dithered LOD Transition). The grasses and every other palette mesh stay Nanite. If the flowers' LOD changes still show, the
  next step is the wind function's previous-frame position under Nanite, not Nanite off for more meshes.
- **Ground cover fades only right at the lens** (15-45 cm, was the foliage's 40-160 cm): grass and flowers under the low third-person camera
  were a moving screen door ("dotted" blades). Shrubs, ferns and trees keep 40-160 cm (the reason the fade exists: never look through leaves).
- **Cliffs are bespoke compositions, not voxel stamps** (closes F9). Wes: "the huge cliffs we just imported look pretty good but we need to
  have more of them and we need stuff spawned on top of them ... there's a feature of voxel plugin to make voxelized meshes, would those big
  ass cliffs work well as actual voxel meshes that get added as voxel stamps? ... either way we probably need to combine some assets to make a
  more bespoke cliff setup like what we have going on with the electric dreams setup, meshes pcg spawned on meshes and all that". Built:
  the Dark Ruins sample's own artist assemblies (Mountain_01, TallPath_01) as world-feature compositions with rock clusters at the foot and
  ground cover on their tops (`Docs/World_Features_Architecture.md` "As built", `Docs/DarkRuins_Assets.md`). Voxel mesh stamps were NOT used
  for the faces: at our 50 cm voxels the scan detail is lost and the face would wear the terrain material, and a volume stamp sends every
  chunk it touches through the slower volume pass (TRAPS: the LOD-range holes); where a composition needs the ground to meet it, the
  existing `AuthoredGround` raises the terrain (a 2D height edit in the terrain node).
- **The TRELLIS formations get a tiling granite detail layer and grow to x0.8-1.6** (Wes: "the large rocks from trellis have shitty
  resolution on the texture ... they're also not very large compared to what i thought"): `Content/Python/rock_detail_layer.py`, `Docs/PCG.md`.

### 2026-10-24
- **Highland crags, far landmarks, granite summits** (Wes: "let's implement a 'sub biome' in the higher up areas where we have the cliffs made up of those huge rock formations ... also the largest rocks, and the cliff faces, let's have them render much further out ... the rocky highlands material is very red ... shift that to something more granite looking"). Built: `bLandmark` + the landmark tier (GRID256, ~2.5 km), `MaterialOverride`, the crags (PCG.md §5), AlpineRock on `VST_Cliff`. Claude, small calls (noted): "sub biome" = a shared clump field inside the three highland biomes, not a new biome row; the red was AlpineRock's quarry rock (RockyHighland's ground is brown-green); GRID256 not GRID512 (the 2 km grid is held past the reveal, so boulders by the spawn would pop in); the Granite variant on the formations; "the largest rocks" = the xlarge tier, the large tier stays near.
- **The clone egg lies beside the first survivor; the giant rocks halved** (Wes: "the egg should be near where the first player starts, i was wrong about the rock features they look kind of bad at that size lets reduce them to about half at the larger end"). Built: `UMOStartSiteSubsystem` (World_Creation.md §2) and the xlarge tier x4-8 (PCG.md §5). Claude, small calls (noted): a NEW world's first survivor only (a load restores the saved site, a respawn gets none, older saves none); 3.3 m behind-right of them, the opening toward them, the hatch between; placed once the world is ready; set on the lowest ground under each prop; "half at the larger end" read as the xlarge tier halved, the large tier (x3-6) kept.
- **Two machines: separate clones and branches** (Wes: "what's the best way to divide the load? i know you can't both work on code at the same time very well, or can you? that seems risky"; then "Yes, set DARKTOWER up as a real git clone: copy .git over, have its Claude work on its own branch"). The risk is two writers in one folder and two machines on one binary asset, not parallel branches. Rules: CLAUDE.md § DARKTOWER. Claude, small calls (noted): DataTables stay DARKSIDE-only (DARKTOWER sends row patches as JSON); DARKTOWER's first lane is N-209 (a C++-only fix); its `.git` came over the LAN (tar over ssh, no GitHub LFS download); every tool takes `ue.py`'s engine root; both machines move to UE 5.8.4 (Wes updates this PC in the Launcher).
- **Buildings go up piece by piece; the fire shows its fuel** (Wes: "make a different fireplace asset that has a ring of stones from our existing assets that are not linked ... while it's building it will switch each stone to it's proper texture as the building timer fills up ... barely full is one stick visible, compeltely full is 20 sticks visible ... after you burnt it it turns to charred looking sticks ... i want to do the same thing with the stick walls as well"). Built: `StagedBuildParts` / `FuelStickParts` (Building.md §3), `BP_Campfire_StoneRing` now placed by BuildCampfire, the four stick Blueprints staged. Claude, small calls (noted): the stones stay ghosted until reached (the plan stays readable), the sticks are hidden until reached; wood laid on a cold fire stays fresh until lit; the charred count is not saved; the placement preview keeps the old campfire mesh.
- **F38, equippable items with no slot: A** (Wes: "F38: go with A -- tools, weapons and shields Held, the tunic Chest, the pouch / waterskin / quiver / beads not equippable, and the validator rejects equippable-with-no-slot"). DT_Items: 23 rows Held (Adze01, Atlatl01, BowDrill01, BronzeShield01, BronzeSword01, CompositeBow01, CrudeChopper01, CrudeClub01, DropSpindle01, FireDrill01, FishSpear01, HaftedAxe01, IronShield01, IronSword01, PolishedAxe01, SimpleBow01, SinewBackedBow01, SmithHammer01, SmithTongs01, SteelSword01, StoneHoe01, StoneScraper01, StoneSickle01), ClothTunic01 Chest, SimplePouch01 / WaterSkin01 / Quiver01 / GlassBeads01 not equippable. The validator: `FMOItemDefinitionRow::IsDataValid` (the editor's data validation flags DT_Items on save) and test `Data.EquippableItemsHaveASlot` (failed on the old rows with all 28 names, passes now). Their held grip fits are BACKLOG N-207.

Wes's reports from his PIE check of round 2 (`Docs/BugReports_2026-10-24.md`). The calls in them:
- **Cliff slabs lie on the slope** ("maybe try laying them down and alligning them to the slope?"): done as asked; closed-geometry cliff
  variants are his art ("i may need to create some 'closed' geometry version of these cliffs").
- **Open plates need a flat spot** ("placement math for determining flatness of a spot versus the size of these open geometry assets").
- **Bigger rocks, buried by size** ("2-20 time larger ... 10-25% of mesh bounds deep"): size tiers, a quarter of the height buried.
- **Enclosed light: a little unnaturally bright over a light switch.** Caves dim to 0.35 of the sky light instead of black.
- **Water movement sounds removed** until better ones are found.
- **Lakes almost still; shore wetness not metallic.**
- Claude, small calls (noted): the rock tiers use each biome's 3 densest closed rocks at a tenth / a fiftieth the density (the first
  draft's fifth / twenty-fifth put two house-sized boulders on every RockyHighland hectare); the high height bands widen with height (190 m
  at 950 m) rather than everywhere; ground displacement keeps rock's relief and flattens soil, litter, grass and sand to 0.3-0.5.

### 2026-10-23
Wes's votes on the first review page (notes word for word: `Docs/BugReports_2026-10-23.md` § Wes's votes). The calls in them:
- **Starting biomes: no high-elevation starts.** "let's gate the higher elevation biomes, so no alpine heath, or alpine rock or highlands";
  "shore strand and nordic strand should face the water" (N-203).
- **Electric Dreams POIs stand in the forest, not a clearing:** "have the trees go right up to the poi, probably just use the electric
  dreams assets to give more ground cover, plus more small assemblies leading up to them" (BR23-3).
- **ED assemblies sink by their slope:** "maybe another meter on the larger ones ... an 'average slope' over the area it's being pushed down
  into ... so the lower side of the slope doesn't expose the bottom" (BR23-4).
- **Rainforest canopy between x1 and x3 wide:** "somewhere between the new and the old because now it's BLACK"; whether Lumen / exposure
  should lighten it is checked first (N-201, N-202).
- **Height bands:** the rock-to-below edge gets the snow / rock edge's noise (BR23-19).
- Thumbs up: the New Game panel, the dry-scrub size, the still lake look.
- Claude, small calls in the batch that answered them (noted):
  - "Lowland" for a start = the band's floor within 50 m of the sea and no minimum slope (`FMOWorldCreation::IsStartingBand`): the rule
    names Wes's three and keeps out any later summit biome; any start with the sea within 300 m faces it, not only the strands.
  - "Average slope" made exact: a spire's base pieces rest on the lowest ground under their own footprint, an ED assembly sinks by the drop to
    the lowest ground round its solid core; "another meter on the larger ones" = spire `EmbedCm` 125 -> 150 (1.2 m at 0.8x, 6 m at 4x) and the
    cliff / embankment assemblies' `SinkCm` + 100 cm.
  - Trees up to a POI: only low growth out to 1.25 core radii (a crown just outside the core stands in the rock's overhang), forest beyond.
  - The "black alder" Wes saw floating in ElectricForest are hornbeams (their resource row is BlackAlder, so the game names them that):
    both earlier burial changes went to the real alders and never reached them. They get the alders' 50 cm, and every upright tree with a
    measured `BaseRadiusCm` sinks by the slope's drop across its base (the alders and hornbeams measured so far).
  - Rainforest canopy x2 wide (Wes's "between"), height x1.25 kept.

### 2026-10-22
- **Wes: per-world creation settings** -- "implement a suite of 'global multipliers' for terraforming, crafting, gatherings, inspecting, etc
  so we can configure those on a per world basis and add those to the world generation menu, along with some knobs to adjust stuff like biome
  size, overall world height, etc, whatever makes sense, and then a 'starting biome' selector". Built: `Docs/World_Creation.md` (shape: biome
  size, world height, mountains, lakes, caves, vegetation; pace: gathering, crafting, building, digging, study times, skill learning; the
  starting-biome cards). Claude, small calls (noted): a pace never goes below 0.1 of real time and never reaches zero (Principle 11's "never
  collapse a simulated action to zero"; the knob is chosen with the world, not a mid-game skip); every knob defaults to 1 = the game as
  authored; the shape ranges are capped where the generator stays sane (biome size 0.5-2.5, height 0.5-1.5, vegetation 0.25-1.5); skill
  learning is a rate (more = faster) where the others are times.
- **Wes: look checks come as screenshots** -- "from now on if there are things that are a 'have wes look at it' see if you can just collect
  a few screen shots and i can review them and give thumbs up/thumbs down etc". Each check-by-eye item gets before / after shots
  (`Tools/review_shots.py`, same spot and camera) on a review page with a thumbs vote; motion that a still cannot show stays a text row.
- **Wes: the rainforest's trees** -- "try increasing the x,y of the trees by about 200% and the height by about 25%, so bigger FATTER trees
  with more canopy cover". Claude, small call (noted): read as +200 % wide (x3) and +25 % tall: the nine Black Alder forest meshes, their far
  stand-ins and canopy proxies; saplings unchanged (new species field `WidthScale`). x2 wide is one data row away if x3 reads too heavy.

### 2026-10-20
- Claude, small call (noted): **a wolf calls its pack when it starts a hunt, not only when hurt** (N-096): a predator that perceives a new
  quarry -- a deer, a person -- brings the wolves of its kind within 15 m. Wolves do hunt together, but it makes a pack converge on a
  player it sees; if that is too harsh before players have spears and fire, keep only the call when a wolf is hurt
  (`AMOCreatureController` perception path, one `if`).
- Claude, small call (noted): **the colony medic** (N-061) treats bleeding at 0.2 mL/s or more (`MedicBleedThresholdMlPerSecond`), runs first
  in the upkeep tick (a bleeding colonist before standing orders; it takes idle villagers only, nobody is pulled off a job), and picks the
  doable treatment of most total benefit (bleed + infection + healing reductions) so a dressing beats a tourniquet ("last resort" in its
  own row); a tourniquet is still used when it is all there is. Players in the settlement are patients too. `Docs/AI_And_Colony.md` section 2.
- Claude, small call (noted): **rain puts a carried torch out** (N-056): nothing up to rain 0.4 at the flame (a resin torch survives light
  rain), then a constant hazard reaching a 2-minute mean in the heaviest rain (UDW "Rain", 0.7: ~4 min). No measurement was found: the numbers
  are profile fields (`RainSafeIntensity`, `MeanSecondsInHeaviestRain`) to tune, 0 = a covered lantern. `Docs/Light_Sources.md`.
- Claude, small call (noted): **a killed wolf leaves a carcass** (`DT_Carcasses` "Wolf", `DT_Creatures` Wolf01's carcass id): the deer's parts and
  items at a ~40 kg animal's yields (meat 11 against the doe's 20, sinew 3, bones 4 + skull, hide, brain, bladder; cuts ~0.7 x the deer's time).
  Before, a dead wolf ragdolled and left nothing (and the log said "using legacy loot system"). No new items: `Docs/PROGRESSION_ROADMAP.md` §5.
- **F33 A, Wes:** "keep the 20% odds but make a failed drill teach more (about 25 XP), so practice helps by the 4th try." A failed attempt teaches
  25 Firemaking XP, a lit one 40 (was 8 / 20). Firemaking's first level is 60 XP (live DT_Skills), so three failures earn +6 % for the 4th try
  (before: eight failures, 24 minutes of drilling). Every attempt shows the drilling player a skill toast ("Firemaking +25 XP"), so the practice is
  seen. `Docs/Crafting_And_Progression.md` §2.
- Claude, under Principle 11 / 7 (small call, noted): **every yield of a standing resource node is finite.** An item not listed in
  `InitialCountByItem` takes `DefaultYieldRange` 2-6 units per node (it was unlimited: a dead tuft gave tinder forever); counts roll per item
  on first take. Respawn timing stays F3's. `Docs/Crafting_And_Progression.md` §3.

### 2026-10-19
- Morning, Wes's answers to the overnight forks ("Yes: F30 A (turn on built cave entrances in limestone), F31 A (building skill slows the work
  and wastes material, never gates), F32 A (I'll buy only Animal Pelts Vol. 1 now; a rigged bow pack later). Build F30 and F31."):
  - **F30 A, caves:** built entrances ON in karst -- `CaveEntranceChance` 0.7 per 500 m cell, generator **v12** (a walk-in mouth in ~25 % of
    1 km tiles of cave country, was 0 %). Saves from v11 load with the new caves (GENERATOR CHANGED, informational: Wes Oct 13, no migration).
    `Docs/Caves_And_Geology.md` §1.
  - **F31 A, building skill:** never a gate. Below a building's level the work takes longer (x2 with no skill) and spoils part of its
    material along the way (a quarter with none), linearly in the shortfall (`FMOBuildSkillPenalty`); a spoiled unit is replaced from the
    builder's sources or the build pauses for it. The menu shows the time at your skill and "(slower, wastes some material)". `Docs/Building.md` §2.
  - **F32 A, Fab:** Wes buys Animal Pelts Vol. 1 now; one rigged bow pack later, when bows animate; everything else goes to the TRELLIS.2 queue.
- **F2 interim, Wes:** "Ship single node now" -- the core runs a heat balance over a real thermal mass (`AdvanceCoreTemperature`, N-178):
  hours of cold, not minutes; core + shell, clo values and shivering glucose stay open in F2. `Docs/Medical_And_Survival.md` §4.
- Claude, under Principle 11 (small calls, noted): **the weather reaches the body at real rates.** Ground: soaked by 10 min of rain, dry
  after ~2 h of sun, snow-covered by 45 min of snowfall, melting only above freezing (BR18-16; UDW's own 20 s / 90 s demo rates are not
  used). Clothing (N-176): a downpour soaks it in 5 min, "Rain" in ~7, it dries on the body in ~2.5 h and by a fire in ~40 min (was
  10 s / 3 min / 18 s). The feels-like temperature's wet-cold and wind-chill terms now actually run (N-175: every pawn read as indoors), so
  rain and wind make a pawn cold: in the open in "Rain" at 11 C it feels 7.7 C. Shelter and fire matter more; that is the design.

### 2026-10-18
- Evening, Wes's answers to the waiting list:
  - **Steam smoothness (N-161 / N-164, F25):** "yes steam seems to be smooth even on low end computer" -> F25 closed with no change
    (the stock forced update stays; the local 124 cm snap after a 500 ms joiner hitch is not what players see); reopen on a report.
  - **Caves (BR18-13):** "yes caves need to be decently common since they're ALSO hard to spot" -> N-170.
  - **Hair buns under water (N-053):** "i originally thought one of the fixes for the processing was breaking the lakes but that was a
    separate issue we will revisit" -> deferred.
  - **Fab purchases (N-100):** "reevaluate these since i've moved a lot of stuff to the comfyui 3d modeling pipeline".
  - **CS399 weekly progress (N-151):** "disregard this".
  - **Law 6 (N-153):** "we got rid of this" (the global files still carried it when checked: N-153).
- ~22:00: **"the deer should "hear" a predator/player coming up behind it if they're not stealthy also, and they should run a long ways
  if they get damaged (like by a wolf) ... right now a player can SPRINT up to the deer from behind and punch it to kill it"** -> creature
  hearing works (it never had: NoTeam vs NoTeam is Friendly to the hearing sense), footfalls are as loud as the pawn's real speed
  (crouched x0.5), a wounded animal flees whoever hurt it for 45 s, and the fist is back to its class default (BP_MOMetaHuman_Female had
  BaseDamage 500 from a Feb debug session). `Docs/AI_And_Colony.md` §1.
- ~22:00: **"a wolf should lose interest after a while of chasing the deer, maybe a set distance from "the den" (spawn point)"** -> a
  predator gives up a chase 200 m from where it spawned or after 20 s without closing in, then ignores that quarry for 2 min.
- ~22:00: **"we need a wooden spear and a stone tip spear and bronze and iron, but wooden and stone are the first ones we need"** -> the
  items, recipes and DT_Weapons profiles already existed (WoodenSpear01 / FireHardenedSpear01 / FlintSpear01 / BronzeSpear01 /
  IronSpear01); what was missing is that a held weapon never reached combat: the right hand's weapon item is now the main-hand weapon,
  the five spear rows in DT_Items are flagged as weapons (`bIsWeapon`, `weaponProfileId`, slot Held: they were not equippable to a
  hand), and ArmorPenetration no longer cuts unarmored damage (a wooden spear did 1). Bronze and iron spears still need meshes / icons.
- Small calls (Claude, Oct 18; Wes may override): the fist stays the class default (5 Blunt = ~4 HP off a 200 HP torso per blow; no fist
  kills a deer); leash 200 m, no-gain 20 s, give-up memory 120 s, wounded flight 45 s (all `FMOCreatureDefinitionRow` fields); the deer's
  20 m hearing range is unchanged (a sprint is heard at 20 m, a walk at 6 m, a crouched creep at ~1.5 m).

### 2026-10-17
- ~21:00: **"F6 is resolved, i tested weather and time updates to the client and those are great"** -> F6 closed: native UDS/UDW
  replication plus the MO clock sync carry weather and time to clients; no client-side apply in `BP_WeatherBridge` is needed
  (nothing weather / clock related changed after his test: no commits since Oct 15).
- ~21:00: **"make sure the client joining has the same world building delay as the host with the status bar"** -> a joiner
  now enters behind the same loading overlay and readiness hold as the host (`Docs/Coop_And_Networking.md` §3).
- ~19:00: **"i'd rather not pay $150 subscription if i can just get decently good results locally"** -> item models come from
  image-to-3D run locally: ComfyUI's native TRELLIS.2 nodes (MIT; Comfy says the outputs are usable commercially). Not the original
  microsoft/TRELLIS.2 repo (needs nvdiffrast: NVIDIA licence, non-commercial only). Not Hunyuan3D 2.1 (outputs barred in the EU,
  UK and South Korea, which a worldwide Steam release can't meet). Wes ran the first ten himself; the import recipe is
  `Docs/Item_Art_Pipeline.md` section 8.
- ~16:30: **"F24: go with A, one save per world. The in-game Save writes to the world's own slot; drop Save As and loading other slots of
  the same world; the main menu lists worlds, not snapshots."** -> the in-game Save button saves this world to its own slot at once
  (toast "World saved"; no slot list, no Save As, no overwrite confirm). `UMOSystemMenuUIController::HandleSaveRequested` ignores any
  slot name a panel passes, so no UI path can write a second save of a world. The in-game Load list hides the world you are in
  (`UMOSaveSlotListPanel::SetHiddenSlot`), and `HandleLoadRequested` refuses it. A world's identity is its one slot: a new game's
  `World_<time>`, or `AutoSave_<map>_<time>` when a world started without one. Old saves are not kept (Oct 13), so the main menu's list
  is one entry per world. The shared save step is now `SaveThisWorld` (`EMOWorldSaveResult`). The developer verbs `MO.Save.SaveAs` /
  `MO.Save.LoadFrom` stay, developer-only (dev mode). Follow-up: store an explicit WorldId in the save (BACKLOG N-158).
- ~15:30: **"pause policy, no pause, ever... if they want to pause they can back out to the main menu"** and **"let's go ahead and get rid of the save confirm and just force the save... i don't want people to be able to 'save-scum'"** -> every way out of a world saves first, with no confirm and no opt-out: Exit to Main Menu, Exit Game, loading another save, the death recap's Return, and the window close (X / Alt+F4).
  It is one step, `UMOSystemMenuUIController::SaveThisWorld` (named `SaveWorldBeforeLeaving` until the F24 change) ->
  `UMOPersistenceSubsystem::SaveToCurrentSlot`. Only the host or a
  standalone game saves; a client never writes the host's world. Only a FAILED save shows a dialog ("Could Not Save -- Leave Anyway / Stay")
  so progress is never lost silently. Saving at the death recap keeps a death permanent (deceased pawns stay deceased on load).
  `Docs/PAUSE_POLICY.md` now says "back out to the main menu". Testing it found a dead end: loading the save made at a death left a sky
  camera with no pawn. The load now raises `AMOGameMode::OnPlayerNeedsNextLife`, and the UI runs the recap's own `ContinueAfterDeath`
  (a respawn ~5 mi from the recorded death spot). The manual-slot route to an older save was closed by F24 (entry above).
- ~04:00: **"discovery should be locked behind 'inspect' if nothing else opens up to it, somewhere in the clay tech line humans would discover
  leftover metals etc, just make it realistic"** -> 46 circular discovery knowledges got precursor inspect grants (level 1, XP 100); metal through
  the clay line (PaintPotteryMalachite -> CopperPrill01 35 % -> inspect = Smelting); MakeBellows needs Smelting; SmeltCopperOre gives a 10 %
  IronBloom01 (inspect = IronSmelting); ValidateData 126 / 136 reachable (was 55). Commit d6a3af69. Rule: a new discovery gets a realistic
  inspect precursor, never only the recipe's own output.
- ~04:00: **"yes, different times for different rocks/minerals"** -> `FMOGeologyYield::HardnessFor` (earth 1, coal 1.5, coal measures 3, limestone 5,
  granite / basalt 40; veins galena 3, hematite 8, chert 10, copper 15, cassiterite 40) scales the player dig and the survivors' excavation; the dig
  slab follows the ground. Commit 37a3d694.
- ~04:00: **"leave night brightness alone for now"** -> UDS night exposure unchanged (numbers for later in `Docs/Light_Sources.md`).
- ~00:00: **"caves only in limestone, and mining yields from the geology ... they should be able to collect pretty much any ore, the actual hurdle
  with iron is having fire hot enough, so smelting and forging technology -- do some research into the actual histories of how technology actually
  advanced and how we got there"** -> generator v11 karst gate, `FMOGeologyYield` (one geology -> item map for both dig paths), outcrops,
  `IronOreDeposit` with a stone pick, `Docs/Technology_History.md`. Commit ba37d659.
- Caves must not reach sea level (the ocean and the underwater effect inside) or sit near shores ("they'd immediately be below water table"); "PCG
  inside caves, rocks, rocky materials on walls and ceilings, rock and dirt floors, different ores ... check what is actually realistic ... I still
  want realism" -> v10 water gate (f92ebebe), geology, MO Cave Spawner.
- Night: "keep working with codex for the next 12 hours" -> carried light (torch, eye adaptation, cave darkness).
- Consolidation answers N1-N9 (table above).

### 2026-10-16
- Answering the Oct 16 playtest calls (`Docs/Archive/BugReports_2026-10-16.md`):
  - **Electric Dreams is broken: "fix it, I don't care what it really changes"** (per-site seed for Blueprint assemblies: every ED assembly's look may change).
  - **Eating goes on a timer** -> `FMOItemActions::EatSeconds` (2 g/s, floor 4 s) on the timed-action component (a4207587).
  - **Building ghosts stay pale white** (PlannedColor / PlannedGlow as shipped).
  - **#15 / #19 cliff + embankment assemblies: "just try different things until it works well every time"** -> iterate and measure, no need to ask.
  - **#18 caves: "I don't care about old saves"** -> generator v8/v9 approved (smaller, near-horizontal galleries, entrances on slopes).
  - **"aim assist is good; pawns should punch in the direction of the camera"** -> arc around the swing, `BeginFacingSwing`, melee aim assist 20 deg.
- Approved the hair engine patch, but the engine is a launcher install (can't be patched); `r.Water.SingleLayer.DepthPrepass=0` restores hair depth
  but Wes **rejected it after watching it live** (Oceanology water loses its look). Creator depth of field off until Epic fixes it (0573ab31).
- **Audio: silent when unfocused** (`UnfocusedVolumeMultiplier=0.0`, declared in DefaultEngine.ini). Wes asked for web research before any engine
  change -> XAudio2 backend instead of an engine patch.
- Alder burial +25 cm `SinkMinCm` on Forest_ / Field_ alders.

### 2026-10-15
- **Survival pacing:** start fully fed / watered / rested; needs become bothersome after ~30 min -> warning moodles at 30 / 45 / 60 game-min, decay on
  the world clock (2d31e0c2). The metabolism single source (A) was NOT chosen or rejected (F1).
- **The starting kit is test-only** ("will ultimately be nothing"): design progression from bare hands.
- **Every world thing gets an action; plain cover (grass) defaults to Forage.**
- **Character creator, in the world while it loads** (built: frames the real pawn; skin / hair melanin / eye presets real; hair style, face, body "coming soon"; gender unset -> F15).
- **Possession menu shows occupancy** ("Current survivor" / "Played by <Steam name>").
- **Progression: "take a fresh look"** (not just options C+A).
- Drink from lakes / rivers / the sea realistically, illness from untreated water, a first-hours path to boiled water ("check online for actual primitive
  survival methods ... instead of game logic") -> `Docs/Water_And_Illness.md` (9f897c13, dcc1f3a9, 32a87e46).
- "keep working on the progression ... making the environment more interactable" -> progression passes 1-4.

### 2026-10-14
- **"build entire pcg POIs that have all these things self contained so they're very fine tuned and then spawn the larger POIs ... rather than just
  spawning a bunch of one off instances"**; Electric Dreams: "the big rock features and vines", the LIVE system ("if there's a baked version, that's
  not the one we're looking at") -> DT_WorldFeatures + placer + feature actor. Later: "drop down another half meter, spread the smaller base cliffs,
  the whole outcropping about 4 times larger, collision is fucked up, i fall through them" -> 4x ED_RockSpire, MO Conform To Ground.
- **"the biomes should have a lot more blending"** -> `SurfaceClimateBlend` 0.03 -> 0.12 + 3-octave breakup.
- **"prioritize getting the console stuff running in the shipped version or just build a 'dev mode' button in options so we just have a menu with
  all the MO. console stuff"** -> both (FMODevMode, devmenu).
- **"add to the bug report an 'item looked at' so it can narrow down what asset is causing issues"** -> LookAt rows.
- **"I want more ground coverage in general too, more trees, more bushes, etc."** -> tier caps, clump fix, plan; Wes's #6: rainforest "VERY dense" at ~72 trees/ha -> don't push past ~110.
- **"merge the duplicate item rows"** -> b0753b0a.
- **Item art:** Wes sources realistic assets over time; icons are rebuilt FROM those meshes; do NOT propose generated art (Wan2GP is bad at icons;
  ChatGPT / paid APIs not wanted). Wanted later: a 3D item preview in the inspection window (built Oct 14, 44fb4416). Free Fab downloads are authorized; Wes's library holds
  the Megascans claimed by Oct 2024 whatever the listed price.
- Game level VoxelWorld QualityExponent 1.1 (was 0.82) for mountain holes.

### 2026-10-13
- **12-hour directive:** far trees performant AND good looking ("In Voxel Plugin 1.2 I spawned ~200k trees from the Brushify forest pack with no real
  perf problem even on a 1080, so there SHOULDN'T be a reason we can't"); ground texture TILING fixed by "tessellation or whatever clever tricks";
  a distant tree-covered mountain must not show the bright MuddyLeaves surface; snow for peaks. The Steam art "looks like shit because the scenes
  captured are all fucky looking with bare materials on the terrain and weird ground spawn stuff all over" -> Wes takes the screenshots himself
  (F23). **Permission for that directive:** commit AND push, and download assets from public CC0 sources (ambientCG, Poly Haven), stating what was
  downloaded; no paid APIs.
- **"co-op build for sure needs to be server/host driven"** -> 757e47c1.
- **Old worlds after a generator change: "we don't care about any old saves or preserving them"** -> no migration; the load-time warning is information only.
- The starting kit is "just a test item" (it no longer completes gather tutorials).
- No caves up high -> `CaveMaxGroundCm` 750 m (generator v7, 60c666f9).
- "add to todo": alpine / snow variation (aspect or bigger noise) and caves half the diameter, more lateral -> aspect snow Oct 15; caves v9 Oct 16.

### 2026-10-12
- Overnight asks (all done that night; `Docs/Archive/Overnight_Polish_2026-10-12.md`): sound; mountains; rock planes at the terrain normal and buried;
  PCG samples caves; splash flicker; sand ripples; grass tiers + longer radius; far trees; a 1:1 Electric Dreams biome; Megascans biomes; UI input
  bugs; sapling collision.
- Terrain material asks: distant snow-capped dark mountains with distance material switching; fade + noise on every elevation / climate split
  (built Oct 13); several sand materials (open) -> "just add these to the to do lists".

### 2026-10-11
- **"im installing oceanology so let's switch to that ... you should be able to use most of the oceanology features for what we're doing"** -> Oceanology replaced Epic Water.
- Underwater wobble "really strong and looks pretty bad, make it 1/4".
- **"commit what you did and keep going ... set a goal of a complete polished AAA level game"** (`Docs/VISION.md`).
- **"yes, we should make a more cohesive ui schema, give me a prompt to feed into claude design ..."** -> the UI kit.
- Generator v6: "1000 really tall skinny mountains instead of larger masses with 1000 small steep features" -> larger masses with small steep features, sheer detail
  up close but never region-wide thin walls, nothing collapses at far LODs; "the height is good".
- Overnight: keep working, flesh out biomes, snapshot each, nothing weird (trees in water). Electric Dreams Env (`D:\UEProjects\ElectricDreamsEnv`) is the
  asset / PCG source to draw from (migrate selectively).
- Standing practice: design-level asks are logged as to-dos rather than built blind.

### 2026-10-10
- Generator v4: "very very flat, realistic but not fun" -> VerticalScale 2; the character always spawns on a coast looking across the water, **never on an
  island**; lakes above sea level ~1 per 5 km2; `weather` and `time` console popups that set the SERVER's weather / time; check ALL scatter for
  collision; a main-menu level with a slow flyby.
- Generator v5: mountains "many times higher" while lowland prairie stays flat; biomes ~half the size; land falls into water at the world edge; a rainforest;
  sand on sea-level shores; more Megascans for beaches. ("the sapling was not the problem" -> the Black Locust variants: `verify-the-asset` lesson.)

### 2026-10-09
- The initial tutorial text goes away while other menus are open and comes back when the menu closes.
- Unattended runs: "there's not much point in doing overnight sessions if I have to click accept every 20 seconds" -> `Tools/ov.bat` (fixed name; edit `Tools/ov_task.sh`).
- C++ terrain generator: terrain controlled from code, Minecraft-style biomes, less "basic noise", PCG samples the same biomes.
- Bug report form: "option A" (in-game form -> the existing crash endpoint as a CR1 bundle, no website change).
- Overnight plans approved with commit + push (Oct 8, Oct 9).

### 2026-10-08
- **"we have a whole weather bridge for UDS and all that, so use the bridge not the native UDS stuff"** -> every MO system goes through the bridge;
  native UDW replication stays as the transport (measured: Change Weather is server-only) -> F6 (closed Oct 17: works on clients).
- New games at 08:00 with clear skies; a `~` console popup; `starter` (50 sticks, 20 stones).
- A real two-account Steam session works (Wes verified; both characters on the terrain).

### 2026-10-05 / 09-22
- "UI work is tedious -- find a toolset so Claude does it, not screenshot-click loops" -> `ue.py ui` (UMG is scripted).
- "real steam integration" (App 1602810, "Metaverse: Origins").

### July 2026
- **July 13: terraform spoil is a designation + pawn-labor loop**, not a carried counter: dig / dump-fill / flatten zones (multiple), villagers automate,
  flatten = dig high + fill low under conservation of earth, a dump-destination popup (designated sites / fill inventory / fill nearby inventory).
  **Forks LOCKED:** sphere zones (center + radius) + settlement-scoped dispatch; 80 items / m3; the solo player digs manually. Earth materials authorized
  (Dirt01, Topsoil01, Gravel01, Mud01, Silt01).
- **July 8 directives:** terraforming realistic time (duration = earth volume x 30,000 s / m3: a house-flatten takes hours, a mine days, "for fucking
  ever"; 14e0d432); **offline catch-up capped at 4 h** for the clock AND crafting (was 7 days; vitals don't fast-forward; b5d7b44f) -- this closed H36;
  H23 per-harvester harvest context (9c7178c6).
- July 7: autonomous execution of the Fable 5 pipeline approved (gated stage cards).
- July 2: "Do not write off the MCP" -- it is complementary to the bridge (editor-side asset / Blueprint / DataTable authoring).

### Earlier (2026)
- 2026-03-28 (Master Plan): `UMOPersonalityComponent` decoupled (option A); the colony overview replaces / extends the possession menu on P; placeholder
  portraits until the art pass.
- 2026-03-18 (native-functionality audit): systems confirmed custom -- persistence / identity (GUID), medical simulation (no GAS), building (weighted
  build parts), FastArraySerializer, TSoftObjectPtr.
- Standing policy: **No pause, ever** (`Docs/PAUSE_POLICY.md`, enforced in `AMOPlayerController::SetPause`).
- **The live DT_Biomes is the palettes' source of truth** (Claude, 2026-10-25, N-204; a tooling call): changed only by targeted patch tools
  with readback; `Content/Python/biome_palettes.py` no longer applies (re-syncing a 700-line script after every live edit drifted within a day;
  its plan would have dropped 89 live entries). `Docs/PCG.md` § 5.
- **Dev saves are disposable** (Wes, 2026-10-25: "scrap my saves/autosaves every build, if i want save games i'll play them on steam"): `ue.py build`
  and `cycle` delete `Saved/SaveGames/*.sav` and `Saved/Autosaves*` before UBT runs (`scrap_dev_saves`, `--keep-saves` skips it). The packaged /
  Steam game's saves (`%LOCALAPPDATA%\MO57\Saved\SaveGames`) are never touched. Replaces "never touch Wes's saves (Harper_Wright-01, Test158, v1gate)".
- Standing rules for unattended runs: own test saves are `zz_*` and deleted; stop only
  processes you launched (recorded PID); local / free only; do not edit Wes's Blueprints (e.g. `BP_WeatherBridge`) or build a logged fork without his yes.

## Superseded

- "Prompt the user to close Unreal Editor before compiling" + "after every successful compile run `git add -A && git commit -m checkpoint && git push`"
  + "user will confirm compile success before git operations" (CLAUDE.md Workflow Rules) -> **superseded 2026-10-17 by N1 and N5** (commit always with
  explicit paths; close the editor when needed).
- "Git commits ONLY when explicitly asked" (user_preferences, CLAUDE.md STOP table, global Law 6) -> **superseded for MO57 by N1 (2026-10-17)**.
- "Survival pacing fork A/B/C" (Oct 12) -> pacing **decided Oct 15**; the single-source question remains as F1.
- "Hair engine patch needs Wes's yes" (Oct 11) -> **approved Oct 16**, blocked on a source engine; Wes waits for an Epic fix.
- "H36 offline crafting is a design fork" (PROJECT_STATUS, July 7) -> **decided July 8** (4 h cap).
- "Drinking water does not exist" (Handoff Oct 13 #2) -> **built Oct 15**.
- "H13 old-world policy is Wes's call" -> **decided Oct 13** (break).
