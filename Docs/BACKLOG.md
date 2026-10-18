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

## Waiting on Wes (decisions; details in DECISIONS "Needs Wes")

F1 survival single source · F2 thermal model · F3 regrowth · F4 perf target · F5 reed source · F7 building refund ·
F8 excavation stage 6 UI · F9 cliff walls · F10 heat-ladder values + kilns · F11 post engine report · F12 river reeds · F13 clock speed ·
F14 downloads (UE logo, kit fonts, tropical / beach packs, sand sets, more Megascans) · F15 gender · F16 era play time · F17 chariot ·
F18 attempt-anything · F19 far-colonist ground · F20 eye adaptation speed · F21 bland far mountains · F22 springs / rain / hauling · F23 rejected Steam art ·
F25 co-op rubber band after a joiner hitch (Steam A/B first).

## In flight (built, uncommitted, other agent)

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-001 | WAITING | Codex | Commit audit batches A-F (equipment slot validation, hand-swap ownership, stack replication, inventory capacity, server-derived station context, cancel RPCs, ingredient reservation + exact refunds, survivor liquid transfer, survival meters persisted, timed wound treatment). Wes now says "commit always" (N1) | `Docs/agent/audit/SESSION_STATE.md` | Deep Dive #1-#10, M18 |
| N-002 | WAITING | Claude | Commit torch wear (`MOCarriedLightComponent.h/.cpp` + the Light_Sources torch-wear text) right after Codex's equipment commit (it calls `UMOEquipmentComponent::ReduceEquippedDurability`) | Night report | |
| N-003 | OPEN | Codex | C3 reload recovery: `SurvivorReload.ActiveLoadAndPossession` fails (the no-nav fixture trips the unreachable move-leg guard; do NOT weaken the guard) | CX-0064 | |
| N-004 | WAITING | Claude | Archive `Docs/Handoff_2026-10-13.md` once Codex commits `MOworldSaveGame.h` (its comment names the doc); then point the comment at `Docs/Archive/` | CL-0048 | |

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
| N-021 | OPEN | Claude | Note for release notes: generator versions are not save-compatible (old worlds regenerate) | Handoff notes | |

## PCG and world features

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-030 | IN PROGRESS | Claude | Cliff / embankment assemblies: iterate and measure until they work every time (Wes) | World_Features "As built" | BR16-15, BR16-19 |
| N-031 | INVESTIGATING | Claude | Barren rock spire in Wes's session (not reproduced; PCG-stall family, first-launch PSO suspected); bug reports carry WorldBuild.* | archived BugReports_2026-10-16 | BR16-16 |
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
| N-053 | BLOCKED | Wes | Hair buns under water + creator DOF: engine patch #1 needs a source engine; Wes waits for an Epic fix | `Docs/Engine_Patches.md` | F11 |
| N-054 | OPEN | Claude | Co-op: a client lighting its own torch at a fire (retest after a9a4de5d; reconcile with Light_Sources "verified") | Night report | |
| N-055 | OPEN | Claude | Torch-raising arm pose (animation) | Light_Sources | |
| N-056 | OPEN | Claude | Not modelled: smoke in dead-end galleries; rain on torches; torch from torch + survivors carrying torches; lamp fuel refill; snuff key; torch as fire for other uses | Light_Sources | |
| N-057 | OPEN | Claude | Weather per biome (rain / snow line at altitude) + the sound layers that go with it | AAA P2 | P-8 |

## Medical and survival

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-155 | OPEN | Codex / Claude | Permadeath gear: a deceased pawn record is skipped entirely on load (`MOPersistenceSubsystem` full-load loop), so the dead pawn's body and gear vanish after a reload; the design says the gear stays where it fell. Spawn the body as a dead, lootable corpse (or drop its inventory as world items at save). `MOPersistenceSubsystem.cpp` has Codex's uncommitted edits: after N-001 | `Docs/Persistence.md` §1; CL-0051 | |
| N-157 | OPEN | Claude | No timed autosave: a crash or a killed process loses everything since the last exit (and killing the process is the one scum route left). Autosave on a game-time interval through the same `SaveThisWorld` step | `Docs/PAUSE_POLICY.md` "Leaving a world" | |
| N-060 | WAITING | Wes | Survival single source (F1); thermal model (F2) | DECISIONS | G10, G11, H10/H11 of the Oct 13 handoff |
| N-061 | OPEN | Claude | Treatment UI (picker), assigned medic job, treating another patient from a client (owner-only wound replication needs an inspection request), HealingSpeedBonus / PainReduction effects | `Docs/Medical_And_Survival.md` §7 | Deep Dive #7 |
| N-062 | OPEN | Claude | Early dressing before cloth exists (MakeBandage needs WovenCloth01) | PROGRESSION_ROADMAP §3 | |
| N-063 | VERIFY | Claude | Thermal extremes have an enforced failure (orphaned IsCritical / cardiac / respiratory broadcasts were the July finding; DieOf systemic clocks landed Oct 13) | dev_status | M13 |
| N-064 | OPEN | Claude | Death recap: remote client's respawn RPC not verified | Medical §2 | |
| N-065 | OPEN | Claude | Villager sleep (fatigue has no recovery) | dev_status leads | |

## Crafting, progression, content

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-070 | WAITING | Wes | Heat ladder values + kiln stations (F10); era play time (F16); chariot (F17); attempt-anything (F18) | PROGRESSION_ROADMAP | Night report #3 |
| N-071 | OPEN | Claude | GAP producers: feathers (birds), horn, flax / nettle, wool (sheep), resin tap target (HarvestSap) -> unblocks the 10 blocked recipes | ROADMAP §5 | |
| N-072 | OPEN | Claude | Agriculture: seeds, crops, fields (hoe and sickle exist) | ROADMAP §3 Era 3 | |
| N-073 | OPEN | Claude | Lime burning (quicklime) once an updraft kiln exists | ROADMAP Era 3 | |
| N-074 | OPEN | Claude | Move the player's harvest clock from `UMOHarvestProgressWidget` onto the pawn's `UMOTimedActionComponent` | Crafting §3 | |
| N-075 | OPEN | Claude | Art: (stone maul DONE Oct 17, its own image-to-3D mesh + grip); copper / flint deposits use boulder meshes (need ore / chalk-flint looks); checker backgrounds baked into some icons (Stick01, Stone01) | progression-pass1, Overnight_Polish | |
| N-076 | OPEN | Claude | Art coverage as a production backlog (135 / 216 items had real meshes + icons on Oct 14; 81 placeholders). Oct 17: +10 via image-to-3D (TRELLIS.2 GLBs, section 8): StoneMaul01, BarkContainer01, Copper/Tin/BronzeIngot01, WroughtIron01, SteelIngot01 (shares the bar, with its own steel tint MI_SteelIngot01), CopperPrill01, Resin01, HideGlue01. (The 3D item preview Wes wanted shipped Oct 14: 44fb4416) | `Docs/Item_Art_Pipeline.md` | G21 |
| N-077 | VERIFY | Claude | Gameplay effect of x10 fewer sticks / stones / bark (Tutorial_GatherStone wants 10 cobbles; starter flow; pawn gather jobs near the coast spawn) | Handoff Verify | G09 |
| N-078 | OPEN | Claude | First-ten-minutes pass 2 with ORDINARY input (pass 1 used console verbs): first craft, first fire, first meal / drink, first night | `Docs/Archive/` first-ten-minutes | G09, #71 |
| N-079 | OPEN | Claude | Fire can sit in tall grass (no wildfire) | first-hours | |

## Building and terraform

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-080 | WAITING | Wes | Excavation stage 6 UI (F8); building cancel / refund (F7) | DECISIONS | G22, G23 |
| N-081 | VERIFY | Claude | `ServerPlaceBuilding` trusted the client transform: reach / collision / slope / rate recheck | Building §2 | H19 residual |
| N-082 | VERIFY | Claude | `ServerApplyTerraform`: server-side timer / tool requirement (a rate limit landed Oct 13) | OVERNIGHT_LOG | H17 |
| N-083 | OPEN | Claude | Site menu nits: right-aligned source labels; the "Rotate and Flip" hint lingers after the first build; "Select Crude Club" + splint / bandage / blanket in the campfire list | first-ten-minutes | |
| N-084 | OPEN | Claude | Voxel Nanite material crash after two simultaneous digs (not reproduced): watch | Terraform header | |

## AI, colony, combat

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-090 | OPEN | Claude | Blocked-job reasons at the data layer (`EMOJobBlockReason`: MissingIngredient / NoPath / TooCold / TooTired / NoStation / StorageFull / ToolMissing / ThreatNearby) + tier-2 alert + `MO.Colony.Status` | `Docs/AI_And_Colony.md` | pipeline B1 |
| N-091 | OPEN | Claude | Possession-transition "while you were away" summary (`BuildAwaySummary`) | AI_And_Colony | pipeline A6 |
| N-092 | OPEN | Claude | Tedium tools: transfer-all with a combined timer (verify the existing TransferAll code), sweep pickup (timed radius gather), haul jobs generalized to food / water restocking | AI_And_Colony | pipeline T1-T3 |
| N-093 | OPEN | Claude | Readability: "why" readouts (cold because wet + wind + no roof), action timers everywhere, a failure string for every bucket | pipeline R1 | |
| N-094 | OPEN | Claude | Wet wood burns poorly (needs per-item wetness: design first); winter + fire soak under load | pipeline F1 | |
| N-095 | OPEN | Claude | Colony UI: colony bar (4B), task assignment without possession (6B), alert tiers (7B) | AI_And_Colony §4 | |
| N-096 | OPEN | Claude | Creature ABP state machines + EQS query assets (Blueprint setup; C++ complete); more fauna (fox / pig / crow assets exist; birds also unblock feathers); creature footsteps (no ball bones); unify the creature flee decision before fire-fear | AI_And_Colony §1 | P-9 |
| N-097 | OPEN | Claude | Survivor quests: no survivor has `PossibleQuests` configured (data task) | AI_And_Colony §2 | |
| N-098 | OPEN | Claude | Combat feel: hit-stop, hit-flash, camera shake, impact VFX, weighty audio, directional hit reactions; movesets + weapon variety; parry / block / dodge windows + stamina; enemy combat AI (predators that fight, hostile humanoids); ranged (bow / atlatl / sling); surface the wound sim | archived Fable 5 charter Pillar 4 | |
| N-099 | OPEN | Claude | Combat weapon subsystem not wired to equipment | dev_status July 7 | old #24 |
| N-100 | WAITING | Wes | V3 real art via Fab (needs Wes's purchases; debug meshes hold) | pipeline V3 | |

## Co-op

| ID | Status | Owner | Item | Detail | Aliases |
|---|---|---|---|---|---|
| N-110 | OPEN | Claude | Friend invite / Steam-overlay join (`InviteUserToGame`) | `Docs/Coop_And_Networking.md` §1 | |
| N-111 | OPEN | Claude | Host-a-SAVE over real Steam not exercised (only Null OSS + a real click) | Coop §1 | |
| N-112 | OPEN | Claude | Residual: 1 of 6 joins sees 17-50 cm ground mismatch for ~10 s | Coop §3 | |
| N-113 | OPEN | Claude | Bug report form: the Open Discord button after the code move not re-clicked; contact field with a value | OVERNIGHT_LOG | |
| N-114 | VERIFY | Claude | No build / harvest MO.Test verbs runnable from a client (nettest actions now has `_client_build`): close if covered | OVERNIGHT_LOG | |
| N-160 | OPEN | Claude | Remote players' pawns read velocity 0 on every other machine: `AMOCharacter::OnUpdateSimulatedPosition` sets `bSimGravityDisabled` on every proxy and the engine zeroes a "stuck" proxy's velocity each tick (no extrapolation between updates). Fix to try: disable sim gravity only when the proxy has no floor under it; control `MO.RemotePawn.SimGravity 1` | Coop §7 | |
| N-161 | WAITING | Wes | Rubber band after a joiner hitch >= 250 ms (the host's forced update): Steam A/B with the toggles, then F25 | Coop §7, DECISIONS F25 | |
| N-162 | OPEN | Claude | ~2 cm height corrections on 0.8% of a joiner's moves (14% with +100 ms lag), invisible (snap <= 2 cm): find which floor disagrees (per-machine voxel collision? floor hover band) | Coop §7 | |
| N-163 | OPEN | Claude | Replication references per-machine PCG ISM components (84 `SupportsObject ... NOT Supported` on the host in 25 min): likely a pawn's movement base on a PCG rock; check what a proxy on such a base does | Coop §7 | |
| N-164 | WAITING | Wes | Steam smoothness test with a remote joiner on the Development package: recipe Coop §7 (stats log by themselves; send both MO57.log) | Coop §7 | |

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
| N-151 | OPEN | Wes | CS399: weekly progress is updated every Sunday (last entry Week 2, Oct 5); check-in Nov 18 | `Docs/CS399/CS399_WEEKLY_PROGRESS.md` |
| N-156 | WAITING | Claude | `MOPlayerController.h`'s SetPause comment lists `AGameMode::SetPause` as caught; a direct `AGameModeBase::SetPause` call bypasses the override (engine source, Oct 17). Fix the comment once Codex commits that file | `Docs/PAUSE_POLICY.md` § Enforcement |
| N-158 | WAITING | Claude | Store an explicit WorldId (FGuid) in `UMOWorldSaveGame`, made at new game and carried by every save, so a world's identity no longer rides on its slot name (F24). `MOworldSaveGame.h` / `MOPersistenceSubsystem.cpp` hold Codex's uncommitted batches: after N-001 | DECISIONS 2026-10-17 F24 |
| N-159 | OPEN | Claude | Optional cleanup: remove the unreachable `WBP_SavePanel` child from `WBP_MOInGameMenu` (`python Tools/ue.py ui remove`); `UMOInGameMenu::ShowSavePanel` / `UMOSavePanel::CreateNewSave` then have no caller | `Docs/UI.md` § 3 |
| N-152 | OPEN | Claude | Codex's July audit docs in `Docs/agent/` say "Status: In progress" and were never closed: ask Codex to close or archive them | `Docs/agent/` |
| N-153 | OPEN | Wes | Global CLAUDE.md and `D:\ClaudeMaster\CLAUDE_BIBLE.md` were corrected for the engine type and the principles' new home on 2026-10-17; Law 6 still says "commit only when asked" globally (MO57 overrides it per N1) | DECISIONS N1, N4 |

## Done in the last week (one line each; older -> Docs/Archive/BACKLOG_DONE.md)

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
