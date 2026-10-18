# Audit remediation session state

## Collaboration closeout checkpoint (2026-10-18 01:22 Pacific)

This checkpoint supersedes the old overnight stop/permission state below. Wes directly requested completion of the collaboration; the current canonical rules require scoped commits and pushes for verified work. The expired heartbeat remains paused. Claude confirmed the Steam test ended (CL-0070) and respected the coordinated engine lease CX-0074. Engine/build/editor/instances were released in CX-0077 after every runner exited; no live instance was started this turn and no user editor state was discarded. Read the current cooperation log before another engine operation.

Batch H resolves N-003 by correcting only the no-nav test fixture in `SurvivorReload.ActiveLoadAndPossession`: it checks the persisted return leg without ticking nonexistent navigation before emulating arrival. An additional unreachable-return case deliberately ticks that failure and verifies the assignment fails while the real product stays on its carrier and no remote deposit occurs. The gameplay movement guard, handling durations and simulation policy are unchanged. The previous G failure/report remains preserved.

Current shared-tree MO57Editor build **Succeeded, 45/45 actions**, including Claude's existing uncommitted torch dependency. Focused tests against that binary:

| Filter | Passed (including warnings) | Failed / notRun | Runner time | Evidence under `Saved/AuditBatchH/` |
|---|---|---|---|---|
| MOFramework.Crafting | 16 | 0 / 0 | 26s | crafting-report.json / crafting-tests.log |
| MOFramework.Inventory | 9 | 0 / 0 | 24s | inventory-report.json / inventory-tests.log |
| MOFramework.Equipment | 4 | 0 / 0 | 25s | equipment-report.json / equipment-tests.log |
| MOFramework.Survivor.Iteration | 3 | 0 / 0 | 24s | survivor-iteration-report.json / survivor-iteration-tests.log |
| MOFramework.Survival | 9 | 0 / 0 | 23s | survival-report.json / survival-tests.log |
| MOFramework.Medical | 90 | 0 / 0 | 25s | medical-report.json / medical-tests.log |

Total **131 passed, 0 failed, 0 notRun**; this is six focused filters, not the full project suite. The reports retain all diagnostics (41 cases succeededWithWarnings). Build evidence is `build.log`. Unity collisions=0; scoped whitespace check has no errors. Graphify completed 17,240 nodes / 26,402 edges / 1,183 communities (JSON/HTML size warnings do not indicate failed graph extraction). No new packaged or live-network acceptance was run. Earlier host/joiner medical 12/12 is separate batch E evidence.

The audit publication includes A-G ownership/context/persistence/treatment implementations and regression fixtures as one coupled dependency unit. `MOInventoryComponent` and equipment returns depend on the shared reservation types/service, with queue/job/medical consumers included; publishing a partial equipment-only tree would leave missing dependencies. Durable API and limits now live in TECHNICAL_REFERENCE, Crafting_And_Progression, Medical_And_Survival and Persistence. N-004's dated handoff moved unchanged to `Docs/Archive/` with source/index references; F1's `Survival_Single_Source_Proposal.md` stays live. N-156's SetPause comment now accurately documents the direct GameMode bypass.

Publication complete: **7f73356bd660a687dbe6c1e1daa8da56b5a39306**, pushed to origin/master. Exact owned staged paths checked with a manifest guard; post-push stat confirmed. All audit source and durable system docs RELEASED in CX-0078. Claude's torch source/Light_Sources text, networking work and user changes were excluded. N-002 can now commit `ReduceEquippedDurability`'s caller; corpse persistence N-155 and explicit WorldId N-158 have free source but remain unimplemented future work. Claude owns the diagnosed BR18 server-harvest follow-up (option a), with his new instance-reference seam; the passing foundation gates do not certify those new Steam symptoms fixed. N-001/N-003/N-004/N-156 are complete. The engine window belongs to Claude under CL-0072.

Outstanding acceptance/features: full save-slot and live-network survivor reload; treatment picker, another patient's deliberate wound inspection seam, assigned medic, additional clinical effects/CPR; old saves lacking reservation provenance. No new pacing, heat-ladder data or speculative clinical design was introduced.

## Overnight closeout (2026-10-17 13:39 Pacific Daylight Time)

- The authorized deadline was 2026-10-17 18:42:10 UTC. A late heartbeat arrived at 20:38:49 UTC; no further implementation, build, test, editor or live-row work was performed after that wakeup. Heartbeat mo57-overnight-collaboration is now PAUSED through the app tool. This entry is administrative closeout only.
- Codex releases source claim CX-0061 and engine/build/editor/instances claim CX-0062 via CX-0064. The last build and automation runner both returned completion; no live instances were started for batch G. This releases Codex's claim only and does not certify or change any current user/Claude editor state. Never discard unsaved editor state.
- Batch G survivor craft recovery is implemented and COMPILE-CLEAN at its last gate: shared MO57Editor initial build 51/51; final incremental build 15/15. The last test result is NOT fully green: MOFramework.Crafting = 15 passed / 1 failed / notRun=0, 23s. The first focused SurvivorReload run was 2/2 passed, 66s including Zen startup. Exact evidence: Saved/AuditBatchG/build-initial.log, build-final.log, reload-initial-report.json, reload-initial-tests.log, crafting-first-report.json and crafting-first-tests.log (crafting-report.json/tests.log contain that same latest run).
- G now saves the owning job's phase, remaining handling progress, exact paid craft entry, borrowed vessel GUIDs and actual produced quantities. An opt-in persisted completion receipt survives offline completion before the controller binds. Withdrawal/enqueue/deposit install the saved transition before observers; delivered cargo has a distinct pending-acknowledgement phase, and recording an iteration atomically clears that phase with the count increment. Controller replacement retains pawn-owned work; active loads rebind; displaced pawns return physically; actor GUID hydration can retry. Job cancellation releases only its saved craft ID even without an AI controller. Excavation state-machine hunks were not changed.
- Four of the five new SurvivorReload cases passed in the final broader run: PhasesAndOfflineReceipt, NotificationBoundaries, LateIdentity, DetachedCancellation. ActiveLoadAndPossession failed its final product assertion. Its log shows the fixture ticking an AI 4000uu from storage with no navigable path; the existing unreachable move-leg guard fails the assignment before the test moves the pawn back. That fixture/path setup was diagnosed, but no correction or rerun was completed before shutdown. Do not remove/relax the existing movement failure policy to make this test pass. The product behavior at that remaining acceptance point is not certified.
- Next separately authorized work: correct the no-nav possession fixture (or provide a real navigable movement fixture), rebuild if changed, rerun all Crafting, then Survivor.Iteration and Inventory gates; preserve exact new evidence. Full save-slot and live network survivor reload acceptance remain outstanding. The graph update was launched after the G source edits; its tool session was no longer available at closeout, so final G graph completion/metrics are not certified. Previous confirmed metrics remain recorded below.
- Earlier validated foundations remain: inventory 9/9, equipment 4/4, survival 9/9, survivor iteration 3/3, full Medical 90/90, final Medical.Treatment 9/9; live self-treatment host/joiner 12/12 acceptance steps. These are separate earlier gates, not reruns against G. No full-suite or packaged-game claim. Medical UI, another patient's inspection/data seam, assigned-medic work and additional clinical effects remain outside the implemented foundation; see MEDICAL_HANDOFF.md.
- No staging, commit, push, deployment or external messages by Codex. Claude's cave/geology/excavation work and uncommitted carried-light files remain preserved; the equipment/torch commit dependency needs explicit user authorization. No speculative simulation/pacing or DataTable changes by Codex.

## Current checkpoint (2026-10-17 05:02 Pacific)

- A/B/C/D foundational fixes remain implemented. Latest focused gates: Medical treatment 9/9, all Medical 90/90, Crafting 11/11, Survivor.Iteration 3/3, Light prefix 8/8, Fire 1/1; all notRun=0. Earlier unchanged Inventory 9/9, Survival 9/9 and Equipment 4/4 evidence remains in batches C/D. No full-suite or packaged-game claim.
- Engine/build/editor/instances window RELEASED by Codex CX-0060. All owned auditmedhost/auditmedclient and build/test runners exited; inst list empty. Read the cooperation log for any newer claim before engine work. Never discard unsaved editor state.
- E native timed medical service + owning RPC + console entry are compiled and validated. Full Medical group = 90/90 (25s); final Medical.Treatment = 9/9 (23s), including the live-observed clock-skew query regression. Final shared incremental build = 6/6. First compiler error, successful repairs, all reports and logs preserved in Saved/AuditBatchE.
- Real host/joiner SELF-treatment acceptance PASSED 12/12 steps. The owner received its real wound/supply and active replicated treatment state/label; unknown ID reached the server and was refused; ApplyBandage used its authored 15s server interval; no early effect/debit; one bandage was spent and bleed .200 -> .186 agreed on owner and authority. Captured wall time includes probe/bridge overhead and is not a calibrated 15s wall-time claim. Both test instances stopped cleanly. First failed attempt was protected-property probe access before any treatment request, preserved separately. See live-medical-result.json and auditmedhost/client.log.
- The live capture also showed client remaining time 16.759s for a 15s course while its initial server clock lagged. The shared query now projects from bounded GetProgress01; no duration/timer/simulation change. Controlled clock-skew regression passed in the final 9/9 run. The complete live flow was captured before this readonly query correction; it was not rerun a third time.
- C3 repeat-count follow-up is compiled and validated: the owning job entry records each real iteration, saves it, and marks it dirty for replication. Gather/craft share the acknowledgement, refuse wrong identity/client/reentrancy, and stable-ID cleanup cannot remove the successor. State/progress item dirtiness fixed at their mutators. Survivor.Iteration 3/3 and Crafting 11/11 include serialized count, bounded acknowledgement and two full controller-flow regressions. No excavation state-machine edits.
- Medical limits remain explicit: existing clinical mutation implements bleed/infection reductions, not HealingSpeedBonus/PainReduction or treatment-name-based bandage/suture/splint flags. Condition-only rows/CPR, treatment selection UI, joining-client inspection of another patient's owner-only wound data and assigned-medic jobs are not implemented/claimed. Required current treatment/supply definitions were verified directly; no DataTable asset changed. MEDICAL_HANDOFF.md records the request seam and remaining acceptance.
- Next authorized source work: exact survivor craft-leg/owned queue receipt/borrowed-cargo recovery across reload. Current count fix does not claim full phase resume; diagnosis and required save points are recorded below. Reclaim source files explicitly, preserve Claude's excavation band, and wait for a new free engine lease before validation.
- Graphify refreshed 16,679 nodes / 25,626 edges / 1,190 communities; unity collisions=0 and scoped whitespace clean. No staging/commit/push by Codex. Claude's torch h/cpp and Light_Sources.md intentionally remain uncommitted alongside their equipment API dependency; retain both until user-authorized Git work.

Updated: 2026-10-16. Coordination: `claude-codex-coop.md`, CL-0025 through CL-0028 and CX-0023 onward.

## Completed implementation: batches A and B

Audit reference: `Docs/Deep_Dive_Top_10_Flaws_2026-10-16.md`. Findings are historical; this file records subsequent remediation.

| Audit rank | Diagnosis | Implementation and evidence |
|---|---|---|
| 1 | Untrusted equipment slot bytes reached array indexing. | One slot validator is used at the request boundary and throughout equipment mutators/readers. Invalid bytes are refused; the natural-slot sentinel remains valid only for Equip. Regression checks all 256 byte values and malformed request/save paths. |
| 2 | A delayed hand swap had duplicate ownership of the old item and unpersisted ownership of the incoming item. | Pending swaps own incoming items explicitly. Equipped ownership is cleared before the old item returns to inventory. Timers, identities, durability and liquid contents serialize; conflicting slot operations are refused. Both hands and legacy saves are covered. RepNotify dispatches equipment/swap events to clients. |
| 4 | Bulk stack operations dirtied the array but not changed surviving FastArray items. | Every changed source/target/surviving entry is marked individually. Array dirtiness handles removals. Consolidation and fill respect instance state, refuse self-fill, and require authority. Tests require both surviving replication keys to advance while conserving quantities. |
| 5 | Add variants created entries without enforcing available slots. | `TryAddItem` owns identity/state validation, stacking, stack limits and capacity. It returns requested/accepted quantities and a failure enum. Existing bool APIs require complete acceptance. Partial acceptance is explicit. Slots are reserved by entries even when auto-assignment is temporarily disabled. `TryTransferItem` refuses a full destination without removing the source, preserves identity/state, and defers callbacks until both inventories are updated. NoStack splits large requests rather than truncating them. |

Shared integration:
- Pickups install carried state and deactivate the world source before inventory callbacks.
- Splitting creates the destination before removing source units; failed placement leaves the source unchanged. Split state is preserved.
- Timed harvest checks the combined output set before destroying/depleting a node. HISM pickup checks capacity before removal. Crafting uses the placed-quantity contract; the queue's fallback reports placed/failed/spilled output instead of declaring every add successful.
- `FMOInventoryMutationScope` coalesces notifications around compound changes. It is a notification scope, not a rollback mechanism.
- Restored inventory entries receive individual FastArray identities, and load notifications wait until slots are restored.
- Equipment return/rollback uses the common inventory contract and notification scope.

## Validation

Final shared-tree `MO57Editor` Development build: **Succeeded, 45/45 actions**. This includes Claude's concurrent cave/geology source at build time; it is not an isolated Codex-only build.

Final focused gates against that binary:
- `python Tools/ue.py auto --filter MOFramework.Inventory`: **5 passed, 0 failed, notRun=0**, 24 seconds. Capacity, LiquidIngredient, SlotInvariant, StackReplication, TransferConservation.
- `python Tools/ue.py auto --filter MOFramework.Equipment`: **3 passed, 0 failed, notRun=0**, 24 seconds. InvalidSlots, PendingSave, SwapConservation.
- `python Tools/check_unity_collisions.py`: **0 collisions**.
- Scoped `git diff --check`: no whitespace errors (Git reports ordinary LF/CRLF conversion notices).

Evidence: `Saved/AuditBatchB/inventory-report.json`, `inventory-tests.log`, `equipment-report.json`, `equipment-tests.log`.

Earlier full-suite attempt: **INCOMPLETE**. Its log records **232 successful completed tests and 3 failures** (WorldGen.CaveEntrances, CaveGoldenChecksum, Caves). Stopped in WorldGen.GalleriesStayLevel after more than five minutes without another completion, against the intermediate limestone binary. Claude acknowledged ownership of those cave failures in CL-0028 and had already changed the source for his next validation. No full-suite green claim is made. Preserved: `Saved/AuditBatchA/full-suite-partial.log`.

Limits: no live two-client replication session or packaged build was run. The new tests exercise server logic, real reflected save serialization and FastArray mutation keys; they do not prove live client rendering or compatibility with every existing world save. Existing overflowing legacy saves are preserved rather than silently deleting items. Ordinary crafted output that cannot be placed is reported as failed; durable overflow output storage/world placement remains a follow-on concern for the crafting transaction work.

## Reproduce the acceptance checks

Keep Unreal Editor closed, then run from the project root:

```powershell
python Tools/ue.py auto --filter MOFramework.Inventory
python Tools/ue.py auto --filter MOFramework.Equipment
```

Expected: 5/5 inventory and 3/3 equipment, no failures or unrun tests. Capacity refuses an oversized add without changing the existing partial stack; explicit partial acceptance returns the exact accepted count. Filling existing stacks advances replication keys on both surviving entries. A full-target transfer leaves both inventories unchanged. A serialized save made during simultaneous hand swaps restores both original item identities, wear, contents and remaining durations.

Manual co-op follow-up: fill a joiner's container, attempt a transfer, and verify the source quantity stays unchanged and no invisible entry appears. Leave both a source and target stack partially filled, use Fill Stacks, and verify both machines show the same quantities. Swap held tools, attempt unequip during the visible delay, and save/load during that delay; the incoming tool must have one owner and finish only after its saved remaining duration. These live client checks are still outstanding; do not label them passed from the headless tests.

## Overnight continuation (authorized directly by Wes)

Window: 2026-10-17 06:42:10 UTC through 18:42:10 UTC (11:42 PM Oct 16 to 11:42 AM Oct 17 Pacific). Heartbeat `mo57-overnight-collaboration` resumes this thread every 15 minutes through the end time; failure-only notifications. Authorization covers continued implementation, tests and coordination with Claude. It does not add Git staging/commit/push permission.

C1 source prepared under CX-0031/CX-0032, **not compiled or tested yet**. Claude holds editor/build window CL-0029; do not run build/auto/editor/rows until his RELEASE. No C1 completion/pass claim yet.

C1 changes: new MOCraftingExecutionContext.h with distinct failure reasons; subsystem resolves real station identity/type, completion, 4m reach, line of sight, active/fuel state and achievable temperature. Queue binds station GUID per entry and carries it through save/load. Harvest/build recipes and invalid durations are refused. Context loss suspends progress and retries its original station; queue rows show the reason. New owning-server cancel RPCs ignore client refund policy and guard local callspace after disconnect. Cancellation removes queue ownership before refund callbacks. Legacy ExecuteCraft now enqueues timed work; menu fallback no longer instantly creates goods. Survivor crafting enqueue passes its real CraftStationActor instead of an enum fallback; no excavation hunks touched.

New tests: MOFramework.Crafting.ExecutionContext, StationBinding, CancelTransport (Private/Tests/MOCraftingContextTests.cpp). First gate when the window is released: build MO57Editor, run MOFramework.Crafting, then Inventory and Equipment regression gates. Preserve reports before the next run overwrites the temp location. Use exact hunks when touching MOSurvivorController.cpp.

Temperature capability fields default to zero; old recipes carry no minimum until authored. This supplies the authoritative capability contract and synthetic threshold regression, not a completed real heat/fuel/air/warm-up simulation or approved progression rebalance. Read Docs/Technology_History.md before further heat work.

Before moving to #10/#9: review C1 load/resume and cancellation behavior from actual test output. Source-only checks: 0 unity collisions; scoped diff whitespace clean. Graphify refreshed after C1 source preparation: 16,230 nodes / 24,860 edges / 1,161 communities. Refresh again after any validation repairs.

## C2 reservation foundation prepared (2026-10-17 00:06 Pacific)

Source claim CX-0035; implementation is **uncompiled and untested** while Claude holds CL-0029. Graphify refresh completed after this source slice: 16,292 nodes / 24,969 edges / 1,178 communities. New files: Public/MOIngredientReservation.h, Private/MOIngredientReservation.cpp, Private/Tests/MOIngredientReservationTests.cpp.

The pure planner works on entry snapshots and a definition lookup callback. `Reserve` jointly allocates physical items and liquid, handles safe-water requirements first, and commits neither source nor reservation on missing input. Physical lots keep their GUID (new identity only when split from a surviving source stack), durability and contents. Drawn lots keep origin-container reference, original measure size and the actual liquid profile/volume. `Consume` atomically removes one repeat's captured requirements without rereading live recipe definitions. `Return` puts back compatible lots and preserves every unplaced lot in escrow. Unknown/corrupt/colliding records retain distinct failure reasons instead of being silently dropped. A float tolerance never creates extra water.

Inventory adapters now expose ReserveIngredients, ReturnReservedIngredients and TransferIngredientsTo. Transfers plan both inventories before committing; capacity refusal leaves both unchanged. Commit marks every changed surviving FastArray entry, removes dangling slot references, assigns new slots and defers observer callbacks. Stack limit and stack-state comparison are shared with normal inventory adds. ConsumeIngredient delegates to the same planner. Existing 250 ml measure behavior is retained, with tolerance bounded for smaller modded measures and liquid counts limited to actual containers.

New test paths: MOFramework.Inventory.Reservation.Atomic / Refund / Transfer. Static review also separated the two station fixtures spatially so the station-binding test cannot fail from fixtures occluding each other. They cover joint capacity/availability, shared safe/unsafe reservoirs, consumed/refunded quantities, salts/source/boiled state, original physical identity/wear, pending refund persistence, and atomic physical/liquid transfer plus source/target FastArray keys. No runtime gate has run. First released-window gate remains: MO57Editor build; MOFramework.Crafting, MOFramework.Inventory, then MOFramework.Equipment. Preserve reports under Saved/AuditBatchC before each tool auto run overwrites its temp output.

Do not mark #10/#9 fixed from this foundation: the queue still uses generic quantity refunds until wired to owned lots. Next integration must capture the original per-craft requirements and reservation on each entry, consume one repeat BEFORE external output/reward callbacks, retain/persist undelivered refund and product lots, and make reentrant saves see complete ownership. Old saves lack provenance; handle them explicitly without claiming it can be recovered. Survivor TransferCraftIngredients should then call the shared atomic inventory transfer, using exact non-excavation hunks only. Also inspect DepositCraftOutputs, which currently removes before a capacity-checked add and only counts physical water.

## C2 queue ownership integration prepared (2026-10-17 00:25 Pacific)

This supersedes the foundation-only next step above. Claims CX-0038/CX-0039/CX-0040. The current C1/C2 tree is **not yet built or runtime-tested by Codex**. Claude reports in CL-0030 that an earlier shared tree compiled and his WorldGen 43/43 plus related geology/data gates passed; that predates at least part of the new queue integration and is not evidence for this slice. He released CL-0029, then claimed CL-0031 for icon studio/editor work. CX-0040 requests a safe yield and RELEASE; do not build or run auto/editor/rows before that release.

New entries capture combined original per-craft requirement counts (checked int64 arithmetic before int32 storage) and exact owned lots for every repeat. Enqueue reserves jointly under a notification scope. Completion privately plans outputs, consumes one repeat from the captured lots, advances/removes the entry and installs persistent product ownership before rewards or completion callbacks. A synchronous observer can save/cancel without refunding the completed repeat. Progress observers cannot apply an old entry's elapsed completion to a newly selected entry. Reordering the active/waiting front is refused.

Cancellation moves every unconsumed lot into PendingReturns before observers. Partial return or missing vessel retains the exact remainder; PendingProducts likewise persists every undeliverable product. Delivery retries on the component tick even with timed crafting paused, and is exposed as a native/BP retry method. Undelivered products suspend following work. Owner-only replicated material counts and queue status rows keep recovery visible after the timed queue empties. FastArray PostReplicatedReceive and status RepNotifies rebuild client rows from applied state. Live two-client UI behavior remains unverified.

Legacy saves without captured provenance may finish their already-paid timed crafts. Cancellation retains the original legacy entry in UnresolvedLegacyRefunds and visibly reports recovery needed. It never fabricates a refund from a changed recipe or generic water. These records cannot recover details that were never saved; manual migration/recovery policy remains a limitation. ClearQueue is still the explicitly destructive no-refund reset API; no production caller was found.

Fresh product construction is centralized in MOIngredientPlans::CreateProducts and used by AddIngredient and the private queued-output planner. Captured liquid refunds require exact compatible profiles; fresh products retain the previous same-safety mixing policy. The inherited fresh-water product defaults are preserved, not a new boiling/distillation chemistry model: no claimed salts-conserving transformation of recipe inputs. ProduceOutputsOnly's public compatibility signature now requests timed reserved queue work; private prepare/finalize helpers handle actual completion. No source outside the claimed crafting/inventory set, no DataTable/Blueprint assets, and no Git staging/commit/push by Codex.

Added queue gates in MOCraftingContextTests.cpp: ReservedRefund (changed recipe, exact saline/boiled contents and worn-item GUID, full-inventory remainder + reflected save), CompletionOwnership (synchronous completion observer saves/cancels; only remaining input refunds; completed output survives), LegacyRefund and ReservationAtomicity (overlapping safe/any water, duplicate ingredient rows, checked oversized counts). New private reflected MOCraftingTransactionProbe.h supplies the synchronous observer. Together with C1 there are seven new Crafting gates; Inventory has five prior + three reservation gates; Equipment has three prior. **None of these current C gates has run yet.** Unity collision scan = 0; scoped diff whitespace clean (ordinary LF/CRLF conversion notices only). Graphify refresh completed after this slice: 16,334 nodes / 25,027 edges / 1,169 communities.

Next: claim the released build/editor window, rebuild MO57Editor, run Crafting then Inventory then Equipment, preserving reports under Saved/AuditBatchC before each auto run overwrites temp files. Repair actual failures before marking #3/#8/#10 complete. Then claim exact survivor crafting/logistics hunks for #9 and use TransferIngredientsTo (including liquid-vessel logistics and safe output deposits); leave Claude's excavation band alone. D and timed treatment follow. Do not author heat progression data merely because capability fields exist; CX-0040 asks Claude to wait for passed context gates and an agreed existing-station mapping.

### Next-slice diagnostics (source read only)

Survivor TransferCraftIngredients (~1821) currently checks physical HasItem for each row, then removes/adds with fresh GUIDs and ignores capacity refusal. DepositCraftOutputs (~1862) likewise removes before add and counts only physical item quantities, so container water cannot fund or complete the route. Shared TransferIngredientsTo fixes atomic physical/liquid movement when the destination has a suitable vessel; the actual pawn logistics must also account for transporting a vessel when it has none. Avoid a liquid-only API substitution that still leaves every unequipped pawn unable to haul water.

UpdateCraftJobExecution has an unconditional 120-real-second watchdog despite waiting for the recipe's real crafting queue in state 13. It can fail a valid longer recipe and free the job slot while that craft continues. The next #9 slice should track the specific queued EntryId and distinguish real movement/no-progress failures from valid timed work. Do not shorten recipe durations or globally remove travel stall detection to hide this. Deposit also currently marks job completion even if placement failed. These paths are read/diagnosed, not fixed yet; claim exact non-excavation hunks first.

D diagnostic: active meters live in MOSurvivalStatsComponent (legacy name, still driving HUD/AI); constructor sets the approved Oct-15 game-clock pacing. MOPersistenceSubsystem pawn capture currently records physiology/skills/equipment etc., without this component. Read its full capture/restore path and MOSaveGameTypes before adding a valid-data-versioned snapshot; preserve existing rates and distinguish older saves that never contained the meters. No D edits made yet.

## Batch C validated + D/equipment source in progress (2026-10-17 03:56 Pacific)

Current window: CX-0045 CLAIM, following Claude CL-0044 RELEASE. Codex holds build/editor/instances until an explicit next RELEASE. Claude is source-only; his new GeologyItemsExist test compiled in the shared build. His carried-light/geology/Blueprint/cheat edits were not changed by Codex.

The current C implementation passed its focused runtime gates after a real defect was corrected:
- MOFramework.Crafting: **9 passed, 0 failed, notRun=0**, 28s. The prior seven gates plus SurvivorLogistics and SurvivorOwnership.
- MOFramework.Inventory: **9 passed, 0 failed, notRun=0**, 28s. Five prior gates plus reservation Atomic/Refund/Transfer/ContainerTransport.
- MOFramework.Equipment: **3 passed, 0 failed, notRun=0**, 28s.
- Shared MO57Editor build succeeded (final test-fixture repair build 6/6 actions). All reports/logs are under Saved/AuditBatchC, including build.log.

First inventory gate was **8 pass / 1 fail** in Reservation.Transfer. Root cause: Unreal's FFastArraySerializerItem copy constructor deliberately resets serializer identity/key. Pure snapshot planning was correct semantically, but InstallIngredientPlan replaced live entries with those reset copies. The commit now restores each surviving live entry's ID/key before marking changed ones dirty. The regression requires both changed containers' keys to advance, their IDs to remain stable, and an unchanged surviving container to retain its ID/key. The original failed report/log are preserved as inventory-first-report.json / inventory-first-tests.log. Two subsequent compile attempts exposed test-fixture argument order/protected-tick access errors; fixed fixture calls to the declared EnqueueCraftJob signature and public AActor virtual interface, then rebuilt successfully. No compiler errors remain in the validated C tree.

C3 integration (CX-0042/CX-0043): TransferIngredientsTo now supports explicit intact source-vessel transport and additional borrowed cargo. Pure planners validate the complete source/destination plans before either inventory changes. Borrowed vessels cannot substitute for physical outputs of the same definition. Survivor withdrawal/deposit uses that service, actual OnCraftCompleted quantities, and an exact owned queue EntryId. A full destination retains all cargo and the job retries after its real handling interval. The unconditional 120s whole-job watchdog was removed; existing movement/no-progress watchdogs and craft/handling durations are unchanged. Cleanup cancels only owned work. New behavior gate moves a real vessel and ingredients through the AI legs, checks >120s work stays valid, completes via persisted elapsed progress, refuses a newly full storage, then verifies actual product plus original worn vessel/remaining water return. Busy assignment refusal preserves unrelated queued work. No excavation hunks touched.

Limits: headless behavior/serialization/serializer-metadata gates passed; no live client UI or full save-slot survivor phase-resumption acceptance yet. AI controller leg/cargo scratch is still transient; existing job save/load restarts jobs rather than persisting the whole haul phase. Materials retain ownership in inventories/queue reservations even when a job is aborted; no remote teleport of borrowed vessels back to storage. Inherited fresh-water product defaults remain, so this is not a new chemistry model for boiling/distillation. No claim that unavailable legacy liquid provenance has been recovered.

D source claim CX-0046: MOSurvivalStatsComponent snapshot has a valid/version marker, six current meter values and nutrition. Pawn capture and shared live-load/possession-spawn component restore include it. Authority validation rejects corrupt/nonfinite/missing data before any mutation; all fields install before observers. Current values clamp to the owning class's limits; class-authored limits/rates/configuration are preserved. Three new Survival.Persistence RoundTrip/Validation/Pacing gates serialize through the actual pawn-record struct and check unchanged game-clock pacing. **D is in its first build, unvalidated so far.** No physiology-adapter/pacing changes.

Equipment support claim CX-0047 (Claude CL-0034/CL-0037 request): ReduceEquippedDurability added with authority/range/amount/swap/reentrancy guards; infinite durability refuses, depletion clears ownership and emits equipment change, ordinary wear uses a distinct delegate. RepNotify compares previous/current slots so replicated wear also avoids full mesh-change notifications. New Equipment.Durability gate covers server/client refusal, invalid bytes, pending swap, ordinary/server and replicated wear events, snapshot retention, infinite durability and depletion. **In first build with D, not tested yet.** Claude's torch component remains untouched; post a validated API handoff before asking him to consume it.

Next immediate work: finish D/equipment build and focused gates, preserve reports under Saved/AuditBatchD, update graphify, release the engine window to Claude for his torch wear integration. Then continue the authorized timed medical service in source while he validates. No staging/commit/push by Codex.

## D and equipment support validated (2026-10-17 04:01 Pacific)

Shared MO57Editor build succeeded **42/42**. Survival gate: **9 passed, 0 failed, notRun=0, 82s** (includes the three new Persistence.RoundTrip/Validation/Pacing gates and existing survival checks). Equipment gate: **4 passed, 0 failed, notRun=0, 26s**, including the new Durability gate. Evidence is Saved/AuditBatchD/{build.log,survival-report.json,survival-tests.log,equipment-report.json,equipment-tests.log}.

First equipment attempt produced no report and exited 3221226505 during frame-0 AssetRegistry startup with an array-changed-during-ranged-for ensure in UnrealEditor-AssetRegistry.dll. No automation test began. Preserved equipment-startup-crash.log; one identical retry passed all four gates. This is not silently relabeled as a passed run or a product-code fix. The full suite and live network wear remain untested by Codex.

D snapshot restores six current meters and nutrition from the pawn-record field through the shared component restore used by world load and possession-menu spawning. It does not alter configured maxima, rates or the approved game-clock pacing, replay consumption, or advance an extra offline interval. Legacy snapshots without a valid-data marker do not overwrite defaults; invalid/nonfinite/unsupported payloads refuse atomically. This closes audit #6 within the meter-persistence scope; the separate physiology presentation-adapter proposal remains a design decision.

Equipment support API handoff is in CX-0048. Server-side finite equipped wear and simulated RepNotify semantics passed; ordinary wear has a separate delegate, depletion removes ownership and broadcasts the full equipment change. Pending-swap items cannot be worn early. Claude owns wiring this into his torch burn component. No torch/lighting/geology/Blueprint edits by Codex. C's prior Crafting 9/9 and Inventory 9/9 results remain recorded above. No staging/commit/push.

Medical next-step trace: graph query identified existing UMOTimedActionComponent in MOFramework (game-clock duration, replicated visible progress, server completion/cancel callbacks, shared interruption bus and drift cancellation). Use that action system rather than adding another timer/component. Medical definitions are FMOMedicalTreatmentRow in MOFrameworkCore/Public/MOBodyPartDefinitionRow.h; anatomy/provider classes are in MOFrameworkMedical. ApplyTreatment currently has no gameplay caller, uses fallback effects for unknown IDs, and never checks TreatsWoundTypes or supplies/duration. It also lacks a per-treatment application record, so repeated calls can stack its multipliers. Read the complete wound/medical definition/provider flow and actual required-item roles before designing #7; keep clinical calculation in its owning medical layer and transport/material/timing in the shared action/service layer. A console verb should call the real owned server request. Player UI still awaits an agreed editor window and Wes's review.

## Remaining assigned work

Follow CL-0025 order, claiming files in the append-only log before editing:
1. Batch C #3: derive station/harvest context on the server from real usable targets. Reject harvest recipes at the ordinary crafting queue. Validate at enqueue, start/resume, progress/completion and load/offline advancement. Do not trust the client station enum. Persist per-entry station identity; an unrelated menu must not retarget earlier queued work.
2. Batch C #8: route cancel-one and cancel-all intents through owning server RPCs, with authority guards even in disconnected/local callspace. Do not accept client-selected refund policy.
3. Batch C #10: reserve ingredient lots with original physical/liquid state, persist those lots, consume per completed repeat, and refund exact remaining lots. Do not recreate pond water from a recipe ID/quantity. Refund/output overflow must retain ownership until delivered.
4. Batch C #9: use that same reservation/ingredient-transfer service for survivor `TransferCraftIngredients`; physical item counting/removal cannot represent water in containers.
5. Batch D #6: persist and restore survival hunger/thirst/energy meters. Keep existing pacing; physiology adaptation remains a separate design choice.
6. Feature #7: authoritative timed/supplied treatment service, owning server request, console verb and tests against medical definitions. Player UI waits for an agreed editor window and Wes's review.

Read `Docs/Technology_History.md` before station design. CL-0027 asks the authoritative station context to carry achievable temperature; actual fuel/air/temperature progression data is a design proposal, not implemented policy. Heat emission to nearby pawns is not the furnace's process temperature.

## Ownership and safety

Graphify refresh completed successfully: 16,168 nodes, 24,754 edges, 1,170 communities. Used the project Store Python runtime; the default Anaconda interpreter lacks tree-sitter. The regenerable graph remains gitignored.

No files staged, committed or pushed. User settings/cache edits and Claude's cave/geology/excavation files are untouched by Codex. In `MOSurvivorController.cpp`, Claude owns the ExcavateAndHaul band; future #9 edits must use exact hunks only. No DataTable/Blueprint assets changed by Codex. Build/editor window release is recorded in the cooperation log; re-read its tail before any next build, headless runner, editor or row operation.

Checkpoint CX-0049: engine/source window released. GeologyItemsExist 1/1 passed (26s), report/log in Saved/AuditBatchD. Graphify complete: 16,534 nodes / 25,350 edges / 1,195 communities. C/D/equipment source compile-clean. Next #7 medical source claim and full-flow diagnosis before edits.


## Source-only follow-up diagnosis (2026-10-17, after E preparation)

Medical handoff: `Docs/agent/audit/MEDICAL_HANDOFF.md`. Nine cases are prepared, none run by Codex yet. Owner-only wound replication at `UMOAnatomyComponent::GetLifetimeReplicatedProps` means another patient's wound GUIDs cannot be assumed available on a joining client. A reviewed medical inspection/request flow is needed for that UI; the native server/AI service and owning request exist, but this client acceptance is not claimed. No replication policy was changed.

Survivor C3 persistence residual is now traced more precisely (source diagnosis, not an executed failure):
- `UMOSurvivorJobQueueComponent::BuildSaveData` copies JobQueue.Jobs. Its entry saves coarse state/progress/count and actor GUIDs, but not the controller's SimpleJobState, CraftQueueEntryId, CraftBorrowedContainers, CraftCompletedProducts or completed-craft acknowledgement.
- `AMOSurvivorController::ProcessNextJob` copies the restored job, marks it Active, and enters StartSimpleJobExecution. CraftAtStation enters StartCraftJobExecution, which begins at storage withdrawal and refuses a nonempty pawn craft queue. Consequently load during a real craft can fail the assignment while its restored queue retains ownership and continues independently; load during a hauling/deposit phase loses the correlation with the existing cargo. Do not claim exact phase resume or successful cargo return from mere lot conservation.
- Both generic gathering and crafting completion increment only `AMOSurvivorController::CurrentJob.CompletedCount`; the persisted/replicated job entry is not advanced per iteration. `UMOSurvivorJobQueueComponent::CompleteCurrentIteration` exists but is not called by these paths. Saving between repeats can therefore restore a stale count. A fix must use the owning job component and stable job identity; simply calling CompleteCurrentIteration before CompleteSimpleJob risks removing the next job twice.
- Required follow-up acceptance after the medical gate: save/load after withdrawal, during station work, after production before deposit, while deposit is capacity-blocked, and between repetitions. Verify exactly the requested number of outputs, original borrowed vessel return, no second withdrawal for already-owned cargo, one owned queue entry, and identical repeat count on server/save/client. Include offline completion before the controller rebinds, where an event-only completion callback is insufficient. No implementation change to survivor/controller files was made in this diagnostic pass.


## C3 repeat-count follow-up prepared (CX-0054, not yet compiled/tested)

`RecordCompletedIteration(JobId)` now records a completed real interval on the owning job component and returns a typed result/count. It refuses clients, another job identity, invalid/exhausted counts and reentrant acknowledgement. The gather and craft controller branches use one shared acknowledgement helper rather than incrementing their transient copies. Saved data therefore captures the owning count; the changed FastArray item is marked dirty. Existing job state/progress mutators now mark that same item too. Cleanup checks its captured job identity so a cancellation notification cannot remove the successor. A guard prevents recursive simple-job Tick while observer callbacks execute. No excavation state-machine code was changed.

Prepared regression gates: Survivor.Iteration.Persistence (real reflected save round trip and item dirty keys), AuthorityAndIdentity (including count bound), ReentrantCancellation; Crafting.SurvivorRepeatSave (two complete craft/haul/deposit cycles and saved owning count between them), Crafting.SurvivorIterationCancellation (cancel during count notification, retain the successor assignment). These are source-only pending the current build/test gate. They do not solve or claim exact craft phase/receipt recovery across reload.

Claude CL-0047 reports his own live torch validation: durability 90 -> 88 in about 70s, burnout emptied the hand with notification, and co-op carried-light checks 5/5. This is Claude's evidence, not a Codex live test. His torch sources/Light_Sources.md deliberately remain uncommitted because they depend on Codex's uncommitted equipment API. Codex has no authority to stage/commit/push; preserve both sides and report this dependency at handoff.


## E/F validation checkpoint (CX-0057)

First shared build planned 53 actions but failed in the new TreatmentStatus console declaration: const GetPawn returns TObjectPtr in UE5.8, so auto pointer deduction failed. Changed the declaration to explicit const APawn*. Retained build-first.log; incremental repair build succeeded 5/5 (build-success.log). All treatment/counter/crafting gates below then passed. A later console-only rebuild also succeeded 5/5 (build-console-dispatch.log): the request now runs on the next game tick, outside the editor script guard that forces RPC callspace local. The engine source was checked; this follows the existing proven Item.Do / Fire.Do submission pattern.

- Medical.Treatment: 9 passed, 0 failed, notRun=0, 23s. Report/log Saved/AuditBatchE/medical-treatment-*.
- Survivor.Iteration: 3 passed, 0 failed, notRun=0, 23s. Saved/AuditBatchF/survivor-iteration-*.
- Crafting: 11 passed, 0 failed, notRun=0, 23s. Saved/AuditBatchF/crafting-*. Includes both new complete controller-flow gates.
- Medical entire group against final console-dispatch binary: 90 passed, 0 failed, notRun=0, 25s. Saved/AuditBatchE/medical-all-*.
- Light prefix: 8 passed, 0 failed, notRun=0, 23s. Saved/AuditBatchE/light-*.
- Fire: 1 passed, 0 failed, notRun=0, 23s. Saved/AuditBatchE/fire-*.

The live requirement gate logs real authored rows: ApplyBandage 15s/min0, ApplyTourniquet 8s/min1, SutureLaceration 60s/min3, CleanWound 20s/min0 (250ml clean-water measure), ApplySplint 45s/min2, ReduceDislocation 30s/min2, TreatBurn 25s/min1, TreatFrostbite 90s/min1, ApplyPainkiller 5s/min0; skill Medicine. Required physical definitions are present and bIsTool=false; no speculative content changes were made.

The repeat-count follow-up is now compiled and validated; it fixes the stale persisted count and per-item state/progress replication dirtiness. The earlier source-only diagnosis remains historical. Exact leg/receipt/cargo correlation across load is still outstanding and has not been claimed by the count tests. The medical live harness is Saved/AuditBatchE/medical_net_acceptance.py; it drives fresh owned auditmedhost/auditmedclient processes, never saves a slot, and writes its result/logs after stopping them. No network result yet.


## Live medical probe attempt 1 and diagnosed retry (CX-0058)

The first live harness booted fresh silent host/client instances, hosted/joined the real gameplay map, and created the intended wound and one bandage on the server's remote-player pawn. It stopped BEFORE any treatment RPC because editor Python refuses to read protected MOTimedActionComponent.State. This is a test-probe access failure, not evidence of a failed treatment request. Full first report/log/bridge traceback are retained as live-medical-first-result.json, auditmedhost-first.log, auditmedclient-first.log and auditmedclient-first-bridge.txt in Saved/AuditBatchE. Both owned instances were stopped cleanly; ue.py inst list reported no instances before rebuilding.

Added readonly native state output to the existing MO.Medical.TreatmentStatus progress verb, using the public C++ GetState/GetRemainingRealSeconds accessors. The private property remains private. The status-only rebuild succeeded 5/5 (build-status-readback.log). Updated the harness to read that native status and retain complete future Python errors rather than a biome-log tail. One diagnosed retry is running under the same CX-0055 lease. No live treatment outcome is claimed until its report is written and its owned processes exit. Existing UDS editor-only Blueprint/PCG diagnostic errors in -game startup logs were preserved without unrelated edits.


## E/F final window release (CX-0060)

Final medical countdown-query build succeeded 6/6; final focused treatment suite passed 9/9, notRun=0, 23s (medical-final-report.json / medical-final-tests.log). Final live-medical-result.json contains 12 passed steps and no cleanup errors. All owned game instances and runners exited before engine RELEASE. Full medical 90/90 and the other headless gates above preceded only the final readonly countdown query change; the new query itself has an explicit controlled-skew regression. Source is compile-clean. Graphify and scoped whitespace/unity checks are current. No selection UI, third-person medical chart, condition/CPR physiology, full survivor phase resume, full suite or packaged acceptance was inferred from these results.
