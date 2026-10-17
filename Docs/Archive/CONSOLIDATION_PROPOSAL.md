# Knowledge consolidation: proposed structure (P2), 2026-10-17

Phase 2 of the consolidation task. It rests on the measurements in `Docs/Archive/INVENTORY.md` (P1).
**Nothing has been moved, renamed, merged or deleted.** The only files written so far are this one and INVENTORY.md.
P3 (building layer 2) starts after Wes answers the questions at the end.

## 0. Summary

| | Today | Proposed | Change |
|---|---|---|---|
| Claude, always loaded (project CLAUDE.md + MEMORY.md + global CLAUDE.md) | 61,700 B | ≤ 16,400 B (10,000 + 3,000 + 3,452 unchanged) | -73 % |
| Codex, always loaded (AGENTS.md) | 44,010 B | ≤ 2,000 B shim, which tells Codex to read CLAUDE.md (≤ 10,000 B) | -73 % |
| Memory topic files | 81 files / 304,543 B | about 15 files / ≤ 1,500 B each (≤ 22 KB) | -93 % |
| Live `Docs/` routed from layer 1 | 75 files, no index, about 25 dated snapshots mixed in | INDEX + BACKLOG + DECISIONS + TRAPS + TOOLING + 19 system docs + references; dated snapshots in `Docs/Archive/` | |
| Places that hold open work | about 12 (PROJECT_STATUS tracker, AAA backlog, 2 bug trackers, 2 handoffs, night report, pipeline cards, ~15 memories, Codex audit state) | 1 (`Docs/BACKLOG.md`) | |
| Places that hold Wes's calls | about 14 (3 wes-calls memories, directive, terraform design, several system memories, handoff, night report, bug tracker, AAA forks ...) | 1 (`Docs/DECISIONS.md`) | |

## 1. Layer 1: always loaded

### 1.1 `CLAUDE.md`: budget ≤ 10,000 B (today 46,519). The canonical rules file for every agent.

| Section | Budget | Content (condensed, never reworded in meaning) | Full text moves to |
|---|---|---|---|
| Header | 400 | What MO57 is, in 2 lines. The Seven Laws are in the global CLAUDE.md (always loaded), so a pointer only, no repetition. This file = project rules + router | `Docs/VISION.md` |
| STOP table | 1,300 | Before X, read Y: CSV / DataTable edit, compile, git, "fix this bug", "add X to A, B, C", engine edit (check `InstalledBuild.txt`), any "instant/auto/fast" mode, pause, deleting or renaming assets from editor Python, live DT row writes | |
| Engineering Principles 1-11 | 2,000 | One line per principle; the tedium rule's three questions in one line; realism pillar #11 in two lines | `Docs/ENGINEERING_PRINCIPLES.md` (verbatim, with every example) |
| Design policies | 300 | No pause, ever (enforced in `AMOPlayerController::SetPause`) | `Docs/PAUSE_POLICY.md` (unchanged) |
| Git and multi-agent | 700 | Only rules that don't conflict: stage explicit paths, never `git add -A` in the shared tree, check the staged column before every commit, never stage `.claude/settings.local.json` or `MegaMaterialCache.uasset`, coordinate through `claude-codex-coop.md`, editor/build/instances have one owner at a time. **The auto-commit-after-compile rule is not restated** (see N1) | `Docs/TRAPS.md` § Git, `Docs/DECISIONS.md` N1 |
| Machine and engine traps | 1,300 | About 10 one-liners that cost a round every session: 5.8 UBT path; launcher engine (no engine .cpp edits); editor-Python RPCs run LOCAL; never delete/rename assets in editor Python; `MSYS_NO_PATHCONV=1`; `-ExecCmds` splits on commas; close the editor before visual/PCG instance tests; verify DT writes by readback; stop only PIDs you launched | `Docs/TRAPS.md` |
| Code conventions | 900 | RemoveAll before bind; input in `AMOPlayerController`; UHT delegate-file rule; template full includes; CommonUI bases; buttons are WBP `MOButton`, never the native class as a node; Voxel only through `MOVoxel`; 5.8 `SetBase` signature; no `GetSubsystem()` of siblings in `Initialize()` (cook) | `Docs/TECHNICAL_REFERENCE.md` |
| Router | 2,400 | Topic → file, one line each (about 30 rows, list in 1.4); "anything else → `Docs/INDEX.md`" | |
| Graphify | 200 | `python -m graphify query/explain/path/affected`; `update .` after code changes | `Docs/TOOLING.md` |

Leaves CLAUDE.md: Project Vision (3.3 KB) and the design principles drawn from other games go to `VISION.md`;
Implementation Notes (11.9 KB) go to `TECHNICAL_REFERENCE.md`; the CSV manual (3.4 KB) goes to `TOOLING.md` § DataTables plus
`Tools/UE_CSV_FORMAT.md`; CLI commands go to `TOOLING.md`; the native refactor roadmap and the coupling list go to
`TECHNICAL_REFERENCE.md`, with open items in BACKLOG; the UMG automation table goes to `UI_TOOLING.md`, which already has it.

### 1.2 `MEMORY.md`: budget ≤ 3,000 B / ≤ 50 lines (today 11,729 B / 81 lines). Target about 18 lines, about 2 KB.

```
## Preferences
- user-preferences · verification-lessons · git-staging-verification · multi-agent-coop
- overnight-runner-ov-bat · miktex-package-installs · verify-the-asset-before-fixing-it · standing-permissions
## Decisions
- decisions (→ Docs/DECISIONS.md: read "Needs Wes" first; never re-ask a decided call)
## Traps
- traps (→ Docs/TRAPS.md + the session-level few) · build-engine-path (5.8 launcher install, UBT path)
## Context (not in the repo)
- project-overview · wes-playtest-context · cs399-practicum · electric-dreams-env-project
```

Each kept file stays ≤ 1.5 KB and uses the current frontmatter (`metadata: type:`). New files:
- `verification-lessons`: LOOK before theorising; a control must be able to fail; rerun the probe on the fixed build and read
  both machines; audit your own fixes; re-verify stale tracker claims. All of these were learned in sessions and live only in
  memory today.
- `standing-permissions`: what Wes has authorized and its scope. Free Fab and CC0 downloads are allowed, but say what was
  downloaded. No paid APIs. No generated item art. Never touch Wes's saves. Commit and push only per task. Posting upstream is
  his call.
- `wes-playtest-context`: how Wes playtests (Shipping on Steam with a joiner, reports through the site, crash_triage),
  `%LOCALAPPDATA%` holds his real settings (back them up before editing), his audio endpoints.

The other 71 files (81 minus the 10 kept; 5 of the 15 are new) retire to `memory/archive/` after their facts are in layer 2. This is a move, not a delete. They stay
outside the repo because some are personal (cs399 names the instructor, who must stay out of the public repo).

### 1.3 `AGENTS.md`: ≤ 2,000 B shim (today a 44 KB stale copy of CLAUDE.md)

Codex does not read CLAUDE.md on its own, and AGENTS.md has no import syntax. So: **`CLAUDE.md` is the one canonical rules
file for every agent**, and AGENTS.md says "read `CLAUDE.md` in full before any work; it is the canonical rules file for this
repo". The Codex-specific part is:
- the coop protocol pointer;
- Codex's durable state lives in `Docs/agent/`;
- the graphify interpreter path (CL-0001: the Store Python, not anaconda);
- don't stage another agent's files.

Nothing shared is duplicated. The four stale divergences (section 2.4 of the inventory) disappear with the copy.

### 1.4 The router (CLAUDE.md), one row per topic

Rules in full, vision, architecture/APIs, **open work → BACKLOG**, **Wes's calls and forks → DECISIONS**, **traps → TRAPS**,
**dev entry points / console verbs → TOOLING**, worldgen/terrain/biomes, caves/geology/ore/dig time, PCG/vegetation,
POIs/world features, Voxel plugin, water/lakes/swimming, drinking/illness, light/darkness/exposure, weather/clock/time,
medical/survival/death, crafting/progression/items, technology history, item art/icons, building, terraform/excavation,
AI/survivors/colony, co-op/sessions/RPC, UI architecture, building UI (`ue.py ui`), persistence/saves, audio,
packaging/cook/Shipping, performance, engine patches, pause policy, DataTable authoring, CS399, Codex coordination, and
"everything else → INDEX".

## 2. Layer 2: one file per topic

Every system doc opens with a header of 10 lines or fewer: what it covers · current state (date) · entry points (files and
classes) · console verbs · tests · open items as BACKLOG ids · related decisions as DECISIONS ids. An agent can stop reading
after the header.

### 2.1 New cross-cutting files

| File | Budget | Built from |
|---|---|---|
| `Docs/INDEX.md` | ≤ 8 KB | One line per live doc: path · what it answers · when to read it. A pointer to `Docs/Archive/INDEX.md` |
| `Docs/BACKLOG.md` | ≤ 40 KB | The one place for open work. Columns: id · status (OPEN / IN PROGRESS / BLOCKED / DONE-date) · owner (Claude / Codex / Wes) · one line · detail pointer · aliases. Sources: PROJECT_STATUS tracker (103 rows), AAA_Polish_Backlog (11 open), Claude_Handoff_Goals H00-H26, Deep_Dive (10 flaws, owner Codex), Night_Report open items, BugReports (2 not-reproduced rows; the other 47 are DONE), Fable5 pipeline open cards, Codex `agent/audit/SESSION_STATE` next steps, about 40 open items scattered across memories, the to-do lists inside Terrain_Generation_Exploration. DONE older than a week → `Docs/Archive/BACKLOG_DONE.md` |
| `Docs/DECISIONS.md` | ≤ 30 KB | "Needs Wes" at the top (section 4 of this file plus the open forks in section 5), then dated binding calls: verbatim quote · what was built · commit. Sources: wes-calls-oct15/16/17, directive-oct13, terraform-excavation-design (July 13), dev_status (July 8 directives), terrain v4/v5/v6 calls, weather-bridge, coop construction, world-features, ui-design-system, audio focus policy, item-art policy, Handoff "Decisions waiting", Night_Report calls, BugReports Oct 16 calls, AAA forks |
| `Docs/TRAPS.md` | ≤ 35 KB | About 130 entries, each symptom → cause → what to do, grouped by area: UE 5.8 engine · editor + Python bridge · ue.py / instances / harness · UBT / packaging / cook · PCG · Voxel · CommonUI / UMG · co-op / RPC · rendering / lights / materials · audio · DataTables / MCP · Git / Windows / shell. Sources: almost every memory file, CLAUDE.md, Overnight_Polish, OVERNIGHT_LOG, AUTONOMOUS_TOOLING hit/miss, UI_Overhaul_Architecture pitfalls, UI_Kit_Implementation traps |
| `Docs/TOOLING.md` | ≤ 20 KB | Catalog of every dev entry point, one line each plus a pointer: `ue.py` verbs (status, run, py, test, rows, cycle, auto, seq, ui, inst, nettest modes, editor), icon studio, kitbash, item_tint, csv utils, crash_triage, pkg_probe, ov.bat, graphify (+ Codex interpreter), terrain_lab, perf tools. `MO.*` verbs grouped by namespace, with `devmenu` named as the live list. `AUTONOMOUS_TOOLING.md` stays as the deep manual |
| `Docs/ENGINEERING_PRINCIPLES.md` | ≈ 14 KB | CLAUDE.md's Principles 1-11 plus the tedium section, verbatim. Plus the RestoreAppropriateInputMode counter-example from memory |
| `Docs/VISION.md` | ≤ 8 KB | CLAUDE.md Project Vision + design principles from other games + the Fable5 Charter's "four gates" and pillars (summary) + a pointer to the MedievalDynasty reference and the colony .docx |

### 2.2 System docs

**Kept in place:** a doc that source code names keeps its filename (59 source and tool files name doc paths, and renaming
means editing them). **New file** = no system doc exists today. **git mv** = an existing file becomes the system doc.

| # | System | Home file | Budget | Merged in (all extracted, then the dated sources archived) |
|---|---|---|---|---|
| 1 | World generation, terrain, biomes | `Terrain_Generation_Exploration.md` (kept, 10 src refs) | header + ≤ 8 KB current-state summary on top | memories terrain-gen-exploration, terrain-v4/v5/v6, biome-blend; Terrain_Foundation_Plan (stale → archive). **Option:** move the dated "Update" sections older than 7 days to `Archive/Terrain_Generation_History.md` (doc drops from 119 KB to about 40 KB) |
| 2 | Caves, geology, ore, dig hardness | `Caves_And_Geology.md` (new) | ≤ 15 KB | caves sections of #1, memories caves-and-lighting, geology-and-cave-content, todo-alpine-snow-and-caves; dig-hardness numbers from Technology_History |
| 3 | PCG and vegetation | `PCG.md` (new; old `PCG_Integration_Plan.md` → archive) | ≤ 30 KB | its "how PCG is hosted" section, Ground_Coverage_2026-10-14, memories biome-aware-pcg, pcg-clump, pcg-load-and-spawner-perf, far-trees, terrain-material-far-tiling, volume-stamp-holes (PCG half), world-removal-ledger, world-readiness, playtest-oct14 (PCG half), overnight-polish-oct12 (PCG tiers) |
| 4 | World features / POIs | `World_Features_Architecture.md` (kept) | header added | memory world-features-pois |
| 5 | Water, lakes, swimming | `Water.md` (new) | ≤ 15 KB | Oceanology section of #1, memories oceanology-integration, swimming-and-breath, water-trace (hair part → Engine_Patches), todo-water-and-shores |
| 6 | Drinking and illness | `Water_And_Illness.md` (kept, 8 refs) | header added | memory first-hours-water-and-fire (water half) |
| 7 | Light, darkness, exposure | `Light_Sources.md` (kept, 5 refs) | header added | the below-horizon light gate + UDS interior fix (memories caves-and-lighting, carried-light) |
| 8 | Weather, clock, time | `Weather_And_Time.md` (git mv from `Weather_Bridge_Coop_Investigation.md`) | ≤ 8 KB | memory weather-bridge, clock offline cap (dev_status), time dial (hud-modes), director, 08:00 / Clear_Skies start |
| 9 | Medical, survival, mortality | `Medical_And_Survival.md` (new) | ≤ 15 KB | TECHNICAL_REFERENCE medical cascade, memories mortality-and-survival-intake, survival-stat-pacing, death-recap, swimming (respiration); Thermal_Model_Reproduction + Survival_Single_Source_Proposal (forks → DECISIONS); a link to Codex's `agent/audit/MEDICAL_HANDOFF.md` |
| 10 | Crafting, progression, items | `Crafting_And_Progression.md` (new) | ≤ 20 KB | Progression_FreshLook, Progression_Reachability (incl. the Oct 17 inspect-grant table), memories progression-pass1, item-progression-data-gaps, first-hours (item/fire action registries), wes-calls-oct17; TechProgressionPlan (Feb) as design-intent pointer; `Technology_History.md` stays as the research reference |
| 11 | Terraform, excavation | `Terraform_Excavation_Plan.md` (kept, 5 refs) | header added | memory terraform-excavation-design |
| 12 | Building | `Building.md` (new) | ≤ 8 KB | memories coop-server-driven-construction, oct16-bug-pass (build state look), first-ten-minutes (placement aim fix) |
| 13 | AI, survivors, colony | `AI_And_Colony.md` (new) | ≤ 15 KB | MobAIPlan (as built vs plan), dev_status (survivor state machine, MO.Colony verbs), post-reveal (perception), MO57_Master_Plan colony track, colony section of CLAUDE.md; the MedievalDynasty reference stays |
| 14 | Co-op, sessions, networking | `Coop_And_Networking.md` (new) | ≤ 15 KB | memories steam-multiplayer (minus history), coop-pawn-lifecycle, host-pawn-under-terrain, ue-multiprocess-testing (design half); TECHNICAL_REFERENCE networking |
| 15 | UI | `UI.md` (new) | ≤ 15 KB | UI_Overhaul_Architecture (15 pitfalls), memories commonui-pending-activation-race, hud-yields, hud-modes, volume-stamp (router half), ui-design-system-brief; `UI_TOOLING.md` and `UI_Kit_Implementation.md` stay; `Docs/agent/ui/` is indexed |
| 16 | Persistence, saves, bug reports | `Persistence.md` (new) | ≤ 8 KB | TECHNICAL_REFERENCE save pattern, IMOSaveDomain list, generator identity, crash-endpoint-and-bug-reports, code_health (persistence cluster) |
| 17 | Audio | `Audio.md` (new) | ≤ 8 KB | memories audio-heartbeat, footsteps (sound half), overnight-polish (master volume) |
| 18 | Packaging, cook, Shipping | `Packaging.md` (new) | ≤ 8 KB | memories packaging-dev-build, package-cook-errors, shipping-diag, overnight-polish (cook trap), oceanology (always-cook) |
| 19 | Performance *(my addition: perf facts are spread across 8 memories)* | `Performance.md` (new) | ≤ 10 KB | hitch-hunt, post-reveal-hitches, pcg-load (perf half), footsteps (benchmark tiers), measuring protocol |

**Kept as references (indexed, header only where useful):** `AUTONOMOUS_TOOLING.md` (its dated hit/miss records →
`Archive/AUTONOMOUS_TOOLING_history.md`), `UI_TOOLING.md`, `Voxel_Plugin_Reference.md`, `Item_Art_Pipeline.md`,
`Technology_History.md`, `ElectricDreams_Lessons.md`, `ElectricDreams_Migrated.md`, `MedievalDynasty_Village_Reference.md`,
`Engine_Patches.md` + `EnginePatches/`, `PAUSE_POLICY.md`, `TECHNICAL_REFERENCE.md` (grows to ≤ 30 KB with CLAUDE.md's
implementation notes), `MO57_Colony_Management_Design.docx`, `CS399/` (untouched), `Tools/UE_CSV_FORMAT.md`,
`Tools/terrain_lab/README.md`, `README.md` (its doc map is updated), `Docs/Ground_Coverage_2026-10-14.plan.json` (live data read
by `biome_palettes.py`: stays where it is).

**`PROJECT_STATUS.md`** shrinks to ≤ 12 KB: metrics, track status, and the quality-gates table from the AAA backlog. The full
current file is git-mv'd to `Archive/PROJECT_STATUS_2026-10-15.md` (the audit detail BACKLOG rows point at), and a new slim
file is written at the old path (10 references keep working).

## 3. Layer 3: archive

`Docs/Archive/` stays flat (the existing 5 files are there) plus `Docs/Archive/coop/`. Each moved file keeps its name
(`git mv`). `Docs/Archive/INDEX.md` gets one line per file: date · what it was · where its live content went.
`Docs/Archive/CONSOLIDATION_LEDGER.md` records source file → destination for each extracted item.

| To archive (after extraction) | Blockers / reference fixes |
|---|---|
| Handoff_2026-10-13.md | Named by `MOworldSaveGame.h` (**Codex-uncommitted: wait for Codex's commit**) and `MOWorldRemovalSubsystem.h` |
| Claude_Handoff_Goals_2026-10-13.md (Codex-authored) | none in code; FYI to Codex |
| Night_Report_2026-10-17.md, OVERNIGHT_LOG.md, OVERNIGHT_PLAN_2026-10-09.md, Overnight_Polish_2026-10-12.md | memory refs only |
| BugReports_2026-10-14.md, BugReports_2026-10-16.md | memory refs only |
| Ground_Coverage_2026-10-14.md | `biome_palettes.py`, `native_grass_biomes.py` comments |
| AAA_Polish_Backlog.md | `Config/DefaultEngine.ini` comment (audio section) |
| Progression_FreshLook_2026-10-15.md, Progression_Reachability_2026-10-14.md | `MOCheatSubsystem.cpp` comment ("Docs/Progression_*.md") |
| Survival_Single_Source_Proposal.md, Thermal_Model_Reproduction.md | the first is named by `MOSurvivalStatsComponent.cpp` (**Codex-uncommitted: wait**) |
| Fable5_Autonomous_Pipeline.md, Fable5_PCG_Path.md, Fable5_Village_Handoff.md, MO57_Fable5_Charter.md | Fable5_PCG_Path is named by `MOBiomeDefinitionRow.h` |
| MO57_Master_Plan.md, CommonUI_Migration_Plan.md, commonui_migration_audit.md, UI_Overhaul_Architecture.md, UI_Design_Brief_Prompt.md | Master_Plan is named by `MOColonyManagerSubsystem.h` |
| MobAIPlan.md, PCG_Integration_Plan.md, Terrain_Foundation_Plan.md, Agent_PIE_Testing.md (named twice in `Content/Python/init_unreal.py`), devlog_*.md | |
| `Plugins/MOFramework/Content/Data/StatusReport_2026-02-01.md`, `TechProgression.txt`, `TechProgressionPlan.md` | none |
| `Content/Python/README_WIDGET_AUTOMATION.md`, `README_UI_TOOLS.md` (merged into UI_TOOLING first) | |
| `claude-codex-coop.md` rotation → `Archive/coop/2026-07.md`, `Archive/coop/2026-10.md` | The live log keeps the protocol header, the last ~2 days and any open CLAIM/REQUEST (none open now: CX-0064 released everything). Announced in the log first (see N8) |
| **Not touched:** `Docs/Deep_Dive_Top_10_Flaws_2026-10-16.md`, `Docs/agent/audit/*` (Codex's, untracked), `Docs/agent/**` (Codex-owned per the coop "Active ownership" section) | Indexed; their open items enter BACKLOG with owner Codex |

**Sequencing constraint.** The 5 doc references inside Codex's uncommitted files can't be edited by me. Those two docs
(Handoff_2026-10-13, Survival_Single_Source_Proposal) move only after Codex commits; until then they stay in place, flagged in
INDEX. Updating doc paths in **committed** C++ headers (`MOColonyManagerSubsystem.h`, `MOBiomeDefinitionRow.h`,
`MOWorldRemovalSubsystem.h`, ...) is a comment-only edit, but it rebuilds every dependent. I'd batch all of them into one change,
done in one build window with the editor closed.

## 4. Contradictions: Needs Wes

Each of these goes into DECISIONS.md "Needs Wes" with both versions quoted. Until Wes answers, layer 1 states neither version.

| # | Conflict | Version A | Version B | Interim in layer 1 |
|---|---|---|---|---|
| N1 | **Git after a compile** | CLAUDE.md Workflow: "After every successful compile: `git add -A && git commit -m checkpoint && git push`" (+ Git Operations block) | CLAUDE.md STOP: "only when user explicitly asks"; global Law 6; multi-agent memory: "never `git add -A`"; this task's rule 7 | Not restated. Global Law 6 already applies, and only uncontested staging rules appear |
| N2 | **Plugins** | CLAUDE.md "Planned Plugins: Oceanology, Ultra Dynamic Weather" | Both are installed and in use (Oceanology since Oct 11; UDW inside `Content/UltraDynamicSky`) | Proposed: list both as installed |
| N3 | **PowerShell recipes** | CLAUDE.md CLI: `Get-Content ... -Tail`, `Get-ChildItem -Recurse` | Global: never round-trip file content through PowerShell; ue-unified-cli: drive `ue.py` from bash (PS 5.1 mangles quotes); harness: use Read/Glob | Recipes leave layer 1 → TOOLING.md with bash equivalents. Should the PowerShell versions stay at all? |
| N4 | **Engine build type** | Global CLAUDE.md + AGENTS.md + CLAUDE_BIBLE §4: "UE 5.8 source build" | Project CLAUDE.md (verified Oct 16 and again today): launcher install 5.8.3, `InstalledBuild.txt` | Project layer says launcher. The global file and the bible are Wes's to edit |
| N5 | **Closing the editor to build** | CLAUDE.md / user_preferences / ue-multiprocess: "prompt the user to close the editor" / "ask Wes" | `ue.py cycle` kills and relaunches the editor; overnight practice stops editors Claude launched by PID; global: "never force-kill by process name" | Proposed reading: "ask Wes to close *his* editor; an editor Claude launched may be stopped by its recorded PID". Is that right? |
| N6 | **DataTable source of truth** | CLAUDE.md: CSV + sqlite via `ue_csv_utils.py`, "CSV files = NEVER edit directly", `add-column` for schema (but Items.csv is archived, and Recipes.csv isn't in Data/) | CLAUDE.md: "Live DataTable authoring = `ue.py rows set` (preferred)". `.claude/skills/ue-datatable-json`: edit `*_JSON.json` + reimport. Memory: the `*_JSON.json` mirrors haven't tracked the DTs "since early checkpoints" | Which path is canonical per table (Items, Recipes, Biomes, Quests ...)? Retire or rewrite the skill? |
| N7 | **Codex's doc ownership vs one system doc per topic** | Coop rule 7: "keep durable subsystem-specific details in their owning `Docs/agent/...` record" | This task: one system doc per system, in `Docs/` | Proposed: Docs/agent stays Codex's working state; finished knowledge lands in the system doc. Needs Wes + a Codex ack |
| N8 | **Coop log is append-only vs rotation** | Coop rule 1: "do not rewrite or delete another agent's entry" | This task: rotate old entries to `Archive/coop/YYYY-MM.md` | Rotation moves entries verbatim (nothing rewritten). OK to proceed with an FYI to Codex? Note: everything after Sept 22 in the log is uncommitted, and the rotation commit would carry Codex's entries |
| N9 | **PATCH_NOTES.md** | Its header: "updated incrementally to preserve history" | No entry since June 30; history lives in commits and night reports | Keep updating it, freeze it, or archive it? |

**Stale facts I will correct unless Wes objects (no policy change):** user_preferences' 5.7 build command; build_engine_path's
"CLAUDE.md points at 5.7"; engineering_principles "ten rules"; the "PIE throttles to ~3 fps unfocused" claims (fixed in
EditorSettings.ini); "coop log is untracked"; every "uncommitted / unpushed" claim (all verified pushed); "game level still on
VHG_Flat"; CLAUDE.md's "monolithic module" coupling entry (split in July); CLAUDE.md colony widgets that don't exist (marked
planned); graph counts; AAA backlog forks that Wes already answered (survival pacing; hair patch, now blocked on a source
engine); the "UMOCommonButton is the standard button" wording (clarify: the WBP `MOButton`, never the native class as a node);
"input handlers go in `SetupInputComponent()`" (clarify: in `AMOPlayerController`, some in `InputKey` because Enhanced Input
removed `BindKey`). Also the bible's pointer to "CLAUDE.md §Engineering Principles" will need to point at
ENGINEERING_PRINCIPLES.md; that is Wes's file, so I flag it rather than edit it.

## 5. Open design forks found while inventorying (seed of DECISIONS "Needs Wes")

| # | Fork | Source |
|---|---|---|
| F1 | Survival single source A/B/C (pacing was answered Oct 15; the source question was not) | Survival_Single_Source_Proposal, wes-calls-oct15 |
| F2 | Thermal model: heat balance over a thermal mass, or keep it | Thermal_Model_Reproduction, AAA |
| F3 | Do removed trees, sticks and stones regrow (on game time)? | Handoff #3, world-removal-ledger |
| F4 | Perf target: reference resolution/quality and target hardware tier | Handoff #5, AAA |
| F5 | Real reed meshes: Fab download, or the Electric Dreams plants | AAA, todo-water |
| F6 | Weather: a client-side apply in BP_WeatherBridge (or keep native UDW replication) | weather-bridge, Weather_Bridge doc |
| F7 | Building cancel/refund semantics (queue path full refund vs ghost-menu skill-partial) | dev_status (July 20) |
| F8 | Excavation stage 6 UI (designation tool + dump popup). Wes said the question wasn't clear: re-ask it plainly | wes-calls-oct15, Terraform plan |
| F9 | Real cliff walls: own composition along a steep contour, or generator crags dressed with meshes | world-features memory, bug #27 |
| F10 | Heat-ladder values (station and recipe temperatures). Codex's fields exist | Night_Report #3, Technology_History |
| F11 | Post the engine patch #1 upstream report (public post) | Engine_Patches, 0001-upstream-report |
| F12 | Narrow rivers get no reeds (ShoreFactor samples one ring) | progression-pass1 |
| F13 | Default clock TimeScale (real time 1.0 means weather and day barely change in a session) | terrain-v4 ("his call") |
| F14 | Downloads waiting for a yes: official "Made with Unreal Engine" logo; the UI kit's real fonts | world-readiness, steam-store-art |
| F15 | Character gender source (creator sets everything but gender) | wes-calls-oct15, first-ten-minutes |

H36 (offline crafting) is listed as an open fork in PROJECT_STATUS but as done in dev_status (July 8: 4 h cap, b5d7b44f).
I'll check the code in P3 rather than ask.

## 6. BACKLOG ids (small call, noted per Law 5)

The audit codes C1-C12 / H1-H55 / M1-M24 / L1-L16 stay unchanged, because source comments carry them as fix tags. Every other
tracker's colliding ids get a prefix, and the old id is kept as an alias so searches still hit:
- Codex handoff H00-H26 → `G00-G26`
- bug reports → `BR14-n`, `BR16-n` (crash rows `BR14-C1`)
- AAA backlog items → `P-n`
- old task numbers (#28, #71, #124-#163) → kept as an alias column
- new items → `N-###`

## 7. Process for P3-P6 (as the task specifies, made concrete)

1. CLAIM in `claude-codex-coop.md` the files I'll restructure (Docs/*.md except Codex's, CLAUDE.md, AGENTS.md); RELEASE at
   the end.
2. Build layer 2 by extraction. Every item gets a ledger row (source → destination). A superseded fact is marked superseded in
   place, with a date and what replaced it.
3. Rewrite layer 1 within budget (byte counts checked by script).
4. Archive with `git mv` and fix references (repo grep, memory, AGENTS, CLAUDE, `.claude/`, `python -m graphify query`).
   Source-comment path edits are batched for one editor-closed build window. The two Codex-blocked moves wait.
5. Verify: (a) every path in any .md / memory / AGENTS resolves (refcheck.py reruns, with a zero-dangling target);
   (b) memory links; (c) ledger covers every archived file; (d) before/after sizes; (e) a cold-start test with 10 questions;
   (f) `python -m graphify update .`.
6. Commit only if Wes authorizes it for this task: explicit paths, staged column checked, never settings.local.json or
   MegaMaterialCache.

## 8. What I need from Wes to start P3

1. Approve the structure in sections 1-3, or change it (in particular: the AGENTS.md shim, the 19 system docs, the optional
   split of the 119 KB worldgen doc, and memory retiring to `memory/archive/`).
2. Answers to N1-N9. P3 can start without them; they stay flagged in DECISIONS.
3. Whether commits are authorized for this task (and push).
