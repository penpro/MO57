# Timed wound treatment handoff

Oct 18 collaboration validation: shared MO57Editor build 45/45 and all Medical 90/90 passed against the final audit/reload fixture tree, alongside Crafting 16/16, Inventory 9/9, Equipment 4/4, Survivor.Iteration 3/3 and Survival 9/9 (all failed=0/notRun=0). Current evidence is `Saved/AuditBatchH`; the following batch E/F and live results remain separately dated evidence. Engine lease released in CX-0077; consult the latest source RELEASE/publication hash before editing shared files. Current commit-always authority supersedes the old overnight Git restriction.

Prepared 2026-10-17. Implementation status and exact validation results belong in `SESSION_STATE.md`. Current headless validation: treatment 9/9, all medical 90/90, survivor iteration 3/3, crafting 11/11, Light prefix 8/8 and Fire 1/1 passed, all notRun=0. The shared build passed. Reports are under Saved/AuditBatchE and Saved/AuditBatchF. Real host/joiner self-treatment acceptance passed 12/12 steps (live-medical-result.json); both owned instances exited and engine window was released in CX-0060. Check the cooperation log for newer ownership. Final countdown query build 6/6 and focused treatment 9/9 also passed after the observed client clock-skew correction.

## Service and request boundary

`FMOMedicalTreatmentActions::Begin(Healer, Patient, WoundId, TreatmentId)` is the common native player/AI entry point. A player's owning `AMOPlayerController::ServerTreatWound(Patient, WoundId, TreatmentId)` resolves its own possessed healer. Clients provide no skill level, duration, self-treatment flag, supply quantities or completion callback.

The server captures the authored treatment row for this course, validates the actual wound, healer skill, self-reach restrictions, capability, two-metre reach and shared line of sight, and uses the existing `UMOTimedActionComponent`. Its replicated label, duration and server start time supply the existing progress bar. Movement, damage and the existing interruption bus stop the action. Continuation also checks patient drift, reach/obstruction, current skill/capability, wound existence and actual supplies.

RequiredItemIds means one unit per occurrence. Items defined as tools are reusable, unbroken presence requirements. Other requirements use the same joint physical/liquid ingredient planner as crafting. No new tool wear or clinical dose has been invented. Materials remain in the healer's inventory during work; a complete joint plan is installed with the clinical effect at completion, before inventory/action-end observers run. Cancellation/reload interrupts work and retains supplies. There is no transient material escrow to lose on save.

The trusted anatomy primitives are for clinical mutation, not player requests. Unknown treatment IDs no longer apply fallback effects. Existing clinical quality scaling is retained; reductions cannot create negative bleeding or infection risk.

## Validation to run after the lease handoff

1. Read the cooperation log, wait for explicit engine RELEASE, then append a fresh CLAIM. Never close an editor with unsaved state to obtain the lease.
2. Build with `python Tools/ue.py build --target MO57Editor`.
3. Run `python Tools/ue.py auto --filter MOFramework.Medical.Treatment`. Nine cases passed: authored requirement integrity, clinical validation, joint supply planning, timed completion/XP, interruption/no free XP, authority/actual skill, obstruction, cancellation/save conservation, and shared timer transition/chaining.
4. Preserve the exact report/log under `Saved/AuditBatchE` before running another test filter. Record failures as well as successful retries. AuthoredRequirements reads the configured assets directly and logs real supply/tool/liquid requirements; stale exports are not evidence of current content.
5. Because the generic timer's completion dispatch ordering changed, run the existing medical tests and affected inventory, pacing, carcass and light/fire gates where available. Existing durations and intentional completion chaining must remain intact.
6. Refresh Graphify, update the session record, release the engine window. Under Wes's current commit-always instruction, commit and push only the completed, verified owned paths after checking the staged column; preserve other agents' uncommitted work. The previous overnight Git restriction is historical.

## Player and co-op acceptance still required

For self-treatment, `MO.Medical.TreatmentStatus` prints the local pawn's wound GUIDs and current authored requirements. With a real matching wound and real supplies, submit `MO.Medical.Treat <TreatmentId> <WoundGuid>`. Observe a visible nonzero authored interval, no early clinical benefit or material removal, then one complete supply charge and clinical change. Movement/damage/cancel must stop work without charging supplies or awarding XP. Repeat as a joiner using the same owning RPC.

The optional third argument to Treat is a loaded PatientGuid; the native AI entry point uses the same service. Joining-client treatment of another patient needs a deliberate inspection/data request seam: anatomy wounds currently replicate **owner only** (`MOAnatomyComponent.cpp`). The client cannot be assumed to possess another patient's wound GUIDs. Do not fix this by indiscriminately broadening wound replication. A medical inspection flow and its access policy need review with Wes, then must use the same reach/LOS and stable wound identity rules.

The treatment selection UI and an assigned medic job are not implemented. UI controllers should submit the owning request instead of calling anatomy mutation functions. A future persisted medic job needs patient GUID, wound GUID and treatment ID and must report interruption/failure through the same service; no second medical timer or supply implementation is needed.

## Clinical/content limits

- This service targets wounds. Condition-only rows, CPR and their physiology are outside its scope and are refused as wound actions.
- The generic clinical implementation applies authored bleed/infection reductions. Existing HealingSpeedBonus and PainReduction fields are not wired into the physiology. No treatment-name mapping to bandage/suture/splint flags was invented. Do not claim those effects or the whole medical player loop complete.
- Current tool/consumable roles must be verified against the configured DataTables. Undefined supplies fail closed. No replacement IDs or clinical doses should be guessed from dated CSV/JSON files.
- Material and wound save conservation is covered by the prepared reflected serialization case. Timed callbacks intentionally do not resume on load; supplies survive and a new course takes its full interval.
- Live client progress rendering, treatment UI, inspected-patient data access, assigned medic behavior and packaged acceptance remain separate gates.


## Live acceptance evidence (CX-0060)

The normal joining-client console request reached the host. An unknown treatment was refused on authority. ApplyBandage started the authored 15s server course; the owner received its replicated active action/label, with no early supply debit or clinical change. Completion changed bandages 1 -> 0 and bleed .200 -> .186 on authority and owner. All 12 harness steps passed and both owned instances stopped. This verifies self-treatment request/state transport, not a treatment-selection interface or another patient's medical chart.

The first harness attempt failed before requests because Python cannot read the protected timer state; its evidence is retained. Native readonly TreatmentStatus progress output replaced the private probe. The successful live capture then exposed an initially skewed client clock reporting 16.759s remaining on a 15s course. The shared remaining-time query now uses bounded progress; its controlled regression passed in the final 9/9 focused run. No server time or treatment policy was changed.
