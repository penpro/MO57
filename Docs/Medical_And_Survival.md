# Medical and survival

> **Covers:** the medical cascade (wounds -> vitals -> mental state), metabolism and the legacy survival meters, body
> temperature, breath and drowning, illness courses, death and respawn, wound treatment.
> **Current state (2026-10-17):** the physiology lives in the **MOFrameworkMedical** module and ticks on real intervals; every
> death goes through one path with a cause; the player's food reaches the metabolism; HUD moodles still read the legacy meters
> (paced by Wes's Oct 15 call); timed wound treatment exists as an authoritative service (Codex, Oct 17) with no treatment UI yet.
> **Entry points:** `UMOAnatomyComponent` (1.0 s), `UMOVitalsComponent` (0.5 s), `UMOMetabolismComponent` (1.0 s),
> `UMOMentalStateComponent` (0.5 s), `UMOAdrenalineComponent` (0.1 s), `UMORespirationComponent`, `UMOMedicalSubsystem`
> (DataTable lookups), `UMOSurvivalStatsComponent` (legacy meters), `AMOCharacter::DieOf`, `FMOMedicalTreatmentActions`,
> `AMOPlayerController::ServerTreatWound`.
> **Dev verbs:** `MO.Player.Kill [Brain|Heart|Head]`, `MO.Test.SystemicDeath [window]`, `MO.Test.DeathFlow`, `MO.Medical.Contract`,
> `.SkipIncubation`, `.TreatmentStatus`, `.Treat <TreatmentId> <WoundGuid> [PatientGuid]`, `MO.Survival.Advance`; cvars
> `mo.Medical.CardiacArrestDeathSeconds`, `mo.Medical.RespiratoryFailureDeathSeconds`. **Tests:** `MOFramework.Medical.*` (90),
> `MOFramework.Medical.Treatment` (9), `MOFramework.Survival.Pacing`, `nettest packaged-death`.
> **Open items:** `Docs/BACKLOG.md` § Medical and survival. **Forks:** DECISIONS F1 (single source: `Docs/Survival_Single_Source_Proposal.md`),
> F2 (thermal model: `Docs/Thermal_Model_Reproduction.md`). Water, drinking, illness: `Docs/Water_And_Illness.md`, `Docs/Water.md`.

## 1. The cascade

The medical simulation is deliberately NOT GAS: physiology needs continuous state, not discrete abilities. New medical effects feed
the existing cascade; don't fork it.

```
Wounds (bleed) → Vitals (blood volume, HR, BP) → MentalState (consciousness)
                      ↓
    Heart/Lung damage → SpO2/BP → Death timers
                      ↓
Metabolism (glucose) → Vitals (blood glucose) → MentalState (confusion)
                      ↓
Dehydration → Vitals (+HR, -BP, +Temp) → Performance penalties
```

Components tick independently: Vitals 0.5 s, Anatomy 1.0 s, Metabolism 1.0 s, MentalState 0.5 s (timer-based, authority-only).

**Blood pressure** = 120 / 80 + exertion + stress/pain + the blood-loss shift (`UMOVitalsComponent::BloodLossSystolicShift`, the
haemorrhage classes, continuous): under 15 % lost the body compensates; 15-30 % the systolic holds (-15 at most, the pulse pressure
narrows); 30-40 % it falls to ~75 mmHg; past 40 % it collapses (45 mmHg at half the blood lost; `IsCritical` below 60). The diastolic
moves half as much. Until Oct 18 2026 the bands were tested in the wrong order: every healthy pawn read 180 / 110 and a bleed never
lowered the pressure (test `MOFramework.Medical.Vitals.BloodPressure.BloodLossBands`).
Cross-module inputs arrive through Core contracts (`Docs/TECHNICAL_REFERENCE.md` § Module structure): feels-like temperature and
local heat (`IMOAmbientEnvironmentProvider`, `IMOLocalHeatSource`), clothing warmth (`IMOInsulationSource`), skills -> adrenaline
(`IMOSkillLevelSource`).
**Until Oct 19 2026 the feels-like temperature never felt the weather** (N-175): the shelter test read every pawn as indoors (its rays
started inside the pawn's own interaction sphere), so the wet-cold (-5 C x rain exposure below 20 C) and wind-chill (-20 C x wind
exposure below 10 C) terms never ran. They run now: a pawn in the open in "Rain" at 11.2 C feels 7.7 C; in a cave 10.9 C
(`Docs/Weather_And_Time.md` §2).

## 2. Death: one path with a cause (commit 4f948b11)

- Every death goes through `AMOCharacter::DieOf(EMODeathCause, Part)` -> virtual `HandleInstantDeath` (creatures override it).
  Causes: BodyPartDestroyed (anatomy `OnInstantDeath`), Drowned (respiration), BledOut / CardiacArrest / RespiratoryFailure / HeatStroke
  (vitals `OnSystemicDeath`), Hypothermia (a cardiac arrest with the core under 30 C: "Froze to death"). The recap prints the cause.
  **Never broadcast anatomy `OnInstantDeath` for a non-organ death.**
- **Systemic clocks** (`UMOVitalsComponent::StepSystemicFailure`): circulatory (HR < 10 or blood < 30 %) 300 s, respiratory
  (SpO2 < 50 or RR < 4) 240 s; game-scaled, reset on recovery, single-fire. Instant bleed-out still at <= 20 % blood.
- **Thermal extremes kill** (N-063, checked Oct 20): cold through the heart -- under a 32 C core the rate falls 10 / min per degree, so near
  26 C it is under 10 and the circulatory clock runs (Hypothermia). Heat had no failure at all; now a core of 42 C or more builds a thermal
  dose (minutes at 42 C, each degree above doubling the rate, the CEM43 rule), fatal at `mo.Medical.HeatStrokeDeathMinutesAt42` (60:
  humans tolerate 42 C for 45 min to 8 h, Bynum et al. 1978); back under 41 C it resets. Test
  `MOFramework.Medical.Vitals.SystemicFailure.SustainedAndReversible`.
- HR ~92 at 25 % blood is deliberate (tachycardia +60, then pre-terminal bradycardia x0.7 below 40 % volume): not a bug.
- **Death recap + respawn** (4d58ed3b): `UMOSystemMenuUIController::ResolveDeathRecapClass()` defaults `DeathRecapWidgetClass` to
  `/MOFramework/UI/WBP_DeathRecap` (it was never assigned: a dead last pawn was a silent soft-lock). Continue -> another living colony
  pawn = possession menu; else `AMOPlayerController::ServerRequestRespawn` -> `AMOGameMode::RespawnPlayerAfterDeath` at
  `FindRespawnLocation` (5 mi +-20 %, dry gentle land, not an upland lake, inside the border; new pawn named / recruited /
  persisted; held by the spawn settle with a 75 s cap). Permadeath per `Docs/VISION.md`. Test: `ue.py inst start ui --res 2880x1620`,
  `MO.NewGame 11`, wait for READY, `MO.Player.Kill`, `python Tools/ui_shoot.py <dir> x Click:ContinueButton`; or
  `python Tools/ue.py nettest packaged-death [--seed N]` (default seed 1029024224).
- **Death then leaving** (Oct 17): the recap's Return to Main Menu saves the death (every exit saves: `Docs/PAUSE_POLICY.md`). Loading
  that save raises `AMOGameMode::OnPlayerNeedsNextLife`, which runs the same `ContinueAfterDeath` as the recap's Continue. With no living
  colony pawn, the respawn measures its 5 mi from the deceased record's location. Verified in a `-game` instance: the new survivor
  landed 4.96 mi from the death spot. Before this it was a sky camera with no pawn. The body and its gear survive the reload (Oct 19,
  N-155: `Docs/Persistence.md` §1).
- **A death reaches every machine** (Oct 18, N-064): until then the whole death (cause, ragdoll, input lock, `OnPawnDied` -> the recap)
  ran on the server only, so a joiner who died got no recap and stood upright on every screen. The server writes a replicated
  `FMODeathRecord` (cause, part, killer's name) in `HandleInstantDeath`; every client enters the dead state from it
  (`OnRep_DeathRecord` -> `AMOCharacter::EnterDeadState`, the one routine both run). The save's deceased record and the Death interrupt
  stay server-only. The recap names the killer for wound deaths ("Brain destroyed (killed by Elijah Jones)"; N-169).
- The recap is delivered by `AMOCharacter::OnAnyPawnDied` (every death, every machine) filtered to the local player's pawn
  (`UMOSystemMenuUIController::HandleAnyPawnDied`): a per-pawn `OnPawnDied` binding was dropped on a joiner by a possession settle
  (unpossess + re-possess, no pawn-changed callback on the client).
- **Continue's successor is a pawn nobody drives** (Oct 19): another player's pawn (it carries a PlayerState) does not count, so a joiner whose
  host is the only other pawn gets a new survivor at once instead of a Characters list of one "Taken" entry.
- **A dead body has no mind** (Oct 19): the server's `EnterDeadState` cancels a survivor's job queue and removes its AI controller, and
  `UnPossessed` never spawns a new AI into a corpse (it did: a dead survivor kept working its jobs).
- Verified on a host + joiner: the joiner's recap names the killer; Continue -> the server respawned the joiner 8.8 km away in a new survivor
  (the remote respawn RPC). Not verified: a death during another spawn settle (refused).

## 3. Food, water and the meters (two models today)

- **Legacy meters** `UMOSurvivalStatsComponent` (Hunger / Thirst / Energy / Stamina 0-100, replicated) drive the HUD moodles, the
  status widget and the sprint gate. **Pacing (Wes, Oct 15):** start fully fed / watered / rested; needs become bothersome after
  ~30 min: warning moodles at 30 / 45 / 60 game-min (thirst / hunger / energy), decay on the WORLD clock (`AdvanceStats`, 2d31e0c2).
  Pacing changes go through the `DecayRate` defaults (+ `MOFramework.Survival.Pacing`). (Before Oct 15: 0.5 / 0.8 / 0.2 per REAL
  second, critical thirst in ~2 min.) **Stamina** is spent by the gait in `AMOCharacter` (jog 2, swim 1.6, climb 3, sprint 8 a second)
  and recovers 10/s at rest, scaled by the work (F52 B, Oct 29): `StaminaRecoveryByExertion` gives a walk half, a jog a quarter and a
  swim / climb / sprint nothing (`MOFramework.Survival.StaminaRecovery`). Combat spends the vitals' own stamina instead (F1).
- **The realistic model** `UMOMetabolismComponent` (glycogen 500 g, hydration, digestion ~1 h per meal, ~2.5 L water/day) feeds
  vitals and mental state. Since Oct 13 the player's eat action (`ConsumeItem`) ALSO calls `UMOMetabolismComponent::ConsumeFood`
  (before, the player's body got nothing; villagers already fed the metabolism only).
- **Single source of truth is an open fork** (DECISIONS F1): A = derive the meters from the metabolism (recommended; full mapping in
  `Docs/Survival_Single_Source_Proposal.md`), B = clock-scaled counters, C = arcade pacing. Until decided, the moodles and the Status
  panel's Thirst row (HydrationLevel) disagree.
- **Eating is a timed action** (Wes, Oct 16): `FMOItemActions::EatSeconds` (mass at 2 g/s, floor 4 s) on `UMOTimedActionComponent`;
  moving cancels and nothing is eaten (a4207587). A finished meal says so ("Ate Hazelnuts": N-181, Oct 20; the moodle alone did not).
- **Drinking** from water bodies (Oct 15): `Docs/Water_And_Illness.md` (salinity, illness per litre, timed drink, containers, boiling).
- **Meter persistence:** the pawn record captures `FMOSurvivalSaveData` through the shared live-load/possession-spawn restore path.
  Its valid/version marker, six meter values and nutrition restore atomically on authority after finite-data validation, clamped to
  the owning class's limits. Missing legacy data leaves defaults alone. It neither changes the approved clock pacing nor advances
  an extra offline interval. Tests: `MOFramework.Survival.Persistence` and the existing pacing gates. The F1 model choice stays open.
- **The body shows its composition** (Oct 27; Wes, Oct 26: "a single character go from skinny to fat and vice versa, starvation etc"). A
  survivor on a base (a Creator MetaHuman: `MOSurvivorBase.h`) carries body shapes (`Plugins/MOBodyShape`, baked in MO_Tools): heavier as body
  fat rises to 40 %, thinner as it falls to 8 %, more muscular toward 40 kg of muscle, wasted toward 20 kg -- from the base's own body, where
  the shapes are off (Jalena 14.1 % / 27.0 kg, Zuri 15.8 / 27.5, Harvey 15.8 / 33.5, Trey 10.2 / 30.0). The weights are continuous, so the body changes as slowly as the metabolism does.
  - The metabolism's `BodyComposition` replicates to its owner only, so the server copies body fat and muscle into the pawn
    (`AMOMetaHumanCharacter::ShownBodyFatPercent` / `ShownMuscleMassKg`, replicated to all, re-checked every 2 s, sent on a 0.1 change).
  - A base gives a new survivor its own body (`ApplyBaseStartingBody`, the character creator): `FMOBodyComposition::SetBody` sets the fat %,
    the muscle and the weight they make together, so it holds through `UpdateBodyWeight`. **The defaults do not:** 75 kg at 18 % becomes
    62 kg at 21.8 % on the first update (fat mass 13.5 kg + muscle 30 + bone 3.5 + the rest 15). Test `MOFramework.Metabolism.StartingBodyHolds`.
  - The pawn Blueprint's own survivor (MH_F_1) has no shapes; its body never shows the composition.

## 4. Body temperature (F2: single node shipped as the interim, Oct 19)

**Since Oct 19 (N-178; Wes, F2: "Ship single node now")** the core runs a heat balance over a real thermal mass:
`UMOVitalsComponent::AdvanceCoreTemperature` (pure) -- the body holds 37 C while its heat production (100 W resting + up to 200 W
shivering) covers what it loses through its insulation to the felt temperature (20 W/K unclothed x (1 - insulation)), or sweating sheds
what it gains (up to 400 W); only the deficit / excess moves the core, through 245 kJ/K (70 kg). One-minute steps on game time; a
recovering core stops at 37. Insulation as before: 0.55 base + clothing Warmth (max 0.40) + 0.35 x cover; water: `ApplyWaterToExposure`.
Measured / tested (`MOFramework.Medical.Vitals.Temperature.HeatBalanceRealRates`): underclothes in "Rain" at 11 C (felt 0.9 C) hold
37.0 for the first minutes (the old drift read 31 C after four) and lose ~0.4 C an hour; -20 C felt: ~-2.6 C an hour; a roof holds it;
winter clothes (0.9) hold at -30 C; 10 C water: 35 C in ~45 min; 35 C rewarms in 20 C air and stops at 37.
`FMOVitalSigns::ThermalLoad` = how hard the body works for it (> 0 shivering, 1 = flat out; < 0 sweating): the comfort moodle reads Cold
while the load is >= 0.5 (people feel cold while they shiver, long before the core drops) and Hot at <= -0.6, besides the core thresholds.
Still open in F2 (`Docs/Thermal_Model_Reproduction.md`): core + shell (vasoconstriction, cold-water shock), clo values per item (+ a wet
multiplier), shivering burning glucose, exertion heat.
Water coupling: `ApplyWaterToExposure` (Ambient -> water temperature 0.7 x air + 3 C; insulation x (1 - 0.85 x immersion)).
**Clothing wetness (N-176, Oct 19):** a downpour soaks it in 5 min (`WetnessGainPerSecond` 1/300 x rain intensity x exposure; "Rain"
~7 min), it dries on the body in ~2.5 h (1/9000) and by a campfire's +20 C in ~40 min (`HeatDryingBoostPerCelsius` 0.14); was 10 s /
3 min / 18 s. Measured: 0.0022 per s in "Rain" in the open (expected 0.0023).
**A joiner's own state (N-177 / N-178, Oct 19):** `WetnessLevel` replicates to its owner (it was server-only: a joiner never saw the wet
moodle), and `Vitals` has `OnRep_Vitals`, which fires the same `OnVitalsChanged` / `OnThermalComfortChanged` the host's tick fires (they
fired on the server only: a joiner's thermal moodle and status panel never changed). Verified on a joiner in rain: its own wetness
0.12 -> 0.40, thermal load 1.2, comfort Cold, both moodles on its screen.

## 5. Breath and drowning

`UMORespirationComponent`: oxygen reserve, `SetAirwayClosedBy(source, bool)`; a closed airway drains it (110 s at rest, /4 at full
exertion), caps SpO2 via `UMOVitalsComponent::SetAirwayOxygenReserve`; asphyxia death 75 s after empty -> `DieOf(Drowned)`.
Drowning comes from the vitals (SpO2), never from a timer. Details and the swim side: `Docs/Water.md` §3.

## 6. Illness

Untreated water gives an illness as a condition COURSE on the anatomy component (incubation -> triangular peak at 1/3 -> cleared),
applied to the body through the survival stats. Chances per litre and the research: `Docs/Water_And_Illness.md`.

## 7. Wound treatment (authoritative timed service)

- `FMOMedicalTreatmentActions::Begin(Healer, Patient, WoundId, TreatmentId)` is the one native player / AI entry point; a player's
  owning `AMOPlayerController::ServerTreatWound(Patient, WoundId, TreatmentId)` resolves its own healer. Clients send no skill,
  duration, supply counts or completion. The server validates wound, skill, self-reach, capability, 2 m reach and line of sight, and
  runs the shared `UMOTimedActionComponent` (replicated label / duration; movement and damage interrupt). Supplies use the crafting
  ingredient planner and are charged at completion. Unknown treatment ids are refused (no fallback effect).
- Verified: headless 9/9; live host + joiner self-treatment 12/12 (ApplyBandage 15 s, bandages 1 -> 0, bleed .200 -> .186).
- Continuation checks patient movement, reach/LOS, capability, actual skill, wound and supplies. Physical/liquid supply planning is
  atomic with the clinical effect at completion, before observers; no early material charge, effect or XP. Tools are reusable presence
  requirements under the authored item definitions. Cancel/reload retains supplies and a new attempt takes its full interval.
- The clinical primitive refuses unknown IDs and inapplicable wounds; authored bleed/infection reductions preserve the existing quality
  scaling with safe bounds. The common timer guards reentrant replacement and derives remaining time from bounded progress, including
  a freshly joined client's initial server-clock skew. No treatment duration or physiology policy is changed.
- **The treatment menu** (N-061, Oct 20): Status > Wounds lists, under each wound, a button per treatment for its type
  (`FMOMedicalTreatmentActions::GetOptions`: "Apply Bandage (15 s)"), greyed with the reason ("Needs Bandage", "Insufficient treatment
  skill.", "Finish or cancel the current action first."). The verdict is `Preview`: Begin's own checks minus the authority ones, so it runs
  on a joiner's machine for its own pawn. A click sends `ServerTreatWound`; the server checks it all again.
  Before, no UI could treat a wound at all (only `MO.Medical.Treat`). A wolf's bite is a laceration (live DT_MedicalTreatment, Oct 20):
  Apply Bandage (Bandage01, 15 s), Clean Wound (clean water, 20 s), Apply Tourniquet (Tourniquet01, Medicine 1, 8 s), Suture Laceration
  (SutureKit01, Medicine 3, 60 s).
- **Healing and pain effects** (Oct 20): a treatment's `HealingSpeedBonus` and `PainReduction` (x the treatment quality) are kept on the
  wound (`TreatmentHealingBonus` / `TreatmentPainReduction`, the best so far, saved): it heals (1 + bonus) times as fast and hurts
  (1 - relief) as much. Bandage / suture / splint flags are still not set by treatments (no name mapping invented).
- **Treating someone else** (N-061 co-op, Oct 20): wounds replicate to every machine (`UMOAnatomyComponent::Wounds`, body parts and
  conditions stay owner-only), and a right-click on a wounded pawn -- another player, an unrecruited person -- opens its menu under its
  name (`AMOCharacter` is an `IMOWorldActionProvider`), one option per wound and treatment, "Forearm Left Laceration: Apply
  Bandage (15 s)", from the same `GetOptions`. A pick runs `FMOMedicalTreatmentActions::Begin(healer, patient, ...)` on the server as the
  healer's timed action. A recruited survivor's right-click is its command menu, which lists the same treatment options after its orders
  (`UMOSurvivorContextMenu::RefreshTreatmentButtons`, Oct 20).
- **The colony medic** (N-061, Oct 20): the colony's upkeep sends an idle villager with the skill and the supplies (its pack, else a
  settlement store it fetches from) to anyone in the settlement bleeding 0.2 mL/s or more; it walks over and runs the same `Begin`
  (`PickForMedic` chooses the wound and treatment). `Docs/AI_And_Colony.md` section 2.
- **Not built:** condition-only rows (infection, hypothermia, shock), CPR, and a medic that cleans or sutures a wound that has stopped
  bleeding (the medic treats bleeding only).
  Detail and acceptance steps: `Docs/agent/audit/MEDICAL_HANDOFF.md`.

## 8. Rules

- Realism pillar: no HP regen, no "hunger 1/sec" (`Docs/ENGINEERING_PRINCIPLES.md` #11). Rates are physiological.
- Server bodies guard `HasAuthority()`; medical state mutates on authority only; `BuildSaveData` / `ApplySaveDataAuthority` pattern.
- A UPROPERTY class "that must be assigned in BP defaults" gets a default policy in code (the death-recap lesson, Principle 7).
