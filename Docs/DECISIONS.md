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
| F2 | Body temperature model | **core + shell heat balance** over a thermal mass (clo values, wet clothing, shivering burns glucose) vs single node vs keep. Today a 13 C lake gives hypothermia in ~40 s and light clothes never cool | `Docs/Thermal_Model_Reproduction.md` | Oct 11-13 |
| F3 | Do removed things regrow? | slow regrowth on game time (litterfall weeks, saplings years) vs permanent (today) | `Docs/PCG.md` §7 | Oct 13 |
| F4 | Performance target | choose the reference resolution / quality and the target hardware tier before tuning scalability (the 4090 floor is ~11 ms at Epic) | `Docs/Performance.md` §1 | Oct 11-13 |
| F5 | Shore reeds | a Fab / Megascans download vs Electric Dreams plants (Taro, ferns; no reed asset there) | `Docs/Water.md` §5 | Oct 11 |
| F7 | Building cancel / refund | queue path (full refund + world drops) vs ghost-menu path (skill-partial); one rule for both | `Docs/Building.md` | Jul 20 |
| F8 | Excavation stage 6 UI | the designation tool + "where do you want to dump the contents?" popup (designated sites / fill inventory / fill nearby inventory). Wes said on Oct 15 the earlier question wasn't clear: **re-ask plainly with a mock-up**. Recommended default flatten target: the zone's MEAN height | `Docs/Terraform_Excavation_Plan.md` | Jul 13 / Oct 14 |
| F9 | Real cliff walls | own composition along a steep contour (massive cliff meshes, vines, talus) vs generator crags dressed with the meshes | `Docs/World_Features_Architecture.md` | Oct 14 |
| F10 | Heat ladder values + missing kilns | author the drafted station / recipe temperatures (campfire 700, forge 1,300, bloomery 1,250; copper 1,100, iron bloom 1,200 ...) and add pit kiln (900) + updraft kiln (1,100) stations; waits also on Codex's fuel-to-temperature rule | `Docs/PROGRESSION_ROADMAP.md` §4 | Oct 17 |
| F11 | Engine patch #1 upstream | post the ready report (hair strands vs Single Layer Water depth) on the Epic forum + bug form (a public post) | `Docs/EnginePatches/0001-upstream-report.md` | Oct 16 |
| F12 | Reeds along narrow rivers | `ShoreFactor` samples one 25 m ring: rings at 5 / 12 / 25 m add reeds along rivers at ~3x shore-tier samples (load time + look change) | `Docs/PCG.md` | Oct 16 |
| F13 | Default clock time scale | real time (1.0, today: weather and day barely change in a session) vs faster (e.g. 24 = a day per hour) | `Docs/Weather_And_Time.md` §1 | Oct 10 |
| F14 | Downloads waiting for a yes | Epic's official "Made with Unreal Engine" logo (splash card); the UI kit's OFL fonts (Bitter, IBM Plex Sans / Mono); a tropical plant pack; beach packs (shells, dune grass, driftwood); more sand sets; Megascans beyond the migrated ED slice | `Docs/UI.md` §6, `Docs/PCG.md` | Oct 10-14 |
| F15 | Character gender | where gender comes from (the creator sets everything else) | `Docs/UI.md`, wes-calls-oct15 | Oct 15 |
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
- Standing rules for unattended runs: never touch Wes's saves (Harper_Wright-01, Test158, v1gate); own test saves are `zz_*` and deleted; stop only
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
