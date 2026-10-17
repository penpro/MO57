# MO57 Project Status

> **What this is:** where each part of the game stands, in one page, plus the strengths to preserve. Rebuilt 2026-10-17. Open work is in
> `Docs/BACKLOG.md` (the audit tracker's open rows moved there with their C/H/M/L codes); Wes's calls and open forks are in
> `Docs/DECISIONS.md`; every live doc is listed in `Docs/INDEX.md`. The full June 11 2026 re-audit tracker, the metrics tables and every
> re-verification banner (July 7, July 8, Oct 13, Oct 14) are preserved verbatim in `Docs/Archive/PROJECT_STATUS_2026-10-15.md`.

## Numbers (2026-10-17)

| Measure | Value | Source |
|---|---|---|
| Plugin C++ files (committed) | 755 (.h + .cpp), ~223k lines | `git ls-files Plugins/MOFramework/Source` |
| Runtime modules | MOFrameworkCore <- MOFrameworkMedical <- MOFramework (+ MOFrameworkEditor) | `Docs/TECHNICAL_REFERENCE.md` |
| Code graph | ~16.7k nodes / 25.6k edges | `python -m graphify` (Oct 17 refresh) |
| Automation | 264/264 passing on Oct 17 across every group except Codex's in-flight Crafting / Inventory / Equipment groups (Codex's own gates: Medical 90/90, Crafting 15/16 with one diagnosed fixture failure) | Night report, CX-0064 |
| Recipes reachable from bare hands | 126 of 136 (the 10 blocked lack fauna / flora) | `MO.Test.ProgressionMap`, `Docs/PROGRESSION_ROADMAP.md` |
| Items with real mesh + icon | 135 of 216 (Oct 14); 81 on gray placeholders | `Docs/Item_Art_Pipeline.md` |
| Frame time (packaged Dev, 1080p, 4090) | ~77-100 fps per biome, p95 11-14 ms (Oct 14) | `Docs/Performance.md` |
| New-game READY (packaged) | ~17 s (7.1 s before the Oct 14 coverage pass) | `Docs/PCG.md` §8 |
| World generator | v11 | `MOWorldGen.h` |
| Engine | UE 5.8.3 launcher install | DECISIONS N4 |

## Tracks

| Track | State (2026-10-17) | Doc |
|---|---|---|
| World generation (terrain, biomes, coasts, rivers, lakes) | Built, v11; open: sand variety, snow from climate, far-mountain look (F21) | `Docs/Terrain_Generation_Exploration.md` |
| Caves and geology | Built (karst galleries, water gate, ore bodies, dig yields + hardness) | `Docs/Caves_And_Geology.md` |
| PCG vegetation + world features | Built (biome tiers, removals ledger, readiness, POI compositions); cliff walls = F9 | `Docs/PCG.md`, `Docs/World_Features_Architecture.md` |
| Water | Oceanology ocean + upland lakes, swimming, drowning, drinking + illness; floating objects open | `Docs/Water.md`, `Docs/Water_And_Illness.md` |
| Light | Carried torch, flames, eye adaptation, dark caves | `Docs/Light_Sources.md` |
| Weather and time | UDS / UDW + MO bridge + director; co-op clock sync | `Docs/Weather_And_Time.md` |
| Medical and survival | Cascade, DieOf + systemic clocks, respiration, illness courses; treatment service (Codex, uncommitted); F1 / F2 open | `Docs/Medical_And_Survival.md` |
| Crafting and progression | Stations, queue, tool gate, liquids, inspect discovery; heat ladder values = F10 | `Docs/Crafting_And_Progression.md`, `Docs/PROGRESSION_ROADMAP.md` |
| Building and terraform | Server-driven construction; excavation stages 1-5 done, stage 6 UI = F8 | `Docs/Building.md`, `Docs/Terraform_Excavation_Plan.md` |
| AI and colony | Deer + wolf on BTs; survivor C++ job machine; July village slice (quotas, schools, relationships, seasons, children, winter, fire); colony UI partial | `Docs/AI_And_Colony.md` |
| Combat | Sweep hits -> body parts -> real wounds; melee aim assist; feel / variety / enemies / ranged open | `Docs/BACKLOG.md` N-098 |
| Co-op | Steam + LAN sessions, host a save, per-machine terrain from a replicated seed, server-driven actions; verified with two real accounts (Oct 8) | `Docs/Coop_And_Networking.md` |
| UI | CommonUI layers, Menu input mode, UI kit live on most menus (stand-in fonts); consolidation stage 4 next (Codex) | `Docs/UI.md` |
| Persistence | Save domains registry, GUID identity, bug / crash reports with LookAt rows | `Docs/Persistence.md` |
| Audio | Heartbeat meter, NaN guard, XAudio2, footsteps by biome | `Docs/Audio.md` |
| Packaging | Development + Shipping packages; ShipDiag; cook traps known | `Docs/Packaging.md` |

Older plan tracks (status as last recorded, kept for orientation): Foundation stages 0-3A COMPLETE; the March "UI Refactor Track" (4A-7A) was
superseded by the CommonUI work and Codex's consolidation plan; Colony track stages 3B / 5B classes exist, 4B / 6B / 7B missing; Creature AI
mostly complete (ABP state machines + EQS assets need Blueprint setup); the March crafting "phase 3: stations" label is stale (stations exist).

## What's excellent (preserve these; re-confirmed June 11 unless noted)

- **CommonUI architecture**: layer stacks, `GetDesiredInputConfig()` throughout (Menu mode), stub root content, subsystem widget registry.
- **Authority discipline in inventory / identity / persistence** (HasAuthority on nearly every mutator, COND_OwnerOnly where right). Combat,
  terraform, building and crafting were brought under authority in July-October; don't cite "multiplayer-ready" without scope.
- **Model implementations to clone:** `MOItemComponent::GiveToInteractorInventory` (canonical authoritative pickup), `UMOTerraformingComponent`'s
  component-owned interruptible timer, `UMOSurvivorJobQueueComponent` (guards every mutator), `UMOTimedActionComponent` (server clock, replicated,
  interruptible), the registry pattern (`Docs/TECHNICAL_REFERENCE.md` § Module structure).
- **Data-driven architecture**: DataTables + UDeveloperSettings; content integrity is checked by `MO.Test.ValidateData` (Data:Sources).
- **Interface decoupling** and the Core-contract-and-registration seam; no upward includes across the three runtime modules.
- **Medical system architecture**: clean cascade, the best test coverage.
- **Systemic patterns to check new code against** (June audit): live-path side effects must be replayed by restore paths; state must be owned
  by the layer that owns its lifecycle (not a widget); no orphaned wiring (both ends built, nobody owns the coupling); convention by structure,
  not by memory (move obligations into base classes).

## Recent milestones (October; details in git log and `Docs/DECISIONS.md`)

| Date | Milestone |
|---|---|
| Oct 17 | Inspect discovery (126/136 reachable), dig time by rock, karst caves + geology yields, torch + eye adaptation + dark caves, knowledge consolidation |
| Oct 16 | 22-report playtest batch fixed (frozen lakes, host-only boxes, world actions, eating timer, interaction targeting, audio NaN guard) |
| Oct 15 | Drinking / boiling / friction fire, progression passes 1-4, first-ten-minutes pass 1, survival pacing, character creator |
| Oct 14 | 29-report playtest batch, world features / POIs, coverage pass, dev mode in Shipping, LookAt bug-report rows |
| Oct 13 | Readiness 7 s, removals ledger, DieOf, server-driven construction, soft biome boundaries, far trees |
| Oct 11-12 | Oceanology, swimming, biome-aware PCG, UI kit, generator v5-v6 |
| Oct 5-9 | Steam sessions, host a save, multi-process test harness, bug-report form, co-op spawn fixes |
| Earlier | `Docs/Archive/PROJECT_STATUS_2026-10-15.md` "Recent Development History" (Feb-July) |
