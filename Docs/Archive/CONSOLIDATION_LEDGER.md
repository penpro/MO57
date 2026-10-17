# Consolidation ledger (2026-10-17)

Working record of the knowledge consolidation (`Docs/Archive/CONSOLIDATION_PROPOSAL.md`). One row per extracted item:
source file (and section) -> destination(s). "verbatim" = copied unchanged; "condensed" = summarised with the full text kept
at the destination named; "superseded" = marked in place with the date and what replaced it. Every archived or retired file
must appear here before it moves (P6 check c).

Memory paths are relative to `C:\Users\penum\.claude\projects\D--UEProjects-MO57\memory\`. Retired memory files move to
`memory/archive/` (outside the repo: some are personal).

## CLAUDE.md (46,519 B before) -> layer 2

| Section | Destination | How |
|---|---|---|
| Graphify knowledge graph | `Docs/TOOLING.md` § Graphify; one line in CLAUDE.md | verbatim + condensed |
| STOP table | CLAUDE.md (rewritten, same rows + new rows) | condensed |
| Engineering Principles 1-11, Tedium, "too slow" ladder | `Docs/ENGINEERING_PRINCIPLES.md` § The rules | verbatim (checked by diff) |
| Consolidated Documentation table | `Docs/INDEX.md` + CLAUDE.md router | rewritten (routes to the new docs) |
| Design Policies (no pause) | CLAUDE.md one line + `Docs/PAUSE_POLICY.md` (unchanged) | condensed |
| Project Vision (pillars, multiplayer, pawns, skills, worldgen, lore, phases, modding) | `Docs/VISION.md` | verbatim |
| Development Environment + UE 5.8 Migration Notes | `Docs/TECHNICAL_REFERENCE.md` | verbatim; engine line updated to 5.8.3 (Wes, Oct 17) |
| Workflow Rules (close editor; git add -A + commit + push after compile; user confirms) | `Docs/DECISIONS.md` N1 + N5 (answered Oct 17); CLAUDE.md git rules | superseded by Wes's answers |
| Research Guidelines | `Docs/TECHNICAL_REFERENCE.md` | verbatim |
| Code Conventions | `Docs/TECHNICAL_REFERENCE.md` (+ clarification on InputKey) | verbatim |
| Standard Utility Classes | `Docs/TECHNICAL_REFERENCE.md` | verbatim |
| UE5.8 Native Refactoring Roadmap | `Docs/TECHNICAL_REFERENCE.md` | verbatim + note that statuses are unverified since March |
| Common UI Standards | `Docs/TECHNICAL_REFERENCE.md` (+ clarification: WBP MOButton as node) | verbatim |
| Architecture | `Docs/TECHNICAL_REFERENCE.md` | verbatim |
| Implementation Notes: Subsystem / Component / Interface / Replication / DataTable / Async / Save-Load / Best practices / Decoupling / Coupling / Pending BP setup | `Docs/TECHNICAL_REFERENCE.md` § Implementation Notes | verbatim; "Monolithic module" marked superseded |
| Implementation Notes: Colony Management System (Planned) | `Docs/AI_And_Colony.md` | verbatim |
| Implementation Notes: Medical System Cascade | `Docs/Medical_And_Survival.md` | verbatim |
| Implementation Notes: UI Widget Patterns | `Docs/UI.md` | verbatim |
| Plugins (installed) + Planned Plugins | `Docs/TECHNICAL_REFERENCE.md` § Plugins | Oceanology + UDW moved to installed (Wes N2) |
| UE DataTable CSV Manipulation | `Docs/TOOLING.md` § DataTables | verbatim; stale CSV paths marked |
| CLI Commands (PowerShell build / file ops / git) | `Docs/TOOLING.md` § Build and fallbacks | verbatim as PowerShell fallback (Wes N3) |
| Design Principles (SCUM, Zomboid, ...) | `Docs/VISION.md` | verbatim |
| UE Python Widget Blueprint Automation | `Docs/UI_TOOLING.md` (already had it) + `Docs/UI.md` pointer | verbatim check |

## Docs/TECHNICAL_REFERENCE.md (16,737 B before; rebuilt in place)

| Section | Destination | How |
|---|---|---|
| CommonUI Architecture, UI Controller Architecture | `Docs/UI.md` | verbatim; `ECommonInputMode::All` lines marked superseded there |
| Networking & Replication | stays | verbatim |
| Performance Guidelines (incl. "Periodic work") | stays | verbatim |
| Creature AI Architecture | `Docs/AI_And_Colony.md` | verbatim |
| Medical Cascade | `Docs/Medical_And_Survival.md` | verbatim (dedup with the CLAUDE.md copy) |
| Subsystem inventory + Dependency map | stays | verbatim; C2 + M11 marked superseded |
| Crafting Validation Pattern | `Docs/Crafting_And_Progression.md` | verbatim |
| Key Subsystem APIs: Job assignment, Colony membership | `Docs/AI_And_Colony.md` | verbatim |
| Key Subsystem APIs: Standard UI delegates | `Docs/UI.md` | verbatim |
| Colony Management Design (Planned) | `Docs/AI_And_Colony.md` | verbatim |
| Presentation Systems: Footsteps | `Docs/Audio.md` | verbatim |
| Presentation Systems: Crouch | stays (§ Character movement) | verbatim |
| Presentation Systems: Graphics quality | `Docs/Performance.md` | verbatim |
| Presentation Systems: HUD breath meter | `Docs/UI.md` | verbatim |
| Data Pipeline | `Docs/TOOLING.md` § DataTables | verbatim |
| Systems Confirmed Custom | stays | verbatim |

## Other sources

| Source | Item | Destination | How |
|---|---|---|---|
| `Docs/MO57_Fable5_Charter.md` §1, §2 | "What A-rated means", "The four gates" | `Docs/VISION.md` | verbatim |
| memory `architecture_decisions.md` | core-contract-and-registration ADR, CommonUI Menu mode, interface decoupling, DataTable-driven, medical module, systems custom | `Docs/TECHNICAL_REFERENCE.md` § Module structure; `Docs/UI.md` (Menu mode) | condensed / verbatim |
| memory `engineering_principles.md` | ten rules (restated CLAUDE.md), RestoreAppropriateInputMode counter-pattern, FMOCraftResult + click-outside templates | `Docs/ENGINEERING_PRINCIPLES.md` § Examples | the unique parts verbatim |
| memory `project_overview.md` | three-module structure | `Docs/TECHNICAL_REFERENCE.md` § Module structure | condensed |

## Memory directory (81 topic files + MEMORY.md) -> destinations

Kept = still a memory file (rewritten where marked); retired = moved to `memory/archive/` after its facts reached the destinations named.
Rewritten files keep their original as `memory/archive/<name>.orig.md`.

| Memory file | Fate | Facts now in |
|---|---|---|
| MEMORY.md | rewritten (26 lines) | (index) |
| user_preferences.md | kept, rewritten | stale 5.7 build command dropped (superseded by build_engine_path); commit rule -> DECISIONS N1 |
| engineering_principles.md | retired | `ENGINEERING_PRINCIPLES.md` (rules = CLAUDE.md text; RestoreAppropriateInputMode + FMOCraftResult + click-outside examples) |
| architecture_decisions.md | retired | `TECHNICAL_REFERENCE.md` § Module structure, `UI.md` §2 (Menu mode), `Medical_And_Survival.md` §1 |
| project_overview.md | kept, rewritten | CLAUDE.md header, `PROJECT_STATUS.md` |
| build_engine_path.md | kept, rewritten | `TOOLING.md` §2, `TRAPS.md` §2 |
| code_health.md | retired | `BACKLOG.md` § Audit, `PROJECT_STATUS.md` strengths + systemic patterns, `Persistence.md` |
| dev_status.md | retired | `DECISIONS.md` (July 8, July 20 refund fork F7), `AI_And_Colony.md` (survivor machine, verbs, gotchas), `TRAPS.md` (14 gotchas), `BACKLOG.md` |
| git-staging-verification.md | kept | `TRAPS.md` §1 too |
| graphify.md | retired | CLAUDE.md, `TOOLING.md` §5 |
| multi-agent-coop.md | kept, rewritten | "untracked" claim corrected; ownership-as-of-July dropped (stale) |
| miktex-package-installs.md | kept | `TRAPS.md` (assets) |
| overnight-runner-ov-bat.md | kept | `TOOLING.md` §9 |
| verify-the-asset-before-fixing-it.md | kept (link fixed) | `ENGINEERING_PRINCIPLES.md` lessons |
| weather-bridge-not-native-uds.md | retired | `Weather_And_Time.md` §3, `DECISIONS.md` 2026-10-08 + F6 |
| wes-calls-oct15.md, wes-calls-oct16.md, wes-calls-oct17.md | retired | `DECISIONS.md` 2026-10-15 / 16 / 17 (verbatim) |
| directive-oct13-far-trees-tiling-snow.md | retired | `DECISIONS.md` 2026-10-13 (incl. scoped permission) |
| terraform-excavation-design.md | retired | `DECISIONS.md` July 13 / July 8, `Terraform_Excavation_Plan.md` header, `BACKLOG.md` F8 |
| survival-stat-pacing-fork.md | retired | `DECISIONS.md` 2026-10-15 + F1, `Medical_And_Survival.md` §3 |
| item-progression-data-gaps.md | retired | `Crafting_And_Progression.md` §6-§7, `PROGRESSION_ROADMAP.md` §5, `TRAPS.md` §4 |
| terrain-gen-exploration.md | retired | `Terrain_Generation_Exploration.md` header, `TRAPS.md` §8 |
| terrain-v4-night-2026-10-10.md | retired | `DECISIONS.md` 2026-10-10, `Water.md` §1, `TRAPS.md`, `BACKLOG.md` (F13) |
| terrain-v5-biome-relief.md | retired | `DECISIONS.md` 2026-10-10, `AI_And_Colony.md` (recruitment), `PCG.md` (collision policy), `TRAPS.md` |
| terrain-v6-mountain-ranges.md | retired | `DECISIONS.md` 2026-10-11, `TRAPS.md` §8 (cm units), `Caves_And_Geology.md` §7 (tear superseded) |
| caves-and-lighting.md | retired | `Caves_And_Geology.md`, `Light_Sources.md` (below-horizon), `AI_And_Colony.md` (blackboard), `TRAPS.md` |
| geology-and-cave-content.md | retired | `Caves_And_Geology.md`, `DECISIONS.md` 2026-10-17 |
| carried-light-and-cave-exposure.md | retired | `Light_Sources.md`, `TRAPS.md` §12 |
| todo-alpine-snow-and-caves.md | retired | `BACKLOG.md` N-014, `DECISIONS.md` 2026-10-13 |
| todo-terrain-material-look.md | retired | `BACKLOG.md` N-013 / F21, `Terrain_Generation_Exploration.md` header (material) |
| todo-water-and-shores.md | retired | `BACKLOG.md` N-032, N-050, N-052, N-053 (swim item marked done) |
| biome-aware-pcg-overnight.md | retired | `PCG.md` §2-§4, `TRAPS.md` §9 |
| biome-blend-surface-weights.md | retired | `PCG.md` §4, `DECISIONS.md` 2026-10-14, `BACKLOG.md` N-015 |
| far-trees-materials-clutter-oct13.md | retired | `PCG.md` §3, `Terrain_Generation_Exploration.md` header, `TRAPS.md` |
| terrain-material-far-tiling.md | retired | `Terrain_Generation_Exploration.md` header, `TRAPS.md` §12 |
| pcg-clump-compensation-and-coverage.md | retired | `PCG.md` §4-§5 |
| pcg-load-and-spawner-perf.md | retired | `PCG.md` §6, §8, `Performance.md`, `TRAPS.md` |
| playtest-oct14-lessons.md | retired | `PCG.md`, `Crafting_And_Progression.md` (E on nodes), `DECISIONS.md` 2026-10-14 (item art), `TRAPS.md` |
| post-reveal-hitches-and-ai-perception.md | retired | `Performance.md` §4, `AI_And_Colony.md` §1, `TRAPS.md` §16 |
| volume-stamp-holes-and-commonui-router.md | retired | `Caves_And_Geology.md` §2, `UI.md` §2, `TRAPS.md` |
| world-features-pois-and-electric-dreams.md | retired | `World_Features_Architecture.md` header, `DECISIONS.md` 2026-10-14, `TRAPS.md` §10 |
| world-removal-ledger.md | retired | `PCG.md` §7 |
| world-readiness-and-splash.md | retired | `PCG.md` §8, `BACKLOG.md` N-144, F14 |
| overnight-polish-oct12.md | retired | `PCG.md` §3, `UI.md`, `Audio.md`, `Packaging.md` §2, `Performance.md`, `TRAPS.md` |
| shipping-diag-and-zero-pcg.md | retired | `Packaging.md` §1, `ENGINEERING_PRINCIPLES.md` lessons |
| electric-dreams-env-project.md | kept (link fixed) | `INDEX.md` references |
| audio-heartbeat-and-menu-return.md | retired | `Audio.md`, `DECISIONS.md` 2026-10-16 |
| coop-pawn-lifecycle.md | retired | `Coop_And_Networking.md` §1-§4, §6 |
| coop-server-driven-construction.md | retired | `Building.md` §1, `DECISIONS.md` 2026-10-13 |
| crash-endpoint-and-bug-reports.md | retired | `Persistence.md` §4, `TOOLING.md` §8 |
| death-recap-and-respawn.md | retired | `Medical_And_Survival.md` §2, `TRAPS.md` |
| dev-console-popup-and-starter.md | retired | `UI.md` §7, `DECISIONS.md` 2026-10-08 / 10-14 |
| engine-installed-build-patches.md | retired | `TRAPS.md` §2, `TOOLING.md` §2, `build_engine_path` memory |
| fab-sourcing-and-icon-studio.md | retired | `Item_Art_Pipeline.md` (already), `DECISIONS.md` 2026-10-14 (ownership), `TRAPS.md` §10 |
| first-hours-water-and-fire.md | retired | `Crafting_And_Progression.md` §2, §4, `Water_And_Illness.md` |
| first-ten-minutes-pass.md | retired | `TOOLING.md` §3, `TRAPS.md` §5, `BACKLOG.md` N-078, N-083 |
| footsteps-and-graphics-quality.md | retired | `Audio.md` §3, `Performance.md` §3, `TRAPS.md` |
| hitch-hunt-lake-query.md | retired | `Performance.md` §2, §4 |
| host-pawn-under-terrain.md | retired | `Coop_And_Networking.md` §3-§4 |
| hud-modes-and-time-dial.md | retired | `UI.md` §3, `Weather_And_Time.md` §5 |
| hud-yields-to-menus.md | retired | `UI.md` §3 |
| mortality-and-survival-intake.md | retired | `Medical_And_Survival.md` §2-§3, `Persistence.md` §3 |
| oceanology-integration.md | retired | `Water.md` §2 |
| oct16-bug-pass-night.md | retired | `Building.md`, `Coop_And_Networking.md` §5, `Water.md` (frozen lakes), `TRAPS.md` |
| package-cook-errors-oct16.md | retired | `Packaging.md` §2 |
| packaging-dev-build.md | retired | `Packaging.md`, `TOOLING.md` §7 |
| progression-pass1-materials.md | retired | `Crafting_And_Progression.md` §3, §6, `PROGRESSION_ROADMAP.md` §5 |
| steam-multiplayer-integration.md | retired | `Coop_And_Networking.md` §1 (the wrong-diagnosis history stays in the archived memory) |
| steam-store-art.md | retired | `SteamArt/README.md` (already), `DECISIONS.md` F23, `TRAPS.md` §12 |
| swimming-and-breath.md | retired | `Water.md` §3, `Medical_And_Survival.md` §5, `TRAPS.md` (PIE throttle superseded) |
| ue-editor-mcp.md | retired | `AUTONOMOUS_TOOLING.md` (already), `TRAPS.md` §3-§4, `user_preferences` (do not write off the MCP) |
| ue-multiprocess-testing.md | retired | `TOOLING.md` §1, §3, `Coop_And_Networking.md` §6, `TRAPS.md` §5 |
| ue-ui-toolset.md | retired | `UI_TOOLING.md` (already), `UI.md` §4, `TRAPS.md` §13 |
| ue-unified-cli.md | retired | `TOOLING.md` §1, `TRAPS.md` |
| ue58-ui-render-traps.md | retired | `UI.md` §4 |
| ue_pie_computer_testing.md | retired | `UI.md` §2 (key map), `TRAPS.md` §3, `Agent_PIE_Testing.md` (kept) |
| ui-design-system-brief.md | retired | `UI.md` §6, `DECISIONS.md` 2026-10-11, `BACKLOG.md` N-124 |
| water-trace-channel-underwater-and-hair.md | retired | `Water.md` §4, §6, `DECISIONS.md` 2026-10-16 |
| animation-retarget-pipeline.md | retired | `TRAPS.md` §10, `TOOLING.md` §6 (recipe stays in the archived memory) |
| commonui-pending-activation-race.md | retired | `UI.md` §3 |
| cs399-practicum.md | kept, rewritten (trimmed) | full original in `memory/archive/cs399-practicum.orig.md` (personal: never in the repo) |
| (new) verification-lessons.md, standing-permissions.md, decisions.md, traps.md, wes-playtest-context.md | created | -- |

## Archived repo documents -> where their live content went

Per-file summary in `Docs/Archive/INDEX.md`. Item-level extraction notes (TRAPS / BACKLOG / DECISIONS lines, each with its source tag) are in
the appendix below (the staging notes used to compose those three files).

| Source | Moved to | Extracted into |
|---|---|---|
| Docs/PROJECT_STATUS.md | Docs/Archive/PROJECT_STATUS_2026-10-15.md (git mv) | BACKLOG § Audit; new slim PROJECT_STATUS.md (numbers, tracks, strengths) |
| Docs/AAA_Polish_Backlog.md | Docs/Archive/ | BACKLOG P-1..P-13 aliases; Performance §1 (quality gates); DECISIONS (forks) |
| Docs/BugReports_2026-10-14.md, _16.md | Docs/Archive/ | BACKLOG BR14-15 / BR16-9, -15, -16; DECISIONS 2026-10-16 |
| Docs/Claude_Handoff_Goals_2026-10-13.md | Docs/Archive/ | BACKLOG G00-G26 (status table in the appendix, stage 21) |
| Docs/Night_Report_2026-10-17.md | Docs/Archive/ | DECISIONS 2026-10-17; BACKLOG N-002, N-054-N-056 |
| Docs/OVERNIGHT_LOG.md, OVERNIGHT_PLAN_2026-10-09.md | Docs/Archive/ | DECISIONS (F19, standing rules, Oct 8-9 approvals); BACKLOG N-113, N-114, N-127 |
| Docs/Overnight_Polish_2026-10-12.md | Docs/Archive/ | PCG, Packaging, UI, Audio, TRAPS; BACKLOG N-034, N-075, N-121 |
| Docs/Ground_Coverage_2026-10-14.md | Docs/Archive/ | PCG §3-§5, Performance |
| Docs/Progression_FreshLook_2026-10-15.md, Progression_Reachability_2026-10-14.md | Docs/Archive/ | PROGRESSION_ROADMAP (incl. the precursor table verbatim), Crafting_And_Progression |
| Docs/Fable5_*.md (3), MO57_Fable5_Charter.md | Docs/Archive/ | VISION (A-rated bar, four gates verbatim), AI_And_Colony, BACKLOG N-090..N-100, DECISIONS F22 |
| Docs/MO57_Master_Plan.md | Docs/Archive/ | AI_And_Colony §4, DECISIONS "Earlier" (March 28) |
| Docs/MobAIPlan.md | Docs/Archive/ | AI_And_Colony §1 |
| Docs/PCG_Integration_Plan.md | Docs/Archive/ | PCG §1 ("how PCG is hosted", verbatim in substance) |
| Docs/Terrain_Foundation_Plan.md | Docs/Archive/ | superseded by the C++ generator (Terrain_Generation_Exploration) |
| Docs/CommonUI_Migration_Plan.md, commonui_migration_audit.md, UI_Overhaul_Architecture.md, UI_Design_Brief_Prompt.md | Docs/Archive/ | UI.md (§5 pitfalls verbatim, §6 kit) |
| Docs/devlog_*.md, MO57_ClaudeCode_Plan.docx | Docs/Archive/ | DECISIONS "Earlier" (March 18 systems custom) |
| PATCH_NOTES.md (root) | Docs/Archive/ | STEAM_RELEASES.md + TOOLING §10 (Wes N9) |
| Plugins/MOFramework/Content/Data/TechProgressionPlan.md, TechProgression.txt, StatusReport_2026-02-01.md | Docs/Archive/ | PROGRESSION_ROADMAP (Feb intent, F16) |
| Content/Python/README_WIDGET_AUTOMATION.md | Docs/Archive/ | UI_TOOLING (already superseded) |
| claude-codex-coop.md July entries | Docs/Archive/coop/2026-07.md (verbatim; 113 headers before = after) | -- |
| Docs/Weather_Bridge_Coop_Investigation.md | git mv -> Docs/Weather_And_Time.md (live) | its text kept verbatim as §4 |
| AGENTS.md (gitignored) | rewritten as a Codex shim | every shared rule is in CLAUDE.md; Codex specifics (graphify interpreter, Docs/agent, ids) kept |

**Kept live instead of archived (decided during P3; deviations from the proposal):** `Agent_PIE_Testing.md` (bridge playbook, header
added), `Content/Python/README_UI_TOOLS.md`, `Survival_Single_Source_Proposal.md` and `Thermal_Model_Reproduction.md` (open fork briefs F1 /
F2), `AUTONOMOUS_TOOLING.md` whole (its hit/miss history was not split out). **Blocked:** `Docs/Handoff_2026-10-13.md` (named by Codex's
uncommitted `MOworldSaveGame.h`; BACKLOG N-004).

**Created in `Docs/Archive/` by this pass (records of the pass, not archived content):** `INVENTORY.md` (P1 inventory),
`CONSOLIDATION_PROPOSAL.md` (P2 proposal plus Wes's N1-N9 answers), `CONSOLIDATION_LEDGER.md` (this file), `INDEX.md` (one line per
archived file), `BACKLOG_DONE.md` (closed BACKLOG items go here) and `coop/2026-07.md` (the July `claude-codex-coop.md` entries,
rotated out verbatim). Five files were already in `Docs/Archive/` before this pass; they are named in `Docs/Archive/INDEX.md`.

**Other path fixes in P6 (paths that never existed):** the Crouch and Swim clip folders are `Plugins/MOFramework/Content/Characters/{Crouch,Swim}`
(fixed in TECHNICAL_REFERENCE and Terrain_Generation_Exploration). `Tools/UE_CSV_FORMAT.md` and the TOOLING CSV block now say that
`Items.csv` / `Recipes.csv` moved to `Content/Data/archive/`. `.claude/skills/ue-datatable-json/SKILL.md` now says the `*_JSON.json`
copies are stale, sends row edits to `ue.py rows set`, and names the weapon row correctly (`FMOWeaponDamageProfileRow`, not `FMOWeaponDefinitionRow`).

**Fixes from the P6 cold-start test (fresh-context agent: 10 questions, CLAUDE.md + INDEX.md + at most two hops):**
- Every "`Docs/X.md` § Name" pointer is now checked against X.md's real headings; 12 that did not match were renamed to the exact heading.
  BACKLOG uses grouped headings, and TRAPS / TOOLING headings are numbered.
- Light_Sources' current-state line now names N-002 (wear uncommitted) and N-054 (co-op retest).
- Engine_Patches' remark about CLAUDE.md saying "source build" is now in the past tense.
- PROGRESSION_ROADMAP §8 now says why the metal path was blocked before Oct 17.
- Coop_And_Networking §6 now starts with a paste-ready host + client block.
- Packaging gives the full `UnrealEditor-Cmd.exe` path.
- The skill's description line now steers to `ue.py rows`.
- The PAUSE_POLICY editor-pause contradiction went to BACKLOG N-154; the policy text is unchanged.

## Source-code comment edits (comment-only, doc paths)

| File | Old reference | New reference |
|---|---|---|
| `Plugins/MOFramework/Source/MOFramework/Public/MOColonyManagerSubsystem.h` | Docs/MO57_Master_Plan.md | Docs/AI_And_Colony.md (history: Docs/Archive/MO57_Master_Plan.md) |
| `Plugins/MOFramework/Source/MOFrameworkCore/Public/MOBiomeDefinitionRow.h` | Docs/Fable5_PCG_Path.md | Docs/PCG.md (history: Docs/Archive/Fable5_PCG_Path.md) |
| `Plugins/MOFramework/Source/MOFramework/Private/MOCheatSubsystem.cpp` | Docs/Progression_*.md | Docs/PROGRESSION_ROADMAP.md (history: Docs/Archive/Progression_*.md) |
| `Content/Python/biome_palettes.py` | Docs/Ground_Coverage_2026-10-14.md | Docs/Archive/Ground_Coverage_2026-10-14.md (the .plan.json path unchanged) |
| `Content/Python/native_grass_biomes.py` | Docs/Ground_Coverage_2026-10-14.md | Docs/PCG.md, history Docs/Archive/... |
| `Config/DefaultEngine.ini` | Docs/AAA_Polish_Backlog.md | Docs/Archive/AAA_Polish_Backlog.md |

## Appendix: item-level staging notes

The following are the staging notes (one file per source batch) from which TRAPS.md, DECISIONS.md and BACKLOG.md were composed. Each line
names its source. They are the item-level record for "nothing lost".

---

### STAGE 01: worldgen + caves (sources: Terrain_Generation_Exploration.md, memories terrain-gen-exploration, terrain-v4/v5/v6, caves-and-lighting, geology-and-cave-content, todo-alpine-snow-and-caves, todo-terrain-material-look, biome-blend-surface-weights)

#### TRAPS
- [voxel] Graph says "Cannot query positions here" -> OutputHeight.Bounds/.HeightRange are evaluated at stamp init with no position -> feed them from a no-input node (`MO Terrain Bounds`). (terrain-gen-exploration; worldgen doc)
- [voxel] Terrain misaligned or wrong -> stamp transform not identity, or graph parameter not named exactly `Seed` -> identity transform; parameter named `Seed`, wired straight in. (terrain-gen-exploration)
- [voxel] Saved graphs lose links after editing an MO node's pins -> pins orphan -> break the links via right-click, re-add. (terrain-gen-exploration)
- [voxel] Whole terrain renders as the grey WorldGrid checker -> the SurfaceType pin of VHG_MOTerrain is unwired -> wire it. (caves-and-lighting)
- [voxel] A new voxel surface type never paints -> it is not listed in the mega material's `surface_types` -> add it there (`make_sand_material.py` does). (terrain-v5)
- [voxel] Far mountains show a blue/white "checkerboard" or a tear at 12 km -> chunk HOLES from a world-spanning volume stamp (the cave stamp) -> the stamp gets `LODRange {0, CaveMaxLOD}`; every new volume stamp needs tight bounds + a LOD range. Diagnose by deleting the stamp actor at runtime, `r.Nanite.Visualize Triangles`, `ShowFlag.Fog 0` + `ShowFlag.Atmosphere 0`. (volume-stamp-holes)
- [voxel] Caves don't line up with the ground -> only the height stamp was seeded -> `ApplySeedToStamps` seeds height AND volume stamps. (caves-and-lighting)
- [voxel] Cave field has a cliff / meshing artefacts -> a hard `return Far` gate -> every gate is a continuous penalty saturating at `CaveFarCm`; tests measure max |gradient| (limit 40). (caves-and-lighting, terrain-v5)
- [voxel] "INVALID STAMP" in the log -> a stray `VoxelStampActor` with no stamp saved in the level -> delete it. (caves-and-lighting)
- [worldgen] Packaged build prints a different `MO.WorldGen.Checksum` than the editor -> UBT compiles Editor targets /fp:precise but Game /fp:fast -> `FPSemantics = Precise` in `MOFrameworkCore.Build.cs`; rerun the checksum in BOTH builds after any build-rule or toolchain change. (caves-and-lighting)
- [worldgen] Terrain looks like 1000 skinny towers -> every wavelength in `FMOWorldGenTuning` is in CENTIMETRES; km-scale masks blended over ~150 m make walls -> range-scale (30 km) masks; check units first. (terrain-v6)
- [worldgen] A flatness test reads 170 m on "prairie" -> its second point was in another climate class -> keep start, middle and end inside one climate class. (terrain-v5)
- [worldgen] A biome hill scale < 1 changes nothing visible -> the continent spline (`LowlandCapM`) alone gives ~130 m per 1.5 km -> flatten the continent spline too. (terrain-v5)
- [worldgen] A coarse cave scan finds no mouths -> a mouth is a few metres wide -> measure at 4 m. (caves-and-lighting)
- [worldgen] Karst field tests time out or starve -> karst gate on in field tests -> run field tests with karst-OFF params. (geology)
- [worldgen] A new v5-style default breaks the v1 golden anchor -> `RunProbe(false)` must restore every new default -> add it there too. (worldgen doc)
- [camera] The camera shows "water in the cave" -> a camera inside the rock sees the ocean plane under all terrain -> probe air with `MO.WorldGen.CaveSample` first. (geology)
- [camera] After a terrain change, PCG rocks seem to float in a shot -> the camera at the old z is inside the mountain -> place cameras from the pawn's REAL z. (playtest-oct14)
- [art] A dark scan tinted pale comes out black -> a material TINT multiplies the scan colour -> start from a pale scan. (geology)
- [light] Cave walls lit from below at night -> a directional light below the horizon lights one-sided voxel surfaces from underneath, shadows on or off; two-sided material, `bCastShadowAsTwoSided`, non-Nanite, VSM off and a shadow quad all failed -> `UMOSubterraneanLightingSubsystem` intensity gate (1 at the horizon -> 0 at -5 deg); A/B with `MO.Light.BlockFromBelow 0/1`. (caves-and-lighting)
- [light] Turning a light off at runtime fails -> `ULightComponent::bAffectsWorld` can't change at runtime; `SetVisibility` is overwritten by the UDS sky BP -> drive the intensity. (worldgen doc)
- [water] A pawn swims inside terrain below sea level -> the ocean plane fills everything under z=0 including underground -> `BuriedMarginCm` + `UMOWaterSubsystem::GetGroundZ` (feet that far under the generator ground = no water). (death-recap)
- [worldgen] Python sets the level stamp graph -> `actor.get_stamp().export_text()` -> replace the graph path -> `VoxelStampRef.import_text` -> `actor.set_stamp` (set_stamp fixes the Seed parameter GUID). (caves-and-lighting) [recipe -> TOOLING]
- [worldgen] Lake SpawnActor with the ocean actor as Template -> composes the template's root transform (lakes 80,000x too big) AND copies its replication Role ("invalid Role" ensure on clients, found only by the packaged two-process test) -> spawn a plain actor of the class; Movable before FinishSpawning. (worldgen v4)
- [worldgen] Menu flyby spline runs away exponentially -> an attached spline rode the camera -> make it absolute. (worldgen v4)
- [editor] Leaving the editor raises a Save dialog for the generated LoadingLevel -> window close box -> `python Tools/ue.py run QUIT_EDITOR`. (terrain-v4)

#### BACKLOG (open)
- World/caves | OPEN | Claude | Per-bedrock / cave-floor voxel surface types (cave walls are all VST_Cliff) | Caves_And_Geology §5 | src geology
- World/caves | OPEN | Claude | Geology outcrops to REPLACE the biome-placed copper / flint deposits (outcrop rows exist; the biome deposits are still placed) | worldgen Geology | src geology
- World/caves | OPEN | Claude | Tin placers in rivers draining granite | Caves_And_Geology §3
- World/caves | OPEN | Claude | Measure the galena share (only asserted "present, rarer than hematite") | worldgen Geology
- World/terrain | OPEN | Wes | Distant mountains bland: pick one of (mountain-scale relief that survives far LOD / clearer Clear_Skies atmosphere / lower or temperature-driven snow line / an artist's far texture); the Oct 13 shader attempt was a measured negative result | worldgen To-do | alias AAA "Terrain material look (1)"
- World/terrain | OPEN | Claude | Sand variety (several sand materials by low-frequency noise + distance to water); more sand sets may need a download | worldgen To-do | src todo-terrain-material-look
- World/terrain | OPEN | Wes | Alpine snow (b): bigger noise if wanted; the 1 km flat-white sunlit snow is a SnowColor / exposure call | worldgen To-do | src todo-alpine
- World/terrain | OPEN | Claude | Biome boundaries: far-LOD vertex step still shows at 3 km; next step = heightmap-based layer blend in the ground material, not a wider band | biome-blend, terrain-material-far-tiling
- World/terrain | OPEN | Claude | Distant lake water shards (pre-existing) | terrain-material-far-tiling
- World/terrain | OPEN | Wes | Rainforest uses temperate packs (no palms / tropical broadleaf): a tropical pack is a download | worldgen v5 open
- World/terrain | OPEN | Claude | Main-menu flyby start chosen against v4 land: re-check the first frame | worldgen v5 open
- World/terrain | OPEN | Claude | Lakes are static (no flow, no seasons); rivers are sea-level channels only | worldgen v4 open
- World/terrain | OPEN | Claude | Determinism on a second physical PC (packaged `MO.WorldGen.Checksum`) | caves-and-lighting
- World/terrain | OPEN | Claude | Upland lake shores / basin: softer MUD bed is a texture-set job (dark pebbles now) | worldgen To-do

#### DECISIONS (dated, binding)
- 2026-10-10 night (v4): "very very flat, realistic but not fun" -> `VerticalScale` 2; spawn always on a coast looking across the water, NEVER on an island (landmass >= 600 km2); lakes above sea level ~1 per 5 km2; `weather` / `time` console popups set the SERVER's weather/time; check ALL scatter for collision; main-menu level with a slow flyby; keep making the land alive. Built: generator v4. (terrain-v4)
- 2026-10-10 afternoon (v5): mountains "many times higher" but lowland prairie stays flat (per-biome height variance); biomes ~half size; land falls into water at the world edge; a rainforest like Epic's PCG forest; sand on sea-level shores; more Megascans for beaches. Built: v5. (terrain-v5)
- 2026-10-11 eve (v6): "1000 really tall skinny mountains instead of larger masses with 1000 small steep features": larger masses, some sheer cliff detail up close but never region-wide thin walls, nothing collapses at far LODs; "the height is good" (keep ~1.4-1.9 km). Built: v6. (terrain-v6)
- 2026-10-13: no caves up high -> `CaveMaxGroundCm` 750 m (v7, 60c666f9). (worldgen To-do)
- 2026-10-13 eve: "add to todo": alpine/snow ground needs more variation ("maybe even run math where there's less snow on the southern facing slopes ... or just make it bigger random noise"); "make caves about half the diameter ... traverse more laterally instead of straight down". Aspect snow built Oct 15 (729a5718); caves built v9 (1b4e7d94). (todo-alpine)
- 2026-10-14: "the biomes should have a lot more blending" -> `SurfaceClimateBlend` 0.03 -> 0.12 + 3-octave breakup (not in the params hash: no version bump). (biome-blend)
- 2026-10-17: caves must not reach sea level / show the underwater effect, and not near shores ("they'd immediately be below water table") -> v10 water gate (f92ebebe); "PCG inside caves ... different ores ... check what is actually realistic ... I still want realism" -> geology + MO Cave Spawner. (geology)
- 2026-10-17 ~00:00: "caves only in limestone, and mining yields from the geology ... they should be able to collect pretty much any ore, the actual hurdle with iron is having fire hot enough, so smelting and forging technology -- do some research into the actual histories" -> v11 karst gate, FMOGeologyYield, IronOreDeposit stone pick, Technology_History.md (ba37d659). (geology)
- 2026-10-17 ~04:00: "yes, different times for different rocks/minerals" -> `HardnessFor` (37a3d694). (wes-calls-oct17)
- Default clock TimeScale 1 (real time: weather and day/night barely change in a session); `MO.Clock.SetTimeScale 24` is "his call" -> OPEN FORK F13. (terrain-v4, worldgen v4 Weather)

---

### STAGE 02: PCG (sources: PCG_Integration_Plan.md last section, Ground_Coverage_2026-10-14.md, memories biome-aware-pcg-overnight, pcg-clump-compensation-and-coverage, pcg-load-and-spawner-perf, far-trees-materials-clutter-oct13, world-removal-ledger, world-readiness-and-splash, playtest-oct14-lessons, overnight-polish-oct12, shipping-diag-and-zero-pcg)

#### TRAPS
- [pcg] Every biome looks the same (grass + locust + alder) -> biome-blind native Static Mesh Spawner chains outside the biome spawner's radius -> limit them with AllowedBiomeIds or cut them; new vegetation = a species row. (biome-aware)
- [pcg] Authored densities realised at 1-5 % -> the Voxel Sampler places at most one point per CellSize lattice cell -> CellSize <= ~70/sqrt(rho) cm; check `in/area_ha` in the spawner log. (biome-aware)
- [pcg] Radius change never takes effect -> radius fields are named by the UNSCALED grid (`radius1600` for GRID16 64 m cells) -> set the right field; verify with `MO.PCG.Rings`. (overnight-polish)
- [pcg] Radius fields don't change via set_editor_property -> PROTECTED -> reflection + `ComputeHash()`. (pcg-load)
- [pcg] Forests read as savanna -> the clump field is not uniform; 1/(1-core) compensation under-plants -> fixed at the spawner (be6506f7); never tune palettes by eye against pre-fix numbers. (pcg-clump)
- [pcg] Regenerating palettes reverts live tuning -> biome_palettes.py drifted from live DT_Biomes (Oct 16) -> diff plan vs `ue.py rows get` first; `rows list` output has CRLF. (pcg-clump)
- [pcg] Far tiers delay near cells -> the 5.8 runtime scheduler generates the LARGEST grid first -> readiness holds grids >= 512 during a cycle (radius -1e9; radius 0 still generates the viewer's own cell). (pcg-load)
- [pcg] Spawner costs 13.5-161 us per point -> FName(TEXT()) / Mesh.ToString() per species per point (name-table lock in parallel) -> classify species once per execution. (pcg-load)
- [pcg] Disabled node's chain still costs -> a disabled PCG node passes input through; its upstream computes -> cut dead branches at the root (cut_pcg_dead_branches.py). (pcg-load)
- [pcg] ALL generation stops silently -> a HiGenGridSize node with no edges -> never leave one disconnected. (pcg-load)
- [pcg] Far trees leave ghosts / InstanceMinDrawDistance ignored -> it needs InstanceEndCullDistance > 0 -> ApplyNearDrawFloor. (pcg-load, overnight-polish)
- [pcg] Raising pcg.FrameTime / threads makes loading SLOWER (frames stall ~5 s) -> do not raise PCG throttles. (world-readiness)
- [pcg] Still camera misses cells -> change detection rescans only after the camera moves > 10 % of the smallest grid -> rely on the readiness release; compare MO.PCG.Rings at 45 s after any timing change. (pcg-load)
- [pcg] Speckled far forest / giant pop-in -> a native biome-blind spawner (StaticMeshSpawner_5 Field_05, 90k trees to 12 km) -> find with MO.PCG.MeshSurvey / HideTag / HideMesh BEFORE theorising. (far-trees)
- [pcg] Item clutter changes when thinning ground cover -> the clutter shares VoxelSampler_0 with the ground tier -> thin the RandomChoice ratio, never the sampler. (far-trees)
- [pcg] Census of far trees over-counts -> component bounds of huge far cells -> census per INSTANCE (census2) / MO.PCG.Census by radius. (biome-aware, pcg-load)
- [pcg] PCG graph edits fail -> not possible during PIE -> edit from editor Python outside PIE (`graph.get_editor_property("nodes")`, `node.get_settings().set_editor_property`, `g.add_node_of_type`, `node.add_edge_to("Out", other, "In")`; pin labels Out / In / "Bounding Shape"). (biome-aware) [recipe -> TOOLING]
- [pcg] New DT struct field via rows set -> works only after the C++ rebuild; JSON keys lowerCamel for row fields, PascalCase inside species entries. (biome-aware)
- [pcg] PCG instances iterate as HISM but aren't -> PCG builds plain ISM components (`ISM_*`) from an HISM descriptor -> iterate UInstancedStaticMeshComponent. (playtest-oct14)
- [pcg] Removed tree regrows / co-op desync -> raw ISM->RemoveInstance -> FMOHISMHarvestHelper::RemoveInstance only. (world-removal-ledger)
- [pcg] Quarrying a cave floor stone removed the ceiling rock above -> ledger keyed by X/Y only -> 1 m CellZ key. (geology)
- [pcg] Removal tests wait forever -> MO.Save.LoadFrom reloads IN PLACE (same UWorld) -> don't wait for a new world. (world-removal-ledger)
- [pcg] "ZERO PCG in game" report -> the coastal spawn on bare shore sand, not PCG -> reproduce the player's view (seed from GameUserSettings PendingWorldSeed, HighResShot via MO.Test.AfterReady + MO.Delay) and census (MO.PCG.ResourceCensus 150) before reading engine code. (shipping-diag)
- [pcg] Pawn relocated by a rescue when teleported above unloaded terrain -> use a spot the generator says is dry with z well above ground. (biome-aware) [open]
- [pcg] Cooked default world reports voxel-ready immediately -> voxel poll started before AMOGameMode's BeginTracking -> don't start the poll earlier. (world-readiness)

#### BACKLOG (open)
- PCG | OPEN | Claude | AlpineHeath ground: a darker heath/moss surface (pale gravel reads as desert) -- recheck after VST_MI_AlpineHeath | biome-aware
- PCG | OPEN | Claude | Real reed / rush / cattail meshes for the shore tier (stand-ins = tall smdrailr grass) | biome-aware, todo-water | F5 decides source
- PCG | OPEN | Claude | Narrow rivers get no reeds (ShoreFactor samples one 25 m ring) | progression-pass1 | fork F12
- PCG | OPEN | Claude | SpawnManager's first spawns (~20 s after READY) start a 3-5 s voxel state (nav invokers of new pawns) | pcg-load
- PCG | OPEN | Claude | One NavigationBuild ~15-20 ms frame; 25-30 ms render-thread frames (RT instance gather / RDG) | post-reveal
- PCG | OPEN | Claude | Survivor spawn flush = uncooked groom emitter /HairStrands/Emitters/StableRodsSystem | post-reveal
- PCG | OPEN | Claude | Packaged-build reveal timing vs test build; a wait-free reveal that gates only on the near field; MoviePlayer boot screen for the engine-boot + LoadMap gap | world-readiness
- PCG | OPEN | Wes | "Made with Unreal Engine" splash card needs Epic's official logo (download = Wes's yes) | world-readiness | F14
- PCG | OPEN | Claude | Legacy native PCG nodes not lake/biome aware (lake trees from legacy nodes) -- most cut; recheck | terrain-v4
- PCG | OPEN | Claude | Desert-canyon biome from the Sandstone/Canyon sets; ED forest biome 1:1 | overnight-polish
- PCG | OPEN | Claude | Item clutter thinning review (overnight-polish open) -- partly done x10 thinner Oct 13 | far-trees

#### DECISIONS
- 2026-10-11 night: "keep working, flesh out biomes, snapshot each one, make sure nothing weird (trees in water)". Built: biome-aware PCG tiers + water mask. (biome-aware)
- 2026-10-13 (12 h directive): far trees performant AND good ("200k Brushify trees in Voxel 1.2 on a 1080, so there SHOULDN'T be a reason we can't"); fix ground TILING ("tessellation or whatever clever tricks"); distant forest must not show the yellow MuddyLeaves surface; snow materials for peaks. Permission in that directive: commit AND push, download CC0 assets (ambientCG, Poly Haven), state what was downloaded; no paid APIs. Steam art "looks like shit" -> Wes takes the screenshots himself. (directive-oct13)
- 2026-10-14 eve: "more ground coverage in general, more trees, more bushes". Built: tier caps + clump fix + plan. (pcg-clump)
- 2026-10-14 (#6): rainforest trees "VERY dense" at ~72/ha -> don't push past ~110. (pcg-clump)
- 2026-10-16: alder burial +25 cm SinkMinCm on Forest_/Field_ alders (Wes). (pcg-clump)
- Removals permanent; regrowth = OPEN FORK F3. (world-removal-ledger)

---

### STAGE 03: water (sources: worldgen doc Oceanology + water sections, memories oceanology-integration, swimming-and-breath, water-trace-channel-underwater-and-hair, todo-water-and-shores, oct16-bug-pass-night (frozen lakes))

#### TRAPS
- [water] Underwater look never appears on the infinite ocean -> its box volume has no collision; in game no physics STATE, EncompassesPoint = FLT_MAX -> EnsureUnderwaterVolumeQueryable (query-only body). (oceanology)
- [water] Lake water over hillside corners -> Oceanology lake plane is world-axis-aligned (quad tree) -> generator buries the AABB; never rotate. (oceanology)
- [water] Huge memory for a big lake -> quad tree pre-allocates ~(2 x extent x scale)^2 nodes -> bigger TileSize, never a big actor scale. (oceanology)
- [water] Lakes doubled on clients -> Oceanology actors replicate by default -> SetReplicates(false) before FinishSpawning for per-machine water. (oceanology)
- [water] Lakes frozen (no waves) on every machine -> Oceanology never registers a runtime-spawned body's wave solver -> register + activate it in SpawnLake. (oct16-bug-pass)
- [water] Underwater distortion cap resets -> presets overwrite it on every load -> enforce on a 0.5 s tick. (oceanology)
- [water] Lake absorption values look backwards -> absorption = how far each channel CARRIES (bigger = clearer). (oceanology)
- [water] Packaged build crashes in InitBubbles on the first lake -> Oceanology soft paths not followed by the cooker -> DirectoriesToAlwaysCook the plugin dirs (but not NS_GerstnerWaves: DX11 X3662). (oceanology, package-cook-errors)
- [water] Cvar values with spaces in -ExecCmds break -> -ExecCmds splits on COMMAS. (oceanology, swimming)
- [water] (Epic Water era, superseded) underwater PP only when looking up -> it runs only on a BLOCKING hit on CollisionChannelForWaterTraces -> dedicated channel; check channel/profile (blocking vs overlap) BEFORE the geometry; native multicast delegates run in REVERSE binding order. (water-trace)
- [water] Buoyancy looked broken in PIE (dive 22 cm/s) -> editor throttled PIE to ~3 fps unfocused (physics dt 0.3 s) -> FIXED for good in EditorSettings.ini (bThrottleCPUWhenNotForeground=False, bAllowSlateThrottling=False, verified 2026-10-17); warnings elsewhere about 3 fps PIE are stale. (swimming)
- [anim] Retargeted clip moves only the root -> a fresh 5.8 IKRetargeter is an OP STACK with no ops -> add_default_ops + auto_map_chains; verify by sampling get_bone_pose_for_frame rotations. (swimming, animation-retarget)
- [content] New MO content vanishes from git -> Plugins/MOFramework/Content/Characters/MetaHumans/ is GITIGNORED -> put new content elsewhere (Characters/Swim). (swimming)
- [build] Build breaks right after committing new files -> adaptive unity moves committed files into unity blobs, exposing duplicate anonymous-namespace names -> run `python Tools/check_unity_collisions.py` before committing C++. (swimming, world-features)
- [shell] bash heredoc with apostrophes breaks in the Bash tool -> write scripts/text with the Write tool, then run them. (swimming, cs399) [confirmed again 2026-10-17]

#### BACKLOG (open)
- Water | OPEN | Claude | Floating objects / boats via Oceanology buoyancy (supersedes MOBuoyancyComponent) | todo-water #5, AAA P1
- Water | OPEN | Claude | Swim polish: load-dependent buoyancy (no inventory mass API), fall damage into water, swimming creatures, clothing distorts on horizontal poses (shorts stretch), swim blends | swimming open, AAA P1
- Water | BLOCKED (source engine / Epic) | Wes | Hair buns under water + creator DOF: engine patch #1; Wes waits for an Epic fix; upstream report not posted | water-trace, Engine_Patches | F11
- Water | OPEN | Claude | Real reed meshes for shores (see PCG) | todo-water #2 | F5
- Water | OPEN | Claude | Softer mud lake bed (texture set) | todo-water #3

#### DECISIONS
- 2026-10-11: "im installing oceanology so let's switch to that ... you should be able to use most of the oceanology features for what we're doing" -> Oceanology replaced Epic Water. (oceanology)
- 2026-10-11: underwater wobble "really strong and looks pretty bad, make it 1/4" -> cap 0.0125. (worldgen doc)
- 2026-10-11: "commit what you did and keep going ... set a goal of a complete polished AAA level game" (after swimming was on his to-do list). (swimming)
- 2026-10-16: approved the hair engine patch, but the engine is a launcher install (can't patch); `r.Water.SingleLayer.DepthPrepass=0` restores hair depth but Wes REJECTED it after watching it live (water loses its look); creator DOF off until Epic fixes it; Wes waits for an Epic patch. (water-trace, engine-installed)
- Standing practice: design-level asks are logged as to-dos rather than built blind. (todo-water)

---

### STAGE 04: light (sources: Light_Sources.md (kept), memories carried-light-and-cave-exposure, caves-and-lighting (below-world light), Night_Report_2026-10-17)

#### TRAPS
- [light] A test light renders dark / "8000 cd" looks dim -> UE point light default units are UNITLESS (x16 vs candela x10000; the campfire's 5000 = 8 cd) -> SetIntensityUnits(Candelas) BEFORE SetIntensity; set_editor_property('intensity_units') on a live light does not refresh the proxy. (carried-light)
- [light] Caves glow like a moonlit night -> UDS "Captured Scene Sky Light" casts no shadow -> UDS Apply Interior Adjustments = True, Sky Light Intensity Multiplier in Interiors = 0 on the level's sky actor. (carried-light)
- [light] Torch blows the frame out white in a cave -> the histogram meters the black majority -> UMOEyeAdaptationComponent exposure floor from registered flames. (carried-light)
- [python] UDS BP property with spaces can't be read in snake_case -> get_editor_property('Apply Interior Adjustments') with the exact name; UDS BP variables like "Time of Day" are unreachable from Python -> go through the bridge (MO.Weather.SkyTime). (carried-light, ue-multiprocess)

#### BACKLOG (open)
- Light | WAITING (Codex commit) | Claude | Torch wear commit: MOCarriedLightComponent.h/.cpp + Light_Sources.md torch-wear text wait on Codex's UMOEquipmentComponent::ReduceEquippedDurability commit (code built + verified, co-op 5/5) | Night_Report "Waiting on a commit"
- Light | OPEN | Claude | Co-op: a client lighting its own torch at a fire -- retest after a9a4de5d (Light_Sources says verified via nettest game --keep; reconcile with the night report) | Night_Report
- Light | OPEN | Claude | Torch-raising arm pose (animation) | Light_Sources, Night_Report
- Light | OPEN | Claude | Not modelled: smoke in dead-end galleries; rain on torches; torch-from-torch + survivors carrying torches; lamp fuel refill recipe; snuff key; torch as fire for other uses | Light_Sources "Not modelled yet"
- Light | SUGGESTION | Claude | Fire-fear for wolves (unify the twice-computed flee decision first: BTService_UpdateCreatureState vs AMOCreatureController::ShouldFlee) | Light_Sources
- Light | OPEN | Wes | Adaptation speed (UDS 3 EV/s up, 1 EV/s down vs real minutes): changes every shade transition -> Wes's call | Light_Sources | fork

#### DECISIONS
- 2026-10-17 ~04:00: "leave night brightness alone for now" -- don't change UDS night exposure (numbers for later: starlight EV100 ~-11, full moon ~-3, a -6 night floor would make moonless nights dark). (wes-calls-oct17, Light_Sources)
- 2026-10-17 night: "keep working with codex" (12 h) -> carried light built. (carried-light, Night_Report)

---

### STAGE 05: weather and time (sources: Weather_Bridge_Coop_Investigation.md (git mv -> Weather_And_Time.md), memories weather-bridge-not-native-uds, hud-modes-and-time-dial, dev-console (08:00 / Clear_Skies), dev_status (July 8 H36), steam-store-art (sky traps), far-trees (sky trap), worldgen v4 (director))

#### TRAPS
- [weather] Client weather stops following the host -> native UDS/UDW replication switched off; UDW Change Weather is server-only -> never switch it off before a client-side apply exists. (weather-bridge)
- [weather] Sky doesn't change after setting the clock back -> the bridge syncs only forward in game time -> set forward, wait ~60 s. (steam-store-art)
- [weather] 4K shot clouds blocky -> r.VolumetricRenderTarget 0 first. (steam-store-art, far-trees)
- [weather] A numeric probe of UDW variables proves nothing about a client gap (identical before/after even on the host in -game) -> look at both screens. (Weather_And_Time §4)

#### BACKLOG (open)
- Weather | OPEN | Wes | Client-side apply in BP_WeatherBridge (only if a real gap is shown: two windows, host sets fog/thunder, look) | Weather_And_Time §4 | fork F6
- Weather | OPEN | Claude | Weather per biome (rain / snow line at altitude) + matching sound layers | AAA P2
- Weather | OPEN | Wes | Default clock TimeScale (1.0 real time) | F13

#### DECISIONS
- 2026-10-08: "we have a whole weather bridge for UDS and all that, so use the bridge not the native UDS stuff." Satisfied for the game's code (all reads/writes go through IMOWeatherProviderInterface); native replication stays as UDW's transport (measured). (weather-bridge)
- 2026-10-08: new games at 8 AM with clear skies; `~` console popup; `starter` command (50 sticks, 20 stones). (dev-console)
- 2026-07-08 (H36): offline catch-up = clock jumps by the real gap capped at 4 h; crafting offline clamp 7 d -> same 4 h; vitals don't fast-forward; kills the queue-a-year exploit (b5d7b44f). PROJECT_STATUS still called H36 a fork: SUPERSEDED. (dev_status)
- 2026-10-15: dev time dialog = clock dial with live sky preview + golden hour (Wes wants clean screenshots/video and golden-hour shots). (hud-modes)

---

### STAGE 06: medical + survival (sources: parked TR/CLAUDE medical cascade, Thermal_Model_Reproduction.md (KEPT LIVE as fork brief), Survival_Single_Source_Proposal.md (KEPT LIVE as fork brief), memories mortality-and-survival-intake, survival-stat-pacing-fork, death-recap-and-respawn, swimming-and-breath, first-hours-water-and-fire, Codex agent/audit/MEDICAL_HANDOFF.md, Deep_Dive #6 #7)

#### TRAPS
- [test] Intermittent failure that moves with the seed -> world-dependent bug (spawn settle accepted a coarse-LOD collision) -> replay the seed with MO.NewGame <seed> before blaming the test. (death-recap)
- [test] nettest gates silently rotted for 4 days after the swimming system -> run ALL nettest gates (lan / hostsave / features / churn / game / actions / packaged) after movement / spawn / water changes. (death-recap)
- [spawn] New-game pawn underground, swimming -> spawn settle accepted a stable but COARSE-LOD collision -> PollSpawnSettle only accepts a trace within SpawnSettleGeneratorToleranceCm (40) of FMOWorldGen::SampleHeightCm. (death-recap)

#### BACKLOG (open)
- Medical | OPEN | Wes | F1 survival single source (A/B/C) + game clock TimeScale + whether Energy models sleep debt | Survival_Single_Source_Proposal | alias G10 (Codex handoff H10), Handoff decision #1
- Medical | OPEN | Wes | F2 thermal model (core+shell recommended, clo values, shivering cost) | Thermal_Model_Reproduction | alias G11 (Codex H11), AAA fork
- Medical | OPEN | Codex | Survival meters not saved/restored (Deep Dive #6): persist them or finish the single-source adapter | Deep_Dive #6
- Medical | IN PROGRESS | Codex | Wound treatment: service built + verified (uncommitted); treatment UI, medic job, other-patient inspection seam, HealingSpeedBonus/PainReduction not built | MEDICAL_HANDOFF, Deep_Dive #7
- Medical | OPEN | Claude | Thermal extremes have no enforced failure (IsCritical + cardiac/respiratory broadcasts orphaned) -- recheck after DieOf systemic clocks (Oct 13) | dev_status (Codex P1, July) | likely superseded by 4f948b11
- Medical | OPEN | Claude | Death recap: remote client's respawn RPC not verified; death during another spawn settle refused | death-recap
- Medical | OPEN | Claude | Villager sleep (fatigue has no recovery) | dev_status leads
- Medical | OPEN | Claude | GatherTinder needs DriedGrass01; fire drill needs Hardwood01 (recheck vs Oct 15-17 producers) | first-hours

#### DECISIONS
- 2026-10-15: survival pacing -- start fully fed/watered/rested; needs bothersome after ~30 min -> warnings at 30/45/60 game-min on the world clock (2d31e0c2). The metabolism single source (A) was NOT chosen or rejected. (wes-calls-oct15)
- 2026-10-16: "eating goes on a timer" -> EatSeconds timed action (a4207587). (wes-calls-oct16)
- 2026-10-13: old worlds after a generator change -> "we don't care about any old saves or preserving them" (H13 decided: break, load-time warning stays informational). (Handoff decision #4)
- Permadeath respawn ~5 mi (VISION pawn system); death recap. (death-recap)

---

### STAGE 07: crafting + progression (sources: Progression_FreshLook_2026-10-15.md, Progression_Reachability_2026-10-14.md, Technology_History.md (kept), TechProgressionPlan.md (Feb), progression_map.json (Oct 17 04:22), memories progression-pass1-materials, item-progression-data-gaps, first-hours-water-and-fire, wes-calls-oct15/17, Codex audit SESSION_STATE, Deep_Dive #3 #4 #5 #8 #9 #10, parked TR crafting validation)

#### TRAPS
- [data] Source analysis claims a material is available but it isn't in the world -> it assumed every DT_Resources row is placed; only species / spawner-placed rows exist -> read `unplaced_resources` first. (progression-pass1)
- [data] Authoring from the *_JSON.json mirrors overwrites live data -> mirrors stale since early checkpoints -> rows get the live table first. (item-progression-data-gaps, multi-agent M21)
- [data] `ue.py rows set` bulk path (> 20 rows) silently wrote nothing when the batch had a new row and never verified arrays -> fixed + tests (Oct 17); verify nested-array writes by readback anyway. (wes-calls-oct17)
- [test] GiveItem refuses ("cannot hold") in a test -> the test kit fills the inventory -> trim stacks via the bridge (inv.remove_item_by_definition_id). (progression-pass1)
- [test] RunAll DropPickup fails -> full inventory -> run RunAll in a fresh game. (playtest-oct14, volume-stamp)
- [test] Crafting gates can't use TimeScale -> crafting ticks on real UtcNow -> read state immediately after resume. (dev_status)
- [design] A widget owns the harvest clock -> state owned by the wrong layer (systemic pattern #2) -> timed actions belong on the pawn's UMOTimedActionComponent. (progression-pass1, code_health)

#### BACKLOG (open)
- Progression | OPEN | Wes | F10 heat-ladder values + missing kiln stations (pit kiln, updraft kiln) | PROGRESSION_ROADMAP §4, Technology_History | Night_Report call #3
- Progression | OPEN | Wes | F16 intended play time per era (Feb intent ~1,000 h) | ROADMAP §2
- Progression | OPEN | Wes | F17 chariot / horseshoe without draft animals | ROADMAP §3
- Progression | OPEN | Wes | F18 attempt-anything rule on top of inspection precursors | ROADMAP §6
- Progression | OPEN | Claude | GAP producers: feathers (birds), horn, flax/nettle, wool (sheep), resin tap target (HarvestSap), tin placers | ROADMAP §5 | progression_map blocked 10
- Progression | OPEN | Claude | Place or delete unplaced resource rows: AppleTree, ClayDeposit, CommonSpruce, ReedBed, TallGrass | progression_map unplaced
- Progression | OPEN | Claude | Agriculture (seeds, crops, fields) -- hoe and sickle exist | ROADMAP §3 Era 3
- Progression | OPEN | Claude | Early dressing for first aid before cloth (MakeBandage needs WovenCloth01) | ROADMAP realism checks
- Progression | OPEN | Claude | Lime burning (quicklime) once a kiln exists | ROADMAP Era 3
- Progression | OPEN | Claude | Orphan items a realistic recipe could feed (night report): SodaAsh01 (done BurnPlantAsh), HideGlue01 (done), TinIngot01 (done), Greenstone01 (replaced by Basalt) -> verify all done | Night_Report "Still open"
- Progression | OPEN | Claude | Art: stone maul uses the hammerstone icon/mesh; copper/flint deposits use boulder meshes (need ore / chalk-flint look) | progression-pass1
- Progression | OPEN | Claude | Harvest timing owned by UMOHarvestProgressWidget -> move to UMOTimedActionComponent | progression-pass1 | alias H22-related
- Progression | OPEN | Claude | "Every ingredient has a producer" check exists now as Data:Sources (done 271830a8) -- close item from item-progression-data-gaps | done
- Crafting | IN PROGRESS (uncommitted) | Codex | Deep Dive #3 (client-invented station context), #8 (cancel never reaches authority), #9 (survivor craft jobs can't withdraw liquids), #10 (cancel refunds different water): batch C built + tested | SESSION_STATE
- Crafting | IN PROGRESS (uncommitted) | Codex | Deep Dive #1, #2, #4, #5 (equipment slot index, hand-swap double ownership, stack replication, capacity): batches A/B built + tested | SESSION_STATE
- Crafting | OPEN | Codex | C3 reload recovery: SurvivorReload.ActiveLoadAndPossession fails (no-nav fixture triggers the unreachable move-leg guard; do not weaken the guard) | CX-0064
- Crafting | OPEN | Wes | Building cancel/refund semantics (queue path full refund world-drops vs ghost-menu skill-partial) | dev_status Jul 20 | F7

#### DECISIONS
- 2026-10-15: "take a fresh look" at progression (not just options C+A); "the starting kit will ultimately be nothing"; "every world thing gets an action; plain cover (grass) defaults to Forage". (wes-calls-oct15)
- 2026-10-15 eve: "keep working on the progression ... making the environment more interactable". (progression-pass1)
- 2026-10-14 eve: "merge the duplicate item rows" -> b0753b0a. (item-progression-data-gaps)
- 2026-10-14: item art -- Wes sources realistic assets over time; icons get rebuilt FROM those meshes; do NOT propose generated art (Wan2GP bad at icons; ChatGPT / paid APIs not wanted). Wanted later: 3D item preview in the inspection window. (playtest-oct14)
- 2026-10-17 ~04:00: "discovery should be locked behind 'inspect' if nothing else opens up to it, somewhere in the clay tech line humans would discover leftover metals etc, just make it realistic" -> d6a3af69 (126/136). (wes-calls-oct17)
- 2026-10-17 ~00:00: ores collectible with primitive tools; iron's hurdle is heat; research real tech history. (geology)
- 2026-10-17 (N6, this consolidation): "we need a master progression document or something ... a proper roadmap that dictates design" -> Docs/PROGRESSION_ROADMAP.md is the design authority; DTs implement it.

---

### STAGE 08: building + terraform (sources: memories coop-server-driven-construction, oct16-bug-pass-night, first-ten-minutes-pass, wes-calls-oct16, terraform-excavation-design, dev_status (Jul 8), code_health; Terraform_Excavation_Plan.md (kept))

#### TRAPS
- [coop] A UI request on the server's copy of a joiner's controller opens nowhere -> interaction handlers run on the server -> ForwardUIContractRequest -> ClientUIContractRequest. (coop-server-driven-construction)
- [test] A co-op build test passes while players can't build -> it placed through a server call, not the ghost -> test the real input path. (first-ten-minutes)
- [ui] Ghost "invalid" looked green -> tint wrote a parameter (Color) the material lacks -> check the MID parameter exists. (first-ten-minutes)

#### BACKLOG (open)
- Terraform | OPEN | Wes | Stage 6: designation tool UI + dump-destination popup; re-ask the question plainly (Oct 15 he didn't understand) | Terraform plan | F8 | alias G22 (Codex H22)
- Building | OPEN | Wes | Cancel / refund semantics (queue path full refund + world drops vs ghost-menu skill-partial) | dev_status Jul 20 | F7
- Building | OPEN | Claude | H19 residual: ServerPlaceBuilding trusted the client FTransform (no reach / collision recheck) -- verify against current code | PROJECT_STATUS July 8 banner
- Building | OPEN | Claude | Live building surface (UI migration deferred fork) | dev_status Jul 20
- Building | OPEN | Claude | Site menu nits: source labels right-aligned (two spec tries failed); "Rotate and Flip" hint lingers after the first build; "Select Crude Club" + splint/bandage/blanket in the campfire list | first-ten-minutes
- Building | OPEN | Claude | Cliff/embankment assemblies: "just try different things until it works well every time" (#15/#19) -- iterate and measure | wes-calls-oct16 | (World features)
- Terraform | OPEN | Claude | Voxel Nanite material crash after two simultaneous digs (not reproduced) -- watch | terraform-excavation-design

#### DECISIONS
- 2026-07-08: terraforming realistic time -- duration = earth volume x TerraformSecondsPerCubicMeter (30000 = 1 cm over 1 m2 = 5 min); house-flatten hours, a mine days ("for fucking ever"); collapses RPC frequency (14e0d432). (dev_status)
- 2026-07-13: terraform spoil is NOT a carried counter: a full designation + pawn-labor loop (dig / dump-fill / flatten zones, multiple allowed; villagers automate; flatten = dig high + fill low under conservation of earth; dump popup: designated sites / fill inventory / fill nearby inventory). Forks LOCKED: sphere zones (center+radius) + settlement-scoped dispatch; 80 items/m3; solo player digs manually. Stage 1b earth materials authorized (Dirt01, Topsoil01, Gravel01, Mud01, Silt01). (terraform-excavation-design)
- 2026-10-13: "co-op build for sure needs to be server/host driven" (757e47c1). (coop-server-driven-construction)
- 2026-10-16: building ghosts stay pale white (PlannedColor / PlannedGlow as shipped). (wes-calls-oct16)

---

### STAGE 09: AI + colony (sources: parked TR creature AI / job APIs / colony design, CLAUDE colony section, MobAIPlan.md (Feb), Fable5_Autonomous_Pipeline.md (July cards), Fable5_Village_Handoff, MO57_Master_Plan colony track, PROJECT_STATUS colony track, memories dev_status, post-reveal-hitches-and-ai-perception, terrain-v5 (recruitment), coop-pawn-lifecycle, wes-calls-oct15 (occupancy))

#### TRAPS
- [ai] Creature perception never fires ("Listener must have a valid id") -> a perception component with no senses at OnRegister is never registered -> RequestStimuliListenerUpdate() before ConfigureSense. (post-reveal)
- [ai] "IsInAttackRange does not exist" -> blackboard values pushed in OnPossess before the BT bound the blackboard -> prime in Tick the first frame the asset exists. (caves-and-lighting)
- [ai] Spawn hitch from soft assets -> LoadSynchronous at spawn -> declare in GatherRuntimeAssets; spawn manager async-loads at the first spawn check. (post-reveal)
- [ai] Fallback deer spawned under the sea -> channel multi-traces stop at the first tree / no water check -> MOSpawnClearance (TraceVoxelGround + IsUnderOpenWater + IsStandingSpotClear). (post-reveal)
- [ai] Job + quota slot wedge forever -> SetStayAtLocation suspends job processing -> AbortCurrentJob() first. (dev_status)
- [test] Pawn teleported next to a building is shoved away -> lands inside the collision hull -> stand gates 2.5 m out. (dev_status)
- [test] "Current job" sampling misses fast jobs -> assert on stock deltas. (dev_status)
- [cpp] Downcasting a plain C++ interface pointer fails -> UE has no RTTI -> iterate the concrete actor type. (dev_status)
- [ai] Survivor BT assets don't exist -> survivors are AMOSurvivorController's C++ state machine -> don't look for BTs. (dev_status)

#### BACKLOG (open)
- AI | OPEN | Claude | B1 blocked-job reasons at the data layer (EMOJobBlockReason: MissingIngredient / NoPath / TooCold / TooTired / NoStation / StorageFull / ToolMissing / ThreatNearby; tier-2 alert; MO.Colony.Status prints it) -- not built | Fable5 pipeline B1
- AI | OPEN | Claude | A6 possession-transition summary (UMOCharacterHistoryComponent::BuildAwaySummary) -- not built | pipeline A6
- AI | OPEN | Claude | T1 transfer-all with combined timer -- partly (TransferAll code exists: verify); T2 sweep pickup (timed radius gather) -- not built; T3 cut 2 generalize haul to food/water restocking | pipeline T1-T3
- AI | OPEN | Claude | R1 readability audit ("why" readouts, action timers everywhere, failure strings for every bucket) | pipeline R1
- AI | OPEN | Claude | F1 remaining: wet wood burns poorly (needs per-item wetness state -- design first) | pipeline F1
- AI | OPEN | Claude | Winter + fire soak under load | dev_status
- AI | OPEN | Wes | V3 real art via Fab (needs Wes purchases; debug meshes hold) | pipeline V3
- AI | OPEN | Claude | Creature ABP state machines + EQS query assets (Blueprint setup; C++ complete) | CLAUDE pending implementation
- AI | OPEN | Claude | More fauna (fox / pig / crow assets exist; need ABP work); birds also unblock feathers | terrain-v4, ROADMAP
- AI | OPEN | Claude | Unify the creature flee decision (computed twice) before fire-fear | Light_Sources
- AI | OPEN | Claude | Creature footsteps (skeletons lack ball_l / ball_r) | AAA P3
- AI | OPEN | Claude | Survivor quests: no survivor has PossibleQuests configured (data task) | terrain-v5
- AI | OPEN | Claude | Colony UI: colony bar (4B), task assignment without possession (6B), alert tiers (7B) | PROJECT_STATUS colony track
- AI | OPEN | Claude | Villager sleep (fatigue has no recovery); teach / clump-density balance | dev_status leads
- AI | OPEN | Claude | Survivor craft jobs carrying containers (water as ingredient) -> Codex batch C #9 in progress | progression-pass1
- AI | OPEN | Claude | AI known issues (M1 hardcoded BB keys, M2 magic numbers, M3 IsCornered placeholder, H6 FindPackMembers GetAllActorsOfClass, M6 activity types) -- audit codes | TR creature AI

#### DECISIONS
- 2026-07: Wes approved autonomous execution of the Fable 5 pipeline (gated stage cards toward village + assets + PCG). (dev_status)
- W-track (drink from world water, springs, rain collector, water hauling) was logged as NEEDS WES APPROVAL in July; W1 was built Oct 15 on Wes's own ask (drink from lakes / rivers / sea). W2-W4 not decided. (pipeline)
- 2026-10-15: possession menu shows occupancy (built). (wes-calls-oct15)

---

### STAGE 10: co-op (sources: memories steam-multiplayer-integration, coop-pawn-lifecycle, host-pawn-under-terrain, ue-multiprocess-testing, oct16-bug-pass-night, wes-calls-oct16 (aim), dev-console (server dev mode), shipping-diag, packaging-dev-build)

#### TRAPS
- [coop] A client's "Server" RPC runs on the client in tests -> editor Python (and console commands it dispatches synchronously) run every RPC LOCAL (GAllowActorScriptExecutionInEditor) -> defer test verbs one tick; assert on game logs. (coop-pawn-lifecycle, ue-multiprocess, dev_status)
- [coop] Lobbies found but nobody can join -> engine uses the FIRST NetDriverDefinitions entry per DefName; a missing class silently falls back to IpNetDriver -> `!NetDriverDefinitions=ClearArray` + SteamSocketsNetDriver; assert the driver class, never "it connected". (steam-multiplayer, ue-multiprocess)
- [coop] LAN host never found -> Null OSS answers only bIsLANMatch=true -> IsLanMode() for Host and Find. (steam-multiplayer)
- [coop] "SteamAPI_Init failed" in the editor -> Steam is disabled in editor/PIE by design -> test in a -game process. (steam-multiplayer)
- [coop] Host again after quitting to menu fails ("already in a session") -> sessions outlive worlds -> ReleaseStaleSession. (steam-multiplayer)
- [coop] Session row unclickable for real while SimulateClick works -> the row button was a NATIVE UserWidget class (no widget tree) -> use the WBP MOButton; do one real click. (steam-multiplayer, ue-ui-toolset)
- [coop] Joiner can't control his pawn -> OnPossess is server-only -> SetPawn runs setup on the owning client. (coop-pawn-lifecycle)
- [coop] Clients see the host's pawn under the terrain -> spawned before the final terrain + rescue fell back to Z=200 + channel trace hid terrain behind trees -> spawn settle + object-type trace + last grounded spot. (host-pawn-under-terrain)
- [coop] Client's copy of the host pawn free-falls -> client predicts a fall for a simulated proxy with no floor; voxel collision exists only around each machine's own pawns -> OnUpdateSimulatedPosition never applies gravity to proxies. (host-pawn-under-terrain)
- [coop] "client pawn z == server z" passes with no client terrain; a first-hit trace agrees at the trace origin -> vacuous checks -> MO.Voxel.SurfaceGrid both machines + skew-seed negative control. (coop-pawn-lifecycle)
- [coop] PIE 2-player listen mode drags the second window to the menu as a connected client ("both screens flip") -> use real processes. (coop-pawn-lifecycle)
- [coop] Joiner's right-click "worked" on its own copy -> MO.Test.Input sent the RPC inside the bridge call -> it now defers every non-Move/Look action a tick. (oct16-bug-pass)
- [coop] Items in a pile steal aim / tumble -> proxy sphere on AMOWorldItem -> removed; the mesh is the target; item object queries must include ECC_PhysicsBody. (oct16-bug-pass)
- [coop] Dropped items fall through terrain on a host near a joiner -> query collision present, physics body not yet -> remember the drop's ground, settle from above. (oct16-bug-pass)
- [harness] -game instance started while the EDITOR is open gets NO vegetation near spawn (readiness capped, 0 within 60 m) -> close the editor (ue.py editor stop) before any visual / PCG / harvest instance test. (ue-multiprocess)
- [harness] `inst start x --nosteam` silently launched on Steam -> argparse REMAINDER swallowed a sibling's options -> real subparsers; never reuse the name `p` in main(). (ue-multiprocess)
- [harness] -game of the editor binary crashes on voxel worlds / EditorLevelLibrary -> no GEditor -> TestMap for lan; Voxel GEditor guard in VoxelSystemUtilities.cpp for game/hostsave/soak; reach subsystems via console verbs. (ue-multiprocess, dev_status)
- [harness] EditDefaultsOnly props invisible to Python -> call UFUNCTIONs. (ue-multiprocess)
- [harness] Restarted editor writes MO57_2.log -> ue.py follows the freshest MO57*.log. (ue-multiprocess)
- [harness] A crashed host's log is gone -> instance game.log is overwritten by the next start -> look in Saved/Crashes/UECC-*/game.log. (ue-multiprocess)
- [harness] Long chains (>10 min) killed by the background timeout -> check for orphaned UnrealEditor.exe, `ue.py inst stop <name>`; only stop instances this tool launched (pid file + image check). (ue-multiprocess)
- [harness] `ue.py seq` prints only DONE/FAILED -> read inst_<name>/ue_out.txt for the traceback. (ue-multiprocess)
- [python] BlueprintReadOnly C++ property: get_editor_property fails -> attribute access or a C++ accessor. (ue-multiprocess)

#### BACKLOG (open)
- Coop | OPEN | Claude | Friend-invite / Steam-overlay join (InviteUserToGame) | steam-multiplayer
- Coop | OPEN | Claude | Host-a-SAVE over real Steam not exercised (only nettest hostsave on Null OSS + a real click) | steam-multiplayer
- Coop | OPEN | Claude | Host/Join panel styling (default light-grey text boxes) -> UI kit pass | steam-multiplayer
- Coop | OPEN | Claude | Residual: 1/6 joins 17-50 cm ground mismatch for ~10 s | coop-pawn-lifecycle
- Coop | OPEN | Claude | Co-op: a client lighting its own torch at a fire (see Light) | Night_Report
- Coop | OPEN | Claude | Joining client treating ANOTHER patient (anatomy wounds owner-only; needs an inspection request seam) | MEDICAL_HANDOFF
- Coop | OPEN | Claude | Audit authority residuals (H17 DoS SweepModifiedZones remainder? H19 transform recheck) -- re-verify | PROJECT_STATUS banners

#### DECISIONS
- 2026-09-22: "real steam integration" (App 1602810 "Metaverse: Origins"). (steam-multiplayer)
- 2026-10-08: Wes verified a real two-account Steam session; both characters on the terrain. (steam-multiplayer)
- 2026-10-16: "aim assist is good; pawns should punch in the direction of the camera". (wes-calls-oct16)
- 2026-10-13: "co-op build for sure needs to be server/host driven". (Building)

---

### STAGE 11: UI (sources: parked TR CommonUI/controllers/delegates/breath, CLAUDE widget patterns + UMG automation, UI_Overhaul_Architecture.md pitfalls (to archive), UI_Kit_Implementation.md (kept), memories architecture_decisions (Menu mode), commonui-pending-activation-race, hud-yields-to-menus, hud-modes, volume-stamp (router half), ue58-ui-render-traps, ue-ui-toolset, ui-design-system-brief, dev-console, overnight-polish (focus, SetIsSelected), oct16-bug-pass (NativeOnInitialized), steam-multiplayer (native class node))

#### TRAPS (short pointers; full text in Docs/UI.md §2-§5)
- [ui] Menus orphaned / unclosable after a fast close -> CommonUI activates a pushed widget only after its transition -> LayerTransitionSeconds 0, CloseCachedMenu, never RemoveWidget a pending entry. (commonui-pending)
- [ui] Menu closes on any click / input dead -> router config cache + root adopted only when painted -> gameplay input only via ApplyGameplayInput; AssertInputConfig. (volume-stamp)
- [ui] WASD leaks into gameplay with a menu open -> ECommonInputMode::All -> Menu mode everywhere. (architecture_decisions)
- [ui] Real clicks pass through a button that simulated clicks hit -> native UserWidget class used as a node -> WBP MOButton; one real click. (ue-ui-toolset)
- [ui] Crash in FCachedTypefaceData -> SetWidgetStyle keeps a pointer to the caller's local -> edit WidgetStyle in place + SynchronizeProperties. (ue58-ui-render-traps)
- [ui] Blank save thumbnail in packaged builds -> ReadPixels from game code -> FScreenshotRequest + OnScreenshotCaptured. (ue58)
- [ui] HUD bar invisible -> SetPositionInViewport resets anchors -> full-screen widget + aligned Overlay. (ue58)
- [ui] Inventory icons vanish -> NativeConstruct re-reads authored state on re-add -> NativeOnInitialized. (oct16)
- [ui] Tab row deselect does nothing -> SetIsSelected(false) on a non-toggleable CommonUI button -> UMOCommonButton::SetSelectedState. (overnight-polish)
- [ui] Reticle / tutorial only learn about a menu on the way out -> synchronous UpdateReticleVisibility after Push sees count 0 -> NativeOnActivated refresh (don't remove). (hud-yields)
- [ui] Every FindWidget query "0 matches" -> an asset tab hides the PIE viewport (not painted) -> ue.py ui menu closes tabs first; asset editors can't close while PIE runs. (ue-ui-toolset)
- [ui] Same-frame menu close + reopen fails in tests -> CommonUI reconciles next tick (F21) -> frame-stepped tests. (dev_status, Codex CX-0003)
- [ui] Keys stop after alt-tab ("needs a refocus") -> window re-activation moves Slate focus to SViewport -> focus fallback + reclaim. (overnight-polish)
- [ui] Engine console swallows Tilde -> UGameViewportClient owns it -> DefaultInput.ini without +ConsoleKeys=Tilde; UEnhancedInputComponent deletes BindKey -> use InputKey. (dev-console)
- [ui] Case-only widget rename returns False -> FName is case-insensitive -> rename through a temporary name. (footsteps, animation-retarget)
- [ui] BindWidget name mismatch silently unbound (MotionBlurCheckBox / MasterVolumeText_1) -> `ui check` shows `absent` -> fix the spec. (footsteps)
- [ui] .uasset save returns False -> a running instance locks it -> inst stop first. (overnight-polish, UI_Kit)
- [ui] save_loaded_asset skips a clean package / CDO change -> only_if_is_dirty=False; importlib.reload(mo_ui) after editing it. (UI_Kit)
- [ui] Fonts render as tofu -> a bare UFontFace as FontObject -> go through UFont (composite fonts from C++). (UI_Kit)
- [ui] Kit colours wrong -> hex is sRGB, UMG wants linear -> kit_lib.color() (mo_ui._color does not convert). (UI_Kit)
- [ui] ClickWidget "PASS" lies -> it logs SENT for non-MO widgets; synthetic pointer click -> read the state it should change. (first-ten-minutes)
- [ui] Instance UI screenshots lie -> instance window defaults to 640x360 (minimum DPI scale) -> r.setres 1920x1080w first. (first-ten-minutes)
- [ui] HighResShot shows no menu -> it renders the 3D scene only -> ui shot / win_input.ps1 shot. (ue-ui-toolset)
- [ui] Python in -game: no access to protected BindWidget members -> WidgetLibrary.get_all_widgets_of_class + get_typed_outer. (ue58, ue_pie)

#### BACKLOG (open)
- UI | OPEN | Claude | Current screen + behaviour inventory (which screens are in the kit, which are not) | alias G14 (Codex H14)
- UI | OPEN | Claude | Finish remaining Field Ledger surfaces (HUD WBP layouts, Options, main / in-game menu, colony / possession, modals) | alias G15
- UI | OPEN | Claude | Connect missing controls + accessibility settings (SetAccessibility not wired to Options) | alias G16, UI_Kit
- UI | OPEN | Wes | Kit fonts: download the OFL fonts (Bitter 600/700, IBM Plex Sans 400/600, IBM Plex Mono 500) -- needs a yes | UI_Kit | F14
- UI | OPEN | Claude | Unbound optional widgets (ui check `absent`): WBP_UnifiedI... list in AAA P3 | AAA
- UI | OPEN | Claude | UI nits: 12 px list rows + 0.5-scaled entry button, white native amount SpinBox, "Characters" possession screen, FuelTimeText / "Current:" placeholders | ui-design-system-brief
- UI | OPEN | Claude | UI polish pass (spacing / focus / controller); Options faint labels and row order | AAA P3
- UI | OPEN | Codex | UI consolidation Stage 4 (skills / knowledge + quest collections); deferred: survivor-queue adapter, live building surface | agent/ui MIGRATION_PLAN, dev_status
- UI | OPEN | Claude | 3D item preview in the inspection / details window (Wes wants later) | playtest-oct14
- UI | OPEN | Claude | MOUITestSubsystem tautological tests (M17) | code_health, architecture_decisions

#### DECISIONS
- 2026-10-09: tutorial text goes away while other menus are open and comes back when the menu closes. (hud-yields)
- 2026-10-05: "UI work is tedious -- find a toolset so Claude does it, not screenshot-click loops" -> ue.py ui (UMG is scripted). (ue-ui-toolset)
- 2026-10-11: "yes, we should make a more cohesive ui schema, give me a prompt to feed into claude design ..." -> UI kit. (ui-design-system-brief)
- 2026-10-14: "prioritize getting the console stuff running in the shipped version or just build a 'dev mode' button in options so we just have a menu with all the MO. console stuff" -> both built. (dev-console)
- 2026-10-15: HUD modes F1 (clean screenshots / video). (hud-modes)

---

### STAGE 12: persistence + audio (sources: memories crash-endpoint-and-bug-reports, code_health, dev_status (persistence re-verify), mortality (generator identity), audio-heartbeat-and-menu-return, footsteps-and-graphics-quality, overnight-polish (sound), terrain-v5 (intro / ambience), PROJECT_STATUS Oct 14 banner; parked TR footsteps)

#### TRAPS
- [audio] Audio faults leave no log -> engine logs nothing; LogAudio=Warning hides Display lines -> run the [MOAudio] HB heartbeat first. (audio-heartbeat)
- [audio] Muting doesn't silence a NaN mix -> NaN x 0 = NaN -> stop components / bypass submix effects to bisect. (audio-heartbeat)
- [audio] Full volume after the first map load -> UEngine::LoadMap resets the device transient primary volume to 1.0 -> re-apply in HandleWorldAudioContext. (audio-heartbeat)
- [audio] Harness instances are silent -> -nosound by default -> inst start --sound. (audio-heartbeat, oct16)
- [debug] A debug FILE logger costs ms per spawn -> Write+Flush per line on by default -> default off + pre-format gate + compiled out in Shipping. (post-reveal)
- [persist] UncompressMemory logs an Error on a size mismatch (fails automation) -> take the exact size from the bundle. (crash-endpoint)
- [persist] UGameInstanceSubsystem tests need a UGameInstance outer (ClassWithin). (crash-endpoint)
- [5.8] GAverageFPS gone; LexToString(EBuildConfiguration) replaced EBuildConfigurations::ToString. (crash-endpoint)

#### BACKLOG (open)
- Audio | OPEN | Claude | Verify the XAudio2 backend at launch (log shows the XAudio2 mixer) + Wes's relaunch test | audio-heartbeat
- Audio | OPEN | Claude | ~20 s of thin menu audio after a map load (UDW DSP) | audio-heartbeat
- Audio | OPEN | Claude | Audio pass: creature footsteps (no ball bones), weather sound layers per biome | AAA P3
- Audio | OPEN | Claude | Verify audible output + finish environmental audio | alias G18 (Codex H18)
- Persistence | OPEN | Codex | Survival meters persisted (Deep Dive #6) -- batch D built, uncommitted | SESSION_STATE
- Persistence | OPEN | Claude | Remaining persistence audit items (M/L) per PROJECT_STATUS tracker -- re-verify | PROJECT_STATUS

#### DECISIONS
- 2026-10-16: "silent when unfocused" (UnfocusedVolumeMultiplier 0.0 declared in DefaultEngine.ini). (audio-heartbeat)
- 2026-10-16: Wes asked for web research before any engine change (audio) -> XAudio2 instead of an engine patch. (audio-heartbeat)
- 2026-10-13: "we don't care about any old saves or preserving them" (generator identity: warning only). (Handoff #4)

---

### STAGE 13: packaging + performance (sources: memories packaging-dev-build, package-cook-errors-oct16, shipping-diag-and-zero-pcg, overnight-polish (cook trap), oceanology (cook), hitch-hunt-lake-query, post-reveal-hitches, footsteps-and-graphics-quality, playtest-oct14, far-trees, AUTONOMOUS_TOOLING packaging sections, AAA quality gates, parked TR graphics quality)

#### TRAPS (pointers: full text Docs/Packaging.md §2, Docs/Performance.md §2)
- [cook] Packaging dies on a fatal ensure -> a subsystem Initialize() GetSubsystem() of a sibling absent in the cook world -> bind in OnWorldBeginPlay. (CLAUDE UE 5.8 notes)
- [cook] Cook aborts "missing FAnisotropyPS" / editor crashes duplicating a migrated master -> Substrate legacy shading-model pin wired to a constant -> fix_substrate_shading_model_pin.py (re-run after any ED / Megascans migration); fix_hornbeam_masters.py for foliage. (overnight-polish)
- [cook] Niagara X3662 cs_5_0 -> NS_GerstnerWaves cooked by a too-wide always-cook dir (ddx in compute, DX11) -> narrow the dir. (package-cook-errors)
- [cook] "Default Material will be used" in an RT hit shader -> Substrate SLW material without the legacy SLW output node -> add it. (package-cook-errors)
- [cook] Editor-UI packaging "Cook failed" with a clean log -> the cook child also auto-started the MCP on port 8000 -> bAutoStartServer=False; ue.py passes -ModelContextProtocolStartServer. (AUTONOMOUS_TOOLING)
- [cook] BUILD FAILED in the first seconds with an IOException -> Update-BuildInfo.ps1 two-target write race -> mutex (fixed); look there first. (packaging-dev-build)
- [cook] UAT packaging fails instantly -> two game targets without DefaultGameTarget=MO57. (shipping-diag)
- [cook] Packaged crash in InitBubbles -> soft-path plugin assets not cooked -> DirectoriesToAlwaysCook + the cooked-assets test. (oceanology)
- [ship] Shipping report undiagnosable -> Shipping writes no log -> package Development; ShipDiag target for Shipping code paths; bug-report fields. (packaging-dev-build, shipping-diag)
- [ship] Editing Wes's settings for a Shipping probe -> %LOCALAPPDATA%/MO57/Saved is his REAL config -> back up byte for byte, restore + cmp. (dev-console)
- [ship] Next BuildCookRun fails deleting the staging dir -> the staged MO57.exe is a launcher; its child kept running -> stop the tree. (pcg-load)
- [ship] Packaged CSV profiles vanish -> wiped by the next --package -> copy them out first. (post-reveal)
- [ship] Computer-use on packaged windows: grant by the REAL image name once running (MO57.exe); a textinputhost window can steal the front and block clicks (don't grant it; drive by command line); open_application starts ANOTHER instance. (packaging-dev-build, ue_pie)
- [ship] -log opens a console window over the game -> omit -log (logs are still written); the first click can land on the desktop: click twice. (ue58-ui-render-traps)
- [perf] Hitch counters look normal -> game/render/GPU counters lag the frame-time measurement by one frame -> log a ring of frames. (hitch-hunt)
- [perf] -hitchdetection does nothing -> compiled out (ALLOW_HITCH_DETECTION 0). (hitch-hunt)
- [perf] Benchmarks inconsistent -> editor running (shared GPU) / stale far cells -> no editor; compare within one session. (far-trees)
- [perf] PIE fps comparisons meaningless -> editor PIE is frame-capped (~58) -> compare gpu= / draws= at a pinned camera. (playtest-oct14)
- [perf] A -game instance runs the hardware benchmark -> GIsEditor false -> every harness passes -NoQualityBenchmark. (footsteps)
- [perf] Engine settings never applied -> UMOGameSettings::ApplySettings skipped Super -> ApplyMOSettings calls ApplyNonResolutionSettings. (footsteps)
- [perf] Lower-tier cvar leaks into Epic -> scalability writes only the applied level's cvars -> pin every overridden cvar at all four levels; run MOFramework.Graphics.ScalabilityFile. (footsteps)
- [perf] "STREAMING POOL OVER BUDGET" -> Epic TextureQuality pool 1000 MB -> 4000 (Cine 6000) + LimitPoolSizeToVRAM. (overnight-polish)

#### BACKLOG (open)
- Perf | OPEN | Wes | F4 reference resolution / quality + target hardware tier | Handoff #5, AAA fork
- Perf | OPEN | Claude | Replace incomparable FPS claims with controlled evidence | alias G06 (Codex H06)
- Perf | OPEN | Claude | NavigationBuild ~15-20 ms frame; 25-30 ms render-thread frames; arrival hitches after teleports (67 -> 145 over 11 arrivals) | post-reveal, Ground_Coverage
- Packaging | OPEN | Claude | Close correctness gates on the actual final package | alias G05 (Codex H05)
- Packaging | OPEN | Claude | First-launch-after-packaging hitch (PSO) suspected behind the PCG stall / barren spire (#16) | oct16-bug-pass
- Packaging | OPEN | Claude | Restore reliable captures and reduce iteration cost | alias G25 (Codex H25)

#### DECISIONS
- (none new; F4 open)

---

### STAGE 14: world features + terrain material (sources: World_Features_Architecture.md (kept), memories world-features-pois-and-electric-dreams, wes-calls-oct16, terrain-material-far-tiling, far-trees-materials-clutter-oct13, electric-dreams-env-project, playtest-oct14)

#### TRAPS
- [dt] DT JSON round-trip silently EMPTIES TMap<FName,FString> columns -> the export nests map values; re-import empties the map -> flatten, verify with get_data_table_column_as_string; ue.py rows set now snapshots and re-sends maps (fails if a map came back empty); recover lost values from the HEAD LFS blob (`git show HEAD:<asset> | git lfs smudge`). (world-features, ue-unified-cli)
- [dt] fill_data_table_from_json_string with a missing field = a blocking MODAL dialog. (world-features)
- [cpp] A default-constructed FMOWorldGenParamsRef (TSharedRef) is FATAL at runtime -> init with MakeDefault. (world-features)
- [editor] EditorStaticMeshLibrary collision counts read -1 during PIE -> audit outside PIE or by tracing. (world-features, terrain-v4)
- [python] unreal.Rotator(a,b,c) is (roll, pitch, yaw). (world-features)
- [editor] The editor will not save during PIE (and save_assets still answers True) -> ue.py refuses saves while PIE runs; save /MOFramework/Data/DT_x after `pie end`. (world-features, AUTONOMOUS_TOOLING, playtest-oct14)
- [pcg] A Blueprint composition's PCG component exists only after FinishSpawning (construction script) -> bind / set seed after it; CleanupLocal before regenerating (GenerateLocal(force) ADDS instances). (oct16-bug-pass, wes-calls-oct16)
- [pcg] Editor-world PCG does not generate for a composition -> measure in PIE (summon /Script/MOFramework.MOWorldFeatureActor + set_graph + generate_local). (world-features)
- [pcg] White orb in POIs -> Electric Dreams PCGAsset elements bake editor markers (/Engine/BasicShapes) -> the feature subsystem hides basic-shape instances after each generation. (oct16)
- [content] Migrated ED meshes have NO collision -> prep_rock_meshes.py / fix_mesh_collision.py (composition rocks complex-as-simple; re-run after ede_migrate: ED content is gitignored). (overnight-polish, world-features)
- [content] New ED folders are not ignored automatically -> add them to .gitignore BY HAND (ede_migrate only prints them). (overnight-polish)
- [content] Editor "bridge not responding" for 15-30 min after new ED assets -> it is building Nanite / textures. (overnight-polish)
- [content] ED ForestGround / ForestTerrain are mesh-UV atlases, not tileable surfaces. (overnight-polish)
- [content] Library Megascans exports point at /Game/MSPresets/M_MS_Default_Material (absent) -> reparent onto M_MS_Base (fix_megascans_import.py). (terrain-v5)
- [content] Editor Python has no PIL -> generate PNGs with system python first. (terrain-v5)
- [material] Rows of snow patches / far checker -> one axis-aligned noise read repeating; detail normal maps at range; MO_NormalMod 32 m read -> rotated reads, fade detail normals past ~250 m / ~600 m; run the shadows-off control. (terrain-material-far-tiling)
- [material] M_QuixelBase structure change -> needs the pristine graph (git checkout f78553e3 -- M_QuixelBase.uasset, editor closed) then upgrade_terrain_material.py; never delete expressions in editor Python (MEL.delete_material_expression asserts !IsRooted). (terrain-material-far-tiling, far-trees)
- [material] A fixed height band spread beach sand over the whole coastal plain -> band = 8 % of the edge's height above sea, 1.2..40 m. (terrain-material-far-tiling)
- [editor] unreal.StaticMeshDescription().create_polygon_group() null-derefs; OBJ import via AssetImportTask is the safe mesh route. After an editor crash the package-restore dialog blocks the bridge: ue.py editor stop, move Saved/Autosaves aside, start again. (far-trees)

#### BACKLOG (open)
- World features | OPEN | Wes | Real cliff WALLS: own composition along a steep contour vs generator crags dressed with the meshes | world-features, bug #27 | F9
- World features | IN PROGRESS | Claude | Cliff / embankment assemblies: iterate and measure until they work every time (#15 / #19) | wes-calls-oct16
- World features | OPEN | Claude | Per-instance grounding (FMOCompositionGrounding) exists as a census, auto-apply OFF (moving pieces broke compositions) | wes-calls-oct16
- World features | OPEN | Claude | Spire "barren" (#16) not reproduced (generation failure in Wes's session; first-launch PSO?) -- watch WorldBuild.* fields | oct16
- World features | OPEN | Claude | Unifying design (caves / rivers / landmarks / ore / player builds as features) mostly unbuilt | World_Features_Architecture

#### DECISIONS
- 2026-10-14: "build entire pcg POIs that have all these things self contained ... rather than just spawning a bunch of one off instances"; Electric Dreams: "the big rock features and vines", the LIVE system where cliffs rebuild when moved ("if there's a baked version, that's not the one we're looking at"). Later: drop, spread, 4x (ED_RockSpire ~140 m). (world-features)
- 2026-10-14: game level VoxelWorld QualityExponent 1.1 (was 0.82) for mountain holes (Wes). (playtest-oct14)
- 2026-10-16: Electric Dreams broken -> "fix it, I don't care what it really changes" (per-site seed for BP assemblies; every ED assembly's look may change); #15 / #19 cliff + embankment: "just try different things until it works well every time". (wes-calls-oct16)
- 2026-10-11: Electric Dreams Env (D:\UEProjects\ElectricDreamsEnv) = the asset / PCG reference to draw from; migrate selectively, never bulk. (electric-dreams-env-project)

---

### STAGE 20: Docs/Handoff_2026-10-13.md (63 KB, dated log Oct 13-14; to archive -- blocked on Codex commit for MOworldSaveGame.h ref; MOWorldRemovalSubsystem.h ref is committed)
Live content extracted (everything else is dated narrative, kept in the archive):

#### DECISIONS (already captured elsewhere; confirmed)
- Decisions waiting list: #1 survival source (F1), #2 drinking water (BUILT Oct 15: SUPERSEDED), #3 regrowth (F3), #4 H13 old saves (DECIDED break), #5 perf reference (F4), #6 starting kit = "just a test item" (kit no longer completes gather tutorials, stays as is), #7 quests credited to the acting player (FIXED), #8 worker split of biome spawner (REVERTED, negative result).
- 2026-10-13 night: "the pcg in the loading level looks a lot better, but when i load into the game there's ZERO pcg" -> coastal spawn on bare sand (fixed 0cc78030).
- 2026-10-14: "add to the bug report an 'item looked at' so it can narrow down what asset is causing issues" -> LookAt rows.
- 2026-10-14 late morning: "drop down another half meter, spread the smaller base cliffs, the whole outcropping about 4 times larger, collision is fucked up, i fall through them if i try to jump up them" -> 4x ED_RockSpire, MO Conform To Ground, complex-as-simple composition rocks.
- Note: the rejected Steam art is still in the repo (SteamArt/, ~15 MB LFS, pushed): delete only if Wes says so.

#### TRAPS
- [editor] A crashed editor's CrashReportClientEditor window keeps the module DLL locked (LNK1104) -> close it by PID. (Handoff Oct 14)
- [python] line_trace_single returns None on a miss. (Handoff)
- [python] Struct array elements read from get_editor_property are COPIES -> rebuild the structs and set the whole array. (Handoff efficiency)
- [python] MaterialExpressionSetMaterialAttributes.inputs is protected -> use Break/Make MaterialAttributes. (Handoff)
- [editor] `ue.py editor stop` kills by image name -> leave the editor with `ue.py run QUIT_EDITOR` (CONFLICT with "stop only processes you launched": recheck what editor stop does today). (Handoff Oct 14)
- [efficiency] Six editor restarts ~4 min each -> batch C++ edits; content edits need no restart; the ui instance can run next to the editor for VISUAL checks, never for perf. (Handoff)
- [tools] Screenshot tools biome_tour.py / ui_shoot.py returned a 320x180 game image inside a 1920x1080 frame after the instance window size changed (Oct 13) -> biome_eye.py / look_sheet.py / steam_scout.py (HighResShot); recheck ui_shoot.py (death-recap used it later). (Handoff notes)

#### BACKLOG (open)
- World/terrain | OPEN | Claude | Distant lake-water shards: fade / cap / ellipse mask / deeper freeboard | Handoff next actions #1
- World/terrain | VERIFY | Claude | Summit notch (H07) not reproduced on the current build; v7 ceiling cure not proven | Handoff Verify | alias G07
- PCG | VERIFY | Claude | Menu-flyby popping recorded over time (hand-off ring in motion) | Handoff Verify
- Progression | VERIFY | Claude | Gameplay effect of x10 fewer sticks / stones / bark: Tutorial_GatherStone (10 cobbles), starter flow, pawn gather jobs near the coast spawn (H09 census: cobbles 3-17 m, sticks 8-53 m, bark 8-95 m) | Handoff Verify + #5 | alias G09
- World/terrain | OPEN | Claude | Snow is one global altitude line on the terrain only (ignores climate / season; rocks + trees get no snow): an MPC driven by weather / season is the next step (aspect added Oct 15) | Handoff notes
- World/terrain | NOTE | Claude | Far tint is per surface, not per biome: a new surface needs a row in SURFACES in upgrade_terrain_material.py | Handoff notes
- Perf | OPEN | Claude | Survivor spawn: 35 ms spawn tick is the MetaHuman actor itself | Handoff #3
- Packaging | NOTE | Claude | Generator version not save-compatible by design (release notes should say so) | Handoff notes

---

### STAGE 21: Docs/Claude_Handoff_Goals_2026-10-13.md (Codex-authored, H00-H26 -> BACKLOG ids G00-G26; to archive)
Status of each as of 2026-10-17 (from Handoff_2026-10-13 evening summary, memories, night report):

| G | Was | Status 2026-10-17 | Evidence / where |
|---|---|---|---|
| G00 | Reconcile live worktree + evidence | DONE-2026-10-13 (process item) | Handoff evening summary |
| G01 | Prompt loading with local vegetation ready (readiness hold) | DONE-2026-10-13 | READY 6.3 s; Handoff "New-game load" |
| G02 | Thin items without changing the locust branch (RandomChoice_3 split 0.5 + separate thinning node) | VERIFY | thin_item_clutter.py; no explicit closure found |
| G03 | Far-vegetation holes + near ghosts (draw floor + end cull) | DONE-2026-10-13 (verify in motion: menu-flyby hand-off) | pcg-load memory |
| G04 | Canopy appearance survives cooking (M_FarCanopy ISM usage flag) | VERIFY | no explicit closure found |
| G05 | Correctness gates on the final package | DONE-2026-10-13 (standing rule: rerun after each change) | Handoff "Gates on the final package" |
| G06 | Controlled FPS evidence | DONE-2026-10-13 (three-run tours; Rainforest p95 closed) | Handoff evening summary |
| G07 | Summit notch / LOD tear | VERIFY (not reproduced; control done) | Handoff Verify |
| G08 | Unify water/cave exclusion across planting paths | DONE-2026-10-13 (not reproducible; graph audit) | Handoff Verify |
| G09 | First ten minutes after thinning, ordinary input | OPEN (pass 1 Oct 15 used console verbs; resource census done) | first-ten-minutes memory |
| G10 | Survival single source | OPEN-Wes F1 | Survival_Single_Source_Proposal |
| G11 | Heat-balance thermal model | OPEN-Wes F2 | Thermal_Model_Reproduction |
| G12 | Physiological deterioration reaches mortality | DONE-2026-10-13 (DieOf + systemic clocks 4f948b11) | mortality memory |
| G13 | Generator/save compatibility policy | DECIDED-2026-10-13 (break; warning only) | Handoff #4 |
| G14 | Current UI screen + behaviour inventory | OPEN | UI |
| G15 | Finish remaining Field Ledger surfaces | OPEN | UI |
| G16 | Missing controls + accessibility settings | OPEN | UI |
| G17 | Swimming as a complete player interaction | OPEN | Water |
| G18 | Audible output + environmental audio | PARTIAL (NaN guard, XAudio2 Oct 16; verification + footsteps on props/creatures open) | Audio |
| G19 | Snow reflects place and weather | PARTIAL (aspect Oct 15; climate/season/accumulation open) | worldgen |
| G20 | Biome/material + world-richness gaps | OPEN | worldgen, PCG |
| G21 | Art coverage as a production backlog | OPEN (135/216 items have real meshes + icons on Oct 14; 81 placeholders) | Item_Art_Pipeline |
| G22 | Pawn excavation loop | PARTIAL (stages 4-5 done Oct 14; stage 6 UI = Wes F8; stage 7 cart optional) | Terraform plan |
| G23 | Catalog/queue reuse (UI stage 4) | OPEN (Codex) | agent/ui |
| G24 | Re-verify residual production risks | DONE-2026-10-13 (rows C3, C7, H7, H11, H17, H18, H20, H28, H35, H36, H39, C12, H53 re-verified) | Handoff evening |
| G25 | Reliable captures, lower iteration cost | OPEN | Handoff notes |
| G26 | Reconcile documents, one usable handoff | DONE-2026-10-17 by this consolidation (BACKLOG / DECISIONS / INDEX) | this pass |

#### DECISIONS (the §7 list, mapped)
- Survival HUD migration -> F1; thermal -> F2; hardware/quality target -> F4; old-world compatibility -> DECIDED Oct 13; excavation UI / flatten defaults -> F8; building refund semantics -> F7; extra assets / hair patch -> F5, F11, F14.
- Codex's rule kept: "Do not repeatedly ask about choices already answered directly in the current user conversation. This handoff does not reopen locked excavation decisions or previously confirmed policies."

#### TRAPS (from its §8 constraints, not elsewhere)
- (none new beyond those staged; §8 restates the engineering rules)

---

### STAGE 22: OVERNIGHT_LOG.md (Oct 7-9), OVERNIGHT_PLAN_2026-10-09.md, Overnight_Polish_2026-10-12.md (all to archive)

#### DECISIONS / standing rules (from the overnight plans Wes approved)
- Standing rules for unattended runs (OVERNIGHT_PLAN 2026-10-09 "Rules (carried over)"): editor closed to build (if Wes has it open, do non-build work and note it) -- NOTE: superseded in part by N5 (2026-10-17: close the editor when needed unless he says otherwise); **never touch Wes's saves (Harper_Wright-01, Test158, v1gate)**; own test saves are `zz_*` and deleted; stop only processes you launched, by recorded pid; local/free only; two failed patches = instrument; negative results reported as negative; **do not edit Wes's Blueprints** (e.g. BP_WeatherBridge), switch off native sky replication, or build the colonist-ground fork without his yes; real-window checks (computer-use, MO57.exe only) are allowed while he sleeps.
- 2026-10-08 overnight approved list: replicate host clock + weather to clients; nettest actions (client pickup/drop/craft/build/harvest/attack); commit + push at the end approved.
- 2026-10-09 overnight: approved, commit + push approved (plan in OVERNIGHT_PLAN_2026-10-09.md).
- Bug report form: Wes chose "option A" (in-game form to the existing crash endpoint as a CR1 bundle, no website change).
- Oct 12 overnight asks (binding then): sound broken; mountains bad; rock planes not at terrain normal / not embedded; PCG must sample caves; Penumbra logo flicker before the video; beach sand ripples too tight; thin the grass, longer radius, near + far grass tiers; far trees (distance trees, LOD, clump proxies); 1:1 Electric Dreams biome; distinct Megascans biomes (nordic beach); bury rocks; UI input bugs (possession P, refocus); saplings missing collision. All addressed that night (see memory overnight-polish-oct12 / PCG.md).

#### OPEN FORK (new for DECISIONS)
- F19: ground for pawns that are NOT human-driven and far from every player (recruited colonists working away from you): no collision invoker -> they fall and get rescued. Options: a collision invoker only while a job is active (costs collision chunks per colonist), or hold / freeze them until ground exists. Deliberately not built (OVERNIGHT_LOG Oct 7, Oct 8 "Not building").

#### BACKLOG (open)
- Coop | OPEN | Claude | Bug report form: Open Discord button after the code move not re-clicked; contact field with a value | OVERNIGHT_LOG summary
- Building | OPEN | Claude | ServerPlaceBuilding: no collision / slope / rate limit (H19 residual) | OVERNIGHT_LOG summary
- Terraform | VERIFY | Claude | ServerApplyTerraform: server-side timer / tool requirement (rate limit added Oct 13, H17) | OVERNIGHT_LOG summary
- Coop | OPEN | Claude | No build/harvest MO.Test verbs runnable from a client (nettest actions covers build via _client_build now -- verify and close) | OVERNIGHT_LOG Oct 8
- UI | OPEN | Claude | In-game Load panel hides Host: no screenshot proof | OVERNIGHT_LOG Oct 8
- PCG | OPEN | Claude | Re-tune densities now that near tiers truly reach 180 m (Oct 12) -- superseded by the Oct 14 coverage pass? verify | Overnight_Polish
- Art | OPEN | Claude | Item icons with a baked checker background (Stick01, Stone01) need re-rendering with real alpha | Overnight_Polish
- UI | OPEN | Claude | Inventory: the design shows equipment / backpack / nearby / info at once -> C++ refactor of UMOUnifiedInventoryMenu (tabbed right panel today) | Overnight_Polish
- UI | OPEN | Claude | Not restyled (Oct 12): colony overview / bar, Knowledge tab content, medical detail pages (death recap done Oct 13) | Overnight_Polish
- PCG | OPEN | Claude | Electric Forest ground = shared MuddyLeaves (a darker mossy surface; ED forest-floor mesh patches not placed) | Overnight_Polish
- Art | OPEN | Wes | Megascans beyond the migrated ED slice need a Bridge / Fab download by Wes | Overnight_Polish

#### TRAPS
- (all already staged from memories: cook substrate pin, PCG radii names, instance locks .uasset, -ExecCmds ';' vs ',', ED atlases, editor busy after ED assets)
- [shell] `-ExecCmds="py x.py;Quit"` runs NOTHING after the ';' and Quit does not exit an editor-cmd -> `py x.py,QUIT_EDITOR`. (overnight-polish)
- [shell] ue.py bodies over ~30 KB overran the Windows command line (WinError 206) -> _curl sends the body on stdin (fixed). (overnight-polish)
- [shell] Background jobs: use run_in_background, not `&` (a stray probe launched with & kept running and teleported the shared instance). (overnight-polish, oct16)

---

### STAGE 23: BugReports_2026-10-14.md, BugReports_2026-10-16.md, AAA_Polish_Backlog.md (all to archive; AAA referenced by Config/DefaultEngine.ini comment)

#### BugReports: not-done rows (everything else DONE with cause + verification in the archived files)
- BR14-15 | CLOSED-not-reproduced | Giant chunk of mud on its edge: open-backed earth slab upright on gentle ground; the slope band (cliff slabs >= 25 deg, plates <= 28 deg) removed that placement.
- BR16-9 | CLOSED-not-reproduced | Joiner falls through the world far from the host / "not treated as possessed": not reproduced after the gait fix (#1); _joiner_long_run nettest step (9 km + 1 km sprint, no fall) guards it.
- BR16-16 | INVESTIGATING (instrumented) | Rock spire shows as a barren opening: not reproduced in a fresh world (same seed 1195513072); generation failure in Wes's session (PCG-stall family; first-launch PSO suspected); bug reports now carry WorldBuild.* to catch it.
- BR16-15 | IN PROGRESS | ED cliff / embankment assembly "wonky", floating trees: the assembly is a ~4 m rock block; terrain rises to the measured AuthoredGround (merged grids from 5 sites); Wes: iterate until it works every time.
- BR16 crash c8414aad | CLOSED | Editor RHI assert "Null Texture on UB Layout Material" (Oct 14): one-off, engine side, not reproduced.

#### AAA_Polish_Backlog open items -> BACKLOG (P-n ids)
- P-1 Swim polish: load-dependent buoyancy, fall damage into water, Space surfacing feel, swimming creatures, clothing distortion on horizontal poses. (= Water)
- P-2 Thermal model has no thermal mass -> F2.
- P-3 Floating objects / boats via Oceanology buoyancy.
- P-4 Real reed / rush / cattail meshes -> F5.
- P-5 Per-biome fauna and POI density review (SeedSurvey, walk each biome 3 min, log empty / repetitive stretches).
- P-6 Far terrain LOD tear on mountains (known "not ours") -- PARTLY SUPERSEDED: the 12 km tear was the cave volume stamp (fixed Oct 14); recheck what remains.
- P-7 Terrain material look (distant mountains; soften splits = DONE Oct 13; sand variety open) -> worldgen items.
- P-8 Weather per biome (rain / snow line at altitude) + sound layers.
- P-9 Audio pass: creature footsteps (no ball bones), structure / prop surfaces (stone, rock, metal need physical materials), ...
- P-10 Unbound optional widgets: WBP_UnifiedInventory (CloseButton, loot-nearby buttons, secondary stack-organise), WBP_ItemContextMenu (TransferButton, PickupButton), WBP_JournalPanel (AbandonButton, TrackButtonText), WBP_BuildingRecipeDetail (craft amount spin / slider / text, resource-source checkboxes), WBP_EquipmentPanel (swap progress bars), WBP_MOColonyPortrait (ClickButton, NameLabel, ActivityIndicator), WBP_HUDRoot (WindIndicator: needs an up-pointing arrow texture); bare native EntryButton in WBP_MOListEntry; ~15 legacy WBPs in /MOFramework/UI/ root that ui check cannot resolve. For each: wire it or delete the dead C++ member. (= G16)
- P-11 UI polish pass (spacing / focus / controller).
- P-12 [~] Moodle icons: generated glyph placeholders (Tools/gen_moodle_icons.py -> Content/UI/Icons/Moodles/T_Moodle_*); a hand-drawn set can replace the textures (same names).
- P-13 [~] UI design system: crafting list rows 12 px + 0.5-scaled button, white native amount spin box, "Characters" possession screen unstyled, FuelTimeText / "Current:" placeholders.
#### AAA forks (all mapped): survival pacing (ANSWERED Oct 15 pacing; single source = F1), thermal (F2), reed meshes (F5), hair engine patch (answered Oct 16; blocked on source engine / Epic: F11), perf vs fidelity target tier (F4).
#### AAA quality gates table -> Docs/Performance.md §1 (verbatim + updated numbers) and Docs/PROJECT_STATUS.md.

---

### STAGE 24: Docs/PROJECT_STATUS.md audit tracker (103 rows) -> BACKLOG § Audit
Source verdict (PROJECT_STATUS banner, Oct 14 2026): "No CRITICAL or HIGH remains open except the C7 class-whitelist residual and C8 LOS tuning (see July 8). Still open: M5 (authority idioms), M6 (two activity taxonomies), M7 (raw UButton in 5 list entries) -- hygiene, no player-visible defect." C7 was then made host-only (a47ef681, Oct 14). The full June 11 tracker text + every re-verification banner moves verbatim to Docs/Archive/PROJECT_STATUS_2026-10-15.md; BACKLOG rows point there.

#### Audit items carried into BACKLOG (codes unchanged: they are fix tags in code)
- C1 | IN PROGRESS | Claude | Monolithic module: phases 1-3 done (Core / Medical / MOFramework); phase 4 = UI module carve, after FMOCraftResult leaves the subsystem header.
- C8 | OPEN | Claude | ISM/HISM harvest RPC validation: LOS tuning residual (mirror ServerExecuteInteract's validation against InstanceTransform). 
- C7 | DONE-2026-10-14 | spawn-anything RPC: host-only, no whitelist (a47ef681).
- M5 | OPEN | Claude | Mixed authority idioms (GetOwnerRole vs HasAuthority) -- hygiene.
- M6 | OPEN | Claude | Two activity taxonomies (EMOActivityLevel vs EMOCreatureActivityState) -- hygiene.
- M7 | OPEN | Claude | Raw UButton in 5 list-entry widgets -- hygiene.
- M1, M2, M3, H6 | OPEN | Claude | Creature AI: hardcoded BB keys, magic numbers, IsCornered placeholder, FindPackMembers GetAllActorsOfClass (TR "AI Known Issues"; not re-verified since June).
- M12-M24, L8-L16 | VERIFY | Claude | Grouped June-11 findings, each a list of sub-items; NOT re-verified per sub-item since June 11. Known fixed since: M12 master volume twice (Oct 12), M18 AddItemByGuid overflow (Codex batch B, uncommitted), M13 OnCardiacArrest/RespiratoryFailure (DieOf systemic clocks Oct 13), M15 ghost Add Materials instant (server-driven construction Oct 13 -- timed? verify), M17 tautological UI tests (open), M21 medical treatment ids (Codex verified definitions Oct 17). Re-verify the rest against code before working them (the tracker has been systematically stale: ~14 of ~20 re-checked defects were already fixed in July).
- H1-H5 | VERIFY | Claude | June perf items (LoadSynchronous in AI tick, combat ticks every frame, sync UI icon loads, O(n) GUID lookup, BB updated every frame): status text still says June; GatherRuntimeAssets / preload work (Oct 13) addresses H1/H3 at least.
- All other C/H rows: DONE per the Jul 7, Jul 8, Oct 13 and Oct 14 re-verification banners (file:line in the archived tracker).

#### PROJECT_STATUS non-tracker content
- Codebase Metrics + Art Debt Baseline (A1, July) -> new slim PROJECT_STATUS.md (marked with dates).
- Master Plan Progress / Feature tracks (Foundation COMPLETE; UI Refactor Track superseded; Colony PARTIAL; Integration NOT STARTED; Creature AI MOSTLY COMPLETE; EQS C++ complete, BP pending; Crafting phases; PCG implemented) -> slim PROJECT_STATUS.md.
- "What's Excellent" + Recent History + Document Map -> slim PROJECT_STATUS (strengths kept: CommonUI architecture, authority discipline in inventory/identity/persistence, model implementations to clone: MOItemComponent::GiveToInteractorInventory, UMOTerraformingComponent timer, UMOSurvivorJobQueueComponent).
- UE5.7 Refactoring Priority -> TECHNICAL_REFERENCE (already has the roadmap).

---

### STAGE 25: remaining dated / plan docs (live-content scan; full text goes to Docs/Archive)
Files: Night_Report_2026-10-17.md, MO57_Fable5_Charter.md, Fable5_Autonomous_Pipeline.md, Fable5_PCG_Path.md, Fable5_Village_Handoff.md, MO57_Master_Plan.md, MobAIPlan.md, PCG_Integration_Plan.md, Terrain_Foundation_Plan.md, CommonUI_Migration_Plan.md, commonui_migration_audit.md, UI_Overhaul_Architecture.md, UI_Design_Brief_Prompt.md, devlog_2026-02-18.md, devlog_2026-03-18.md, PATCH_NOTES.md, Plugins/MOFramework/Content/Data/{StatusReport_2026-02-01.md, TechProgression.txt, TechProgressionPlan.md}, Content/Python/README_WIDGET_AUTOMATION.md, Ground_Coverage_2026-10-14.md, Progression_FreshLook_2026-10-15.md, Progression_Reachability_2026-10-14.md
KEPT LIVE (decided during P3, deviation from proposal): Agent_PIE_Testing.md (bridge playbook still the transport under ue.py; add a header), Content/Python/README_UI_TOOLS.md (API reference beside its scripts), Survival_Single_Source_Proposal.md + Thermal_Model_Reproduction.md (open fork briefs F1 / F2), AUTONOMOUS_TOOLING.md whole (no history split).

#### DECISIONS (earlier, still standing unless superseded)
- 2026-03-28 (Master Plan): UMOPersonalityComponent decoupled (Option A); colony overview replaces / extends the possession menu on P; placeholder portraits until the art pass.
- 2026-03-18 (devlog / UE5.7 audit): systems confirmed custom (persistence/identity GUID, medical sim (no GAS), building, FastArraySerializer, TSoftObjectPtr) -- still in TECHNICAL_REFERENCE.
- 2026-07-02/03 (Fable 5 charter): A-rated indie bar (Valheim / Satisfactory / Core Keeper / V Rising / Enshrouded tier: cohesion, feel, a loop that holds a co-op group 40+ h); the four gates (co-op correct, legible, fun in the loop, verified) -> VISION.md.
- 2026-07 (pipeline): Wes approved autonomous execution of the gated stage cards. W-track (springs, rain collector, water hauling) logged as needs-approval; W1 drinking built Oct 15 on his own ask.
- Night report Oct 17 calls: #1 inspect discovery (ANSWERED + BUILT), #2 dig time by rock (ANSWERED + BUILT), #3 heat-ladder data (OPEN -> F10), #4 night look (ANSWERED: leave it).

#### BACKLOG (open)
- Combat | OPEN | Claude | Game feel: hit-stop, hit-flash, camera shake, blood / impact VFX, weighty audio, directional hit reactions | Charter Pillar 4
- Combat | OPEN | Claude | Movesets + weapon variety (light / heavy / combo per class; reach / speed / damage identity; wind-up / recovery) | Charter Pillar 4
- Combat | OPEN | Claude | Defense timing (parry / block / dodge windows + stamina cost) | Charter Pillar 4
- Combat | OPEN | Claude | Enemy combat AI (predators that hunt / fight; hostile humanoids / factions) | Charter Pillar 4
- Combat | OPEN | Claude | Ranged (bow / atlatl / sling: aim, draw, projectile -> wound pipeline); arrows blocked by feathers | Charter Pillar 4, ROADMAP
- Combat | OPEN | Claude | Combat weapon subsystem not wired to equipment (#24) | dev_status July 7
- Combat | OPEN | Claude | Surface the wound sim (legible wound / treatment UI; "I broke his leg and he bled out") | Charter Pillar 4, MEDICAL_HANDOFF
- Release | OPEN | Wes | Steam patch notes: record the last Steam patch date/time + commit in Docs/STEAM_RELEASES.md each time Wes asks for patch notes (N9) | new
- Docs | NOTE | Claude | PATCH_NOTES.md frozen at 2026-06-30 (archived per N9); history since lives in git log + DECISIONS + night reports

#### TRAPS (from Agent_PIE_Testing / ue_pie_computer_testing memory; unique ones)
- [pie] Clicks on editor UI while PIE runs get captured by the viewport (CapturePermanently + LockOnCapture); keystrokes then leak into the game -> press Shift+F1 before clicking editor UI; stop PIE with Shift+F1 + the toolbar Stop (never Escape with editor focus). (ue_pie_computer_testing)
- [pie] Keys injected by computer-use do NOT reach Enhanced Input actions in PIE (human keyboards work) -> drive code paths in-process (MO.Test.PressKey / Input, agent_test_lib), never test keybinds with injected keys. (ue_pie_computer_testing)
- [pie] An UNCONSUMED simulated Escape ends PIE -> guard every injection on verified has_keyboard_focus(); push / open widgets in a separate bridge command (CommonUI defers construction a tick). (ue_pie_computer_testing)
- [computer-use] request_access needs "UnrealEditor.exe" (the exe name); never open_application for the editor (starts a second project-less instance / UE 5.7) or the packaged game (another instance); foreground via SetForegroundWindow. (ue_pie_computer_testing)
- [bridge] When parsing bridge probes from ue_out.txt, exclude the [py-ok]/[py-err] echo lines (they contain your marker). (ue_pie_computer_testing)
- [bridge] Bridge scripts are gitignored dev-only files in the MAIN tree's Content/Python (not in git worktrees). (ue_pie_computer_testing)
- [bridge] A python exception inside `ue.py py --file` shows as "bridge not responding" -> small steps with unreal.log tags, or try/except. (animation-retarget)
- [python] UE 5.7+ InputMappingContext.Mappings reads empty -> get_editor_property("default_key_mappings").mappings. (ue_pie_computer_testing)
- [pie] showdebug is not available (no AHUD class; UMG-only HUD). (ue_pie_computer_testing)

---

### STAGE 26: cross-cutting traps from memory (git, shell, harness, MCP, data, editor, assets)
Sources: dev_status (LIVE GOTCHAS), multi-agent-coop, git-staging-verification, ue-unified-cli, ue-editor-mcp, animation-retarget-pipeline, fab-sourcing-and-icon-studio, miktex-package-installs, cs399-practicum (tooling), build_engine_path, engine-installed-build-patches, user_preferences, CLAUDE.md UE 5.8 notes + code conventions, global CLAUDE.md machine section.

#### Git and Windows
- [git] `git add` fails SILENTLY: the worktree is case-insensitive but git's index is case-sensitive (MOItemDataBaseSettings.cpp with a lowercase b matched nothing); one missing pathspec in `git add a b <typo>` aborts the whole add and stages NOTHING -> before EVERY commit run `git status --short` and check the staged column; never pipe git add stderr to /dev/null; copy paths from git status; after a push `git show HEAD --stat` and count. (git-staging-verification, dev_status, playtest-oct14)
- [git] Local builds pass while the remote doesn't compile -> files exist on disk but were never staged -> the staged-column check above. (git-staging-verification)
- [git] A "2 insertions" stat on a .uasset is the git-lfs POINTER changing (oid + size), not a red flag -> `git lfs status`. (dev_status)
- [git] The editor's source-control integration auto-stages (`git add`) every asset it saves, temp ones included, and rewrites Content/Developers/penum/MegaMaterialCache.uasset -> check git status before committing; `git rm --cached` strays; never stage MegaMaterialCache or .claude/settings.local.json. (animation-retarget, terrain-gen-exploration)
- [git] Shared tree with Codex -> never `git add -A` (it commits Codex's uncommitted WIP); stage your exact paths. (multi-agent-coop)
- [shell] Git Bash rewrites `/MOFramework/...` and `/Game/...` to `C:/Program Files/Git/...` -> `MSYS_NO_PATHCONV=1` (ue.py repairs its own args with _unmsys). (many)
- [shell] PowerShell 5.1 mangles embedded quotes in native args -> drive ue.py from bash. (ue-unified-cli)
- [shell] Never round-trip file content through PowerShell Get-Content / Set-Content (UTF-8 mojibake); reading a log tail is fine; use Write/Edit, Git Bash or sed. (global CLAUDE.md)
- [shell] Windows Store python can't read AppData\Roaming paths (copy scripts out first) and resolves /tmp/x to C:\tmp (Git Bash /tmp is AppData\Local\Temp) -> explicit Windows paths in Python. (cs399)
- [shell] bash/heredoc/sed collapse backslash escapes; heredocs with apostrophes or `'''` break; python heredocs with `\U...` Windows paths crash -> write text and scripts with the Write tool, then run them. (cs399, swimming, Handoff, UI_Kit; confirmed again 2026-10-17)

#### Engine and build (this machine)
- [engine] D:\UnrealEngine\UE_5.8 is the LAUNCHER install 5.8.3 (Engine/Build/InstalledBuild.txt; no Setup.bat): engine .cpp edits never compile -> stage engine fixes in Docs/Engine_Patches.md + Docs/EnginePatches/ with a diff; check InstalledBuild.txt before proposing any engine edit. (engine-installed-build-patches)
- [build] Build fails in ~5 s with `CS0117 ... BuildSettingsVersion ... V7` / RulesError -> the UE_5.7 UBT was used -> `D:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe MO57Editor Win64 Development -Project=D:\UEProjects\MO57\MO57.uproject` (or `ue.py build` / `cycle`). Target names are case-sensitive: MO57Editor, MO57. (build_engine_path)
- [build] "Unable to build while Live Coding is active" / DLL locked -> the editor (or a CrashReportClientEditor window: LNK1104) holds the binaries -> close the editor (ue.py editor stop, graceful); check DLL mtime: Live Coding sometimes hot-patched instead. (user_preferences, Handoff, overnight-polish)
- [build] A one-off build / shader-compile failure may be the downclocked 14900K: retry once. (user_preferences, global)
- [build] Moved DT-row / enum / class types resolve None after an editor restart (rowless tables, collapsed gates) -> module carves REQUIRE CoreRedirects (class redirect paths drop the U prefix); add redirects BEFORE the commit, restart, run a table-touching gate. (dev_status)
- [cpp] 5.8 `ACharacter::SetBase` takes `FMovementBaseInterfaceData*` -> override the NEW signature (version-gated) or the engine never calls it (MOCharacter.h). (CLAUDE 5.8 notes)
- [cpp] Delegates in a header not found by other headers -> a header declaring DECLARE_DYNAMIC_MULTICAST_DELEGATE at file scope needs at least one USTRUCT/UCLASS/UENUM for UHT. (CLAUDE conventions)
- [cpp] Template method compile errors -> a type used in a header template needs the full include, not a forward declaration. (CLAUDE conventions)
- [cpp] Duplicate delegate bindings -> RemoveAll(this) / RemoveDynamic before binding in NativeConstruct. (CLAUDE conventions)
- [cpp] Next deprecation before any 5.9 move: FCoreDelegates::OnPostEngineInit -> GetOnPostEngineInit(). IncludeOrderVersion is still Unreal5_7 in Target.cs (harmless nag). (CLAUDE, build_engine_path)

#### Editor, Python bridge, MCP
- [python] `str()` on a wrapped unreal struct is the wrapper ADDRESS -> GUID / key sets must use to_string() / export_text(), or matching silently returns nobody and asserts pass vacuously. (dev_status)
- [python] Python strips the enum E-prefix and bool b-prefix (`unreal.RelationshipType`, `enable_environmental_temperature`); `rc.is_recruitable` is a property (the b-UPROPERTY shadows the method); a bool UFUNCTION with an out param returns None when false; Python has no `actor.is_hidden()` -> get_editor_property('hidden'). (dev_status, terrain-v5, oct16)
- [python] str(enum) contains spaces (`<MOBuildState.GHOST: 0>`) -> regex the name out. (oct16, ue-multiprocess)
- [python] `inst do NAME py` returns only out(...), never print; the Python name is unreal.MaterialLibrary (not KismetMaterialLibrary); server RPCs are not exposed to Python (call the server-side function). (oct16)
- [python] `set_control_rotation` swings the spring-arm camera: a single look-at misses by 86-378 cm -> converge (~5 ticks, re-aim from the new camera); compute look-at from player_camera_manager.get_camera_location(). (oct16, first-ten-minutes)
- [python] Items ROLL on slopes (bark 3-13 m) -> stand the pawn near where it rests; `add_item_by_guid_no_stack` caps at the stack size. (oct16)
- [python] Editor Python env vars do NOT reach scripts run through `ue.py py`. (pcg-load)
- [editor] A modal editor dialog opened from the bridge (reload_packages on a dirty asset) blocks the call; if Windows Security (pickerhost) blocks computer-use clicks, post Enter to the dialog's HWND (EnumWindows by editor PID, title "Message", PostMessage WM_KEYDOWN VK_RETURN). (dev_status)
- [editor] Never delete or rename assets in editor Python (EditorAssetLibrary.delete_directory / delete_asset: ForceDeleteObjects ensure -> CrashReportClientEditor, bridge dead) -> delete files on disk with the editor CLOSED. (animation-retarget, swimming)
- [editor] PIE-wedge flake: back-to-back seqs sometimes can't begin_play -> editor restart clears it (not root-caused); after a code change run `ue.py cycle`; between gates `editor stop` / `start`. (dev_status)
- [editor] The editor's first launch after a rebuild is slow: `cycle --boot` may say "bridge not responding" -> `ue.py editor wait` then `ue.py boot`. (ue-ui-toolset)
- [editor] A hand-launched editor has no MCP until `ue.py status` (auto StartServer through the bridge). The MCP disconnects from the harness when the editor closes -> drive it with curl (initialize -> Mcp-Session-Id -> notifications/initialized -> tools/call; tools/call answers SSE; returnValue double-nested; Python urllib gets an EMPTY body for tools/call: shell out to curl). (ue-editor-mcp)
- [dt] MCP `add_rows` is ALL-OR-NOTHING (one existing name rejects the batch) and a large multi-row `set_rows` fails SILENTLY -> one row at a time (add then set) + readback; `set_rows` REPLACES a whole nested struct (omitted fields reset) -> `rows_set_safe` deep-merges; never call raw MCP set_rows with a partial struct; after MCP edits invalidate the static caches (`ue.py refresh-data`). (ue-editor-mcp, CLAUDE)
- [dt] An MCP call re-sent after an EMPTY first response may already have applied ("Rows do not exist" on the retry) -> read back. (AUTONOMOUS_TOOLING)
- [dt] Writing shared DataTable rows from a stale source (Items_JSON.json) overwrote Codex's in-editor-only lore (M21, unrecoverable) -> `ue.py rows get` the LIVE table first; post the rows in the coop before writing shared data. (multi-agent-coop)
- [harness] Sandboxed `ue.py status` can falsely report `editor: DOWN` for the protected UE process -> the coop log is the source of truth for who holds the editor. (multi-agent-coop)
- [harness] Background agents resolve into a git worktree even when given main-tree paths -> verify which tree got the changes before building. (ue-unified-cli)

#### Assets and content
- [anim] Root-motion source clips fold the travel into the target PELVIS when retargeted -> zero the root translation on duplicates (set_bone_track_keys inside open_bracket / close_bracket), retarget the copies, enable_root_motion=False; get_raw_track_data returns EMPTY in 5.8 (sample get_bone_pose_for_frame); mount engine plugin content with unreal.MOEditorContentUtils.mount_content_path. (animation-retarget)
- [fab] Fab panel imports pop a maximized Content Browser that eats keystrokes (check the front window first; icon imports suppress it via Interchange.FeatureFlags.Import.SyncToBrowser off); right-column cards ignore clicks; the panel re-imports assets already in the project (same folder id) -> check before wiring. (fab-sourcing)
- [fab] A tint can't make metal (metallic lives in the ORM texture); Megaplants are procedural presets, not item meshes; foliage master tint param "Base Color Tint" has no Brightness. (fab-sourcing)
- [fab] Before item_mesh_prepare adds a hull to a plant mesh, check it isn't world foliage (a hull on world grass blocks the player). (fab-sourcing)
- [icons] Icon studio crashed the editor on HDR-source textures (RGBA16F) with an explicit exporter -> let UE pick by extension (png -> exr fallback). (geology)
- [latex] MiKTeX opens a GUI package-install dialog on Wes's screen -> check kpsewhich first or announce it; emit_latex.py uses -disable-installer; needspace / tocloft are not installed. (miktex)
- [docs] PyMuPDF reflows .docx (useless for pagination); pdftotext works; the Read tool's PDF mode needs poppler (absent). (cs399)
