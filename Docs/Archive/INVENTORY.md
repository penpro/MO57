# Knowledge inventory (consolidation P1), 2026-10-17

Inventory of every file that holds project knowledge for MO57, made before anything is moved. Phase 2 (the proposed
structure) is in `Docs/Archive/CONSOLIDATION_PROPOSAL.md`. Nothing has been moved, renamed, merged or deleted yet.

**Method.** I measured every file with `wc -c`, took last-commit dates from `git log -1`, and read every memory file in full,
plus the always-loaded files and AGENTS.md. For every doc I read the opening and all section headings, and read in full the
sections that hold open work or decisions. Staleness checks:
(1) a script resolved every repo path, every `UMO/AMO/FMO/IMO/EMO*` symbol and every `MO.*` / `mo.*` console verb named in
the 167 knowledge files against the working tree and source (scratchpad `refcheck.py`);
(2) I checked by hand each claim of the form "uncommitted", "planned", "source build" or "5.7" against git and disk.

**Type codes:** R rule · D decision · B backlog / open work · T trap · S system knowledge (current state, how it works) ·
L log / dated snapshot · F reference (external, research, how-to). **Staleness:** OK = spot-checks match the code;
PARTIAL = some claims out of date (listed); STALE = mostly superseded; HIST = a historical record, staleness doesn't apply.

---

## 1. Totals

| Area | Files | Bytes | Loaded |
|---|---|---|---|
| `CLAUDE.md` (project) | 1 | 46,519 | every Claude session |
| `memory/MEMORY.md` (81 lines) | 1 | 11,729 | every Claude session |
| `~/.claude/CLAUDE.md` (global) | 1 | 3,452 | every Claude session, all projects |
| **Layer 1 total, Claude** | 3 | **61,700** | |
| `AGENTS.md` | 1 | 44,010 | every Codex session |
| memory topic files | 81 | 304,543 | on demand |
| `Docs/**/*.md` | 75 | 1,402,616 | on demand |
| of which `Docs/agent/` (Codex) | 16 | 167,961 | |
| of which `Docs/Archive/` | 5 | 70,873 | |
| of which `Docs/CS399/` | 4 | 70,826 | |
| Root: README, PATCH_NOTES, coop log | 3 | 175,402 | on demand |
| Plugin / Content / Tools / skill docs | 9 | 160,683 | on demand |

---

## 2. Always-loaded layer

### 2.1 `CLAUDE.md` (46,519 B, last commit 2026-10-17). Types: R S F D. Staleness: PARTIAL.

Bytes per section: Engineering Principles 12,598 · Implementation Notes 11,850 · DataTable CSV 3,385 · Project Vision 3,347 ·
Doc table 2,253 · UMG automation 2,207 · Dev environment and 5.8 notes 1,522 · Native refactor roadmap 1,471 · STOP table 1,019 ·
CLI 992 · Code conventions 943 · Graphify 612 · Design principles from other games 582 · rest under 500 each.

Verified stale or contradictory content:
- **Git, three rules that disagree.** "After every successful compile: `git add -A && git commit -m checkpoint && git push`"
  (Workflow Rules, plus the Git Operations block) contradicts the STOP table ("only when user explicitly asks"), global Law 6,
  and the multi-agent rule "never `git add -A`".
- **Planned Plugins** lists Oceanology, which has been installed since Oct 11 (`Plugins/Marketplace/Oceanology_Plugin`, enabled in
  the .uproject). It also lists Ultra Dynamic Weather, which ships inside `Content/UltraDynamicSky` and is used in code (UDW
  MetaSounds, Change Weather).
- **CLI section** gives PowerShell `Get-Content` / `Get-ChildItem` recipes. The global rules say never to round-trip file content
  through PowerShell, and the `ue-unified-cli` memory says to drive `ue.py` from bash because PowerShell 5.1 mangles quotes.
- **CSV section** points at `Plugins/MOFramework/Content/Data/Items.csv` and `Recipes.csv`. Neither exists: Items.csv is in
  `Data/archive/`, and the recipe table is `Recipes.uasset`. Live tables are edited with `ue.py rows set`. The `*_JSON.json` mirrors
  are documented as stale (memory `item-progression-data-gaps`), but the `ue-datatable-json` skill still edits them.
- **Known Coupling: "MEDIUM - Monolithic Module Structure: all 60+ classes in single MOFramework module."** This is stale: the
  module has been split into Core, Medical and MOFramework (plus Editor) since July (C1 phases 1-3).
- **Colony UI table**: `UMOCharacterCardWidget` and `UMOTaskAssignmentWidget` do not exist (the section is labelled Planned).
  The other classes listed there do exist.
- **Graphify**: "11k nodes, 741 code files". Codex's refresh on Oct 17 gave 16,679 nodes.
- **Native refactor roadmap** is dated March 18 with statuses Planning / Not Started, which nobody has re-checked since.

### 2.2 `memory/MEMORY.md` (11,729 B, 81 lines). Types: index. Staleness: OK as an index.
All 81 files are listed and none dangle. 62 of the 81 lines are bold, so the emphasis no longer singles anything out.

### 2.3 `~/.claude/CLAUDE.md` (3,452 B, Wes's global rules). Types: R. Staleness: one claim.
It says "UE 5.8 source build at `D:\UnrealEngine\UE_5.8`". `Engine/Build/InstalledBuild.txt` exists, and `Build.version` is
5.8.3 CL 58210709: this is the Epic launcher install. The project CLAUDE.md was corrected on Oct 16; the global file was not
(it is Wes's file).

### 2.4 `AGENTS.md` (44,010 B, last commit 2026-07-07). Types: R S. Staleness: STALE (it is a fork of CLAUDE.md).
A line diff against CLAUDE.md (903 vs 897 lines) shows **no Codex-specific content at all**. It differs in 4 places, and AGENTS.md
is the stale side in every one:
1. It lacks 4 doc-table rows (Item_Art_Pipeline, Engine_Patches, Technology_History, Light_Sources).
2. It says "UE 5.8 (source build)".
3. It still lists Persistence-Inventory as a CRITICAL circular dependency (fixed Oct 15).
4. Its UMG section says Is Variable cannot be set from Python and points at the legacy scripts (superseded Oct 5 by `ue.py ui`).

---

## 3. `Docs/` (75 .md + 2 .docx)

Src refs = the number of source or tool files (C++, Python, ini, sh) whose comments or strings name the doc. Renaming such a doc
means editing those files.

| Doc | Bytes | Last commit | Type | Staleness / notes | Src refs |
|---|---|---|---|---|---|
| AAA_Polish_Backlog.md | 19,958 | 10-16 | B D | PARTIAL: 11 open / 4 done items, 5 forks. Fork "survival pacing" was answered Oct 15 (only single-source is still open). Fork "hair engine patch needs a yes" was answered Oct 16 and is now blocked on a source engine | 1 |
| AUTONOMOUS_TOOLING.md | 56,125 | 10-14 | F S T | OK, but about half of it is dated hit/miss history | 3 |
| Agent_PIE_Testing.md | 16,845 | 07-20 | F | STALE: June bridge playbook, superseded by `ue.py`. It still says "SubsystemBlueprintLibrary not exposed in 5.7 python" | 2 |
| Archive/UE57_Refactoring_Plan.md | 11,046 | 06-10 | L | HIST, already marked superseded | 0 |
| Archive/UIManagerSplit_TestingNotes.md | 11,807 | 06-10 | L | HIST | 0 |
| Archive/UI_Architecture_Audit.md | 28,114 | 06-10 | L | HIST | 0 |
| Archive/UI_Class_Structure_Report.md | 9,427 | 06-10 | L | HIST | 0 |
| Archive/consolidation-session-checklist.md | 10,479 | 06-10 | L | HIST | 0 |
| BugReports_2026-10-14.md | 12,707 | 10-14 | B L | Tracker for 29 playtest reports plus crash uploads, each with a DONE/OPEN/LOGGED status | 0 |
| BugReports_2026-10-16.md | 20,226 | 10-16 | B L D | Tracker for 22 reports plus 6 crashes; includes Wes's Oct 16 calls | 0 |
| CS399/CS399_FALL2026_PLAN.md | 30,574 | 10-05 | F | OK. It names `MO.Bench.Run` (planned, not built) and `Docs/CS399/playtests/` (not created) | 0 |
| CS399/CS399_FINAL_REPORT.md | 4,797 | 10-04 | F | Skeleton, by design | 0 |
| CS399/CS399_HISTORICAL_CLOSURE.md | 21,376 | 10-04 | F L | HIST | 0 |
| CS399/CS399_WEEKLY_PROGRESS.md | 14,079 | 10-05 | L B | Its own rule is "update every Sunday", but the last entry is Week 2 (Oct 5); Oct 11 has no entry | 0 |
| Claude_Handoff_Goals_2026-10-13.md | 36,531 | 10-13 | B D L | Codex-authored work plan with items H00-H26. **Its IDs collide with the audit's H-codes** (see section 7) | 0 |
| CommonUI_Migration_Plan.md | 35,347 | 05-21 | L S | STALE: March plan. PROJECT_STATUS calls the track superseded. It names 3 classes that don't exist | 0 |
| Deep_Dive_Top_10_Flaws_2026-10-16.md | 25,366 | untracked | B | Codex's uncommitted source audit with 10 ranked flaws; Codex's batches A-G addressed several of them | 0 |
| ElectricDreams_Lessons.md | 9,672 | 10-11 | F | OK | 0 |
| ElectricDreams_Migrated.md | 3,925 | 10-11 | F | OK (asset manifest) | 1 |
| EnginePatches/0001-upstream-report.md | 3,834 | 10-15 | F B | OK. Not posted yet (posting is Wes's call) | 0 |
| Engine_Patches.md | 8,976 | 10-16 | R S B | OK | 2 |
| Fable5_Autonomous_Pipeline.md | 21,534 | 10-14 | B L | STALE as "current focus" (July campaign). Still-open cards: F1 wet wood, T3 cut 2, V3 Fab, winter soak | 0 |
| Fable5_PCG_Path.md | 9,980 | 07-04 | L | STALE: the July goal doc, since implemented differently (biome spawner) | 1 |
| Fable5_Village_Handoff.md | 12,747 | 07-04 | L | STALE: July plan; V0-V2.5 landed | 0 |
| Ground_Coverage_2026-10-14.md | 11,641 | 10-14 | S L | OK. Its `.plan.json` sibling is live data read by `biome_palettes.py` | 2 |
| Handoff_2026-10-13.md | 63,496 | 10-14 | L D B | Opens with "Decisions waiting for Wes" (8): #2 drinking was built Oct 15, #4 H13 was decided, #6/#7 are closed. The rest is a dated log | 2 (1 in a Codex-uncommitted file) |
| Item_Art_Pipeline.md | 6,141 | 10-14 | F | OK | 0 |
| Light_Sources.md | 11,314 | 10-17 | S | OK. **Uncommitted edit** (torch wear, Claude's, waits on Codex's equipment commit) | 5 |
| MO57_Fable5_Charter.md | 24,405 | 07-03 | L D | HIST: July north-star charter | 0 |
| MO57_Master_Plan.md | 18,433 | 03-31 | L | STALE: the CLAUDE.md doc table still routes UI and colony work here | 1 |
| MedievalDynasty_Village_Reference.md | 16,922 | 10-14 | F | OK (design north star) | 0 |
| MobAIPlan.md | 16,183 | 02-18 | L S | PARTIAL: February plan; creatures built (deer, wolf) | 0 |
| Night_Report_2026-10-17.md | 7,206 | 10-17 | L D B | Current: 2 of its 4 calls answered and built; open items listed | 0 |
| OVERNIGHT_LOG.md | 48,548 | 10-09 | L | HIST (Oct 7-9 runs) | 0 |
| OVERNIGHT_PLAN_2026-10-09.md | 4,658 | 10-08 | L | HIST | 0 |
| Overnight_Polish_2026-10-12.md | 37,330 | 10-13 | L T | HIST. Says "all UNCOMMITTED", which is now false (pushed Oct 13) | 0 |
| PAUSE_POLICY.md | 5,107 | 05-23 | R | OK | 10 |
| PCG_Integration_Plan.md | 11,569 | 10-12 | L S | STALE: names 5 classes that were never built; its last section ("how PCG is hosted", Oct 12) is current | 0 |
| PROJECT_STATUS.md | 83,404 | 10-15 | B S L | PARTIAL: tracker rows still carry "Status (Jun 11)" text, corrected by re-verification banners (Jul 7, Jul 8, Oct 13, Oct 14). 107 C/H/M/L rows | 0 |
| Progression_FreshLook_2026-10-15.md | 19,915 | 10-15 | S L B | OK (passes 1-4) | 0 |
| Progression_Reachability_2026-10-14.md | 11,911 | 10-17 | S B | OK (updated Oct 17 with the inspect-grant table) | 1 (prefix) |
| Survival_Single_Source_Proposal.md | 4,887 | 10-13 | D | Open fork (A/B/C); the pacing part was answered Oct 15 | 1 (Codex-uncommitted) |
| TECHNICAL_REFERENCE.md | 16,737 | 10-11 | S R | PARTIAL: header says "Last updated May 17" but has Oct sections; `Tools/perf_experiments` doesn't exist (`.sh`) | 0 |
| Technology_History.md | 19,799 | 10-17 | F D | OK (research) | 1 |
| Terraform_Excavation_Plan.md | 17,477 | 10-14 | S D B | OK. Stage 6 needs Wes's checkpoint. Names 3 planned symbols | 5 (1 Codex-uncommitted) |
| Terrain_Foundation_Plan.md | 14,538 | 10-09 | L | STALE: graph recipes for VHG_Realistic, superseded by the C++ generator | 0 |
| Terrain_Generation_Exploration.md | 118,984 | 10-17 | S L D T B | OK, but it is a newest-first stack of dated updates (Oct 9-17) with to-do lists inside | 10 |
| Thermal_Model_Reproduction.md | 5,045 | 10-13 | D | Open fork (heat balance) | 0 |
| UI_Design_Brief_Prompt.md | 12,104 | 10-11 | L | HIST: the prompt that produced the UI kit | 0 |
| UI_Kit_Implementation.md | 6,413 | 10-12 | S T | OK | 0 |
| UI_Overhaul_Architecture.md | 24,215 | 03-31 | S T L | PARTIAL: 15 pitfalls still useful; names 2 classes that don't exist | 0 |
| UI_TOOLING.md | 12,309 | 10-11 | F | OK | 6 |
| Voxel_Plugin_Reference.md | 62,772 | 10-14 | F T | PARTIAL: titled "Pro 2.0" (we use the open-source dev-phy build); `Content/Penumbra/Sandbox/` doesn't exist | 0 |
| Water_And_Illness.md | 6,217 | 10-15 | S | OK | 8 |
| Weather_Bridge_Coop_Investigation.md | 5,167 | 10-09 | S D | OK; open fork (client-side apply) | 0 |
| World_Features_Architecture.md | 34,121 | 10-16 | S | Design plus as-built; 6 planned symbols | 1 |
| agent/*.md (8 files, July repo/MCP audit) | 34,003 | 07-20 | L B | Codex's. Most say "Status: In progress" and were never closed | 0 |
| agent/audit/MEDICAL_HANDOFF.md | 7,204 | untracked | S B | Codex's, current (Oct 17) | 0 |
| agent/audit/SESSION_STATE.md | 45,573 | untracked | L B | Codex's live state (batches A-G, closeout 13:39) | 0 |
| agent/ui/*.md (6 files) | 81,181 | 07-20 | L S | Codex's UI audit; Stage 4 is next | 1 |
| commonui_migration_audit.md | 30,392 | 05-21 | L | HIST | 0 |
| devlog_2026-02-18.md, devlog_2026-03-18.md | 9,823 | 02/03 | L | HIST | 0 |
| MO57_ClaudeCode_Plan.docx | 30,939 | 03-31 | L | HIST (binary, not grep-able) | 0 |
| MO57_Colony_Management_Design.docx | 34,730 | 03-31 | D F | Colony design source (CLAUDE.md points at it) | 0 |

---

## 4. Other repo knowledge files

| File | Bytes | Last commit | Type | Staleness / notes |
|---|---|---|---|---|
| README.md | 7,475 | 07-20 | F | OK (Codex refreshed it July 12). It has its own doc map |
| PATCH_NOTES.md | 62,172 | 06-30 | L | Frozen: the last entry is the 5.8 upgrade (June 30); nothing since |
| claude-codex-coop.md | 105,755 | **09-22** | L R | Tracked, but every entry after Sept 22 (~65 Oct entries) is uncommitted. Mixed July/Oct entries, 112 messages, protocol header at top. The `multi-agent-coop` memory says the file is "untracked", which is wrong |
| Plugins/MOFramework/README.md | 75,404 | 06-30 | F S B | PARTIAL: modder-facing overview plus "Technical Debt", "Code Health Metrics" and "Roadmap" sections from June |
| Plugins/MOFramework/Content/Data/TechProgressionPlan.md | 31,407 | 02-12 | D L | February 8-era design plan; partly superseded by Technology_History.md |
| Plugins/MOFramework/Content/Data/TechProgression.txt | 18,046 | 02-01 | L | Generated from the February CSVs: STALE |
| Plugins/MOFramework/Content/Data/StatusReport_2026-02-01.md | 5,076 | 02-01 | L | HIST |
| Plugins/MOFramework/Content/Items/_FolderStructure.md | 1,513 | 02-01 | R | Convention doc, not checked against content |
| Content/Python/README_UI_TOOLS.md | 4,779 | 03-31 | F | PARTIAL (MOWidgetEditorUtils API; `audit_output/` dir absent) |
| Content/Python/README_WIDGET_AUTOMATION.md | 12,283 | 10-05 | F | STALE, says so in its own banner |
| Tools/UE_CSV_FORMAT.md | 6,103 | 02-18 | F | PARTIAL (points at Items.csv, which is archived) |
| Tools/terrain_lab/README.md | 971 | 10-11 | F | OK |
| .claude/skills/ue-datatable-json/SKILL.md | 5,101 | 05-21 | R F | STALE workflow: edits `*_JSON.json`, which do not track the live DTs; names `FMOWeaponDefinitionRow` (absent) |
| SteamArt/README.md, DesignKit/.../README.md + screens/*.md | ~18 K | 10-12 | F | Asset-pack docs; out of scope (indexed only) |
| `.claude/worktrees/quirky-snyder/` | | 04-04 | | A stale git worktree with April copies of CLAUDE.md / README / PATCH_NOTES. Out of scope; pruning it is Wes's call |

---

## 5. Memory directory (81 topic files)

Class: P preference/working style · D binding decision · T traps · S system state · K backlog/to-do · F external reference ·
H history/log. Stale means a claim in the file has been checked and is no longer true.

| File | Bytes | Class | Staleness / notes |
|---|---|---|---|
| user_preferences.md | 2,577 | P | Build command uses the **UE_5.7** path (wrong, contradicts build_engine_path and CLAUDE.md). Top-level `type:` frontmatter (old format) |
| engineering_principles.md | 3,019 | R | Restates CLAUDE.md ("ten rules"; CLAUDE.md has 11). One example (RestoreAppropriateInputMode) exists only here |
| architecture_decisions.md | 3,366 | S R | OK, except `IMOCraftingCapable` does not exist |
| project_overview.md | 2,066 | S | OK; graph counts stale |
| build_engine_path.md | 1,687 | T | Its headline ("CLAUDE.md's build commands point at 5.7") is fixed now; the rest is valid |
| code_health.md | 5,174 | S B H | June 11 audit snapshot plus a July 7 update; superseded by the PROJECT_STATUS banners |
| dev_status.md | 17,058 | H S T D | Header "Current focus (as of July 7)" is stale. Holds ~14 still-live gotchas and the July 8 Wes directives |
| git-staging-verification.md | 1,250 | P T | OK |
| graphify.md | 1,092 | F | Counts stale; otherwise restates CLAUDE.md |
| multi-agent-coop.md | 2,616 | P T | Calls the coop log "untracked" (it is tracked) |
| miktex-package-installs.md | 1,272 | P | OK |
| overnight-runner-ov-bat.md | 1,539 | P F | OK |
| verify-the-asset-before-fixing-it.md | 1,412 | P | OK |
| weather-bridge-not-native-uds.md | 2,187 | D T K | OK; open fork (client-side apply) |
| wes-calls-oct15.md / -oct16.md / -oct17.md | 7,239 | D | OK |
| directive-oct13-far-trees-tiling-snow.md | 1,788 | D | OK; includes a permission scoped to that directive (commit+push, CC0 downloads) |
| terraform-excavation-design.md | 6,777 | D S K | OK; stage 6 checkpoint open |
| survival-stat-pacing-fork.md | 2,039 | D K | Pacing RESOLVED Oct 15; single-source fork open |
| item-progression-data-gaps.md | 4,454 | S K T | PARTIAL: the "still open" list predates the Oct 15-17 passes (126/136 reachable now) |
| terrain-gen-exploration.md | 4,565 | S T | Says the game level is "still on VHG_Flat" (switched Oct 10) |
| terrain-v4-night-2026-10-10.md | 3,987 | H D T | Says "All UNCOMMITTED", which is false (VHG_MOTerrain committed 9835c8be Oct 11) |
| terrain-v5-biome-relief.md | 6,009 | H D T | Same "UNCOMMITTED" claim, also false |
| terrain-v6-mountain-ranges.md | 2,305 | H D T | Same. Its "12 km tear = plugin" was corrected by volume-stamp-holes (Oct 14) |
| caves-and-lighting.md | 7,531 | S T | "All UNCOMMITTED" is false; the Oct 17 v9 appendix is current |
| geology-and-cave-content.md | 5,000 | S D T | OK |
| carried-light-and-cave-exposure.md | 3,081 | S T | OK |
| todo-alpine-snow-and-caves.md | 2,358 | K | 1 of 3 items open |
| todo-terrain-material-look.md | 1,527 | K | Says "nothing built", but parts were built Oct 13 (far tiling, snow, soft thresholds) |
| todo-water-and-shores.md | 2,533 | K | Item 4 (swim) was done Oct 11 but is not marked |
| biome-aware-pcg-overnight.md | 5,436 | S T | OK |
| biome-blend-surface-weights.md | 1,868 | S | OK |
| far-trees-materials-clutter-oct13.md | 3,083 | S T | OK |
| terrain-material-far-tiling.md | 3,023 | S T | OK |
| pcg-clump-compensation-and-coverage.md | 3,550 | S T | OK (Oct 16 drift note current) |
| pcg-load-and-spawner-perf.md | 4,969 | S T K | "Harvested PCG instances are NOT persisted" was fixed by the removal ledger (7e145b73) |
| playtest-oct14-lessons.md | 4,294 | S T D | OK |
| post-reveal-hitches-and-ai-perception.md | 3,738 | S T K | OK |
| volume-stamp-holes-and-commonui-router.md | 2,773 | S T | OK |
| world-features-pois-and-electric-dreams.md | 3,837 | D S T | OK |
| world-removal-ledger.md | 1,783 | S R | OK |
| world-readiness-and-splash.md | 3,137 | S K | "UNCOMMITTED (Oct 12)" is false |
| overnight-polish-oct12.md | 10,195 | H S T | "NOTHING is committed" is false |
| shipping-diag-and-zero-pcg.md | 2,078 | T | OK |
| electric-dreams-env-project.md | 1,619 | F | OK |
| audio-heartbeat-and-menu-return.md | 6,490 | S T D | Engine "5.8.1" is now 5.8.3; the XAudio2 switch is not yet verified at launch |
| coop-pawn-lifecycle.md | 5,899 | S T | "All UNCOMMITTED when written" is false; the Z=200 fallback was replaced (host-pawn-under-terrain) |
| coop-server-driven-construction.md | 1,890 | D R | "Butchering may still be client-side" is superseded (Oct 16 world actions) |
| crash-endpoint-and-bug-reports.md | 3,365 | F | OK |
| death-recap-and-respawn.md | 3,651 | S T | "UNPUSHED" is false (4d58ed3b is on origin/master) |
| dev-console-popup-and-starter.md | 3,166 | S T | OK |
| engine-installed-build-patches.md | 1,969 | R T | OK |
| fab-sourcing-and-icon-studio.md | 2,478 | F D T | OK |
| first-hours-water-and-fire.md | 2,808 | S K | Open nits partly fixed by Oct 15-17 producers |
| first-ten-minutes-pass.md | 4,101 | F T K | OK |
| footsteps-and-graphics-quality.md | 5,000 | S T | OK |
| hitch-hunt-lake-query.md | 2,431 | T R | OK |
| host-pawn-under-terrain.md | 3,880 | S T | OK |
| hud-modes-and-time-dial.md | 1,182 | S | OK |
| hud-yields-to-menus.md | 2,118 | S R | OK |
| mortality-and-survival-intake.md | 1,907 | S K | "Drinking from water bodies has no path" was fixed Oct 15 |
| oceanology-integration.md | 4,354 | S T | "Uncommitted" and "pawn walks on the seabed" are false (swimming Oct 11) |
| oct16-bug-pass-night.md | 6,310 | S T | OK |
| package-cook-errors-oct16.md | 2,459 | T F | OK |
| packaging-dev-build.md | 1,840 | F T | OK |
| progression-pass1-materials.md | 4,586 | S K | "Open: discovery" was done Oct 17 |
| steam-multiplayer-integration.md | 12,970 | S H T | About 40% is history (a wrong diagnosis, the WBP setup); several "uncommitted" claims are false; "clock/weather not synced" was fixed Oct 8 |
| steam-store-art.md | 1,473 | F | "Uncommitted" is false |
| swimming-and-breath.md | 5,746 | S T K | OK. Note: it says the PIE 3 fps throttle is fixed permanently (verified: `bThrottleCPUWhenNotForeground=False` in EditorSettings.ini), so the "PIE runs at 3 fps" warnings in 5 other memories are stale |
| ue-editor-mcp.md | 3,736 | F P | Links `[[claude_bridge]]`, which doesn't exist. Says "do not write off the MCP" (a Wes preference) |
| ue-multiprocess-testing.md | 5,906 | F T | OK |
| ue-ui-toolset.md | 4,269 | F T | Restates Docs/UI_TOOLING.md; "PIE ~3 fps" is stale |
| ue-unified-cli.md | 3,743 | F T | Restates AUTONOMOUS_TOOLING; "~3fps" is stale |
| ue58-ui-render-traps.md | 3,302 | T | OK |
| ue_pie_computer_testing.md | 8,738 | F T | PARTIAL: a UE_5.7 shortcut, and "UE 5.7 InputMappingContext"; mostly superseded by ue.py. The key bindings and capture-lock trap are still valid |
| ui-design-system-brief.md | 1,955 | D K | The kit arrived Oct 12 (UI_Kit_Implementation.md) |
| water-trace-channel-underwater-and-hair.md | 5,709 | H T D | Water part superseded (Oceanology); "Oceanology installed in NO engine" is false; hair part current (engine patch #1) |
| animation-retarget-pipeline.md | 3,691 | F T | OK |
| commonui-pending-activation-race.md | 2,236 | S R | "Uncommitted" is false |
| cs399-practicum.md | 6,294 | F K P | OK; personal (instructor name stays out of the repo) |

**Memory hygiene:** 8 files have a `name:` that differs from the file stem. 4 still use the old top-level `type:` frontmatter.
Two links don't resolve: `[[claude_bridge]]` (ue-editor-mcp, twice) and `[[ue-pie-computer-testing]]` (caves-and-lighting,
terrain-v4: the name is "UE PIE Computer Testing").

---

## 6. Reference check results (dangling names in the knowledge files)

After removing false positives (plugin-relative paths, placeholders such as `MO.Test.X`, and classes a plan doc labels as
future), these names in live-routed files do not resolve:

| Where | Unresolved | Meaning |
|---|---|---|
| CLAUDE.md, AGENTS.md, Tools/UE_CSV_FORMAT.md | `Plugins/MOFramework/Content/Data/Items.csv`, `.../Recipes.csv` | CSV workflow points at files that moved or no longer exist |
| CLAUDE.md, AGENTS.md, Master_Plan, Fable5 docs, MD reference | `UMOCharacterCardWidget`, `UMOTaskAssignmentWidget` | Colony widgets planned, never built |
| memory architecture_decisions | `IMOCraftingCapable` | Never existed under that name |
| AUTONOMOUS_TOOLING, Terrain_Generation_Exploration, memory water-trace | `MO.Water.UnderwaterTrace` | Removed with Epic Water (Oct 11) |
| CS399 plan, memory cs399 | `MO.Bench.Run` | Planned, not built |
| Terraform_Excavation_Plan | `MO.Terraform.Designate`, `FMODumpDestination`, `UMODumpDestinationContextMenu` | Stage 6, not built yet |
| PCG_Integration_Plan | 5 classes (`UMOPCGSpawnerSubsystem`, ...) | Plan never built that way |
| TECHNICAL_REFERENCE | `Tools/perf_experiments` | Actual file is `Tools/perf_experiments.sh` |
| Voxel_Plugin_Reference | `Content/Penumbra/Sandbox/...`, `Content/Voxel/Curves/` | Sandbox map absent |
| .claude skill ue-datatable-json | `FMOWeaponDefinitionRow` | Absent |
| claude-codex-coop.md | `Content/Python/test_terraform_increment.py`, `Tools/validate_ui` (a prefix) | Removed or renamed scripts |

Everything else resolved: 2,532 path, symbol and verb references across 167 files.

---

## 7. ID collisions across trackers (one BACKLOG must not merge these blindly)

| ID | Audit tracker (PROJECT_STATUS, also in code tags) | Other meaning |
|---|---|---|
| H10 | audio "breathe" drift | Codex handoff H10: survival single source (used in Handoff_2026-10-13 and Survival_Single_Source_Proposal) |
| H11 | blood-death threshold | Codex handoff H11: thermal model (Thermal_Model_Reproduction.md title) |
| H13 | weather to body temp | Codex handoff H13: generator/save compatibility (used in memory mortality, Handoff) |
| H17-H26 | MP authority, harvest, persistence | Codex handoff H17 swimming, H22 excavation, H23 catalog reuse ... |
| C1 | monolithic module | BugReports_2026-10-14 crash row "C1" |
| #N | old task numbers (#28, #71, #124-#163) | BugReports "#2", "#15" ... per report file |

The audit codes appear in source comments as fix tags (for example "each fix carries its code tag" in PROJECT_STATUS), so they
cannot be renumbered.

---

## 8. Facts verified while inventorying (they feed the contradiction list)

- Engine: `D:\UnrealEngine\UE_5.8` is a launcher install (`InstalledBuild.txt` present), version 5.8.3, CL 58210709.
- Editor PIE throttle is disabled in the user config: `bThrottleCPUWhenNotForeground=False`, `bAllowSlateThrottling=False`.
- Oceanology is installed and enabled. Ultra Dynamic Weather ships inside `Content/UltraDynamicSky`.
- `claude-codex-coop.md` is tracked; its last commit is Sept 22.
- Work that memories call "uncommitted" was checked against git and is committed and pushed: VHG_MOTerrain (9835c8be),
  readiness (68d999b0), Oceanology integration (648f8bf9), sessions (e5ed6861), SteamArt (768e8fe7), death recap
  (4d58ed3b, on origin/master).
- Codex has released every claim (CX-0064, 13:39 PDT). Codex's uncommitted files include 5 source files that name docs:
  `MOworldSaveGame.h` names Handoff_2026-10-13.md, `MOSurvivorJobTypes.h` names Terraform_Excavation_Plan.md,
  `MOSurvivalStatsComponent.cpp` names Survival_Single_Source_Proposal.md, and `MOPlayerController.h/.cpp` name PAUSE_POLICY.md.
  Claude's uncommitted `MOCarriedLightComponent.h` names Light_Sources.md.
- 59 source/tool files in total name a doc path. The most-referenced docs are Terrain_Generation_Exploration (10),
  PAUSE_POLICY (10), Water_And_Illness (8), UI_TOOLING (6), Terraform_Excavation_Plan (5) and Light_Sources (5).
