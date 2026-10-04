# Project Plan

**Project Title:** Metaverse: Origins (repository: MO57)
**Student Name:** Wesley Weaver
**Course:** CS399 Software Development Practicum
**Quarter & Year:** Fall 2026
**Instructor:** named in the submitted Word version (kept out of this public repository)
**Plan version:** 1.1, 2026-10-04 (Week 1)
**Companion documents:** `CS399_HISTORICAL_CLOSURE.md`, `CS399_WEEKLY_PROGRESS.md`, `CS399_FINAL_REPORT.md`

> **Submission versions.** Two documents are kept outside the repository (they name the instructor): a short Word summary built from the instructor's template and rubric (`Metaverse_Origins_Project_Plan_Fall2026.docx`), and the **full plan** as a PDF of about 93 pages (`Metaverse_Origins_Project_Plan_Fall2026_Full.pdf`), which follows the same ten template sections and adds the review of previous work, the tool and AI-use sections, the risk register, test specifications and appendices. **This file is the compact working plan** behind both. Where they differ, the full plan is newer: it adds the H19 placement residual to AC-3 and Week 4, a C++ benchmark runner (`MO.Bench.Run`) to the performance protocol, and the untracked-Voxel-plugin note to the dependencies.

> **Calendar.** Confirmed against Olympic College's 2026-2027 academic calendar: Fall classes begin Mon 2026-09-21, Thanksgiving holiday is Nov 26-27 (Nov 25 is a non-student day), finals and/or instruction run Dec 8-11, and the term ends Dec 11. The plan keeps the Fall 2025 plan's week-numbering convention: **Week 0 = Mon 2026-09-21, Week 1 = Sep 28-Oct 4, ..., Week 10 = Nov 30-Dec 6.** Thanksgiving falls in Week 9.

## 0. Summary

**Recommended deliverable for this quarter: an integrated two-player cooperative vertical slice.** Two players start a shared persistent world entirely from the main menu (Host / Join), play the core survival loop together (harvest, craft, build, terraform, save/load), with server authority verified by automated two-client tests, a measured performance baseline, and a reproducible packaged Windows build, released as the first tagged version, **`v0.1.0`** (the tag promised in Fall 2025 and never created).

**Why this one.** Individual systems already exist in depth. What is missing is the integration that turns them into something two people can play together, and it touches all five workstreams at once:

| Candidate | Integration breadth | Measurability | Risk | Verdict |
|---|---|---|---|---|
| **Co-op vertical slice** | Possession, inventory, persistence, harvest, build, terrain, UI, network authority, packaging, performance | High: automated two-process and two-client tests, benchmark numbers | Medium: Steam init is blocked externally, mitigated by a LAN-first path | **Recommended** |
| Terraform excavation stages 4-7 (colony auto-dispatch) | Narrow: one deep simulation feature | Medium | Low | Stretch only; it deepens, but does not integrate |
| Erosion / diffusion / sparse Poisson solver | Narrow; none of it is in the repo today | Low until integrated | High: research-flavored, unclear game benefit | Stretch behind a Week 5 decision gate |
| UI stage 4 (skills, knowledge, quests) | Moderate; polish work | Medium | Low | Out of scope |
| Earth-scale planet | Very large | Low | Very high | Out of scope (carried forward) |

### Baseline snapshot (measured 2026-10-04)

| Area | Baseline |
|---|---|
| Engine / plugins | UE **5.8** (`MO57.uproject`); Voxel Plugin open-source dev-phy build, vendored, behind the `MOVoxel` facade |
| Code | 565 C++ files, about **172,300 LOC**: `MOFramework` 500 files / 153.6K, `MOFrameworkCore` 36 / 8.0K, `MOFrameworkMedical` 17 / 8.7K, `MOFrameworkEditor` 6 / 2.0K, `MO57` 6 / 0.2K |
| Automated tests | **127** headless (Medical 79, Colony 24, UI 10, Survival 5, Skills 3, Knowledge 3, Integration 1, Terraform 1, Clock 1); **26** editor Python gate scripts; **15** offline tooling tests; live UI aggregate 84, WBP 15, cold-Escape 7 (2026-09-22 run) |
| Coverage gap | No headless test is named for or targets inventory, persistence, networking, harvesting, building, or possession; those rely on editor Python gates only |
| Multiplayer | 2-client PIE harness 8/8 (2026-07-06). Session subsystem (Host / Find / Join) built 2026-09-22 and verified over LAN via console verbs; **uncommitted**, no UI yet, Steam init (`SteamAPI_Init`) failing for App 1602810. Open authority items (tracker codes): **H17**, the terraform RPC has no rate or duration limit; **C7**, the possession RPC accepts a client-supplied pawn class |
| Persistence | Works (save-domain registry); **no save-format version field** |
| Performance | **Never measured.** No benchmark harness, no recorded baseline |
| Packaging | Manual build only; `Packaged/Windows/MO57.exe` 2026-09-20 (3.0 GB); no packaging script |
| Art | 757 of 808 art slots unset (2026-07-04 baseline) |
| Repository | 261 commits, 245 co-authored with Claude models, no tags; **73 uncommitted paths** (25 source/config/doc, 48 content assets) at the time of measurement. They were landed in eight commits on 2026-10-04 after the gate (build, 127 / 127, 15 / 15); see the weekly record |
| Trackers | `Docs/PROJECT_STATUS.md` is the stale 2026-06-11 audit; re-verification is a Week 2 task |

---

## 1. Project Overview

**Description.** Metaverse: Origins is an open-world survival and civilization-building game, first published on Steam as a UE4 early-access title (2021-05-17). It is being rebuilt in Unreal Engine 5 as a maintainable C++ framework (`MOFramework`) with mutable voxel terrain, a detailed medical and survival simulation, crafting, colony and village AI, and persistence. The design pillars are realism, emergent settlements, total world mutability, and small-group cooperative play. This quarter's work converts the accumulated systems into one coherent, demonstrable, measured game slice rather than adding new isolated features.

**Continuity statement.** **(c) one phase of a larger, multi-quarter project, and (b) a continuation.** Fall 2025 established the redevelopment objectives, and Winter and Spring 2026 continued development without a renewed formal plan. Historical objectives are closed, carried forward, or superseded in `CS399_HISTORICAL_CLOSURE.md`. Only work done from Fall 2026 forward is claimed as Fall 2026 progress.

## 2. Scope

### Project scope (this quarter)

**In scope**

- Player-facing co-op session flow: main-menu Host / Join UI, session creation, discovery, joining, travel (LAN first).
- Server-authority hardening for core-loop actions (harvest, craft, build, terraform, possession) and the two known open items (H17, C7).
- Persistence: explicit save-format versioning, migration test, and co-op save/load verification.
- Terrain under co-op: seed determinism across host and client, terraform edit replication.
- Verification: automated two-process and two-client tests; extension of the headless suite into the uncovered areas.
- Performance: a defined benchmark scene, a measurement protocol, a recorded baseline, and fixes for the worst hitches.
- Packaging: one scripted command that produces the packaged build, plus a packaged smoke test.
- Documentation: systems map, reconciled defect register, weekly progress record, final report, demonstration video.

**Out of scope (explicit)**

- Earth-scale / spherical planet; large-world streaming.
- Redesign of the survivor-as-egg concept (decision recorded only).
- Public Steam release or pushing builds to a Steam depot (a private beta branch is a stretch item).
- Dedicated-server architecture.
- Large architectural rewrites (module split phase 4, enum-to-tag migration, EQS / Smart Objects refactors) and any engine upgrade past 5.8.
- New biomes, art passes, or content expansion.

### This quarter's goals

| # | Goal |
|---|---|
| G1 | A player can Host and a second player can Find and Join from the main menu of a packaged build, over LAN, with no console commands. |
| G2 | Core-loop actions initiated by a client are validated by the server; H17, C7 and the H19 placement residual are closed with negative tests. |
| G3 | The save format carries a version and migrates; a two-player session survives save and load without item loss or duplication. |
| G4 | Host and client see the same terrain from the same seed, and a host terraform edit appears on the client. |
| G5 | Performance is measured with a documented protocol, reported honestly against the 60 FPS target, and the top hitches are addressed. |
| G6 | The packaged build is reproducible from one command and smoke-tested. |
| G7 | Every claim in the final report is backed by a commit, test, or artifact. |

### Deliverables

| # | Deliverable | Form |
|---|---|---|
| D1 | `UMOSessionSubsystem` plus Multiplayer panel (Host / Join tabs, session list) wired into the main menu | Committed code and widget assets |
| D2 | Scripted two-process LAN session test (host process, join process, exit-coded) | `ue.py` command and script |
| D3 | Extended 2-client authority suite (client harvest, craft, build, terraform, possession) | Automated tests, count tracked weekly |
| D4 | H17, C7 and H19-residual fixes with negative tests (rejected request is observed) | Code and tests |
| D5 | Save-format versioning with migration and co-op round-trip tests | Code and headless tests |
| D6 | Terrain determinism and terraform replication test; fixed benchmark scene definition | Test and scene asset |
| D7 | Benchmark harness and report (Appendix C filled weekly from Week 4) | Script, CSV captures, report |
| D8 | Scripted packaging command and packaged smoke test | `ue.py package` (or equivalent) and test |
| D9 | Systems map and reconciled defect register (replaces the stale tracker) | Docs |
| D10 | Final report, 3-5 minute demonstration video, tag `v0.1.0` | Docs, video, git tag |

### Stretch goals (only if M2 is green by the end of Week 5)

1. **Real Steam discovery.** Requires fixing `SteamAPI_Init` for App 1602810 (likely a Steamworks partner-dashboard configuration step). Time-boxed to one session; LAN remains the guaranteed path.
2. Gamepad focus verification test (the Fall 2025 objective that was never verified).
3. Icons for the roughly 30 items the demo loop touches.
4. A numerical-technique prototype (heightfield diffusion / thermal erosion, or a sparse Poisson solve) as an offline commandlet experiment. **Not integrated by default**, and decided at the Week 5 gate.
5. Terraform excavation stage 4 (colony auto-dispatch).
6. Private Steam beta branch for testers on other machines.

## 3. Timeline & Milestones

### Milestones

| Milestone | Week (dates) | Definition |
|---|---|---|
| **M1: Menu-driven LAN session** | 3 (Oct 12-18) | Host and Join work through the UI; the scripted two-process test passes 10 of 10 consecutive runs. |
| **M2: Authoritative co-op loop** (midterm demo) | 5 (Oct 26-Nov 1) | H17 and C7 closed; extended 2-client suite green; save version plus co-op save/load round trip green. |
| **M3: Integrated packaged build** | 7 (Nov 9-15) | Scripted packaging produces the build; packaged smoke test (host, join, play, quit) passes; terrain determinism and replication verified. |
| **M4: Release candidate `v0.1.0`** | 10 (Nov 30-Dec 6) | Benchmark report, soak test, final report, video, and tag. |

### Advisor check-ins (scheduled)

| When | What to show |
|---|---|
| **Wed 2026-10-07, 3:00-3:20 PM (Week 2)** | The submitted plan, the baseline measurements, the historical closure summary, and the earlier session-system work with its test results. **Commit the verified Week 0 work before this meeting** so the evidence is in git |
| **Wed 2026-11-18, 4:00-4:20 PM (Week 8)** | M1-M3 results (menu-driven session, validated co-op loop, packaged build) and the first complete benchmark report |

Each meeting is preceded by a short written status (progress, risks, next steps). Short screen recordings of M1 (Week 3) and M2 (Week 5) are shared with the written status rather than at a meeting. The final demonstration in Week 10 is scheduled with the instructor.

### Weekly schedule (revised from the provisional schedule using the audited status)

| Week | Dates | Planned activity | Evidence | Gate |
|---|---|---|---|---|
| 0 | Sep 21-27 | Setup and baseline: re-verify the whole suite on a cold build; cleanup commits; volume, material, and session-backend work (see weekly progress) | 5 commits; 127/127 | Done |
| 1 | Sep 28-Oct 4 | Repository audit and historical reconciliation | Closure matrix; this plan; baseline snapshot | This package |
| 2 | Oct 5-11 | Land pending verified work as small commits; architecture review; defect register re-verified against code; systems map; backlog prioritized. **Check-in 1: Wed Oct 7** | Commits; systems map; defect register | No unverified claims carried forward |
| 3 | Oct 12-18 | **Core integration I:** Multiplayer panel and widgets (main-menu button, Host/Join tabs, session list); two-process LAN test script | Code, widgets, test | **M1** |
| 4 | Oct 19-25 | **Core integration II:** H17, C7 and the H19 placement residual (reach and collision recheck); extend the 2-client suite (client harvest, craft, build, terraform); disconnect and reconnect behavior; benchmark scene defined, a small C++ benchmark command (`MO.Bench.Run`, because the Python bridge is editor-only) written, and the first baseline captured | Code and tests; Appendix C row 1 | Suite green |
| 5 | Oct 26-Nov 1 | **Persistence, inventory, interaction verification:** save-format version plus migration test; co-op save/load round trip; headless tests for inventory and persistence; **midterm demo**; stretch decision gate | Tests; demo | **M2**; cut list reviewed |
| 6 | Nov 2-8 | **Procedural-world integration** (the world exists; this week proves it under co-op): seed determinism across host and client; terraform replication; world-load time | Terrain test; demonstration | Test green |
| 7 | Nov 9-15 | **Integrated playable build:** scripted packaging; packaged smoke test; fix cook-only defects | Build, script, smoke test | **M3**; **feature freeze** |
| 8 | Nov 16-22 | **Performance, stability, regression:** benchmark protocol (3 runs per configuration, single-player and host plus 1 client); 2-hour soak; top-3 hitches; full regression. **Check-in 2: Wed Nov 18** | Benchmark report; soak log | Report complete |
| 9 | Nov 23-29 | Defect correction, release candidate, documentation (short week: Thanksgiving) | RC build; docs | RC cut |
| 10 | Nov 30-Dec 6 | Final demonstration and retrospective; report, video, tag | Report; video; `v0.1.0` | **M4** |

The provisional schedule is adjusted in two places. Week 6 is narrowed to integration proof, because the voxel world already exists. Weeks 3-5 carry the main engineering risk, so Weeks 6-9 contain slack and no new features after the Week 7 freeze. No week is padded with a deliverable the evidence does not call for.

## 4. Tools & Technologies

| Area | Choice |
|---|---|
| Engine / language | Unreal Engine 5.8 (source build), C++ (primary) with Blueprint for widget layout |
| Core plugins / frameworks | Voxel Plugin (open-source dev-phy, vendored), CommonUI, Enhanced Input, PCG, Niagara, Gameplay Tags, OnlineSubsystem (Null for LAN) with OnlineSubsystemSteam (optional) |
| Testing | UE Automation (headless), editor Python gate scripts and the `Tools/ue.py` CLI (build, boot, PIE, two-client `mptest`), offline tooling tests |
| Profiling | `stat unit` / `stat gpu`, CSV Profiler, Unreal Insights, `nvidia-smi` for VRAM |
| Tooling | Rider, Git with LFS (`.gitattributes` for `.uasset` and other binaries), MiKTeX for the PDF report, Claude Code and Codex as development assistants |

**Rationale.** UE5 with C++ is the platform the project already runs on, and the Fall 2025 rationale still holds: it is industry-relevant and it exposes real C++ design choices. The tooling choices exist because this project's networking and persistence behavior cannot be verified by compiling: the editor-Python bridge cannot drive real RPC transport, and the existing 2-client PIE harness validates replication but never exercises the `IOnlineSession` Host / Find / Join calls, so session tests are run as separate processes. The chosen tools make those tests scriptable and repeatable.

## 5. Risks & Challenges

| ID | Risk | Likelihood / impact | Mitigation |
|---|---|---|---|
| R1 | **Steam init fails** (`SteamAPI_Init` for App 1602810): external, dashboard-side | Present / Medium | The LAN (Null OSS) path is the guaranteed demo path. Steam is a stretch item, time-boxed to one session; the config side is already proven correct (read-back verified). |
| R2 | **Scope too ambitious** for 10 weeks | Medium / High | Four milestones with exit criteria; Week 5 decision gate; Week 7 feature freeze; an ordered cut list (stretch items first, then Week 6 replication depth, then soak length). |
| R3 | **No performance baseline exists**, so the 60 FPS target may be badly missed | Medium / Medium | Benchmark scene and first capture in Week 4, not Week 8. A missed target is reported with profile evidence, not hidden. |
| R4 | **Two-process networking tests are new tooling** and may be flaky | Medium / Medium | Time-boxed in Week 3; every run logged; 10-of-10 acceptance rule; fall back to a manual two-window procedure documented as such. |
| R5 | **Hardware instability.** The development CPU is configured below stock and has shown flaky builds; PIE occasionally wedges | Medium / Low | One retry for a one-off build failure; discarded benchmark runs are logged, not silently dropped; `ue.py cycle` / editor restart for PIE wedges; hardware recorded in Appendix C. |
| R6 | **Authorship and assessment.** 94% of 2026 commits are AI co-authored | Present / Medium | Disclosed in Appendix D. Decisions are logged weekly; the student runs every acceptance test and explains the code in demos. Confirm the instructor's policy (closure document section 6, item 9). |
| R7 | **Cadence slips.** 73 verified paths are uncommitted and there were no commits Sep 23 to Oct 4 | Present / Low | Land pending work in Week 2 as small logical commits; at least 3 commits per week; the weekly file records the count. |
| R8 | **Version churn** (engine, Voxel dev-phy plugin) | Low / High | Engine and plugin pinned for the quarter; no upgrade work. The plugin is vendored on disk but untracked in git (`048d3b1f`), so Week 7 records its version and source in the README to keep a build from the tag reproducible. |
| R9 | **Stale trackers mislead planning.** The June 2026 audit was largely out of date | Realized once / Medium | Week 2 re-verifies the defect list against the code before any item is scheduled. |

## 6. Learning Objectives

- Network programming in UE: sessions and listen servers, authority, replication, validating client requests, and testing them across processes.
- Test design for hard-to-test systems: two-process harnesses, negative tests, deterministic seeds, and round-trip and migration tests.
- Performance engineering method: defining a repeatable benchmark, capturing frame-time percentiles, attributing hitches, and reporting results without bias.
- Persistence engineering: versioned formats, migrations, and conservation checks (no loss or duplication).
- Release engineering: scripted, reproducible packaging, tagging, and smoke testing.
- Professional process: traceable evidence, honest status reporting, and disclosed tool use. This contributes to my growth as a computer scientist by tying systems work to measurement and verification rather than to "it compiles."

## 7. Stakeholders & Communication

| Stakeholder | Interest |
|---|---|
| Student (Wesley Weaver) | Owner; engineering decisions; the demonstration |
| Instructor / advisor | Assessment; check-ins (proposed above) |
| Playtesters (one to two outside testers, Weeks 8-9; co-op needs two humans) | Feedback on host/join usability; notes stored in `Docs/CS399/playtests/` |
| Existing Steam audience for the 2021 UE4 title | Downstream only; no commitment this quarter |

**Communication plan.** `CS399_WEEKLY_PROGRESS.md` is updated every Sunday (objectives, work, measured outcomes, difficulties, commits, next priorities). At least 3 commits per week are pushed to the repository. Check-ins follow the schedule in section 3. A short screen capture is recorded at M1 and M2. Milestone tags and a one-paragraph summary are posted at each milestone.

## 8. Definition of Success

Core criteria must all be met. Criteria marked *measured and reported* are process-based: they require the measurement and an honest report, and they do not require a favorable number.

| ID | Criterion | How it is measured | Evidence artifact | Class |
|---|---|---|---|---|
| AC-1 | **Menu-driven session flow.** From the packaged build's main menu, one process hosts and a second process finds and joins using only the UI | Scripted two-process test, 10 of 10 consecutive runs pass | Test script, run log | Core |
| AC-2 | **Authoritative co-op loop.** In a two-client session each player harvests an item, crafts an item, and places a building; host state equals what the client observes | Automated 2-client tests, 0 failures; the existing 8 pass checks stay green | Test file and results | Core |
| AC-3 | **Authority hardening.** H17 and C7 closed, and a forged building placement is rejected | Negative tests: a client request over the rate/duration limit is rejected, a client-supplied pawn class outside the whitelist is rejected, and a placement outside reach or inside a collision is rejected (valid requests still succeed) | Tests and commit | Core |
| AC-4 | **Versioned persistence.** The save carries a version; a save written at the Week 5 commit loads in the final build; a two-player save/load round trip loses or duplicates nothing | Migration test; item-GUID multiset comparison before and after (equal) | Headless tests | Core |
| AC-5 | **Terrain under co-op.** Same seed gives identical terrain on host and client; a host terraform edit is visible on the client | Height samples at N fixed points (N recorded in the test) match exactly; edit visible within a recorded time | Test and log | Core (measurement required) |
| AC-6 | **Performance.** 6a: the protocol in Appendix C is executed and reported for single-player and host plus one client. 6b: target of **60 FPS median at 1080p** on the specified hardware | Median FPS, 1% low, 0.1% low, 3 runs each | Benchmark report with CSV captures | 6a: measured and reported. 6b: target |
| AC-7 | **No regression, more coverage.** Headless suite at least 127 passing, tooling tests and all existing gates pass, and at least 15 new automated tests | `ue.py auto`, gate runs, test count in the weekly file | Weekly results | Core |
| AC-8 | **Reproducible packaging.** One command builds the package; the packaged smoke test (boot, host, join, quit) passes | Command exit code | Script and log | Core |
| AC-9 | **Process.** At least 3 commits per week; weekly progress file current each Sunday | `git log` per week | Repository | Core |
| AC-10 | **Evidence integrity.** Each factual claim in the final report cites a commit, test, or artifact; no unverified claim is presented as fact | Citation check in the final report | `CS399_FINAL_REPORT.md` | Core |

**Pass rule.** The quarter is successful if all Core criteria are met and AC-5 and AC-6a are measured and reported. Missing the 60 FPS target (AC-6b) is recorded as a result, not a failure, provided the profile and the top-three-hitch work are documented.

## 9. Version Control Setup

- **Repository:** `https://github.com/penpro/MO57` (public). The links in the Fall 2025 plan are historical: `Metaverse-Origins` is empty and `Unreal-Inventory-System` and `MO56` no longer resolve (see the closure document, section 2).
- **Commit plan:** at least 3 commits per week (target 5 or more), each a small logical unit passing the pre-commit gate (headless suite green; `git status --short` staged column verified before every commit, because `git add` can fail silently on this Windows repo).
- **Convention:** `type(scope): message`, as already used in the history (for example `fix:`, `docs:`, `test:`).
- **Workflow:** trunk-based on `master`, as practiced since January 2026 and a recorded deviation from the Fall 2025 branch-and-PR plan. Stability is protected by the gate rather than by branches. Large binaries use Git LFS.
- **Tags:** `cs399-f26-baseline` (after Week 2 landing), `cs399-f26-m1` to `m4`, and **`v0.1.0`** for the release candidate.
- **Backups:** back up the MO56 history (closure document, section 8) and push weekly.

## 10. Previous Quarter Summary (continuing project)

Condensed; full detail, with evidence levels, is in `CS399_HISTORICAL_CLOSURE.md`.

**Planned vs. achieved.** Fall 2025 planned a narrow UE5 vertical slice. Inventory and save/load were built in about two weeks (509 commits) and a Windows build was staged. CommonUI, measured performance, scripted packaging, the `v0.1` tag, and a Steam push were not delivered in Fall 2025. CommonUI arrived in Spring 2026. From January to September 2026 the project was rebuilt as a UE 5.8 C++ framework of about 172,000 lines.

**Lessons learned.** Build the test and tooling loop early (it made later regression cheap); measure performance from week one; keep trackers current; commit and tag weekly; keep remotes and backups; log which decisions were the student's.

**Next steps.** This quarter's scope builds directly on last quarter's carried-forward items: multiplayer, performance measurement, save versioning, packaging, and the release tag.

---

## Appendix A: Workstream mapping

| Workstream | This quarter's work | Deliverables |
|---|---|---|
| A. Architecture and integration | Systems map; remove the session/travel/UI seams' duplication (`ValidateGameplayLevelExists`, one travel chokepoint); reconcile docs and tracker; no rewrites | D9, parts of D1 |
| B. Core gameplay systems | Co-op loop across possession, inventory, harvest, build; UI host/join; authority hardening | D1, D3, D4 |
| C. Procedural environment | Determinism and replication proof under co-op; benchmark scene; optional numerical-technique experiment (stretch, gated) | D6, stretch 4 |
| D. Verification and performance | Two-process and two-client tests; headless coverage for inventory and persistence; benchmark harness; scripted packaging; soak | D2, D3, D5, D7, D8 |
| E. Documentation and deliverable | Weekly record; final report; demonstration video; tag | D10 |

## Appendix B: Decision gates and cut order

- **Week 3 end (M1):** if the scripted two-process test cannot reach 10 of 10 in the time box, fall back to a documented manual two-window procedure and proceed (R4).
- **Week 5 end (M2):** stretch items start only if M2 is green. The numerical-technique stretch is decided here and nowhere else.
- **Week 7 (feature freeze):** after this, only defects, packaging, performance, and documentation.
- **Cut order if time runs short:** stretch items, then terraform replication depth (keep determinism), then soak duration (2 h to 30 min), then the second playtester. The Core criteria are not cut.

## Appendix C: Performance protocol and tracking (replaces Appendix A of the Fall 2025 plan)

**Test hardware.** CPU: Intel Core i9-14900K (24 cores / 32 threads; Windows reports a 3,200 MHz maximum, so it is configured below stock boost and has shown instability). GPU: NVIDIA GeForce RTX 4090, 24 GB, driver 616.92 (WDDM 32.0.16.1692). RAM: 128 GB. OS: Windows 11 Pro build 26200. Display: 1920x1080, fullscreen.

**Method.**
- Packaged build (record the git SHA and the package time); first captures use the Development configuration, the final report states the configuration used.
- Fixed scene: world seed 12345, level `MOPCGScattering`, game clock set to noon, scripted camera path of 3 minutes, a fixed pawn and survivor count; a 60 s warm-up is discarded.
- Capture with the CSV Profiler (and Unreal Insights for diagnosis). Report average FPS, **median FPS**, **1% low**, **0.1% low**, worst frame time, Game / Render / GPU thread milliseconds, and VRAM and working-set memory.
- Three runs per configuration; report the median of runs and the min-max. Configurations: (a) single-player; (b) host plus one client on the same machine (both processes share one GPU, so (b) is conservative).
- Before each run, check `nvidia-smi` for free VRAM and close other GPU-heavy applications. A run interrupted by a crash is logged as discarded, with the reason.

| Week | Date | Build tag / SHA | Config | Scene | Duration | Avg FPS | Median FPS | 1% low | 0.1% low | CPU / GPU / driver | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 4 | | | | | | | | | | | first baseline |
| 5 | | | | | | | | | | | |
| 6 | | | | | | | | | | | |
| 7 | | | | | | | | | | | |
| 8 | | | | | | | | | | | full protocol |
| 9 | | | | | | | | | | | |
| 10 | | | | | | | | | | | final |

## Appendix D: AI-assisted development and authorship

This is a factual disclosure, not a policy claim.

- **Fall 2025 (MO56):** about 139 `codex/*` branches and 138 pull-request merges; branch naming is consistent with OpenAI Codex cloud tasks merged by the student.
- **2026 (MO57):** 245 of 261 commits carry a `Co-Authored-By: Claude` trailer (several model versions); Codex also worked in the same working tree in July 2026 (coordination log: `claude-codex-coop.md`).
- **Practice this quarter:** the student sets the requirements, the acceptance criteria (section 8), and the design decisions, and runs and interprets the acceptance tests. Each week's progress entry records the decisions made and who made them. Demonstrations include a live walk-through of the code behind each acceptance criterion. Assistant-generated work is held to the same gates as any other work.
- **To confirm:** the instructor's policy on AI-assisted development (closure document, section 6, item 9). If the policy is restrictive, the scope of the acceptance criteria that depend on assistant-written code should be reviewed in Week 2.

## Appendix E: Open items needing the student's input

Carried from the closure document, section 6: Fall 2025 submission status, Weeks 8-10 activity, Steam push, "web console", the 1,900-line figure, the character/inventory bug, survivors-as-eggs, the Fall 2026 report format and length, and the AI-policy question. The instructor and the calendar are now confirmed.
