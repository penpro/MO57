# MO57 deep dive: ten ranked flaws

Reviewed October 16, 2026 (America/Los_Angeles), against HEAD `f92ebebe` and the current working tree. The active cave/geology changes and staged assets were left untouched.

Historical findings and original source locations follow. Later remediation, tests and remaining acceptance limits are recorded in `Docs/agent/audit/SESSION_STATE.md` and the current per-system documentation; this report is not a current-defect checklist.

## Scope and confidence

This was a source audit, not a new playtest. I used the local Graphify graph, traced callers and callees, inspected current changes, checked the actual UE 5.8 FastArray implementation, and inspected relevant existing tests. I checked the player inventory/RPC, equipment, crafting, persistence, medical, combat, item actions, water/PCG and survivor-job paths. The ten findings below are ranked by impact, reachability and frequency, with the most severe first.

The code paths and missing integration are confirmed by inspection. The reproduction procedures below are verification instructions, not claims that I executed them in Unreal. The editor bridge returned `MCP not responding`, so current live DataTable readback was unavailable. JSON exports were read with `Tools/ue_json_utils.py`; they are supporting schema/examples, not proof of the latest cooked table. Current liquid behavior is also established by source and the existing `MOLiquidIngredientTests.cpp` contract. No C++ compilation, game-state mutation, intentional crash, commit or push was performed.

Previously fixed October 13 loading/vegetation issues, the old combat-profile dangling pointer, the old rapid-decay defaults, and the already fixed co-op drop/gait failures are not recycled as current findings.

## Ranking

| Rank | Severity | Flaw | Typical exposure |
|---|---|---|---|
| 1 | Critical | Network inventory request can index equipment outside its array | Invalid request from a connected client |
| 2 | High | Hand swaps duplicate the old item and can lose the pending item on save | Normal equip/unequip/save during a swap |
| 3 | High | Crafting RPC accepts untrusted world context and harvest recipes | Modified request; cold/absent station prerequisites |
| 4 | High | Stack consolidation changes quantities without marking FastArray items dirty | Co-op Fill Stacks / Organize |
| 5 | High | Inventory mutation bypasses capacity and creates hidden entries | Transfer or add into a full inventory |
| 6 | High | Hunger, thirst and energy are absent from the save payload | Ordinary save/exit/load |
| 7 | High | Wound treatments have no reachable player integration | A wounded player tries to treat an injury |
| 8 | Medium | Joiners can enqueue crafts but cannot cancel them | Ordinary client queue cancellation |
| 9 | Medium | Survivor crafting still treats liquid measures as physical item stacks | Assign a water-using recipe to a survivor |
| 10 | Medium | Cancelling a craft changes the refunded water's identity/composition | Cancel a water-using craft |

## 1. A client-supplied equipment slot can index outside the array

**Diagnosis:** The inventory request carries an arbitrary `uint8 EquipSlot`, but the server rejects only the `MAX` sentinel before passing it to an equipment function that indexes without a bounds check.

**Verified chain:**

1. `AMOPlayerController::ServerInventoryRequest` is a reliable Server RPC with an `FMOInventoryRequest` payload.
2. `FMOInventoryRequests::Execute` checks inventory access, casts the slot byte to `EMOEquipmentSlot`, and calls `UnequipToInventory` whenever it is not `MAX`.
3. `UnequipToInventory` rejects only `MAX`, initializes eight slots, and immediately evaluates `EquippedSlots[SlotIndex]`.
4. Values outside 0-7 other than sentinel 8 therefore reach an out-of-bounds access. The request can name the requester's own inventory, so the ownership check does not prevent this.

**Impact:** A malformed request from one connected client can trigger an array assertion on a checked build; the unchecked build has invalid memory access/undefined behavior. This is a concrete unsafe boundary, not a claim of demonstrated remote code execution.

**Reproduction / regression gate:** In an isolated automated server test, submit Unequip with the pawn's own target inventory and an out-of-range slot. Assert refusal with no mutation; cover every valid slot, `MAX`, and other byte values. Do not reproduce against an active player world.

**Fix at the owning layer:** Validate enum/index range at the authoritative request boundary and validate it again inside the equipment mutator before indexing. Preserve the natural-slot sentinel only for the operation that supports it.

**Evidence:** [request payload](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Public/MOInventoryRequest.h:68), [server dispatch](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryRequest.cpp:225), [unchecked indexing](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOEquipmentComponent.cpp:252).

## 2. The hand-swap transaction temporarily owns the same item twice

**Diagnosis:** Equipping a replacement returns the old hand item to inventory immediately, leaves it in `EquippedSlots`, and stores the new item only in an unsaved pending field until the delay ends.

**Verified chain:** `EquipFromInventory` returns old item A to inventory at lines 186-197, removes new item B at 199-204, then records B in `PendingRightHandItem`/`PendingLeftHandItem`. It does not clear A from the equipped slot. `UnequipToInventory` has no in-progress-swap guard and returns A again. The timer later installs B. Separately, `BuildSaveData` serializes only `EquippedSlots`; pending B and its timing are omitted.

**Impact:** Normal operations during the swap can duplicate A. Saving and exiting before completion can preserve A both in inventory and equipment while losing B entirely. The conservation/identity error exists before any rendering or UI behavior.

**Reproduction / regression gate:** Equip A, begin equipping B, then unequip that hand before its timer finishes. After completion, count A and B across inventory, equipment and pending state. Repeat with save/exit/load during the delay. Each original item must exist exactly once, with its durability and identity preserved.

**Fix at the owning layer:** Make equipment movement a transaction with explicit ownership during the pending state. Resolve or persist the pending transaction on save and reject/handle conflicting operations centrally. A UI-only button disable is insufficient.

**Evidence:** [swap begins](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOEquipmentComponent.cpp:186), [unequip during swap](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOEquipmentComponent.cpp:252), [save omits pending items](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOEquipmentComponent.cpp:492).

## 3. The crafting boundary trusts context the client is allowed to invent

**Diagnosis:** The authoritative craft queue validates a client-supplied station enum and recipe requirements, but does not establish a real usable station or reject harvest recipes that require a world target.

**Verified chain:** `ServerRequestEnqueueCraft` forwards `RecipeId`, `Count` and `Station` directly into `EnqueueCraft`. `CanCraftRecipe` checks `RequiredStation == Station`, not an authoritative station actor, completion, distance, fuel or active state. The queue does not reject `bIsHarvestRecipe` or require its target tag/resource/depletion context. Those recipes are filtered out of the normal menu instead, which is not an authority boundary. Completion calls `ProduceOutputsOnly` without a harvest target. `ActiveStation` exists but is not consulted by validation or the processing tick.

**Impact:** The boundary permits targetless harvesting and spoofed station use. A valid station menu also does not make a cold campfire a working heat source. The resource/production simulation can be bypassed at the queue layer.

**Reproduction / regression gate:** Submit a harvest recipe with otherwise satisfied requirements but no matching resource target; it must refuse. Submit a station recipe naming the correct enum but with no usable station; it must refuse. Also test an unlit/fuel-depleted station and a station that becomes unavailable after enqueue. An actual valid target/station is the positive control. `HarvestStick` and `CookMeat` exist in the checked JSON export as useful candidate rows, but resolve the live recipe rows before running the test.

**Fix at the owning layer:** Separate harvest execution from crafting and reject wrong recipe kinds in the queue. Derive station context from an authoritative actor/session and enforce the recipe's real prerequisites through execution, including any legitimate background-work policy.

**Evidence:** [enqueue RPC](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingQueueComponent.cpp:185), [enum-only station check](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingSubsystem.cpp:226), [output-only completion](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingQueueComponent.cpp:811), [UI-only harvest exclusion](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingMenu.cpp:199).

## 4. Fill Stacks and Organize do not replicate changed quantities correctly

**Diagnosis:** Bulk stack operations alter surviving FastArray entries but call only `MarkArrayDirty`, leaving their per-item replication keys unchanged.

**Verified chain:** `StackAndOrganize` overwrites retained quantities and removes merged entries. `FillStacksFrom` increments target quantities and decrements source quantities. Both dirty the arrays, but neither marks the modified retained entries dirty. `BroadcastInventoryChanged` only rebuilds the GUID index and broadcasts a local delegate. The actual UE FastArray serializer skips an existing entry when its replication key still matches the previous key; array dirtiness alone does not transmit its changed quantity.

**Impact:** A joiner can see old counts or apparent item loss after a successful server-side consolidation. For example, a source stack of 15 and target of 10 with max stack 20 become source 5/target 20 on the server while the owning clients can retain 15/10. Later actions operate against server quantities, so the UI and gameplay disagree.

**Reproduction / regression gate:** On an already replicated connection, fill a target stack without deleting either entry and compare server/client quantities after replication. Repeat Organize with two stacks that consolidate into one; the surviving stack must gain the correct quantity on the client. A reconnect/full initial snapshot is not a sufficient test.

**Fix at the owning layer:** Mark every modified surviving item dirty, alongside array dirtying for removals. Correct the header comment claiming `MarkArrayDirty` is sufficient for bulk changes.

**Evidence:** [organize mutation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:1726), [fill mutation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:1816), [engine's item-key comparison](/D:/UnrealEngine/UE_5.8/Engine/Source/Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h:932).

## 5. Inventory capacity is advisory, allowing invisible overflow

**Diagnosis:** The mutation functions insert entries even when no slot is available; capacity checks live in optional preflight calls rather than the authoritative add operation.

**Verified chain:** `AddItemByGuid` computes whether a slot exists but never uses that result to stop creation. `AddItemByGuidWithDurability` unconditionally appends an entry and returns success even if auto-assignment fails. `TransferItem` calls the latter without checking target capacity, then removes the source stack. `ReconcileSlots` can report unslotted entries but cannot display them without free slots. The inventory grid renders the configured slot count.

**Impact:** Transferring into a full backpack can remove the visible source item and place it in an inaccessible target entry. The same add primitive can let crafting/rewards exceed capacity. This is hidden state, not necessarily destroyed data, but it breaks both capacity policy and reliable access to the items. The durability-add path also bypasses normal stack splitting.

**Reproduction / regression gate:** Fill every target slot, then transfer another distinct stack into it. Assert that either the operation is refused with the source unchanged or an explicitly reported valid overflow policy executes. There must be no success with an unslotted item. Cover multiple-stack quantities and outputs that need more than one slot.

**Fix at the owning layer:** Make adds/transfers transactional and capacity-aware, returning the actual accepted quantity/reason. All callers should receive the same contract; adding another preflight in one widget leaves other paths broken.

**Evidence:** [unchecked entry creation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:191), [durability add](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:302), [transfer removes the source](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:1618), [unslotted-state warning](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:899).

## 6. Save/load resets the survival meters the HUD actually uses

**Diagnosis:** Pawn persistence saves the physiology components but has no payload or restore path for `UMOSurvivalStatsComponent`, which remains the authoritative source for hunger/thirst/energy moodles.

**Verified chain:** The survival component initializes those meters to 100. `CapturePersistedPawnsAndInventories` captures vitals, anatomy, metabolism, mental state, skills, equipment, recruitment, combat and jobs, but no survival stats. `FMOPersistedPawnRecord` has no corresponding field, and `ApplyPawnComponentState` cannot restore it. The HUD explicitly subscribes to this separate survival component rather than deriving the meters from the restored metabolism.

**Impact:** A fresh actor restored from a save starts with full legacy hunger/thirst/energy despite its previous need state and restored body data. Restarting can erase survival pressure and make the HUD contradict the physiology. This finding does not repeat the old rapid-decay default: those rates were changed October 15.

**Reproduction / regression gate:** Reduce the three meters, save, exit the world, then restore into freshly spawned pawns. Compare the meters and moodles to pre-save values, independently of the metabolism/vitals values. Include an unpossessed survivor and co-op restoration. An in-place load on an existing actor is not enough to expose default reconstruction.

**Fix at the owning layer:** Either persist and restore the still-authoritative meters or complete the already proposed single-source physiology adapter so they are derived consistently. Keep the agreed pacing and avoid a HUD-only workaround.

**Evidence:** [meter defaults](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOSurvivalStatsComponent.cpp:29), [saved component payload](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Public/MOworldSaveGame.h:164), [capture flow](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOPersistenceSubsystem.cpp:1242), [HUD's actual source](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCharacterUIController.cpp:1360).

## 7. Wound treatment is implemented as unused APIs, not a player loop

**Diagnosis:** The anatomy simulation exposes wound-treatment functions, but the player/AI interaction layers do not call them, and treatment supplies are not consumed through a treatment execution path.

**Verified chain:** Searches across current C++ found only declarations/definitions for `ApplyTreatment`, `ApplyBandage` and `ApplySutures`; there are no production call sites. A binary name-reference search of the checked UI and character assets also found no calls to these functions. `RequiredItemIds` for medical treatment definitions is read by the data validator, not a treatment transaction. The existing generic treatment function applies immediate wound changes and does not implement the required timed, supplied action.

**Impact:** Players can acquire wounds and have a detailed medical simulation, but the current player loop cannot use these wound procedures. Having treatment rows, medicines and a status display does not provide an actual way to bandage or suture a wound. Natural recovery is separate and is not claimed to be absent.

**Reproduction / regression gate:** With a real wound and the relevant supplies, perform treatment through the normal interface. Demonstrate the visible duration, movement/damage interruption, skill/self-treatment restrictions, supply consumption and resulting wound change. Repeat as a joiner and, where supported, via an assigned medic. The current audit finds no native player request that can start this flow.

**Fix at the owning layer:** Add a shared authoritative treatment-action service that validates the real medic/patient, wound, supplies, reach and skill. UI and pawn jobs request that action rather than calling the low-level mutation directly. Reuse timed-action and ingredient abstractions.

**Evidence:** [unused treatment mutation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFrameworkMedical/Private/MOAnatomyComponent.cpp:273), [bandage mutation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFrameworkMedical/Private/MOAnatomyComponent.cpp:338), [treatment supply schema](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFrameworkCore/Public/MOBodyPartDefinitionRow.h:411), [dated integration note](/D:/UEProjects/MO57/Docs/Archive/Progression_FreshLook_2026-10-15.md:183).

## 8. Joiners can start crafts but the cancel buttons never reach authority

**Diagnosis:** Enqueue has client-to-server transport; row and whole-queue cancellation call authority-only component methods locally with no equivalent transport.

**Verified chain:** `UMOCraftingQueueWidget::ExecuteCancelRow` and `ExecuteCancelAll` directly call the component. Both component methods return immediately on non-authority. The component header declares a Server RPC for enqueue only, and no cancellation request route appears in the native callers. Unlike the inventory gestures repaired October 16, cancellation has no server request seam.

**Impact:** A joiner clicks Cancel or Cancel All and nothing changes or refunds. The host can use the same interface successfully, so a host-only UI test misses the bug. A related control audit should cover pause/resume, but those are not being counted as separate findings here.

**Reproduction / regression gate:** Join a host, enqueue a craft that lasts long enough to observe, then cancel it via the visible row button and Cancel All. Verify removal and exactly one refund on both machines; test a forged entry ID from another pawn as a refusal control.

**Fix at the owning layer:** Add one ownership-validated server request path for queue-control intents and have the existing UI adapters use it. Do not weaken the component's authority guard.

**Evidence:** [local UI cancellation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingQueueWidget.cpp:181), [server-only mutation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingQueueComponent.cpp:202), [enqueue-only RPC](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Public/MOCraftingQueueComponent.h:288).

## 9. Survivor craft jobs cannot withdraw the new liquid ingredients

**Diagnosis:** Player crafting was migrated to liquid-aware ingredient APIs, but survivor job logistics still checks and transfers `Water01`/`CleanWater01` as physical stacks.

**Verified chain:** `TransferCraftIngredients` checks every ingredient with storage `HasItem`, which calls `GetItemCountByDefinitionId`. It then removes physical items and creates new item stacks on the pawn. The liquid model instead represents water in another item's `Liquid` field, and `CountIngredient` includes those measures. Neither a filled storage pot nor a pot already carried by the survivor satisfies the withdrawal routine's item-only storage check.

**Impact:** Valid water-using jobs fail as if the storage were short even when sufficient water exists in containers. Tanning and water-based brews cannot reliably be delegated through this path. Simply renaming the count call would still leave transport/container ownership unresolved.

**Reproduction / regression gate:** Stock storage with the non-liquid ingredients and a suitable pot of the needed water; assign the survivor a water-using recipe. Observe the storage-short refusal, then verify a fixed flow transports/uses real contents, preserves containers and water quality, and deposits results. Cover insufficient quantity and safe-versus-unsafe water.

**Fix at the owning layer:** Make job logistics use the same typed ingredient model as crafting, with explicit container/liquid reservations and transport. Do not materialize fictitious loose `Water01` stacks to satisfy the old loop.

**Evidence:** [item-only withdrawal](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOSurvivorController.cpp:1803), [liquid-aware counting](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:1984), [existing liquid contract test](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/Tests/MOLiquidIngredientTests.cpp:79).

## 10. Craft cancellation reconstructs different water rather than refunding what was taken

**Diagnosis:** A queued craft records that ingredients were consumed, but not the actual liquid lots; cancellation recreates generic water from the ingredient definition.

**Verified chain:** Enqueue draws liquid via `ConsumeIngredient`. The queue entry contains the recipe/count/progress, not a reservation ledger of source composition. `RefundIngredientsForCraft` calls `AddIngredient` with the row's ingredient ID and quantity. For ordinary `Water01`, `AddIngredient` constructs the pond source profile. Salt, illness risk, source and boiled state from the consumed water are therefore not retained. `IsSuitableMeasure(false)` accepts seawater as ordinary water, so a cancel round-trip can replace salt water with the generic freshwater profile. Even boiled freshwater can return unboiled.

**Impact:** Cancel is a transformation of matter and water safety, not an undo. It can erase useful treatment state or change salinity/source without the simulated process. If the original container is no longer available, the refund also ignores the actual placed amount; that policy should be explicit rather than logging an unconditional successful refund.

**Reproduction / regression gate:** In a host/single-player test, queue a valid ordinary-water recipe using a known volume of saline water and cancel before completion. Compare the returned volume, salinity, illness risk, source and boiled state to the original lot. Repeat with boiled freshwater and with the source container moved away. Use the live water-using recipe rows; the dated progression notes identify tanning as one such flow.

**Fix at the owning layer:** Store a real consumed/reserved ingredient ledger, including liquid composition and item state, and refund those contents transactionally through the same inventory contract. A cancellation must not fabricate a replacement from an abstract recipe ID.

**Evidence:** [refund by ID/count](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOCraftingQueueComponent.cpp:931), [generic pond-water creation](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Private/MOInventoryComponent.cpp:2062), [ordinary-water eligibility](/D:/UEProjects/MO57/Plugins/MOFramework/Source/MOFramework/Public/MOItemInstanceState.h:40).

## Recommended repair order and verification limits

1. Close the unsafe equipment-slot boundary first; it is an isolated server-validation fix with a clear refusal contract.
2. Repair equipment transactions and their save representation before adding more equipment actions.
3. Establish authoritative craft execution context and a typed reservation/refund model; these underpin findings 3, 9 and 10 without collapsing them into UI patches.
4. Fix inventory mutation contracts and replication together, then test them from real connected clients, including full-capacity and partial-stack cases.
5. Close the missing persistence and player-action integrations, keeping the existing physiology/pacing decisions explicit.

The tests inspected cover important positive cases, including starting-kit slot reconciliation, liquid count/consume/pour semantics and existing co-op operations. They do not establish the negative boundaries above. A past green 255-test run or packaged smoke is not evidence that invalid enum inputs, mid-swap saves, post-replication bulk quantity changes, joiner queue cancellation, or liquid reservation identity are correct.

This report identifies the responsible layers and proposes regression checks. It does not claim that any flaw has been repaired or dynamically reproduced during this audit.
