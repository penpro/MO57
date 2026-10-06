# CS 399 Weekly Progress: Metaverse: Origins (Fall 2026)

**Student:** Wesley Weaver  |  **Repository:** `https://github.com/penpro/MO57`
**Calendar (confirmed against the Olympic College academic calendar; week numbering follows the Fall 2025 plan):** Week 0 = Mon 2026-09-21; Week 1 = Sep 28-Oct 4; ...; Week 10 = Nov 30-Dec 6.
**Advisor check-ins:** Wed 2026-10-07, 3:00-3:20 PM (Week 2); Wed 2026-11-18, 4:00-4:20 PM (Week 8).
**Update rule:** every Sunday. Each entry lists objectives, work performed, measurable outcomes, difficulties, relevant commits, and next priorities. A claim of progress needs a commit, a test result, or an artifact. Work that exists only in the working tree is labeled **uncommitted** and is not counted as delivered until it is committed.

Evidence levels follow the closure document: **V** verified directly, **P** partially verified, **U** reported only.

## Trackers

### Commit cadence (target: at least 3 per week)

| Week | Dates | Commits | Target met | Note |
|---|---|---|---|---|
| 0 | Sep 21-27 | 5 (all on Sep 22) | Yes | Cleanup and baseline commits |
| 1 | Sep 28-Oct 4 | 8, all on Oct 4 | Yes, on the last day only | Audit week. No commits Sep 23 to Oct 3; the Week 0 work and these documents were landed on Oct 4 after a full gate (build, 127 / 127, 15 / 15) |
| 2 | Oct 5-11 | | | Landing pending work as small commits |

### Test and metric baseline (compare each week)

| Date | Headless automation | Editor Python gates | Tooling tests | Live UI aggregate | New tests this quarter | Source LOC |
|---|---|---|---|---|---|---|
| 2026-09-22 / 10-04 | 127 / 127 pass | 26 scripts | 15 / 15 | 84 / 84 (2 order-dependent tests flake in batch, pass alone) | 0 | about 172,300 |

### Decision log (who decided what)

| Date | Decision | By |
|---|---|---|
| 2026-09-22 | Integrate real Steam sessions for App 1602810 ("Metaverse: Origins") rather than a direct-IP interim | Student |
| 2026-09-22 | Add console verbs for testing first, then build the real menu UI | Student |
| 2026-09-22 | Fix the gray-material defect with project-local material duplicates instead of editing the engine install | Assistant proposed; student accepted the verified result |
| 2026-10-04 | Recommended deliverable: integrated two-player co-op vertical slice, first tag `v0.1.0` | Assistant recommended; **student to confirm** |
| 2026-10-05 | Replace the single Host/Join "Multiplayer" panel with separate Host Game and Join Game panels, each behind its own main-menu button (same pairing as New Game and Load Game), so the student can build the Join UI by copying and renaming existing buttons | Student asked for a Join Game button with C++ backing; assistant chose the split design; **student to confirm** |

---

## Week 0 (Sep 21-27): Setup and baseline *(retrospective entry; objectives reconstructed from the session record)*

**Objectives**
1. Re-establish a verified, stable baseline after an idle gap (previous commit 2026-07-20).
2. Fix two live defects: flat-gray materials at runtime, and saved volume not applied until the Options menu is opened.
3. Begin the player-facing co-op path (host and join), which had a backend but no way for a player to use it.

**Work performed**
- Cold re-verification on a fresh build: headless automation, tooling tests, widget-blueprint checks, a cold Escape-key test, the live UI aggregate, and four excavation and harvest gameplay gates, all green.
- Cleanup and landing of long-pending changes, including `DT_Items.uasset` (M21 medical items and excavation materials, pending since July), repository hygiene (`.gitignore`), and a freshness note on the stale status document.
- **Material defect.** Root cause: five shared Fab master materials, which live in the engine install, lacked the Nanite and instanced-mesh usage flags, so every dependent Material Instance silently fell back to the default gray material at runtime. Fix: five project-local duplicates (`Content/Materials/FabOverrides`) with the flags set, and 41 Material Instances reparented (found through an Asset Registry referencer search). Verified with before and after screenshots and a cold restart.
- **Volume defect.** Root cause: master volume reached the audio device's transient primary volume only through the Options panel's Apply flow. Fix: `UMOAudioSubsystem::ApplyMasterVolumeToAudioDevice()`, called at boot and on change. Verified with a saved value of 0.37 surviving a cold boot before any menu was opened.
- **Co-op session backend** (new): `UMOSessionSubsystem` (Host / Find / Join / Leave over `IOnlineSession`), a listen-server option on the single travel chokepoint (`UMOTravelUtils`), `AMOMainMenuPlayerController::HostSession`, the Multiplayer panel classes (`UMOMultiplayerPanel`, `UMOSessionListWidget`, `UMOSessionListEntry`), main-menu wiring (C++ only), and `MO.Session.*` console verbs (Host, Find, Join, Leave, Status).
- Steam configuration (plugin, App ID 1602810, net driver) added and read back through the engine's config API to confirm every value is correct.
- Started the main-menu widget work by mapping the existing menu (a Grid Panel: buttons in column 0, rows 0-3; the panel switcher in column 1). **No widget edits were saved**; the widget asset is unchanged in the working tree.

**Measurable outcomes**
- Headless automation **127 / 127** (re-run after the session and UI code was added); tooling 15 / 15; UI aggregate 84 / 84; WBP 15 / 15; cold-Escape 7 / 7 (V for the 127 re-run on 2026-09-22 and 2026-10-04; P for the others, from the 2026-09-22 run notes).
- Console-verb verification (V): `MO.Session.Host` created a session and the world became a real **ListenServer** (confirmed by a net-mode lookup and a screenshot of the spawned pawn); `Find` completed its async search (0 results, since a host does not discover itself); `Join` rejected an invalid index; `Leave` and `Status` round-tripped to "no active session".
- Correction made during the work: the Null online subsystem is not a stub. Its `CreateSession` and `FindSessions` run a LAN broadcast beacon (confirmed in the engine source), so the full session pipeline is testable across two processes without Steam.

**Difficulties**
- `SteamAPI_Init` fails for App 1602810 even with Steam running and logged in. All config values are verified correct, so the cause is likely external (partner-dashboard setup or a Steam client restart). Tracked as risk R1; LAN is the guaranteed path.
- Computer-control tooling lost focus to a phantom Windows text-input process and briefly opened a stray UE 5.7 launcher; interactive widget editing was paused. No project data was affected.

**Commits (5)**
`032e0d48` M21 medical items and excavation materials; `80db71d1` coordination-log entries; `f9cc832e` ignore rules, tooling permissions, stale toolset reference; `353c17cc` editor-touched material cache assets; `42ac9387` status-document freshness note.

**Uncommitted at end of week (verified locally):** material fix (48 content assets: 6 new, 42 modified), volume fix, session subsystem and UI classes, travel option, console verbs, Steam config, `steam_appid.txt`. See Week 2.

---

## Week 1 (Sep 28-Oct 4): Repository audit and historical reconciliation

**Objectives**
1. Audit the repositories, history, and artifacts and reconcile them against the Fall 2025 plan.
2. Produce the planning and continuity package (closure, plan, progress record, report skeleton).
3. Establish a measured baseline for the quarter.

**Work performed**
- Audited four project lineages: the UE4.27 original (`Origins_10`), Fall 2025 `MO56` (UE 5.6), the transitional `MO_1`, and `MO57` (UE 5.7 to 5.8), plus GitHub state for each repository named in the handoff.
- Located and read the actual Fall 2025 plan (PDF) and the instructor's plan template; the new plan follows the template's sections.
- Built the historical closure matrix: **34** objectives classified (CLOSED, CARRIED FORWARD, SUPERSEDED), each with an evidence level, plus an 11-row verification of the May 2026 progress report.
- Measured the baseline: lines of code, test inventory by area, packaging, art debt, and repository state (see the plan's baseline snapshot).
- Wrote the Fall 2026 plan: recommended deliverable, four milestones, ten-week schedule, ten acceptance criteria, risks, and a performance protocol with the test hardware filled in.

**Measurable outcomes**
- Findings that change the retrospective (V): the repo linked in the handoff (`Metaverse-Origins`) is **empty**; `penpro/MO56` and `penpro/Unreal-Inventory-System` **no longer resolve**; the MO57 history starts on 2026-01-12, so Fall 2025 evidence exists only in the local MO56 history (509 commits, 2025-10-25 to 2025-11-10).
- Gaps found in the Fall 2025 objectives (V): no CommonUI in Fall 2025, no profiling, no scripted packaging, no `v0.1` tag, no 60 FPS measurement, no Steam push of a UE5 build; and in the current code, no save-format version field and no headless tests for inventory, persistence, or networking.
- Claims that could **not** be verified and need the student: the "web console", the "about 1,900 lines" figure, the character/inventory bug, survivors-as-eggs, and the Fall 2025 report, video, and Weeks 8-10 activity (closure document, section 6).

**Difficulties**
- The handoff's repository links and plan description did not match the filesystem; the audit relied on local histories and artifacts instead (documented in the closure document, section 2).
- No commits were made between Sep 23 and Oct 3, so the verified Week 0 code sat uncommitted for 11 days and the AC-9 cadence (3 per week) was met only on the last day. It was landed on Oct 4 in eight small commits.

**Commits (8, all Sunday Oct 4, after the gate: `ue.py build` succeeded, `ue.py auto` 127 / 127, tooling tests 15 / 15):**
`7dbcf0f1` project-local Fab master overrides (48 assets: the gray-material fix); `4fe9031e` apply the saved master volume at boot; `b7ec73a0` listen-server option on the travel chokepoint; `2120af2d` `UMOSessionSubsystem` plus Build.cs and uproject dependencies; `1c00aacc` Multiplayer panel classes and host flow (C++ only); `b366ec08` `MO.Session.*` console commands; `335e71fc` Steam online subsystem configuration and App ID; and the commit that adds the four documents in `Docs/CS399/`.
Nothing is pushed yet and `cs399-f26-baseline` is not yet tagged. Left uncommitted on purpose: `.claude/settings.local.json` (local tooling permissions; decide whether it belongs in the repository).

**Next priorities (Week 2)**
1. Student review of the open items (closure, section 6) and confirmation of the calendar, instructor, and deliverable.
2. ~~Land the verified Week 0 work as small logical commits~~ (done Oct 4, eight commits). Remaining: push `master`, tag `cs399-f26-baseline`, decide on `.claude/settings.local.json`.
3. Re-verify the defect register against the code, and produce the systems map.

---

## Week 2 (Oct 5-11): Landing, architecture review, defect register

**Planned objectives:** land pending work (at least 3 commits, **before the Wed Oct 7 check-in so the evidence is in git**); defect register reconciled against code; systems map; tag `cs399-f26-baseline`. **Check-in 1: Wed Oct 7, 3:00-3:20 PM.**
**Work performed:** *(to be filled)*
**Measurable outcomes:** *(commits, tests, items re-verified)*
**Difficulties:** *(to be filled)*
**Commits:** *(to be filled)*
**Next priorities:** Week 3, Host Game and Join Game widgets and the two-process LAN test.

## Week 3 (Oct 12-18): Core integration I, M1

**Planned objectives:** main-menu Host Game / Join Game buttons and panel widgets (C++ done on Oct 5: `UMOHostGamePanel`, `UMOJoinGamePanel`; copy-and-rename work remains in the blueprints); two-process LAN test script; **M1: 10 of 10 consecutive runs pass.** Send a short recording with the next written status.
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 4 (Oct 19-25): Core integration II

**Planned objectives:** H17 and C7 fixed with negative tests; 2-client suite extended (client harvest, craft, build, terraform); disconnect and reconnect behavior; benchmark scene defined and first baseline captured (Appendix C row 1).
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 5 (Oct 26-Nov 1): Persistence, inventory, interaction verification, M2 (midterm)

**Planned objectives:** save-format version and migration test; co-op save/load round trip (item-GUID comparison); headless tests for inventory and persistence; base-building snapping demonstrated; **M2**; stretch decision gate. Send a short recording with the next written status.
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 6 (Nov 2-8): Procedural-world integration

**Planned objectives:** seed determinism across host and client; terraform edit replication; world-load time recorded.
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 7 (Nov 9-15): Integrated playable build, M3, feature freeze

**Planned objectives:** scripted packaging command; packaged smoke test (boot, host, join, quit); cook-only defects fixed.
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 8 (Nov 16-22): Performance, stability, regression

**Planned objectives:** full benchmark protocol (3 runs per configuration); 2-hour soak; top-3 hitches; full regression. **Check-in 2: Wed Nov 18, 4:00-4:20 PM.**
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 9 (Nov 23-29): Defect correction, release candidate, documentation (Thanksgiving week)

**Planned objectives:** release candidate cut; documentation complete; playtest notes.
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*

## Week 10 (Nov 30-Dec 6): Final demonstration and retrospective, M4

**Planned objectives:** final demonstration; `CS399_FINAL_REPORT.md`; 3-5 minute video; tag `v0.1.0`.
**Work performed / outcomes / difficulties / commits / next:** *(to be filled)*
