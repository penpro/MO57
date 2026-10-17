# MO57: Claude handoff, goals, and acceptance checks

Prepared by Codex on October 13, 2026, America/Los_Angeles. This is a review-based work plan for handing the project back to Claude. Refresh the repository and current session state before executing it: Claude was actively testing the regression fixes while this document was prepared.

## Start here

Continue the existing work. First reconcile H00-H05 against the live checkout, then establish the controlled performance baseline in H06. Resolve the starter-loop and physiology issues before expanding content. Work through one coherent change and its acceptance checks at a time. Keep design decisions separate from implementation defects; continue independent work while a decision is pending.

For every completed goal, record the diagnosis, affected files, build/revision, commands, result counts, artifact paths, concrete player reproduction, and remaining limitations. A successful build or a green structural test alone does not close a visual or gameplay goal.

This document records recommendations and known state. It does not grant new permission to commit, push, download, delete assets, or modify the engine. Follow the user's direct authorization in the current Claude conversation and the repository rules. Do not treat copied session history as fresh authorization.

## 1. Current rundown and evidence

### What the recent work accomplished

| Area | Landed work | Remaining uncertainty |
|---|---|---|
| World generation | Shared C++ generator and Voxel sampling integration; climate-aware biome selection; caves, rivers/lakes, mountain relief; generator v7 high-ground cave gate | Summit holes/LOD tears, old-save regeneration, lake/PCG timing |
| Vegetation | Biome palettes; ground, tree, stand-in and canopy tiers; far canopy reaches about 7.5 km; reduced native grass/items; disabled biome-blind Field_05 trees | Tier handoffs in motion, permanent generation holes, startup scheduling, tutorial resource availability |
| Terrain appearance | Shared ground-material upgrade: macro variation, anti-tiling offsets, distant tint, altitude snow | Climate/season-driven snow, snow on vegetation/rocks, per-surface coverage, close-up texture artifacts |
| UI | Field Ledger tokens, shared buttons, icons and styling across inventory, crafting, building, skills, quests, status and possession; CommonUI lifecycle/focus/selection fixes | Remaining screens, actual optional-widget bindings, accessibility controls, gamepad navigation, stale status tables |
| Traversal/survival | Swimming, breath/drowning feedback, crouching, footsteps, underground-spawn rescue, death recap/respawn | Thermal rate, survival HUD's legacy source, physiological lethality, swim transitions/buoyancy |
| Stability/tooling | Packaged correctness suite, multiplayer harnesses, perf tours, PCG diagnostic verbs, async/cached upland-lake query, audio mix/buffer fixes | Comparable performance measurements, audible verification, screenshot dimensions, synchronized evidence |

### Snapshot at preparation

- HEAD was `60c666f9` (terrain materials, lobed canopy, generator v7). Its preceding vegetation/clutter commit was `b4846421`.
- The three top regressions in `Docs/Handoff_2026-10-13.md` have follow-up implementations in the working tree. They are not simply untouched TODOs.
- Claude's session reported a successful C++ build, six focused tests passing, and an item-thinning negative control failing against the pre-fix graph for the expected reasons. The fixed graph was restored with a matching checksum. The full MOFramework suite was being run; this review did not independently execute it.
- Pending source work includes `MOWorldReadinessSubsystem`, `MOPCGBiomeSpawnerSettings`, their tests, `thin_item_clutter.py`, and `make_far_canopy.py`. Pending assets include the scatter graph and canopy material.
- `.claude/settings.local.json` and the developer `MegaMaterialCache.uasset` were already local changes; they are not part of the gameplay fix by default.
- The existing package at `Saved/StagedBuilds_DevTest/Windows/MO57.exe` predates the follow-up fixes. Its actual game binary was timestamped October 13 at 02:42:55 PDT. The wrapper executable alone is not evidence of a new cook.
- That package logged READY at **45.7 seconds**, a vegetation hard-cap exit at about 32 seconds, and **753 nearby instances within 60 m**. The initial suggestion that the 200-instance threshold had simply become unattainable after thinning is not established by this log.
- The engine scheduler actually adds grid size to priority and generates large grids first: `D:/UnrealEngine/UE_5.8/Engine/Plugins/PCG/Source/PCG/Private/RuntimeGen/PCGRuntimeGenScheduler.cpp`. This supports investigating canopy scheduling ahead of local vegetation.
- The package also logged that `M_FarCanopy` lacked `InstancedStaticMeshes` usage and would use the default material. The follow-up script sets that flag; a new cook must prove it.
- The earlier roughly 79 -> 102 FPS claim is not a controlled comparison. Claude explicitly recorded the confounding conditions in the handoff.

### Read in this order

1. Root `AGENTS.md` / `CLAUDE.md`, including Engineering Principles and current session authorization.
2. This document and `Docs/Handoff_2026-10-13.md`.
3. Recent commits/diffs, current Claude validation results, and relevant packaged logs.
4. `Docs/Archive/Overnight_Polish_2026-10-12.md` and `Docs/Archive/AAA_Polish_Backlog.md` for recent evidence and remaining work.
5. `Docs/TECHNICAL_REFERENCE.md`, `Docs/AUTONOMOUS_TOOLING.md`, and subsystem-specific docs before changing a system.
6. `Docs/PROJECT_STATUS.md` for historical audit candidates. Its June metrics and many status tables are stale; source code and dated verification take precedence.

## 2. Priorities and dependency order

| Priority | Goals | Outcome |
|---|---|---|
| P0 | H00-H06 | Current regressions closed in a fresh package, with honest baseline measurements |
| P1 | H07-H13 | Reliable terrain/water/startup behavior and one coherent survival simulation |
| P2 | H14-H21 | Complete, usable presentation, audio, world appearance and content coverage |
| P3 | H22-H26 | Pawn delegation, remaining architecture work, better evidence and current documentation |

Suggested sequence: **H00 -> H01-H04 -> H05 -> H06 -> H07-H09 -> H10-H13 -> H14-H21 -> H22-H26**. H25 and H26 can be addressed between verification runs. H09 should be repeated after H10/H11 because survival pacing changes the tutorial experience. H13 should precede further generator changes intended to preserve existing saves. H24 begins with diagnosis, not a pre-approved list of patches.

## 3. P0: close the active regressions and establish a trustworthy build

### H00 — Reconcile the live worktree and evidence

**State:** Active work overlaps this handoff.

Read current Git status, recent commits, Claude's latest report, and build/package timestamps. Identify which fixes are source-only, compiled, tested, cooked, and player-verified. Preserve existing local changes. Do not revert a regression fix or repeat asset edits merely because the older handoff still says "fix."

**Acceptance:** A short current-state table names the revision, pending files, exact package and test artifacts, and remaining checks. No unqualified "all done" claim based on the earlier package.

### H01 — Restore prompt loading with local vegetation ready

**Diagnosis:** Large PCG grids are scheduled before small grids, so the canopy pass can postpone local vegetation and push the reveal to its hard cap.

Inspect the new readiness hold rather than rewriting it. Verify it takes effect before unwanted jobs are queued, applies to the actual runtime PCG components, and restores authored default/per-quality radii on Ready, StopTracking, Deinitialize and restarted cycles. Check maps/components without radius overrides. Keep the near-count trace. The end goal is a local readiness condition that works for sparse biomes, rather than dependence on arbitrary resource density or unrelated far-field activity.

**Acceptance:** Fresh single-process packaged new games across representative seeds/quality levels show local ground and vegetation at reveal, zero stuck holds, and a materially improved measured load versus the 45.7-second regression. Test new game, load, main-menu backdrop and respawn. Record stage times, first local tile, near count, held/released component count and exit reason; explain any remaining cap exits.

**Anchors:** `MOWorldReadinessSubsystem.{h,cpp}`, `MOWorldReadinessTests.cpp`, GameMode/menu tracking callers.

### H02 — Thin items without changing the locust branch

**Diagnosis:** Lowering `RandomChoice_3` from 0.5 to 0.05 sent 95% of points into a branch that was incorrectly called a dead end, nearly doubling native locust clusters.

Verify the saved graph has its original 0.5 split and a separate approximately 0.1 thinning node only on the item path. Preserve the locust chain; confirm the thinning node's discarded output is unused and its seed is independent. Re-running the script must not add another node or change unrelated edges.

**Acceptance:** Readback and `MOFramework.PCG.ItemThinningKeepsLocustShare` agree; compare actual item and locust census counts with the same seed. Retain the already reported negative-control evidence rather than repeatedly mutating the asset. H09 proves the resulting resource distribution is playable.

**Anchors:** `Content/Python/thin_item_clutter.py`, `MOPCG_StampScatter1`, `MOPCGGraphDensityTests.cpp`.

### H03 — Eliminate permanent far-vegetation holes and near ghosts

**Diagnosis:** Viewer-distance filtering during generation permanently omitted instances around the camera's old position. The per-frame draw floor also needs a valid end cull distance to take effect in this engine.

Verify all far points are generated and their descriptors consistently apply the near draw floor/end-distance pair. Read authored graph floors/radii and quality scaling; do not infer visible coverage solely from the generation radius. Keep the shared descriptor helper.

**Acceptance:** Travel 3-4 km from the initial location and look back: no camera-centered bald disc. Approach and retreat through every handoff in both gameplay and the menu flyby: no giant stand-ins beside near trees, obvious density gap, or flickering overlap. Capture motion, not just still frames. Repeat a debug relocation and an ordinary traversal.

**Anchors:** `MOPCGBiomeSpawnerSettings.{h,cpp}`, `MOFramework.PCG.FarTiersDrawFloor`, authored tier nodes.

### H04 — Make canopy appearance survive cooking

**Diagnosis:** The old package lacked the instanced-static-mesh shader permutation for `M_FarCanopy`, so the cooked game selected the default material.

Confirm both creation and refresh paths set the usage flag, save the real asset, and cook it. Audit any material actually showing the same failure before extending the repair elsewhere.

**Acceptance:** Fresh packaged logs contain no canopy usage/default-material warning and distant clumps visibly use the intended dark material. Confirm the editor and packaged views match under the same conditions.

**Anchors:** `Content/Python/make_far_canopy.py`, `Content/Penumbra/FarCanopy/M_FarCanopy.uasset`.

### H05 — Close correctness gates on the actual final package

Confirm the full MOFramework automation result after the final code edit, relevant Widget Blueprint compilation and live UI checks after affected UI work, and the packaged correctness suite. After movement/spawn/water changes, include the relevant multiplayer `game`, `actions`, save/load and churn gates; do not assume older multiplayer results cover the new code. Exercise death recap for both remaining-pawn and last-pawn branches when their flow is affected.

**Acceptance:** Successful BuildCookRun; final binary/cooked-content timestamps; registered/pass/fail/not-run counts and exit codes; current packaged suite and applicable co-op gates; no unexplained crash, ensure, or boot error. Mark art-coverage reports separately: 7/7 correctness is not evidence that art is complete. Avoid re-running broad suites after passing unless a new change or unresolved issue justifies it.

### H06 — Replace incomparable FPS claims with controlled evidence

Use the packaged Development build with one game process and the editor closed. Hold seed, camera/path, resolution, quality, weather, clock, warm-up and duration fixed. Start a fresh process per configuration to avoid retained far cells. Change one variable at a time: field-tree contribution, canopy contribution, item density. Restore normal authored assets after any measurement-only changes.

**Acceptance:** Record at least three comparable runs per configuration, their variation, average/p95/p99 frame times, worst hitches, memory and relevant CPU/render/GPU limits. Re-tour all 11 biomes on the final configuration and repeat a warm-up-plus-soak check. Use the existing reference target of p95 <=16.6 ms and p99 <=25 ms at the stated hardware/settings as a target, not a promise for all GPUs. Explain misses and obtain the intended hardware/quality target before broad scalability retuning. Correct the 79 -> 102 FPS claim in living docs without rewriting historical commits.

**Anchors:** `MO.Perf.Tour`, `MO.Perf.Bench`, `Tools/ov.bat`, perf CSV/log artifacts.

## 4. P1: world correctness and the survival loop

### H07 — Diagnose and close the summit notch/LOD tear

Reproduce on a fresh v7 world near the recorded alpine ridge `(2372000, 1020000)` using `MO.WorldGen.FindPeak 7 5 go`; preserve seed and camera details. Separate cave carving, Voxel mesh/LOD transitions, material masking and missing geometry. The new cave ceiling is an implementation, not proof of the visual diagnosis.

**Acceptance:** Near-to-far approach/retreat recordings show no sky hole or fractured summit; generator and collision probes agree. Run `MOFramework.WorldGen.NoCavesOnHighGround` with a control that truly disables the gate and demonstrably fails the assertion, then restore normal tuning. Inspect the gate's actual disable semantics before assuming `CaveMaxGroundCm=0` is the correct control. Any engine/plugin change needs a demonstrated responsible layer and focused regression evidence.

### H08 — Unify water/cave exclusion across every planting path

Investigate the reported AlpineHeath lake near `(-211333, 1053333)`. Trace native and biome spawners, initial lake availability, the authoritative water oracle, cave masks, and regeneration/cleanup. Distinguish invalid placement from deliberately authored aquatic species; dry-land debris and trees do not belong underwater.

**Acceptance:** Fresh boots and revisits at ocean, upland lake, shore and cave-mouth locations show no unintended flooded vegetation or cave-mouth grass disc. Water audit and placement probes agree with collision/visual water. Test the timing case where PCG and lake actors become ready in different orders; fixes live in the shared sampling/exclusion layer.

### H09 — Prove the first ten minutes after resource thinning

Run main menu -> new game -> tutorial -> gather -> first craft using ordinary input and naturally available resources. Include the 10-cobble tutorial requirement, sticks, bark, flint, inventory transfer, and a recruited pawn gather job. Do not use a starter/debug grant as evidence of resource availability. Test representative spawn seeds, not only the preferred screenshot seed.

**Acceptance:** Record route/time-to-required-resource, reachable counts, tutorial progression, first successful craft, missing instructions and all rough edges. Every simulated action keeps its visible realistic duration and movement interruption. Tune resource distribution or quest expectations using evidence; do not restore a carpet of debris just to satisfy a test. Repeat after physiology changes.

### H10 — Give survival state one authoritative source

**Confirmed diagnosis:** Hunger/thirst/energy moodles subscribe to `UMOSurvivalStatsComponent`, whose defaults decay by 0.5/0.8/0.2 per real second. The physiology/metabolism simulation is a separate model. The HUD therefore reports rapid deprivation independently of realistic state.

Map the full flow: eating/drinking/sleeping -> authoritative physiology -> replicated presentation -> HUD, AI, tutorial and save/load consumers. Recommend adapting the legacy API to the physiological source, preserving compatibility while removing independent depletion. Changing three decay constants or patching HUD labels alone does not resolve the dual-source problem. Existing docs mark pacing as Wes's decision: present the concrete migration/mapping and gameplay effects before changing the agreed pacing. Realism remains the standing design constraint; arcade decay is not an equally compliant default.

**Acceptance:** Idle, exertion, eating, drinking, sleep, possession, save/load and co-op show consistent underlying state and indicators. A healthy idle pawn does not become critically dehydrated merely because two real minutes passed. Document the mapping and justified physiological rates/timebase, and prove consumption is not applied twice.

**Anchors:** `MOSurvivalStatsComponent`, `MOCharacterUIController::BindToPawnSurvivalStatsForMoodles`, `MOMetabolismComponent`, medical/clock contracts.

### H11 — Replace thermal drift with a justified heat balance

The existing backlog reports very rapid cooling and a thermal model without body thermal mass. Reproduce its current behavior first, including air exposure and partial/full immersion; do not generalize a single cooling rate to all bodies or environments.

Design one heat-balance model using justified production/loss terms, body mass/composition, activity/shivering, clothing/wetness, exposure and elapsed simulation time. Reuse the existing environmental provider/water coupling. Establish current primary physiology references and Unreal patterns before new implementation. Resolve the documented thermal-design checkpoint without blocking unrelated fixes.

**Acceptance:** Documented scenarios for dry shelter, cold air, wet clothing and cold water produce justified trends and rates. Clock scaling, timestep variation, authoritative replication, recovery and save/load agree. Skin/cold-shock and core cooling remain distinguishable where relevant. Preserve genuine hazards; do not obtain tolerable rates by hiding indicators or suppressing downstream effects.

### H12 — Verify physiological deterioration reaches the mortality system

The source search during this review found declarations/broadcasts of `OnCardiacArrest` and `OnRespiratoryFailure` without C++ subscribers. Blueprint/native indirect consumers and the whole mortality flow still need inspection before calling this a complete diagnosis. Audit vitals -> mental state -> mortality -> deceased persistence -> death recap/possession/respawn.

**Acceptance:** Verified irreversible scenarios eventually produce one authoritative death; reversible scenarios recover before their applicable deadline; repeated signals do not duplicate corpse/death/respawn work. Multiplayer observes the same death and deceased pawns do not resurrect on load. Use one shared mortality mechanism, not separate death patches in cold, drowning and wounds. Tests must distinguish immediate events from conditions that require sustained failure.

### H13 — Define generator/save compatibility before more generation changes

The Oct 13 handoff reports v7 is hashed/logged without a save compatibility check. Confirm the actual payload and restoration path. Changing cave rules can regenerate terrain under existing construction and sculpt history.

**Acceptance:** A documented policy covers stored generator identity/tuning, mismatch detection, legacy saves, and any migration or clear incompatibility message. Test an old-version save with buildings and terrain edits, a current-version save, and co-op host restoration. Do not silently regenerate incompatible worlds or add compatibility flags with no load-path enforcement. Wes chooses whether pre-alpha saves may break; record that decision explicitly rather than treating the old handoff's "acceptable" note as authorization.

## 5. P2: finish presentation and world quality

### H14 — Establish a current UI screen and behavior inventory

The UI kit doc says several implemented screens are still pending. Audit the assets/classes actually used by the packaged flow, not every similarly named legacy asset. Inventory HUD, inventory, crafting, building, status/medical, skills, knowledge, quest log, possession, colony, death recap, options, main/in-game menus and modals.

**Acceptance:** One matrix per live screen records kit coverage, bindings, real-input behavior, keyboard/gamepad focus, capture artifact and remaining work. Include open/close/reopen, window reactivation, rapid menu replacement, covered stack widgets and modal results. Keep the existing CommonUI lifecycle fixes and C++ input ownership.

### H15 — Finish the remaining Field Ledger surfaces

Use the current inventory from H14 to finish actual gaps: colony bar/overview, death recap presentation, Knowledge content, medical subpages, options/menu/modal layouts and any HUD layouts not yet rebuilt. Inspect all claimed-complete screens at usable full resolution. Preserve the existing shared token/button/row/queue seams.

**Acceptance:** Readable text/contrast and consistent states over bright/dark backgrounds; no placeholders, clipping or dead controls; real mouse, keyboard and gamepad navigation; accessible focus and deliberate warning severity. Finish outstanding font imports only under current download authorization, with a recorded fallback. A four-panel inventory redesign is a separate composition decision, not necessary to finish the existing tabbed screen.

### H16 — Connect missing controls and accessibility settings

Re-run UI contract checks before using the old absent-widget list. Resource-source checkboxes and other controls were subsequently added. Trace each currently absent member to a reachable feature: connect the control, deliberately omit optional functionality, or retire truly dead code after checking Blueprint references. Wire existing accessibility token swaps to saved Options preferences rather than reimplementing styling.

**Acceptance:** Reachable transfer/pickup, close, quest abandon/track, quantity, equipment progress, portrait, wind and nearby-item controls work as intended. Color-blind/high-contrast/reduced-motion/text-scale choices persist and affect existing/new widgets without layout failure. Do not introduce instant pickup/transfer where a timed world action is required.

### H17 — Finish swimming as a complete player interaction

Verify the ordinary wade -> swim -> dive -> fatigue -> recover -> exit loop, input, animation transitions and clothing. Investigate load-dependent buoyancy, fall entry and creature swimming as separate subgoals after the human loop is reliable. Keep physiology, MET/stamina, breath feedback and drowning authoritative.

**Acceptance:** A recorded packaged human loop has coherent water/land transitions and controls, visible breath/action feedback, appropriate recovery and no buried-pawn swimming regression. Co-op agrees on movement/death. Exertion and cooling reflect the chosen physiology, with no teleport/instant recovery shortcut.

### H18 — Verify audible output and finish environmental audio

The double-master-volume and buffer settings were changed, but muted/headless tests cannot verify the original scratchy output report. Use `MO.Audio.Diag`/`MO.Audio.Tone` and an audible scene-transition run. Preserve user settings while diagnosing. Trace the `RecursiveApplyAdjuster` fault; log suppression is not proof it is fixed.

**Acceptance:** Intro/game/menu transitions produce clean audible output; master/category volumes apply once; no recurrent audio fault or spam is hidden by a log threshold. Extend surface-aware footsteps to reachable props/buildings and creatures through shared surface/gait abstractions; then validate indoor/outdoor/weather mixes. Separate device/output problems from mix/content failures using logs and audible evidence.

### H19 — Make snow reflect place and weather

Current snow is a global approximately 1250 m terrain line. Connect it to the generator's spatial climate and the existing weather/season state; model accumulation/melt and slope consistently. A global Material Parameter Collection can carry common weather state but cannot alone represent different local climates across a large world. Add compatible rock/tree snow presentation after the shared environmental model works.

**Acceptance:** Contrasting climates/elevations/seasons show coherent snow coverage and gradual transitions; wetness/temperature state and terrain/props agree; no coarse-LOD checker pattern or sudden material boundary. New biomes/materials consume the shared policy with authored exceptions.

### H20 — Close biome/material and world-richness gaps

Review all ground surfaces for anti-tiling, macro variation, tint defaults and smooth biome/elevation transitions. Check near/mid/far views under fixed lighting. Improve Electric Forest floor patches, Nordic/shore vegetation and distinct sand/rock sets only after diagnosing current repetition. Establish fauna/POI/resource density with a seed survey and ordinary walks; consult `World_Features_Architecture.md` before designing a new kind of world feature.

**Acceptance:** All 11 current biomes remain visually distinct with textured ground, no floating/misaligned slabs, unintended water/cave planting, hard material bands or biome-blind far forests. New surfaces have verified parameters/coverage. Richness improvements have measured spatial distribution and use existing/shared catalogs; additional assets are sourced only under current authorization.

### H21 — Turn art coverage into an explicit production backlog

The old packaged log's informational art reports counted recipes: 150 slots, 130 missing, 11 placeholder; items: 657 slots, 414 missing, 201 placeholder; skills: 23 icons, 23 missing. These are dated slot counts, not a current count of unique assets needed. Re-run the validator and group missing/placeholder slots by reusable asset family and gameplay importance.

**Acceptance:** A current art manifest maps each high-priority item/recipe/skill/building slot to a real asset or an explicit placeholder with owner/next step. Complete first-loop assets first, check alpha/checker backgrounds and cooked rendering, and track measurable burn-down. Keep the rejected Steam art as existing content unless Wes directs removal; store artwork is not evidence that the gameplay art backlog is closed.

## 6. P3: extension, tooling and documentation

### H22 — Complete the already planned pawn excavation loop

Follow `Docs/Terraform_Excavation_Plan.md`; its Oct 9 refresh says stages 1/1b/2/3 landed and 4-7 remain. Re-verify rather than rebuilding the designation/job primitives. Next is Stage 4 settlement-scoped dispatch, then conservative flatten decomposition; Stage 6 UI has an explicit Wes checkpoint and Stage 7 cart is optional.

**Acceptance:** Idle recruited villagers take distinct designated jobs automatically, honor shelter exclusions, move spoil through actual inventory/fill destinations, and conserve estimated volume under realistic timed, interruptible work. Save/load, trip watchdogs, reach/authority checks and repeated passes hold. Decide flatten target height, exhausted-zone lifecycle and UI defaults at the existing design seam. No instant dig/haul or bypass of the simulation.

### H23 — Finish catalog/queue reuse without losing domain behavior

Resume the UI consolidation plan from the latest `Docs/agent/ui/SESSION_STATE.md` and `TOUCH_LOG.md`, not the obsolete master-plan table. Stage 4 is skills/knowledge/quest collection lifecycle. Deferred Stage 3 work includes the survivor-queue adapter and a live building queue surface; building refund semantics are a recorded Wes decision.

**Acceptance:** Stable identity/selection, refresh/delegate lifecycle, domain-specific detail subscriptions and queue cancellation survive pooling, possession and save/load. Shared renderers display state and emit intents; domain adapters perform changes. Adding another queue/catalog does not require copying existing controllers. Record current test totals; do not use the historical 127/84 numbers as today's expected registration counts.

### H24 — Re-verify residual production risks before expanding architecture

Use Graphify and targeted call-flow inspection for high-risk candidates in `PROJECT_STATUS.md`: combat profile lifetime (H7), persistence/world lifecycle (C3), offline crafting policy (H36), authority/reach/rate/class validation, building deposit/refund persistence and pawn restore integrity. Many of these were fixed after the original audit; none is an automatic patch instruction.

**Acceptance:** Each candidate is marked fixed/refuted/reachable defect with current file/line and reproduction evidence. Fix proven defects where the bad state originates and add a meaningful regression/control where appropriate. Preserve GUID identity, save-domain registration, FastArrays and module boundaries. Do not run a speculative wholesale refactor or weaken tests to resolve old labels. Offline crafting must follow the chosen whole-simulation time policy rather than advancing one subsystem independently.

### H25 — Restore reliable captures and reduce iteration cost

The Oct 13 handoff reports window-capture tools returning a 320x180 game image inside a 1920x1080 frame. Verify actual viewport dimensions and repair capture/probing at the common layer. Meanwhile use the already documented HighResShot tools for stills, plus a valid motion-capture path for handoffs.

**Acceptance:** Every delivered capture declares actual pixel/viewport dimensions and shows the intended screen; no enlarged tiny render is presented as full-resolution QA. Real input is foreground-guarded and never targets unrelated applications. Batch C++ work to reduce restarts, keep content-only work in the editor, identify processes by owned PID, and keep performance runs free of competing editor/game processes. Do not delete or restore assets blindly after a crash.

### H26 — Reconcile documents and leave one usable handoff

Update current screen/goal status in `UI_Kit_Implementation.md`, the polish backlog, dated handoff and project-status freshness notes. Preserve historical evidence while correcting misleading current summaries. Clarify that PCG/world/pawn tracks have moved beyond their old "not started" labels. Refresh Graphify after code changes; do not commit `graphify-out/`.

**Acceptance:** Each active goal has one owner/status and links to current evidence; no conflicting done/pending statements without dates. Publish exact packaged build path/revision and remaining decision points. End with a concrete next action so the next agent does not have to reconstruct the session.

## 7. Decisions to surface without stalling the whole list

| Decision | Recommended preparation | Unaffected work |
|---|---|---|
| Survival HUD/pacing migration | Map physiological state, compatibility adapters and tutorial effects (H10); keep realism as the constraint | P0 validation, terrain/water diagnosis, UI inventory |
| Thermal model scale | Reproduce current rates and prepare the shared heat-balance design (H11) | Audio, rendering, resource and UI checks |
| Supported hardware/quality target | Present controlled p95/p99 measurements by tier (H06) | Correctness and visual regression closure |
| Old-world compatibility | Demonstrate current v7 load behavior and propose versioned-save options (H13) | Current-world polish and test tooling |
| Excavation UI/flatten defaults | Follow existing locked sphere/settlement/conservation decisions; prepare remaining defaults (H22) | Stage 4 dispatch backend |
| Building refund semantics / inventory composition | Describe current domain behavior and resulting UI choices (H23/H15) | Other catalog migration and current-screen polish |
| Additional assets / engine hair patch | List concrete missing assets or reproduce the engine issue; use existing local content where possible | Existing assets, shared material fixes, audible verification |

Do not repeatedly ask about choices already answered directly in the current user conversation. This handoff does not reopen locked excavation decisions or previously confirmed policies.

## 8. Verification tools and engineering constraints

Existing commands verified in the CLI/docs; check `--help` and current process ownership before invoking them:

```powershell
git status --short
git log -8 --oneline
git diff --stat
python -m graphify query "MOWorldReadinessSubsystem MOPCGBiomeSpawnerSettings"
python Tools/check_unity_collisions.py
python Tools/ue.py auto --filter MOFramework
python Tools/ue.py nettest packaged --package
python Tools/ue.py nettest packaged-suite
python Tools/ue.py nettest actions
python Tools/ue.py nettest packaged-death
```

`auto` and package/build work require the editor closed. Follow the repository's user-close prompt rule before C++ compilation; do not introduce `--force` as a workaround. In this Codex environment, `python` resolved to Anaconda without the installed Graphify package; a read-only command with `PYTHONPATH=D:/UEProjects/graphify` successfully queried the existing graph. Use the intended interpreter/environment instead of reinstalling tooling blindly.

Use `MO.Readiness.Dump`, `MO.PCG.Census`, `MO.PCG.Rings`, `MO.PCG.MeshSurvey`, `MO.PCG.HideMesh`, `MO.PCG.HideTag`, `MO.PCG.WaterAudit`, `MO.Audio.Diag` and `MO.Audio.Tone` where relevant. Preserve the existing measured `MO.Perf.Tour` protocol; verify arguments before introducing a new benchmark recipe.

Load-bearing constraints:

- Diagnose the producer of bad state; read callers, callees and sibling functions before editing. Two failed speculative patches mean instrument and reproduce.
- Centralize shared behavior; new consumers plug into interfaces/delegates/base models. Fix defaults and distinguish failures in the data layer.
- Preserve realistic duration, visible feedback and movement interruption. Batch repeated gestures with total simulated time or delegate work to pawns. No runtime pause, instant craft/dig, passive pickup or arbitrary HP regeneration.
- CommonUI and `UMOCommonButton` remain standard; C++ owns input. Remove previous delegate bindings before pooled-widget rebinding. Read current UI architecture rather than trusting stale class diagrams.
- CSV edits go through `Tools/ue_csv_utils.py`; JSON exports use `Tools/ue_json_utils.py` and its skill. Live DataTable row edits use the readback-verified safe merge via `Tools/ue.py rows set`; raw partial nested structs can erase omitted fields.
- Voxel access stays behind `MOVoxel`; use current engine signatures and avoid sibling-subsystem initialization dependencies in cook worlds.
- Do not delete rooted material expressions or use the known-crashing Python StaticMeshDescription path. Reuse the existing safe material/OBJ scripts, protect user changes, and inspect current asset state before any structural rebuild.
- For new code, consult current primary Unreal documentation/local engine source as the project requires; small extensions of verified existing patterns need not trigger redundant research.

## 9. Required next handoff format

For the next delivery, report:

1. Goals completed, goals still active, decisions pending, and exact next action.
2. One-sentence diagnoses and why the owning layer was changed.
3. Code/assets touched and existing local changes excluded from the work.
4. Build/revision and exact runnable package path, with timestamps.
5. Test commands, registered/passed/failed/not-run counts, exit codes, and relevant control results.
6. Packaged loading/performance measurements with complete conditions and variation.
7. Player reproduction steps and expected visible behavior, including motion evidence for vegetation/LOD and audible evidence for sound.
8. Known limitations, art debt, save compatibility, and any evidence that remains session-reported rather than independently reproduced.

The immediate next action is to finish Claude's already running full-suite result, cook the follow-up fixes, and prove H01-H04 in the resulting package before treating them as closed.
