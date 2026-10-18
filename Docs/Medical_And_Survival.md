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
Cross-module inputs arrive through Core contracts (`Docs/TECHNICAL_REFERENCE.md` § Module structure): feels-like temperature and
local heat (`IMOAmbientEnvironmentProvider`, `IMOLocalHeatSource`), clothing warmth (`IMOInsulationSource`), skills -> adrenaline
(`IMOSkillLevelSource`).

## 2. Death: one path with a cause (commit 4f948b11)

- Every death goes through `AMOCharacter::DieOf(EMODeathCause, Part)` -> virtual `HandleInstantDeath` (creatures override it).
  Causes: BodyPartDestroyed (anatomy `OnInstantDeath`), Drowned (respiration), BledOut / CardiacArrest / RespiratoryFailure (vitals
  `OnSystemicDeath`). The recap prints the cause. **Never broadcast anatomy `OnInstantDeath` for a non-organ death.**
- **Systemic clocks** (`UMOVitalsComponent::StepSystemicFailure`): circulatory (HR < 10 or blood < 30 %) 300 s, respiratory
  (SpO2 < 50 or RR < 4) 240 s; game-scaled, reset on recovery, single-fire. Instant bleed-out still at <= 20 % blood.
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
  landed 4.96 mi from the death spot. Before this it was a sky camera with no pawn. Gap: the dead pawn's body and gear don't survive the
  reload (BACKLOG N-155).
- Not verified: a remote client's respawn RPC; a death during another spawn settle (refused).

## 3. Food, water and the meters (two models today)

- **Legacy meters** `UMOSurvivalStatsComponent` (Hunger / Thirst / Energy / Stamina 0-100, replicated) drive the HUD moodles, the
  status widget and the sprint gate. **Pacing (Wes, Oct 15):** start fully fed / watered / rested; needs become bothersome after
  ~30 min: warning moodles at 30 / 45 / 60 game-min (thirst / hunger / energy), decay on the WORLD clock (`AdvanceStats`, 2d31e0c2).
  Pacing changes go through the `DecayRate` defaults (+ `MOFramework.Survival.Pacing`). (Before Oct 15: 0.5 / 0.8 / 0.2 per REAL
  second, critical thirst in ~2 min.)
- **The realistic model** `UMOMetabolismComponent` (glycogen 500 g, hydration, digestion ~1 h per meal, ~2.5 L water/day) feeds
  vitals and mental state. Since Oct 13 the player's eat action (`ConsumeItem`) ALSO calls `UMOMetabolismComponent::ConsumeFood`
  (before, the player's body got nothing; villagers already fed the metabolism only).
- **Single source of truth is an open fork** (DECISIONS F1): A = derive the meters from the metabolism (recommended; full mapping in
  `Docs/Survival_Single_Source_Proposal.md`), B = clock-scaled counters, C = arcade pacing. Until decided, the moodles and the Status
  panel's Thirst row (HydrationLevel) disagree.
- **Eating is a timed action** (Wes, Oct 16): `FMOItemActions::EatSeconds` (mass at 2 g/s, floor 4 s) on `UMOTimedActionComponent`;
  moving cancels and nothing is eaten (a4207587).
- **Drinking** from water bodies (Oct 15): `Docs/Water_And_Illness.md` (salinity, illness per litre, timed drink, containers, boiling).
- **Meter persistence:** the pawn record captures `FMOSurvivalSaveData` through the shared live-load/possession-spawn restore path.
  Its valid/version marker, six meter values and nutrition restore atomically on authority after finite-data validation, clamped to
  the owning class's limits. Missing legacy data leaves defaults alone. It neither changes the approved clock pacing nor advances
  an extra offline interval. Tests: `MOFramework.Survival.Persistence` and the existing pacing gates. The F1 model choice stays open.

## 4. Body temperature (open fork F2)

`UMOVitalsComponent::ApplyEnvironmentalTemperature`: `dT/dt = 0.01 x E x (Ambient - T) + 0.1 C/s toward 37` (E = 1 - Insulation;
insulation = 0.55 base + clothing Warmth (max 0.40) + 0.35 x cover). No thermal mass: naked at 0 C is "very cold" in 32 s, while
light clothes never cool at all; a 13 C lake takes the core 36.6 -> 33.4 in 36 s (real: ~1 C per 15-20 min). The proposal (heat
balance over a thermal mass, core + shell, clo values, shivering burns glucose) and the decisions it needs are in
`Docs/Thermal_Model_Reproduction.md`. Do not paper over it with a constant.
Water coupling: `ApplyWaterToExposure` (Ambient -> water temperature 0.7 x air + 3 C; insulation x (1 - 0.85 x immersion)).

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
- **Not built:** a treatment-picker UI, an assigned medic job, treating ANOTHER patient from a client (anatomy wounds replicate
  owner-only: needs a deliberate inspection request), `HealingSpeedBonus` / `PainReduction` effects, condition-only rows and CPR.
  Detail and acceptance steps: `Docs/agent/audit/MEDICAL_HANDOFF.md`.

## 8. Rules

- Realism pillar: no HP regen, no "hunger 1/sec" (`Docs/ENGINEERING_PRINCIPLES.md` #11). Rates are physiological.
- Server bodies guard `HasAuthority()`; medical state mutates on authority only; `BuildSaveData` / `ApplySaveDataAuthority` pattern.
- A UPROPERTY class "that must be assigned in BP defaults" gets a default policy in code (the death-recap lesson, Principle 7).
