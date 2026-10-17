# Archive index

Superseded and dated documents, kept for history (`git log --follow` shows each file's life before the move). One line each: date of the
content, what it was, where its live content went. The extraction record per item is `CONSOLIDATION_LEDGER.md`.

| File | Date | What it was | Live content now in |
|---|---|---|---|
| `INVENTORY.md` | 2026-10-17 | P1 inventory of every knowledge file (sizes, types, staleness) | (record of the consolidation) |
| `CONSOLIDATION_PROPOSAL.md` | 2026-10-17 | P2 proposed structure + contradictions list | `DECISIONS.md` (N1-N9 answered, F-forks) |
| `CONSOLIDATION_LEDGER.md` | 2026-10-17 | source -> destination for every extracted item | (record) |
| `BACKLOG_DONE.md` | rolling | DONE backlog rows older than a week | -- |
| `coop/2026-07.md` | 2026-07-12..20 | Claude-Codex coordination log, July entries (verbatim) | live log `claude-codex-coop.md` |
| `PROJECT_STATUS_2026-10-15.md` | 2026-06-11 .. 10-15 | The June 11 re-audit tracker (C/H/M/L), metrics, feature tracks, re-verification banners, history Feb-July | `BACKLOG.md` § Audit, `PROJECT_STATUS.md` |
| `AAA_Polish_Backlog.md` | 2026-10-11..16 | "Complete polished AAA" backlog: quality gates, P0-P3 items, forks | `BACKLOG.md` (P-n aliases), `Performance.md` §1, `DECISIONS.md` |
| `BugReports_2026-10-14.md` | 2026-10-14 | Tracker for 29 playtest reports + crash uploads (all done / not reproduced) | `BACKLOG.md` (BR14-n), `DECISIONS.md` |
| `BugReports_2026-10-16.md` | 2026-10-16 | Tracker for 22 Steam co-op playtest reports + 6 crashes; Wes's Oct 16 calls | `BACKLOG.md` (BR16-n), `DECISIONS.md` 2026-10-16 |
| `Claude_Handoff_Goals_2026-10-13.md` | 2026-10-13 | Codex's work plan for Claude (H00-H26) | `BACKLOG.md` (G00-G26 aliases; status table in the ledger) |
| `Night_Report_2026-10-17.md` | 2026-10-17 | Night report: torch, geology, Wes's four calls | `DECISIONS.md` 2026-10-17, `Light_Sources.md`, `BACKLOG.md` |
| `OVERNIGHT_LOG.md` | 2026-10-07..09 | Overnight runs: co-op seed / pawns / host-a-save, bug-report form, host-pawn fixes | `Coop_And_Networking.md`, `Persistence.md`, `DECISIONS.md` (F19, standing rules), `BACKLOG.md` |
| `OVERNIGHT_PLAN_2026-10-09.md` | 2026-10-08 | Approved overnight plan + standing rules | `DECISIONS.md` "Earlier" (standing rules) |
| `Overnight_Polish_2026-10-12.md` | 2026-10-12 | Overnight polish log (PCG tiers, cook blocker, UI restyle, sound) | `PCG.md`, `Packaging.md`, `UI.md`, `Audio.md`, `TRAPS.md` |
| `Ground_Coverage_2026-10-14.md` | 2026-10-14 | Ground-coverage pass: tier caps, clump bug, plan, measurements | `PCG.md` §3-§5, `Performance.md` (its `.plan.json` stays in `Docs/`) |
| `Progression_FreshLook_2026-10-15.md` | 2026-10-15..16 | Progression from bare hands + passes 1-4 | `PROGRESSION_ROADMAP.md`, `Crafting_And_Progression.md` |
| `Progression_Reachability_2026-10-14.md` | 2026-10-14..17 | Data:Sources reachability, circular discovery, the inspect-precursor table | `PROGRESSION_ROADMAP.md` §6, `Crafting_And_Progression.md` §5-§6 |
| `TechProgressionPlan.md` | 2026-02-12 | February 9-era tech progression plan (~1,000 h intent) | `PROGRESSION_ROADMAP.md` (F16 asks whether the time budget stands) |
| `TechProgression.txt` | 2026-02-01 | Progression listing generated from the February CSVs | superseded by the live DataTables + `MO.Test.ProgressionMap` |
| `StatusReport_2026-02-01.md` | 2026-02-01 | February tech-debt report | superseded by the June audit (`PROJECT_STATUS_2026-10-15.md`) |
| `MO57_Fable5_Charter.md` | 2026-07-02 | The "Framework to A-Rated Indie" charter (pillars, four gates) | `VISION.md` (A-rated bar, four gates), `BACKLOG.md` (combat N-098) |
| `Fable5_Autonomous_Pipeline.md` | 2026-07-04..10-14 | Gated stage cards (S / A / V / P / F / T / B / R / W) | `AI_And_Colony.md`, `BACKLOG.md` (N-090..N-100), `DECISIONS.md` F22 |
| `Fable5_PCG_Path.md` | 2026-07-04 | The July PCG goal doc | `PCG.md` |
| `Fable5_Village_Handoff.md` | 2026-07-04 | The July village plan | `AI_And_Colony.md` §2 |
| `MO57_Master_Plan.md` | 2026-03-28..31 | March master plan (UI track, colony track, progress log) | `AI_And_Colony.md` §4, `DECISIONS.md` "Earlier" |
| `MobAIPlan.md` | 2026-02-18 | Creature AI plan (mostly built) | `AI_And_Colony.md` §1 |
| `PCG_Integration_Plan.md` | 2026-02 .. 10-12 | PCG world-items plan (never built that way) + "how PCG is hosted" | `PCG.md` §1 |
| `Terrain_Foundation_Plan.md` | 2026-10-09 | VHG_Realistic voxel-graph recipes (superseded by the C++ generator) | `Terrain_Generation_Exploration.md` |
| `CommonUI_Migration_Plan.md` | 2026-03-31 | CommonUI migration plan v1.1 | `UI.md` |
| `commonui_migration_audit.md` | 2026-03-31 | Audit of that plan | `UI.md` |
| `UI_Overhaul_Architecture.md` | 2026-03-31 | UI overhaul architecture + the 15 pitfalls | `UI.md` §5 (pitfalls verbatim) |
| `UI_Design_Brief_Prompt.md` | 2026-10-11 | The prompt that produced the UI kit | `UI.md` §6, `UI_Kit_Implementation.md` |
| `README_WIDGET_AUTOMATION.md` | 2026-03-29 / 10-05 | Pre-`ue.py ui` widget automation notes (self-declared superseded) | `UI_TOOLING.md` |
| `PATCH_NOTES.md` | 2026-02 .. 06-30 | Changelog, frozen at the 5.8 upgrade | git log; `STEAM_RELEASES.md` (Wes N9) |
| `devlog_2026-02-18.md`, `devlog_2026-03-18.md` | Feb / Mar 2026 | Development logs | -- |
| `MO57_ClaudeCode_Plan.docx` | 2026-03-31 | March planning document (Word) | -- |
| `UE57_Refactoring_Plan.md`, `UIManagerSplit_TestingNotes.md`, `UI_Architecture_Audit.md`, `UI_Class_Structure_Report.md`, `consolidation-session-checklist.md` | March 2026 | Superseded in March (into the Master Plan) | -- |
