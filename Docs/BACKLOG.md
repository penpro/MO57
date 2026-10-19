# Backlog: all open work

> **What this is:** the ONE place for open work: every TODO, bug, playtest report, audit finding and polish item, with an id, a status,
> an owner, one line and a pointer to the detail. Built on 2026-10-17 from PROJECT_STATUS's audit tracker, the AAA polish backlog, both
> playtest trackers, Codex's handoff goals, the Deep Dive audit, the night report, the Fable 5 pipeline, Codex's audit state and ~40
> open items scattered through agent memory.
> **Statuses:** OPEN · IN PROGRESS · WAITING (on someone else) · BLOCKED · VERIFY (probably done: check against current code before closing)
> · DONE-date. **Owners:** Claude, Codex, Wes. DONE items older than a week move to `Docs/Archive/BACKLOG_DONE.md`.
> **Ids:** audit codes C/H/M/L are unchanged (code comments carry them as fix tags); Codex's Oct 13 handoff items H00-H26 are **G00-G26**
> here; playtest rows are **BR14-n / BR16-n**; the old polish backlog items are **P-n**; new items are **N-nnn**. Old ids stay in the
> Aliases column so a search for either finds the row. Forks for Wes are **F-n** (`Docs/DECISIONS.md`).
> **How to use:** pick work here; when you finish, set DONE-date and the commit; when you find work, add a row (next free N id).

## Wes: check by eye (look-and-feel results deferred to Wes, Oct 18; add a line, he ticks them off)

| What to look at | Where / how | Fixed looks like | From |
|---|---|---|---|
| Megascans tree wind far from the world origin | Wes's world (seed 670711536), several km from spawn; any Megascans tree in wind | leaves and branches move smoothly; no ~0.75 s move / 0.25 s freeze / snap cycle | N-166 / N-167 (4e349e37, 6d956730) |
| Survivor task menu | Recruit a survivor, open its task menu | solid dark panel, readable names / descriptions, clear Queue buttons | BR18-26 (a736a2df) |
| Bark container in the world | Drop one on the ground next to a stick | about 14 x 13 x 12 cm: a small, real 1 L folded-bark vessel (deliberate) | BR18-29 |
| Grass cleared on a building site | Place a campfire (or any building) in tall grass, host and joiner | a cleared circle around it on both screens, still clear after a reload; trees and bushes untouched | N-079 (Oct 18) |
| Build site menu's material sources | Right-click a placed ghost | Inventory / Nearby containers / Surrounding area labels sit left, next to their boxes | N-083 |
| Item icons with no checkerboard | Inventory with sticks and stones | clean icons on the dark slot, no grey checker pattern | N-075 |
| A joiner's death screen | Kill the joiner (a wolf, or a host punch) | the joiner's own screen: the body falls, "You have died" names the killer; Continue works | N-064 (Oct 18) |
| Forest footsteps (by ear) | Walk in a temperate or locust woodland | leaf-litter steps, not sand | BR18-8 |
| A survivor's failed job | Order a survivor to gather where there is nothing | a notice says why; its task menu says "Idle. Couldn't ..." | N-090 |

## Waiting on Wes (decisions; details in DECISIONS "Needs Wes")

F1 survival single source · F2 thermal model · F3 regrowth · F4 perf target · F5 reed source · F7 building refund ·
F8 excavation stage 6 UI · F9 cliff walls · F10 heat-ladder values + kilns · F11 post engine report · F12 river reeds · F13 clock speed ·
F14 downloads (UE logo, kit fonts, tropical / beach packs, sand sets, more Megascans) · F15 gender · F16 era play time · F17 chariot ·
F18 attempt-anything · F19 far-colonist ground · F20 eye adaptation speed · F21 bland far mountains · F22 springs / rain / hauling · F23 rejected Steam art ·
F26 how long studying an item takes · F27 cliff bands that read as a staircase · F28 teaching the bark container (a "Safe water" tutorial step) · F29 how a pawn sleeps (nobody can) · F30 how caves become common (built entrances).

## Collaboration dependencies and recent handoffs

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-001 | DONE-2026-10-18 | Codex | Published audit A-G foundations: equipment/hand swaps, inventory capacity/state/replication, authoritative crafting/cancel, exact material ownership/logistics, survivor craft reload, persisted survival meters and timed wound treatment. Shared build 45/45; six focused filters 131 passed, 0 failed/notRun; explicit clinical/UI/full-slot/network limits remain | `7f73356b`; `Docs/agent/audit/SESSION_STATE.md` | Deep Dive #1-#10, M18 |
| N-002 | DONE-2026-10-18 | Claude | Torch wear committed in 9714c1c3: the burn wears the item (was: commit own verified torch wear (`MOCarriedLightComponent.h/.cpp` + Light_Sources text)). Dependency UNBLOCKED: `UMOEquipmentComponent::ReduceEquippedDurability` published in `7f73356b`; Codex source RELEASE CX-0078 | Night report; CX-0078 | |
| N-003 | DONE-2026-10-18 | Codex | Corrected no-nav ActiveLoadAndPossession fixture; unreachable guard preserved and explicit failed-return cargo conservation covered. Crafting 16/16, including five SurvivorReload gates; full save-slot/live-network acceptance remains separate | `7f73356b`; Saved/AuditBatchH | |
| N-004 | DONE-2026-10-18 | Codex | Moved dated Handoff_2026-10-13 unchanged to Docs/Archive; updated MOworldSaveGame source comment and both indexes. Survival single-source fork brief stays live | `7f73356b`; CX-0078 | |

## World: terrain, caves, geology

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-010 | OPEN | Claude | Per-bedrock / cave-floor voxel surface types (cave walls are all VST_Cliff) | `Docs/Caves_And_Geology.md` | |
| N-011 | OPEN | Claude | Geology outcrops to REPLACE the biome-placed copper / flint deposits | Caves_And_Geology §5 | |
| N-012 | OPEN | Claude | Tin placers in rivers draining granite; measure the galena share | Caves_And_Geology §3 | |
| N-013 | OPEN | Claude | Sand variety: several sand materials by low-frequency noise + distance to water (extra sets may be F14) | worldgen To-do | P-7 |
| N-014 | OPEN | Claude | Snow from place and weather: climate / season / accumulation (aspect done Oct 15; today one altitude line, rocks and trees get no snow); bigger noise only if Wes wants (F21) | worldgen To-do, Handoff notes | G19 |
| N-015 | OPEN | Claude | Far-LOD vertex step still shows at biome boundaries at 3 km: a heightmap-based layer blend in the ground material | biome-blend | G20 |
| N-016 | OPEN | Claude | Distant lake water shards (fade / cap / ellipse mask / deeper freeboard) | Handoff next actions | |
| N-017 | VERIFY | Claude | Summit notch / far-LOD tear: not reproduced on the current build; the 12 km tear was the cave stamp (fixed Oct 14) | Handoff Verify | G07, P-6 |
| N-018 | OPEN | Claude | Determinism on a second physical PC (packaged `MO.WorldGen.Checksum`) | caves-and-lighting | |
| N-019 | OPEN | Claude | Main-menu flyby: start point chosen against v4 land; watch the hand-off ring in motion | worldgen v5 open, Handoff | |
| N-020 | OPEN | Claude | Lakes are static (no flow, no seasons); rivers are sea-level channels only | worldgen v4 open | |

## PCG and world features

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-030 | IN PROGRESS | Claude | Cliff / embankment assemblies: iterate and measure until they work every time (Wes) | World_Features "As built" | BR16-15, BR16-19 |
| N-031 | INVESTIGATING | Claude | Barren rock spire in Wes's session (not reproduced; PCG-stall family, first-launch PSO suspected); bug reports carry WorldBuild.*. Caught once on Oct 18 (nettest actions run 9, the joiner): its PCG made nothing at all -- 100 partition actors, 0 instance components (host: 400 / 3135) -- busy through the whole 60 s vegetation stage with near=0, then idle, still empty 25 min later; the next fresh pair was fine. Next time: dump the PCG components (WorldBuild.PcgComponents: generated vs generating) before stopping the instance | archived BugReports_2026-10-16 | BR16-16 |
| N-032 | OPEN | Claude | Shore reeds are stand-ins (tall smdrailr grass): real reed / rush / cattail meshes once F5 is decided | `Docs/PCG.md` §3 | P-4 |
| N-033 | OPEN | Claude | AlpineHeath ground: a darker heath / moss surface (check after VST_MI_AlpineHeath) | biome-aware | |
| N-034 | OPEN | Claude | Electric Forest ground = shared MuddyLeaves (a darker mossy surface; ED forest-floor mesh patches not placed) | Overnight_Polish | |
| N-035 | OPEN | Claude | Desert-canyon biome from the Sandstone / Canyon sets; ED forest biome 1:1 | overnight-polish | |
| N-036 | OPEN | Claude | Per-biome fauna and POI density review (SeedSurvey; walk each biome 3 min; log empty / repetitive stretches) | AAA P2 | P-5, G20 |
| N-037 | OPEN | Claude | Place or delete the paper-only resource rows: AppleTree, ClayDeposit, CommonSpruce, ReedBed, TallGrass | progression_map unplaced | |
| N-038 | VERIFY | Claude | Thin items without changing the locust branch (RandomChoice_3 split + separate thinning node) | Codex handoff | G02 |
| N-039 | VERIFY | Claude | Far canopy appearance survives cooking (M_FarCanopy ISM usage flag) | Codex handoff | G04 |
| N-040 | OPEN | Claude | Per-instance grounding of compositions exists only as a census (auto-apply OFF: it broke compositions) | wes-calls-oct16 | |
| N-041 | OPEN | Claude | Unifying world-features design (caves / rivers / landmarks / ore / player builds) mostly unbuilt | World_Features_Architecture | |

## Water, light, weather

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-050 | OPEN | Claude | Floating objects / boats via Oceanology buoyancy (supersedes MOBuoyancyComponent) | `Docs/Water.md` | P-3 |
| N-051 | OPEN | Claude | Swimming as a complete loop: load-dependent buoyancy (no inventory mass API), fall damage into water, Space surfacing feel, swimming creatures, clothing distorts on horizontal poses, swim blends | Water §3 | P-1, G17 |
| N-052 | OPEN | Claude | Softer mud lake bed (texture set) | worldgen To-do | |
| N-053 | DEFERRED | Claude | Hair buns under water + creator DOF: engine patch #1 needs a source engine. Wes, Oct 18: "i originally thought one of the fixes for the processing was breaking the lakes but that was a separate issue we will revisit" | `Docs/Engine_Patches.md` | F11 |
| N-054 | DONE-2026-10-18 | Claude | A joiner lights its own torch at a fire: `MO.Fire.Do LightCarried` from the joiner, 6 s on the server, lit on the joiner (6 cd, eye adapts) and rendered on the host (6.45 cd light on the joiner's pawn) | Light_Sources | |
| N-055 | OPEN | Claude | Torch-raising arm pose (animation) | Light_Sources | |
| N-056 | OPEN | Claude | Not modelled: smoke in dead-end galleries; rain on torches; torch from torch + survivors carrying torches; lamp fuel refill; snuff key; torch as fire for other uses | Light_Sources | |
| N-057 | OPEN | Claude | Weather per biome (rain / snow line at altitude) + the sound layers that go with it | AAA P2 | P-8 |

## Medical and survival

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-155 | OPEN | Codex / Claude | Permadeath gear: a deceased pawn record is skipped entirely on load (`MOPersistenceSubsystem` full-load loop), so the dead pawn's body and gear vanish after a reload; the design says the gear stays where it fell. Spawn the body as a dead, lootable corpse (or drop its inventory as world items at save). Source dependency UNBLOCKED by `7f73356b` / CX-0078; feature still unimplemented | `Docs/Persistence.md` §1; CL-0051 | |
| N-157 | DONE-2026-10-18 | Claude | Timed autosave: `UMOAutosaveSubsystem` saves the host's world every `MO.Save.AutosaveMinutes` (10) real minutes through the one save step `MOWorldSave::SaveThisWorld`, with a "Saving the world..." notice; host + joiner: 11-16 ms per save, the joiner writes nothing | `Docs/Persistence.md` §1 | |
| N-060 | WAITING | Wes | Survival single source (F1); thermal model (F2) | DECISIONS | G10, G11, H10/H11 of the Oct 13 handoff |
| N-061 | OPEN | Claude | Treatment UI (picker), assigned medic job, treating another patient from a client (owner-only wound replication needs an inspection request), HealingSpeedBonus / PainReduction effects | `Docs/Medical_And_Survival.md` §7 | Deep Dive #7 |
| N-062 | OPEN | Claude | Early dressing before cloth exists (MakeBandage needs WovenCloth01) | PROGRESSION_ROADMAP §3 | |
| N-063 | VERIFY | Claude | Thermal extremes have an enforced failure (orphaned IsCritical / cardiac / respiratory broadcasts were the July finding; DieOf systemic clocks landed Oct 13) | dev_status | M13 |
| N-064 | DONE-2026-10-18 | Claude | A joiner's death reaches the joiner (replicated FMODeathRecord + EnterDeadState; the recap delivered by OnAnyPawnDied); host + joiner: recap with the killer, Continue -> Characters -> Create character -> a new survivor | Medical §2 | |
| N-065 | WAITING | Wes | Nobody can sleep (players or survivors) while Energy only recovers by sleeping: every pawn runs down for good. How sleep works is fork F29 | DECISIONS F29 | |

## Crafting, progression, content

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-070 | WAITING | Wes | Heat ladder values + kiln stations (F10); era play time (F16); chariot (F17); attempt-anything (F18) | PROGRESSION_ROADMAP | Night report #3 |
| N-071 | OPEN | Claude | GAP producers: feathers (birds), horn, flax / nettle, wool (sheep), resin tap target (HarvestSap) -> unblocks the 10 blocked recipes | ROADMAP §5 | |
| N-072 | OPEN | Claude | Agriculture: seeds, crops, fields (hoe and sickle exist) | ROADMAP §3 Era 3 | |
| N-073 | OPEN | Claude | Lime burning (quicklime) once an updraft kiln exists | ROADMAP Era 3 | |
| N-074 | OPEN | Claude | Move the player's harvest clock from `UMOHarvestProgressWidget` onto the pawn's `UMOTimedActionComponent` | Crafting §3 | |
| N-075 | OPEN | Claude | Art: (stone maul DONE Oct 17, its own image-to-3D mesh + grip); copper / flint deposits use boulder meshes (need ore / chalk-flint looks); checker backgrounds baked into some icons (Stick01, Stone01: re-rendered Oct 12-14, the inventory showed clean sticks Oct 18 -- Wes: check by eye) | progression-pass1, Overnight_Polish | |
| N-076 | OPEN | Claude | Art coverage as a production backlog (135 / 216 items had real meshes + icons on Oct 14; 81 placeholders). Oct 17: +10 via image-to-3D (TRELLIS.2 GLBs, section 8): StoneMaul01, BarkContainer01, Copper/Tin/BronzeIngot01, WroughtIron01, SteelIngot01 (shares the bar, with its own steel tint MI_SteelIngot01), CopperPrill01, Resin01, HideGlue01. (The 3D item preview Wes wanted shipped Oct 14: 44fb4416) | `Docs/Item_Art_Pipeline.md` | G21 |
| N-077 | VERIFY | Claude | Gameplay effect of x10 fewer sticks / stones / bark (Tutorial_GatherStone wants 10 cobbles; starter flow; pawn gather jobs near the coast spawn) | Handoff Verify | G09 |
| N-078 | OPEN | Claude | First-ten-minutes pass 2 with ORDINARY input (pass 1 used console verbs): first craft, first fire, first meal / drink, first night | `Docs/Archive/` first-ten-minutes | G09, #71 |
| N-079 | DONE-2026-10-18 | Claude | A placed building clears the grass on its site (worked-ground zone + sweep, every machine, saved); a campfire no longer sits in tall grass. Host + joiner: 17 instances swept on both | `Docs/Building.md` §2 | |

## Building and terraform

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-080 | WAITING | Wes | Excavation stage 6 UI (F8); building cancel / refund (F7) | DECISIONS | G22, G23 |
| N-081 | DONE-2026-10-18 | Claude | `ServerPlaceBuilding` re-checked: the server holds the transform to the pawn's reach and build requests to `MaxBuildRequestReachCm`; the rest -> N-171 | Building §2 | H19 residual |
| N-082 | DONE-2026-10-18 | Claude | `ServerApplyTerraform` re-checked: server reach (25 m) and a rate limit (80 % of the action time); the tool requirement is client-side only -> N-171 | OVERNIGHT_LOG | H17 |
| N-171 | OPEN | Claude | Server re-checks a hostile client could skip (low priority for friends-only co-op): building collision / slope / recipe knowledge in `ServerPlaceBuilding`, the terraform tool requirement in `ServerApplyTerraform` | Building §2 | N-081, N-082 |
| N-083 | DONE-2026-10-18 | Claude | Site menu nits: the campfire listed hand recipes (fixed: a station lists its own + heat recipes, test StationListsItsOwnRecipes); the "Rotate and Flip" hint lingered after the first build (fixed: Tutorial_BuildControls completes on the first placed ghost); the source labels: left-aligned since the kit spec (`ui_specs/kit_build_context.py`) -- Wes: check by eye | first-ten-minutes | |
| N-084 | OPEN | Claude | Voxel Nanite material crash after two simultaneous digs (not reproduced): watch | Terraform header | |

## AI, colony, combat

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-090 | DONE-2026-10-18 | Claude | A failed survivor job says why: LastFailure (replicated), a notice to every player, the task menu's idle line; reasons at the meaningful sites (`MOSurvivorJobWhy`). Not built: TooCold / TooTired / ThreatNearby (no code refuses for those yet), tier-2 alerts, `MO.Colony.Status` | `Docs/AI_And_Colony.md` | pipeline B1 |
| N-091 | OPEN | Claude | Possession-transition "while you were away" summary (`BuildAwaySummary`) | AI_And_Colony | pipeline A6 |
| N-092 | OPEN | Claude | Tedium tools: transfer-all with a combined timer (verify the existing TransferAll code), sweep pickup (timed radius gather), haul jobs generalized to food / water restocking | AI_And_Colony | pipeline T1-T3 |
| N-093 | OPEN | Claude | Readability: "why" readouts (cold because wet + wind + no roof), action timers everywhere, a failure string for every bucket | pipeline R1 | |
| N-094 | OPEN | Claude | Wet wood burns poorly (needs per-item wetness: design first); winter + fire soak under load | pipeline F1 | |
| N-095 | OPEN | Claude | Colony UI: colony bar (4B), task assignment without possession (6B), alert tiers (7B) | AI_And_Colony §4 | |
| N-096 | OPEN | Claude | Creature ABP state machines + EQS query assets (Blueprint setup; C++ complete); more fauna (fox / pig / crow assets exist; birds also unblock feathers); creature footsteps (no ball bones); unify the creature flee decision before fire-fear | AI_And_Colony §1 | P-9 |
| N-097 | OPEN | Claude | Survivor quests: no survivor has `PossibleQuests` configured (data task) | AI_And_Colony §2 | |
| N-098 | OPEN | Claude | Combat feel: hit-stop, hit-flash, camera shake, impact VFX, weighty audio, directional hit reactions; movesets + weapon variety; parry / block / dodge windows + stamina; enemy combat AI (predators that fight, hostile humanoids); ranged (bow / atlatl / sling); surface the wound sim | archived Fable 5 charter Pillar 4 | |
| N-099 | DONE-2026-10-18 | Claude | Combat weapon subsystem not wired to equipment: the right hand's weapon item is now the main-hand weapon (`SyncMainHandFromEquipment`), wear lands on the held item | AI_And_Colony §1 Melee damage | old #24 |
| N-168 | OPEN | Claude | Bronze and iron spears (BronzeSpear01 / IronSpear01, their spearheads): items, recipes and DT_Weapons profiles exist, no meshes / icons yet (wooden, fire-hardened and flint spears have both) | `Docs/Item_Art_Pipeline.md` | Wes Oct 18 |
| N-169 | IN PROGRESS | Claude | Hit reactions: `AMOCreature::PlayHitReaction` has no caller and its montage would play on the server only (needs a multicast or a replicated hit counter); human deaths name the killer since Oct 18 (`FMODeathRecord::KillerName`, on the recap) | AI_And_Colony §1 | Oct 18 |
| N-100 | OPEN | Claude | Re-evaluate the Fab purchase list against the ComfyUI image-to-3D pipeline (Wes, Oct 18: "reevaluate these since i've moved a lot of stuff to the comfyui 3d modeling pipeline"): what still needs a bought asset (rigged / animated, large sets) vs what TRELLIS.2 can make | pipeline V3, `Docs/Item_Art_Pipeline.md` §8 | |

## Co-op

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-110 | OPEN | Claude | Friend invite / Steam-overlay join (`InviteUserToGame`) | `Docs/Coop_And_Networking.md` §1 | |
| N-111 | OPEN | Claude | Host-a-SAVE over real Steam not exercised (only Null OSS + a real click) | Coop §1 | |
| N-112 | OPEN | Claude | Residual: 1 of 6 joins sees 17-50 cm ground mismatch for ~10 s | Coop §3 | |
| N-113 | OPEN | Claude | Bug report form: the Open Discord button after the code move not re-clicked; contact field with a value | OVERNIGHT_LOG | |
| N-114 | DONE-2026-10-18 | Claude | Covered: `nettest actions` runs client cases for build, harvest, craft, forage, inspect, pickup, eat, drink, carcass, attack and more (`Tools/ue_inst.py` `_client_*`) | OVERNIGHT_LOG | |
| N-160 | OPEN | Claude | Remote players' pawns read velocity 0 on every other machine: `AMOCharacter::OnUpdateSimulatedPosition` sets `bSimGravityDisabled` on every proxy and the engine zeroes a "stuck" proxy's velocity each tick (no extrapolation between updates). Fix to try: disable sim gravity only when the proxy has no floor under it; control `MO.RemotePawn.SimGravity 1` | Coop §7 | |
| N-161 | DONE-2026-10-18 | Wes | Rubber band after a joiner hitch: Wes, Oct 18 after the Steam test, "steam seems to be smooth even on low end computer". F25 closed with no change (stock); reopen on a report | Coop §7, DECISIONS 2026-10-18 | |
| N-162 | DONE-2026-10-18 | Claude | ~2 cm height corrections on 0.8% of a joiner's moves (14% with +100 ms lag), invisible (snap <= 2 cm): find which floor disagrees (per-machine voxel collision? floor hover band). **Measured Oct 18 (nettest actions, `_joiner_long_run`):** the joiner standing still 1 km from the host was corrected ~10x/s by 69 cm for good: the HOST's floor there was voxel collision at z 5865.3, the joiner's own at 5796.2, the generator's ground 5796.6 on both -- the host's terrain collision around a far remote pawn is too high (coarser than the joiner's; the pawn's own invoker is 30 m, LOD unknown). Next: which collision component the host's floor is (invoker chunk or a far render chunk's collision) and its LOD | Coop §7 **Cause and fix (Oct 18):** two voxel collision layers, the pawn on the topmost; the host's render-chunk collision 1 km from its camera was up to ~1 m high. `AMOCharacter::RemotePlayerTerrainDetail` (MinLOD 0, 30 m on every other player's pawn): +93.1 -> +2.8 cm, 0 of 136 moves corrected (Coop section 7). |
| N-163 | DONE-2026-10-18 | Claude | Replication references per-machine PCG ISM components: the floor session measured PCG meshes Static (an unresolvable base costs a warning, not a position; 3b2c6da9), and client-to-server instance requests no longer carry PCG pointers (`FMOInstanceRef`, BR18-2) | Coop §5, §7 | |
| N-164 | DONE-2026-10-18 | Wes | Steam smoothness test with a remote joiner: "yes steam seems to be smooth even on low end computer" (Wes, Oct 18) | Coop §7 | |
| N-165 | DONE-2026-10-18 | Claude | Every healthy pawn read 180 / 110 mmHg (seen on the status panel, Oct 18): `CalculateBloodPressure` tested the blood-loss bands in the wrong order, a full volume took the worst class's shift and a bleed never lowered the pressure. `BloodLossSystolicShift` (continuous haemorrhage classes) + test BloodLossBands; five vitals tests that called authority-gated functions on an unowned component (and so tested nothing) now have an owner and assert; the status panel reads 120 / 80 (7a83bda8) | Medical_And_Survival §1 | |
| BR18-1 | DONE-2026-10-18 | Claude | A joiner's harvest said "Inventory Full!": it ran in the joiner's UI on its own copy. Now server-owned and timed on the pawn (`FMOHarvestActions`, `ServerBeginHarvest`); `_client_harvest` (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-2 | DONE-2026-10-18 | Claude | Ground items: E did nothing for a joiner (PCG pointers unresolvable on the host) and right-click showed "Unknown Object" with no buttons. `FMOInstanceRef`; the menu names the item with Pick Up / Inspect; `_client_e_on_ground_item`, `_trust_instance_far` (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-3 | DONE-2026-10-18 | Claude | A joiner can't drink: E hit a PCG plant first and went to the broken instance path; fixed by FMOInstanceRef (e316e4b5), verified: `_client_drink` (the host logs the joiner's draught) (7a83bda8) | `Docs/BugReports_2026-10-18.md` | |
| BR18-4 | DONE-2026-10-18 | Claude | Host foliage removal never reached a joiner: the worked-ground zones replicate now (`FMOReplicatedTerrainZoneArray`); `_host_foliage_reaches_client` (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-5 | DONE-2026-10-18 | Claude | A joiner leaving "reset" the pawn's skills / knowledge: inspection (and harvest XP) ran on the joiner's copy only. Server-owned inspection (`FMOInspectionActions`, `ServerInspectItem`); `_client_inspect` (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-6 | DONE-2026-10-18 | Claude | Possession window took no click: the row's full-row EntryButton (overlay) covered Select and nothing handled it; bound to the same select; `_host_possession_click` (a real click) (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-7 | DONE-2026-10-18 | Claude | A joiner couldn't order a survivor: every order ran on the clicking machine. One request + server RPC (`FMOSurvivorCommands`, `ServerSurvivorCommand`); also the gather search's log stall (2 min 44 s on the host); `_client_survivor_command` (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-8 | DONE-2026-10-18 | Claude | Footsteps: attenuation fixed (e316e4b5); the forest biomes (TemperateForest, LocustWoodland) played SandTrail in the data, now LeafCrunch | `Docs/BugReports_2026-10-18.md` | |
| BR18-9 | DONE-2026-10-18 | Claude | Status vitals once per showing (pooled panel re-added its fields) and the crafting menu's far campfire (active station never cleared): both fixed; the status panel checked on screen (each vital once on the third showing), the far campfire not yet looked at (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-10 | DONE-2026-10-18 | Claude | Tool / focus hint showed through menus and world popups: one visibility rule (menu or world popup), checked on screen (7a83bda8) | `Docs/BugReports_2026-10-18.md` | |
| BR18-11 | DONE-2026-10-18 | Claude | Terraform 1 s in developer mode: `MO.Dev.TerraformSeconds` (default 1) while the world's HOST is in developer mode (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-12 | DONE-2026-10-18 | Claude | Lakes calmer: `mo.Lakes.WaveScale` 0.4 of the preset's wave heights; checked (lake amplitude 20 vs the sea's 50; calm in a screenshot) (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-13 | DONE-2026-10-18 | Wes | "All the caves are gone": answered, Wes, Oct 18: "yes caves need to be decently common since they're ALSO hard to spot" -> N-170 | `Docs/BugReports_2026-10-18.md` | |
| N-170 | WAITING | Wes | Caves decently common (BR18-13): the shipped caves put a walk-in mouth in 0 % of ordinary 1 km tiles; built entrances (doline hollow + uphill passage) are built, tested and OFF -- on, 25 % (karst) or 62.5 % (any rock). Wes picks in F30; turning them on is a tuning default + a generator version bump + re-pinned probes | DECISIONS F30, `Docs/Caves_And_Geology.md` | BR18-13 |
| BR18-14 | OPEN | Claude | Cliffs not seated, see-through back side: the aligned `ed_cliff` slab species (SM_HugeSandstoneCliff_07) tilted with a steep slope (N-030 family, F9) | `Docs/BugReports_2026-10-18.md` | |
| BR18-15 | DONE-2026-10-18 | Claude | Caves in the POI list: "Cave entrance" (`FMOWorldGen::FindNearestCaveMouth`); checked in game (a mouth 6.4 km away, found in 1.8 s) (e316e4b5) | `Docs/BugReports_2026-10-18.md` | |
| BR18-16 | OPEN | Claude | UDW dynamic landscape weather effects: feed UDW's wetness / snow / puddles into M_QuixelBase's MO_Finish (scoped in the tracker) | `Docs/BugReports_2026-10-18.md` | |
| BR18-17 | DONE-2026-10-18 | Claude | Dev hang glider: `MO.Dev.Glider`, a joiner too (`_client_glide`: 1100 cm/s, 100 cm/s sink, 0 corrections) (7a83bda8) | `Docs/BugReports_2026-10-18.md`, `Docs/UI.md` §7 | |
| BR18-18 | WAITING | Wes | "goffy tran jan" (joiner): the generator's v8 cliff bands read as a staircase on a 42 deg AlpineRock flank; fork F27 | `Docs/BugReports_2026-10-18.md` | |
| BR18-19 | OPEN | Claude | "goooose" (joiner): a 16.7 m SM_HugeSandstoneCliff_04 slab (`ed_cliff`) tilted 36 deg with the slope, its loose chunks floating -- same cause as BR18-14 | `Docs/BugReports_2026-10-18.md` | |
| BR18-20 | DONE-2026-10-18 | Claude | Strip bark unreachable without loose bark; trees could not be studied, so Study the Forest / Harvest Bark never ran: Stick01 teaches BarkFiber 1; resource nodes study their yields (c53d3bf4) | `Docs/BugReports_2026-10-18.md` | |
| BR18-21 | DONE-2026-10-18 | Wes | Camp floor corrections were unbuilt ghosts that looked built; resolved by the ghost look on every mesh | `Docs/Coop_And_Networking.md` §7 | |
| BR18-22 | DONE-2026-10-18 | Claude | Pink wire spheres on every character (666f4dac's LOD volume visible in game): hidden in game (c53d3bf4) | `Docs/BugReports_2026-10-18.md` | |
| BR18-23 | DONE-2026-10-18 | Claude | "Gather dry grass" twice in the forage menu: one cover entry for both grasses (c53d3bf4) | `Docs/BugReports_2026-10-18.md` | |
| BR18-24 | DONE-2026-10-18 | Claude | Inventory slots stayed dark after a drag and moved around: the drag resets its source slot; pooled slots reset | `Docs/BugReports_2026-10-18.md` | |
| BR18-25 | DONE-2026-10-18 | Claude | Crafting / building menus: a highlighted pick with an empty preview on reopening; every opening now starts with nothing selected | `Docs/BugReports_2026-10-18.md` | |
| BR18-26 | DONE-2026-10-18 | Claude | Survivor task menu hard to read: restyled to the kit look (`ui_specs/kit_survivor_tasks.py`) | `Docs/BugReports_2026-10-18.md` | |
| BR18-27 | DONE-2026-10-18 | Claude | ">>" / "<<" acted on a hidden or stale container: the buttons now follow the visible panel (GetTransferTarget) and reopening clears the old container | `Docs/BugReports_2026-10-18.md` | |
| BR18-28 | DONE-2026-10-18 | Claude | Tool refusals name the tool and the simplest item ("Needs a tool: Hammer (Hammerstone or better).") | `Docs/BugReports_2026-10-18.md` | |
| BR18-29 | DONE-2026-10-18 | Claude | Bark container: a 1 L water vessel at its real size (14 x 13 x 12 cm); teaching it is fork F28 |
| BR18-31 | DONE-2026-10-18 | Claude | Joiner UI kept removed stacks / jobs / conditions: FastArray notifications moved to PostReplicatedReceive | `Docs/BugReports_2026-10-18.md` | | `Docs/BugReports_2026-10-18.md` | |
| BR18-30 | DONE-2026-10-18 | Claude | Survivor job typos in DT_SurvivorJobs fixed | `Docs/BugReports_2026-10-18.md` | |

## UI

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-120 | OPEN | Claude | Current screen + behaviour inventory (which screens are in the kit, real input, focus, captures) | `Docs/UI.md` §6 | G14 |
| N-121 | OPEN | Claude | Finish the remaining Field Ledger surfaces: HUD WBP layouts, Options, main / in-game menus, colony / possession, modals, Knowledge tab, medical detail pages; inventory showing equipment / backpack / nearby / info at once (a C++ refactor of `UMOUnifiedInventoryMenu`) | UI §6, Overnight_Polish | G15 |
| N-122 | OPEN | Claude | Unbound optional widgets: WBP_UnifiedInventory (CloseButton, loot-nearby, stack-organise), WBP_ItemContextMenu (Transfer, Pickup), WBP_JournalPanel (Abandon, TrackButtonText), WBP_BuildingRecipeDetail (amount spin / slider / text, resource-source checkboxes), WBP_EquipmentPanel (swap progress), WBP_MOColonyPortrait (ClickButton, NameLabel, ActivityIndicator), WBP_HUDRoot (WindIndicator: needs an arrow texture); native EntryButton in WBP_MOListEntry; ~15 legacy WBPs in /MOFramework/UI/ root. Wire each or delete the dead member | archived AAA backlog | P-10, G16 |
| N-123 | OPEN | Claude | Accessibility token swaps (`SetAccessibility`) not wired to Options | UI §6 | G16 |
| N-124 | OPEN | Claude | UI nits: 12 px list rows + 0.5-scaled entry button, white native amount SpinBox, unstyled "Characters" possession screen, `FuelTimeText` / "Current:" placeholders; spacing / focus / controller polish | archived AAA backlog | P-11, P-13 |
| N-125 | OPEN | Claude | Moodle icons are generated placeholders (a hand-drawn set replaces the textures, same names) | archived AAA backlog | P-12 |
| N-126 | OPEN | Codex | UI consolidation Stage 4 (skills / knowledge + quest collections); deferred: survivor-queue adapter, live building queue surface | `Docs/agent/ui/MIGRATION_PLAN.md` | G23 |
| N-127 | OPEN | Claude | In-game Load panel hides Host: no screenshot proof | OVERNIGHT_LOG | |
| N-128 | OPEN | Claude | MOUITestSubsystem tautological tests (false confidence) | code_health | M17 |

## Audio

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-130 | OPEN | Claude | Verify the XAudio2 backend at launch (log shows the XAudio2 mixer) + Wes's relaunch test | `Docs/Audio.md` | G18 |
| N-131 | OPEN | Claude | ~20 s of thin menu audio after a map load (UDW DSP) | Audio §2 | |
| N-132 | OPEN | Claude | Footsteps on props / buildings (needs physical materials) and creatures | Audio §3 | P-9, G18 |

## Performance, packaging, tooling

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-140 | WAITING | Wes | Reference resolution / quality + target hardware tier (F4) | `Docs/Performance.md` | Oct 13 handoff #5 |
| N-141 | OPEN | Claude | NavigationBuild ~15-20 ms frame; 25-30 ms render-thread frames; arrival hitches after teleports | Performance §4 | |
| N-142 | OPEN | Claude | Survivor spawn: groom emitter `/HairStrands/Emitters/StableRodsSystem` not cooked (flush); the MetaHuman actor's 35 ms spawn tick | post-reveal, Handoff | |
| N-143 | OPEN | Claude | SpawnManager's first spawns start a 3-5 s voxel state (nav invokers of new pawns) | pcg-load | |
| N-144 | OPEN | Claude | Packaged timing of the reveal; a wait-free reveal gated only on the near field; MoviePlayer boot screen for the engine-boot + LoadMap gap | `Docs/PCG.md` §8 | |
| N-145 | OPEN | Claude | First-launch-after-packaging hitch (PSO) suspected behind the PCG stall / barren spire | Packaging §1 | |
| N-146 | OPEN | Claude | Reliable captures: window-capture tools returned a 320x180 image inside a 1920x1080 frame (Oct 13); declare real pixel sizes; a motion-capture path | Handoff notes | G25 |
| N-147 | OPEN | Claude | Correctness gates on every final package (MOFramework automation + packaged suite + co-op gates after movement / spawn / water changes): a standing rule, keep doing it | Packaging §3 | G05 |

## Audit carry-over (codes from the June 11 tracker; full text in `Docs/Archive/PROJECT_STATUS_2026-10-15.md`)

Oct 14 verdict: no CRITICAL or HIGH open except C7 (made host-only Oct 14) and C8. The tracker was systematically stale (in July ~14 of ~20
re-checked defects were already fixed): **re-verify against current code before working any row.**

| ID | Status | Owner | Item |
|---|---|---|---|
| C1 | IN PROGRESS | Claude | Monolithic module: phases 1-3 done (Core / Medical / MOFramework); phase 4 = the UI module carve, after `FMOCraftResult` leaves the subsystem header |
| C8 | OPEN | Claude | ISM / HISM harvest RPC validation: mirror ServerExecuteInteract's validation against InstanceTransform (LOS tuning residual) |
| M5 | OPEN | Claude | Mixed authority idioms (GetOwnerRole vs HasAuthority) -- hygiene |
| M6 | OPEN | Claude | Two activity taxonomies (EMOActivityLevel vs EMOCreatureActivityState) -- hygiene |
| M7 | OPEN | Claude | Raw UButton in 5 list-entry widgets -- hygiene |
| M1, M2, M3, H6 | VERIFY | Claude | Creature AI: hardcoded blackboard keys, magic numbers, IsCornered placeholder, FindPackMembers via GetAllActorsOfClass |
| H1-H5 | VERIFY | Claude | June perf items (LoadSynchronous in AI tick, combat ticks every frame, sync icon loads, O(n) GUID lookup, blackboard every frame); the Oct 13 preload work addresses H1 / H3 at least |
| M12-M24, L8-L16 | VERIFY | Claude | Grouped June findings, each a list of sub-items, never re-verified per sub-item. Known fixed since: M12 master volume twice (Oct 12), M13 cardiac / respiratory broadcasts (Oct 13), M18 add-overflow (Codex batch B, uncommitted), M21 treatment ids (Codex Oct 17). M17 tautological UI tests is N-128 |

## Docs and process

| ID | Status | Owner | Item | Detail |
|---|---|---|---|---|
| N-150 | OPEN | Claude | Record the last Steam patch (date / time / commit) in `Docs/STEAM_RELEASES.md` whenever Wes asks for Steam patch notes (N9) | `Docs/TOOLING.md` § Steam patch notes |
| N-151 | CLOSED-2026-10-18 | Wes | CS399 weekly progress: Wes, Oct 18: "disregard this" | `Docs/CS399/CS399_WEEKLY_PROGRESS.md` |
| N-156 | DONE-2026-10-18 | Codex | Corrected MOPlayerController SetPause comment and policy reference: controller-routed paths refuse, direct AGameModeBase::SetPause bypasses and must never be called. No runtime pause change | `7f73356b`; `Docs/PAUSE_POLICY.md` § Enforcement |
| N-158 | OPEN | Claude | Store an explicit WorldId (FGuid) in UMOWorldSaveGame, made at new game and carried by every save, so identity no longer rides on the slot name (F24). Source dependency UNBLOCKED by `7f73356b` / CX-0078; feature still unimplemented | DECISIONS 2026-10-17 F24 |
| N-159 | OPEN | Claude | Optional cleanup: remove the unreachable `WBP_SavePanel` child from `WBP_MOInGameMenu` (`python Tools/ue.py ui remove`); `UMOInGameMenu::ShowSavePanel` / `UMOSavePanel::CreateNewSave` then have no caller. TRAP (Oct 18): `SwitchToPanel` uses fixed indices (`PanelIndex_Save` 2, `PanelIndex_Load` 3), so removing the child shifts Load: switch by widget first | `Docs/UI.md` § 3 |
| N-152 | OPEN | Claude | Codex's July audit docs in `Docs/agent/` say "Status: In progress" and were never closed: ask Codex to close or archive them | `Docs/agent/` |
| N-153 | WAITING | Wes | Law 6 ("commit only when asked"). Wes, Oct 18: "we got rid of this"; checked 2026-10-18: the global `C:/Users/penum/.claude/CLAUDE.md` (Law 6, line 11) and `D:/ClaudeMaster/CLAUDE_BIBLE.md` (lines 30 and 183) still say it. MO57's CLAUDE.md overrides it here (N1) | DECISIONS N1, N4 |

## Done in the last week (one line each; older -> Docs/Archive/BACKLOG_DONE.md)

- DONE-2026-10-18 hunting (Wes, Oct 18: deer should hear what comes up behind them, run far when hurt; wolves lose interest; spears): one punch no longer kills a deer (BP_MOMetaHuman_Female's fist BaseDamage 500 -> the class default 5), deaths name their killer (the attacker reaches the anatomy), creatures hear (NoTeam vs NoTeam was Friendly to the hearing sense), footfalls follow the real speed, a wounded deer flees 45 s, a wolf gives up 200 m from its den or after 20 s without gaining, a held spear fights with its own profile (N-099) and ArmorPenetration no longer cuts unarmored damage (`Docs/AI_And_Colony.md` §1).

- DONE-2026-10-18 N-167 (follow-ups to N-166): the other wind graphs that add time to a world-space term (Epic SimpleGrassWind in the hornbeam master, MF_WindGust MSPresets / LATEST, Fab QMF_WindAnimation / QMF_WindGust) scale the world term down first and hold: no distance trend in `motion_probe`'s new jump share, in the world (hornbeams 10.9 km vs 0.2 km, black locust 11.6 vs 1 km) and with the same meshes against the sky at 0 / 11.6 / 15 / 40 / 80 km (wild grass 0-2 % everywhere); no graph needed the Fmod. `MOFramework.Data.FoliageWindPrecision` fails if a Pivot Painter wind graph adds time to the pivot without the `MO_WindPrecision` Fmod (10 functions, 3 sites, all reduced). The 6 BlackAlder Forest_08 / Sapling_01 / Seedling_01 Pivot Painter textures no longer have mips or streaming (`Content/Python/pivot_painter_textures.py`; gitignored vendor folder: rerun after a reimport) (`Docs/PCG.md` §9).
- DONE-2026-10-18 N-166 (Wes, Oct 18: the Megascans trees' wind "moves smoothly ... then freeze[s] ... then glitch[es]"): the Pivot Painter wind added time to a WORLD-space pivot term in float, so 5-15 km from the origin each branch level moved one texel at a time; `Content/Python/foliage_wind_precision.py` reduces the offset mod 1200 first (MF_AdvancedWind_Level, PN_WindAnimation). Step share 34-47 % -> 0-1 %, TSR jumps 4-7 per 15 s -> 0 (`Docs/PCG.md` §9).
- DONE-2026-10-18 N-162: a joiner far from the host was corrected on every move (host's coarse render-chunk collision up to ~1 m high); every machine now refines the terrain around every other player (`RemotePlayerTerrainDetail`): +93.1 -> +2.8 cm, 0 of 136 corrected.
- DONE-2026-10-17 N-021: the next Steam patch notes warn that worlds from generator v8 (0573ab31) load with the v11 terrain, and that extra Save As slots show as separate entries.
- DONE-2026-10-17 co-op smoothness tools for Steam tests: [MONet] stats logged in every co-op session, [MONetEvent] hitch / rubber-band events, Net.* bug-report rows, MO.Net.Show, `Tools/ue_netdiag.py logs` for two machines' logs, `nettest netdiag` (instrument controls); findings in Coop §7 (N-160..N-164, F25).
- DONE-2026-10-17 SteelIngot01 looks like steel: MI_SteelIngot01 on the bar mesh (bright cool grey, smoother, softer scale), new icon; wrought iron stays rusty.
- DONE-2026-10-17 ten items got real meshes + icons from Wes's TRELLIS.2 GLBs (levelled, scaled, decimated in Blender; StoneMaul01 grip fitted to the hand).
- DONE-2026-10-17 F24 (Wes: A): one save per world -- in-game Save writes the world's own slot with a toast (no Save As, no confirm), the in-game Load hides the world you are in, the main menu lists worlds.
- DONE-2026-10-17 N-154 + Wes's pause call: every way out of a world saves with no confirm (no save-scumming); loading a save made at a death respawns instead of a sky camera; PAUSE_POLICY's two pause-path claims corrected from the engine source.
- DONE-2026-10-17 G26 / this consolidation: one BACKLOG, DECISIONS, TRAPS, INDEX, per-system docs, slim layer 1.
- DONE-2026-10-17 inspect discovery (d6a3af69), dig time by rock (37a3d694), torch + eye adaptation + dark caves (4b1e1265, 057e525a), galena,
  karst caves + geology yields (ba37d659), caves stay dry (f92ebebe).
- DONE-2026-10-16 Oct 16 playtest batch (22 reports): frozen lakes, host-only green boxes, world actions, eating timer, interaction targeting, audio NaN guard, XAudio2.
- DONE-2026-10-15 drink / fill / boil, friction fire, containers, progression passes 1-4, first-ten-minutes pass 1, survival pacing, character creator.
- DONE-2026-10-14 Oct 14 playtest batch (29 reports), world features / POIs, coverage pass, item merge, Data:Sources check, dev mode in Shipping.
- DONE-2026-10-13 G00 G01 G03 G05 G06 G08 G12 G24, readiness 7 s, removals ledger, DieOf, server-driven construction, soft biome boundaries.
