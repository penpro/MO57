# CS 399 Historical Closure and Continuity: Metaverse Origins

**Prepared:** 2026-10-04 (Fall 2026, Week 1) from a direct audit of the local repositories, build artifacts, and the Fall 2025 plan PDF.
**Student:** Wesley Weaver  |  **Course:** CS 399 Software Development Practicum
**Companion documents:** `CS399_FALL2026_PLAN.md`, `CS399_WEEKLY_PROGRESS.md`, `CS399_FINAL_REPORT.md`

## How to read this document

Every claim carries an evidence level.

| Level | Meaning |
|---|---|
| **V** | Verified directly in this audit: a file, commit, artifact, or command output was inspected. |
| **P** | Partially verified: supported by commit messages, project docs, or dev notes, but not re-executed in this audit. |
| **U** | User-reported only, or not verifiable from the evidence available. |
| **X** | Searched for and **not found** (or contradicted by the evidence). |

Dispositions follow the practicum closure policy: **CLOSED**, **CARRIED FORWARD**, or **SUPERSEDED**. Where the policy's three words are not enough, a qualifier is added (for example "CLOSED, evidence incomplete"). Nothing below is marked closed to improve the picture; the unfinished items are in section 6.

---

## 1. Central statement

*Metaverse Origins is an established, continuing software engineering project. The original Fall 2025 practicum established its redevelopment objectives. Development continued during Winter and Spring 2026 without a requirement for renewed formal planning. The Fall 2026 practicum establishes a new technical baseline, closes or carries forward historical objectives, and focuses on measurable progress toward a stable, integrated and demonstrable game.*

---

## 2. Project lineage and evidence sources

| Era | Artifact | Where it lives | Engine | Dates | Level |
|---|---|---|---|---|---|
| Published original | Steam App **1602810**, "Metaverse: Origins", Early Access, $9.99 | Steam store API (queried 2026-10-04) | UE4 | Released **2021-05-17** | V |
| UE4 source project | `D:\UEProjects\Origins_10` (15 GB content, includes `InventorySystem` and `SkillSystem` folders) | Local disk, no git | 4.27 | `.uproject` dated 2022-01-14 | V (presumed source of the published build; not cross-checked against the Steam depot) |
| Fall 2025 plan | `Metaverse_Origins_Project_Plan_By_Template (1).pdf` (dated 2025-09-27) and the instructor template `Project Plan Template.docx` | `C:\Users\penum\Downloads` | n/a | 2025-09-27 | V |
| Fall 2025 development | **MO56**, 509 commits | `D:\UEProjects\MO56` (local `.git`) | 5.6 | 2025-10-25 to 2025-11-10 | V |
| Transition | **MO_1**, 7 commits ("working on multiplayer pawn possession", "game menu and loading level working") | `D:\UEProjects\MO_1` | 5.x | 2025-11-10 to 2025-11-12 | V |
| Winter 2026 to present | **MO57**, 261 commits | `D:\UEProjects\MO57`; GitHub `penpro/MO57` (public) | 5.7 at creation (inferred from the name and `PATCH_NOTES.md`), **5.8** now | 2026-01-12 to 2026-09-22 | V |

### Provenance caveats (read before citing any historical repo)

1. **The repo linked in the handoff and the Fall 2025 plan, `penpro/Metaverse-Origins`, is empty.** GitHub reports it created 2025-09-27 with no commits and no default branch. It is not evidence of anything. **(V)**
2. **`penpro/MO56` no longer resolves on GitHub.** The local `D:\UEProjects\MO56\.git` is the only surviving copy of the Fall 2025 history. See section 8 for a backup recommendation. **(V)**
3. **`penpro/Unreal-Inventory-System` (the CSI Blueprint inventory) does not resolve** under that name. The only surviving trace found is the `InventorySystem` content folder inside the UE4 project. **(V)**
4. **MO57 contains no Fall 2025 history.** It was created fresh on 2026-01-12 (a UE Third Person template plus a 13-file `MOFramework` plugin). The MO56 to MO57 lineage is inferred from naming (MO56 = UE 5.6, MO57 = UE 5.7), dates, and matching early commit messages ("Save game with guid working"). A file-level port was not traced. **(P)**
5. Local git histories are only as authoritative as the working copy; they were not cross-checked against any remote.

---

## 3. Commit activity by period

Commit counts are not effort, and other coursework ran in parallel. They are reported because they are the one objective, repeatable measure.

**Fall 2025 (MO56 + MO_1)**

| Plan week (Mon-Sun, Week 0 = 2025-09-22) | Dates | Commits |
|---|---|---|
| 4 | Oct 20-26 | 4 |
| 5 | Oct 27-Nov 2 | 390 |
| 6 | Nov 3-9 | 114 |
| 7 | Nov 10-16 | 1 (MO56) + 7 (MO_1) |
| 8, 9, 10 | Nov 17-Dec 7 | **0 found** in either repo |

Authorship: "Wesley Weaver" 332, "penpro" 177 (two git identities). 138 merge commits reference pull requests (highest #153); 194 merge commits reference `codex/*` branches; 139 `codex/*` remote-tracking branches exist. The branch naming is consistent with OpenAI Codex cloud tasks merged by the student; the git data alone does not show who or what authored each change. **(V for the counts; P for the attribution)**

**2026 (MO57)**

| Month | Jan | Feb | Mar | Apr | May | Jun | Jul | Aug | Sep |
|---|---|---|---|---|---|---|---|---|---|
| Commits | 23 | 68 | 8 | 0 | 26 | 48 | 83 | 0 | 5 |

All 261 are authored as `penpro`. **245 of 261 (94%) carry a `Co-Authored-By: Claude` trailer** (Opus 4.5: 86, Opus 4.8: 65, Fable 5: 63, Opus 4.7: 26, Sonnet 5: 5). A second agent (Codex) also worked in the shared tree in July 2026 (`claude-codex-coop.md`; 11 commit messages mention it). **(V)**

---

## 4. Closure matrix: Fall 2025 plan (source: plan PDF dated 2025-09-27)

### A. In-scope items

| ID | Objective | Disposition | Evidence | Lvl |
|---|---|---|---|---|
| F25-S1 | Third-person demo character, Enhanced Input (move, look, jump, interact, sprint) | **CLOSED** | MO56 `AMO56Character`; MO57 `AMOCharacter` with Enhanced Input bound in `AMOPlayerController::SetupInputComponent`; exercised by every PIE boot run (2026-09-22 boot to pawn spawn, screenshot taken) | V |
| F25-S2 | Single graybox demo level, short traversal loop, pickup flow | **SUPERSEDED (exceeded)** | Replaced by the procedural voxel world level `MOPCGScattering` with PCG biome scatter. Pickup flow covered by the 2-client item-identity gate (8/8, commit `99ffed66`) | P |
| F25-S3 | Inventory data-driven via **Primary Data Assets** and Gameplay Tags (pickup, equip, consume, stack) | **CLOSED (function) / SUPERSEDED (mechanism)** | MO57 `UMOInventoryComponent` with item rows in **DataTables** (`DT_Items`; 214 items). `UPrimaryDataAsset` appears in **0** MO57 files (4 in MO56). `FGameplayTag` appears in 10 files only. Behavior is covered by editor Python gates, **not** headless tests | P |
| F25-S4 | Core UI with **CommonUI** (HUD, inventory panel, pause, options); options persisted | **CLOSED** | MO56 contains **no CommonUI source** (it was not used in Fall 2025). Adopted in MO57: layer-stack foundation 2026-03-18 `479f648b`; stage 3A `2f467cee`; lifecycle and queue consolidation 2026-07-20 `8a73640c`, `8b0b0573`. Live UI aggregate 84/84 on 2026-09-22 (two order-dependent CommonUI tests flake in batch, pass alone). A saved-volume-not-applied-at-boot defect was found 2026-09-22 and fixed locally (uncommitted) | V/P |
| F25-S5 | Save/load with a **versioned** SaveGame | **CLOSED, with a gap** | Save/load works (MO56 "saving and loading works! yay!" 2025-11-04; MO57 save-domain registry `IMOSaveDomain` plus persistence gates). MO56 had an explicit `SaveVersion`; **MO57 has no save-format version field** (none found in save types or `MOPersistenceSubsystem`). Versioning and migration are **CARRIED FORWARD** (Fall 2026 AC-4) | V |

### B. Stretch items

| ID | Objective | Disposition | Evidence | Lvl |
|---|---|---|---|---|
| F25-X1 | Crafting with one or two recipes | **CLOSED (exceeded)** | 122 recipes, crafting queue, crafting stations (2026-07-04 data baseline); `test_craft_resume.py` gate | P |
| F25-X2 | Lightweight multiplayer (host + one client): movement and inventory replication | **CARRIED FORWARD** (partial; becomes the Fall 2026 primary deliverable) | `mptest` 2-client PIE harness validates replication and RPC transport (8/8, 2026-07-06). **No player-facing host/join existed** until the 2026-09-22 session subsystem, which is uncommitted, verified over LAN only through console verbs, and blocked on Steam init | P/V |
| F25-X3 | Skill system with cooldowns, or a small GAS prototype | **SUPERSEDED** | Skill system implemented as XP/levels (`UMOSkillsComponent`, 22 skills) plus `UMOKnowledgeComponent`; no cooldown abilities, no GAS. Project policy (`CLAUDE.md`) deliberately keeps GAS out unless prediction is needed | V |

### C. Weekly timeline

| ID | Objective | Disposition | Evidence | Lvl |
|---|---|---|---|---|
| F25-W1-5 | Udemy course sections 4-13 | **CLOSED (academic)** | Course project repos/directories: `ObstacleAssault` (dir, 2025-09-16), `BattleBlasterStarterProject` (15 commits, 09-29 to 10-21), `DungeonEscape` (10-21), `ShooterSam` (9 commits, 10-21 to 10-22). Toon Tanks / Simple Shooter projects not found locally. Course completion itself is not repo-verifiable | P |
| F25-W6 | Inventory v1 and SaveGame persistence | **CLOSED** | MO56 `MOInventory` plugin (29 files, 8,119 LOC), `Save/` (7 files). Work began 2025-10-25, about 10 days before the planned Week 6 start. Equip/consume/stack rules were not re-tested in this audit | V/P |
| F25-W7 | UI v1 with CommonUI and **gamepad focus** | **CARRIED FORWARD (verification)** | MO56 UI (33 files + 8 menu files) existed **without CommonUI**; CommonUI landed in MO57 (see S4). **No gamepad verification evidence**; only 2 source files mention gamepad | X |
| F25-W8 | Profiling and stability (`stat unit`, `stat gpu`), remove top three hitches | **CARRIED FORWARD** | **Not delivered.** MO56 history has 0 commits mentioning profiling/fps/perf; no captures found. MO57 has an FPS counter widget but no benchmark harness and no recorded baseline | X |
| F25-W9 | Content pass, lighting, **packaging scripts**, reproducible RC build | **CARRIED FORWARD (partial)** | A staged build exists: `MO56/Saved/StagedBuilds/Windows/MO56.exe`, 2025-11-02, 509 MB. MO57 packaged builds exist (2026-06-30 documented in `PATCH_NOTES.md`; `Packaged/Windows/MO57.exe` 2026-09-20, 3.0 GB). **No scripted packaging command exists in either repo**; packaging is manual UAT (`Docs/AUTONOMOUS_TOOLING.md`) | V |
| F25-W10 | Technical report (8-12 pp), 3-5 min video, tag **v0.1** | **CLOSED, evidence incomplete** | No report or video found under Documents, Desktop, Downloads, or ClaudeMaster. **No git tag exists** in MO56, MO_1, or MO57, so `v0.1` was never tagged. Whether the report and video were submitted is a student/instructor record, not a repo fact. Tagging intent is carried into the Fall 2026 milestones | U/X |

### D. Definition of success (Fall 2025)

| ID | Criterion | Disposition | Evidence | Lvl |
|---|---|---|---|---|
| F25-D1 | Packaged build **pushed to Steam** spawns the player in the demo level | **CARRIED FORWARD (long term)** | **Not delivered.** The Steam listing shows only the 2021 UE4 early-access title; no evidence of a UE5 depot or branch push. Local packaged builds spawn the player | X |
| F25-D2 | Inventory correctness: pickup, equip, consume, persist without duplication or loss | **CLOSED (functional), test gap** | Pickup identity round trip 8/8; persistence gates `test_persist_jobrefs`, `test_designation_persist`. **None of the 127 headless tests is named for or targets inventory or persistence** (checked by test name: Medical 79, Colony 24, UI 10, Survival 5, Skills 3, Knowledge 3, Integration 1, Terraform 1, Clock 1) | P |
| F25-D3 | UI usability: icons, tooltips, gamepad focus, options persist | **CARRIED FORWARD (partial)** | **757 of 808 art slots (94%) are unset**, including 600 of 642 item icons (2026-07-04 baseline). Gamepad unverified. Options persist (boot-apply defect fixed locally 2026-09-22) | V/X |
| F25-D4 | **60 FPS median at 1080p** on mid-range hardware | **CARRIED FORWARD** | **Never measured.** No benchmark, capture, or Appendix A record exists. Target hardware is now specified in the Fall 2026 plan | X |
| F25-D5 | 8-12 page report and 3-5 min video | see F25-W10 | | U |

### E. Process and administrative objectives

| ID | Objective | Disposition | Evidence | Lvl |
|---|---|---|---|---|
| F25-P1 | At least 3 commits per week | **CLOSED for Weeks 4-7; not met for Weeks 8-10 per git** | See section 3. Whether Weeks 8-10 work exists elsewhere is unknown | V/U |
| F25-P2 | Git LFS with `.gitattributes` | **CLOSED** | `.gitattributes` present in MO56 and MO57; MO57 LFS pointers verified 2026-09-22 | V |
| F25-P3 | Feature branches, PRs, tags M1-M10, `type(scope):` messages, one issue per task | **SUPERSEDED (practice differs)** | MO56 used PRs heavily (138). MO57 is **trunk-based on `master`**, with **0 GitHub issues, 0 PRs, 0 tags**; tracking lives in `Docs/PROJECT_STATUS.md` and `claude-codex-coop.md`. Recorded as a deviation; Fall 2026 adds milestone tags | V |
| F25-P4 | Two check-ins (Oct 20, Nov 17); two outside playtesters in Week 8 | **CLOSED, evidence incomplete** | Not repo-verifiable | U |
| F25-P5 | Appendix A weekly performance table from Week 5 | **CARRIED FORWARD** | No records exist. Rebuilt as Appendix A of the Fall 2026 plan | X |

### F. Long-term design objectives (from the handoff, section 2)

| ID | Objective | Disposition | Evidence | Lvl |
|---|---|---|---|---|
| LT-1 | Earth-scale procedural planet | **CARRIED FORWARD (out of scope for Fall 2026)** | **Not implemented.** Current world is a finite large flat world; `CLAUDE.md` lists spherical support as future. Zero code | V |
| LT-2 | Voxel terrain generation and modification | **CLOSED for current scope** | Voxel plugin vendored (open-source dev-phy build) behind the `MOVoxel` facade; terraform realism 1-2 and excavation stages 1-3 shipped; PCG biomes P1-P4. Excavation stages 4-7 remain | P |
| LT-2b | "Voxel Plugin 2.0" | **SUPERSEDED** | Replaced by the dev-phy build (the only line supporting UE 5.8), commit `4a5eb116`, 2026-06-30. Functionality is delivered by a different mechanism | V |
| LT-3 | Survivors as **eggs**; character switching and possession | **Possession CLOSED; eggs NOT IMPLEMENTED** | `UMOPossessionSubsystem`, persistent pawns, join-spawn (S0). The only egg is a `BirdEgg01` food item; no design document records the egg concept as superseded. **Open design decision** | V/X |
| LT-4 | Resource harvesting and preparation | **CLOSED** | Per-harvester contexts (H23), `test_harvest_concurrent.py` 9/9, timed foraging, 122 recipes | P |
| LT-5 | Autonomous character jobs and task allocation | **CLOSED for current design** | `UMOSurvivorJobQueueComponent`, `AMOSurvivorController` job state machine, village gates (quota, school, seasons, dynasty, winter shelter) | P |
| LT-6 | Inventory, persistence, crafting | **CLOSED with gaps** | See S3, S5, X1 (save versioning and test coverage) | P |
| LT-7 | Skills, progression, knowledge transfer | **CLOSED for current design** | Skills/knowledge components; `MOFramework.Colony.Teach`; school and skill-decay gates | P |
| LT-8 | Settlement and civilization development | **CARRIED FORWARD** | Substantial progress (colony manager, village loop through the winter-fire loop); later stages pending | P |
| LT-9 | Multiplayer and server-authoritative gameplay | **CARRIED FORWARD: primary Fall 2026 deliverable** | Open items: H17 (terraform RPC has no rate or duration limit), C7 (possession RPC accepts a client-supplied class), host/join UI absent, Steam init blocked | P |

---

## 5. Verification of the progress reported on 2026-05-22

| Reported item | Finding | Lvl |
|---|---|---|
| Migration toward UE 5.6/5.7 | Verified, and **superseded**: the project is now **UE 5.8** (`MO57.uproject` `EngineAssociation: "5.8"`; upgrade commit `52697f8e`, 2026-06-30) | V |
| CommonUI migration replacing about 1,900 lines | Migration verified (commits in S4). The **1,900-line figure was not recomputed** and is unverified | P/U |
| Functional base-building snapping | Code verified (`TryEdgeSnap` in `MOBuildingComponent.cpp`). **Runtime behavior not re-demonstrated** in this audit and no dedicated automated test exists; scheduled for the Week 5 demonstration | P |
| Resolution of an identified character/inventory bug | **Cannot be matched** to a commit without the bug's description | U |
| Continued playtesting and bug correction | Consistent with the commit history | V |
| Architectural refactoring assisted by Claude Code | Verified: 245 of 261 commits carry Claude co-author trailers | V |
| Procedural voxel systems | Verified (section 4, LT-2) | V |
| Multiplayer | Partial (section 4, X2 and LT-9) | P |
| **Web console** | **No evidence found.** Zero matches for "web console", `webconsole`, `web_console`, or "WebRemoteControl" in `Tools`, `Docs`, `Source`, `Content/Python`, or `Config`, and zero commit messages. It may refer to the loopback MCP/bridge tooling (which is not a web console) or to another repository. **Clarify before it appears in any instructor-facing statement** | X |
| Identity/GUID tracking, interaction validation, subsystem organization, persistence | Verified (`UMOIdentityComponent`, `UMOIdentityRegistrySubsystem`, distance-validated pickup RPC `02549252`, 3 runtime modules plus an editor module, `IMOSaveDomain`) | V |
| Server-authoritative systems | Partial; known open items listed under LT-9 | P |

---

## 6. Items that need the student's input

These stay open until the student confirms them. None should be presented to the instructor as settled.

1. **Fall 2025 submission.** Was the technical report and video submitted, and where are they? No file was found, and no `v0.1` tag exists anywhere.
2. **Weeks 8-10 of Fall 2025.** No commits exist in MO56 or MO_1 after 2025-11-12. Is there work on another machine or repo?
3. **Steam push (F25-D1).** Confirm that no UE5 build was pushed to Steam.
4. **"Web console."** What does it refer to?
5. **"About 1,900 lines" (CommonUI).** Source of the figure?
6. **"Resolution of a character/inventory bug."** Which bug?
7. **Survivors as eggs.** Is the concept still intended, or has the design moved on? If it has, record a one-line supersession note.
8. **Fall 2026 report length and format.** (Resolved since this audit: the instructor, the quarter dates from Olympic College's published calendar, and the two scheduled check-ins, Wed Oct 7 and Wed Nov 18; see the plan.) Still open: the required length and format of the final report.
9. **Authorship policy.** Whether the instructor has a stated policy on AI-assisted development (section 3 of the plan discloses the extent).

---

## 7. Previous Quarter Summary (text for the plan template)

**Planned vs. achieved.** The Fall 2025 plan aimed at a narrow UE5 vertical slice: a demo character, one graybox level, a data-driven inventory, CommonUI-based UI, and versioned save/load, with profiling, packaging, a report, a video, and a `v0.1` tag as closing deliverables. In practice the inventory and save/load systems were built in about two weeks (2025-10-25 to 2025-11-10, 509 commits) and a Windows build was staged on 2025-11-02. CommonUI, measured performance, scripted packaging, the `v0.1` tag, and the Steam push were **not** delivered in Fall 2025. CommonUI was delivered in Spring 2026; the rest are carried forward. Between January and September 2026 the project was rebuilt as the C++ `MOFramework` plugin (three runtime modules, about 172,000 lines) on UE 5.8, and now includes a voxel world, medical and survival simulation, colony and village AI, crafting, terraforming, persistence, and a 2-client multiplayer test harness.

**Lessons learned.** What worked: building the core quickly with heavy AI assistance, then investing in a repeatable test and tooling loop (headless automation, editor Python gates, a unified CLI) that made later regressions cheap to catch. What to change: measure performance from the first week rather than the eighth; keep issue/defect trackers current (the June 2026 audit table was badly stale, with most listed defects already fixed); commit and tag milestones weekly; keep remote repositories and backups of historical work; and record which decisions were the student's versus implemented by an assistant.

**Next steps.** Fall 2026 converts accumulated systems into one demonstrable integrated feature: a two-player cooperative session started entirely from the main menu, with server-authority verification, a version-aware save format, a measured performance baseline, and a reproducible packaged build (see `CS399_FALL2026_PLAN.md`).

---

## 8. Evidence-preservation recommendations

1. **Back up the MO56 history.** The GitHub remote is gone. From `D:\UEProjects\MO56`, run `git bundle create D:\Backups\MO56-fall2025.bundle --all` and keep the bundle outside the working tree. (It is not LFS-heavy once assets are excluded, but check the size first.)
2. **Do not copy the Fall 2025 plan PDF or the template into this public repository.** They name the instructor. Reference them by filename and location only.
3. **Landing the baseline.** The audit-time working tree holds uncommitted verified work (see the weekly progress file). Land it as small logical commits so that the Week 1 baseline is reproducible from git. Consider tagging the result `cs399-f26-baseline`.
