# Body temperature: what the current model does, and a proposal (H11)

*Oct 13 2026 (Claude). Reproduction first, as H11 asks; the model change is a design checkpoint for Wes -- nothing in the code was changed.*

## The current model (code as of commit 430830fc)
`UMOVitalsComponent::ApplyEnvironmentalTemperature` (`MOFrameworkMedical/Private/MOVitalsComponent.cpp:342-394`), called every vitals update with the real elapsed time (x the clock TimeScale, 1.0 = real time):

```
Exposure E   = 1 - Insulation
dT/dt        = 0.01 * E * (Ambient - T)          // per second
             + 0.1 toward 37 C (capped at the gap) // "regulation", a fixed 0.1 C/s
T clamped to 25..45 C
```
Inputs (`MOVitalsComponent.cpp:1340-1376`): Ambient = the weather's feels-like temperature (+ local heat sources); Insulation = clamp(0.55 base + clothing (sum of equipped Warmth scalars, max 0.40) + 0.35 x overhead cover, 0, 0.95). In water (`ApplyWaterToExposure`): Ambient -> water temperature (0.7 x air + 3 C, 1..26 C), Insulation x (1 - 0.85 x immersion).

Equilibrium (below 37 C): **T = Ambient + 10 / E**. Thresholds: Cold < 36.5 C, Very cold < 35 C.

## Reproduction (exact replica of the update rule, 0.5 s steps)
| Scenario | Ambient | Insulation | Cold (36.5) | Very cold (35) | 32 C | Settles at |
|---|---|---|---|---|---|---|
| naked, dry air | 10 C | 0.55 | 25 s | 2 min | never | 32.2 C |
| naked, dry air | 0 C | 0.55 | 8 s | 32 s | 92 s | 22.2 C |
| naked, dry air | -10 C | 0.55 | 5 s | 19 s | 50 s | 12.2 C |
| light clothes (+0.2) | 10 C / 0 C | 0.75 | never | never | never | 37 C |
| full clothes (+0.4) | -10 C / -25 C | 0.95 | never | never | never | 37 C |
| naked, swimming (air 13 C) | water 12.1 C | 0.08 | 4 s | 17 s | 48 s | 23.0 C |
| clothed, in water (air 13 C) | water 12.1 C | 0.14 | 4 s | 20 s | 56 s | 23.8 C |
| naked, swimming (air 25 C) | water 20.5 C | 0.08 | 10 s | 48 s | 4 min | 31.4 C |

**Diagnosis (the layer): the model has no thermal mass and treats insulation as a threshold.** The 0.01/s coupling is a ~100 s time constant for a 70 kg body, and the fixed 0.1 C/s "regulation" either fully cancels the loss (insulation above ~0.78 at 0 C: the core never moves, at any cold) or is overwhelmed (below it: minutes to severe hypothermia). Real cooling takes hours in air and tens of minutes in cold water, and clothing changes the RATE continuously -- it does not flip the outcome. This is why the backlog reports "very rapid cooling", and also why a clothed pawn can stand in -25 C indefinitely.

## Proposal: a heat balance (for the checkpoint, not built)
One model, used for air and water alike (the existing water coupling and insulation sources plug in):

```
C * dTcore/dt = M_basal + M_activity + M_shiver - Q_loss          [W]
C          = body mass * 3.49 kJ/(kg K)       (70 kg -> ~244 kJ/K)
Q_loss     = A * (Tskin - Tenv) / (R_clothing + R_boundary)      (+ evaporation / respiration, small in cold)
A          = body surface (~1.8 m2, DuBois from height / mass)
R_clothing = clo * 0.155 m2K/W      (per-item clo values replace the 0..0.4 Warmth sum; wet clothing loses most of its clo)
R_boundary ~ 0.1 m2K/W in still air (h ~ 9 W/m2K), falling with wind; ~0.003-0.01 in water (h ~ 100-300)
Tskin      = a second (shell) node, or Tcore - vasoconstriction offset: cold skin cuts the loss (this is what makes cold water a 30-60 min problem, not 30 s)
M_shiver   = 0 .. ~250-400 W, rising as Tcore / Tskin fall; costs glucose / energy from UMOMetabolismComponent (couples to the existing cascade)
```
Orders of magnitude this should land near (to be checked against primary sources -- ISO 11079 / the Gagge two-node model / immersion survival data -- before tuning):
- naked, still air 10 C, resting: shivering roughly balances the loss; a slow decline (~1-2 C per hour) without shelter;
- 12 C water, light clothing: core 37 -> 35 C in roughly 30-60 min (lean bodies faster, more body fat slower); cold-shock and swim failure come earlier than core hypothermia;
- warm winter clothing (~2.5-3 clo) at 0 C: indefinite at rest, cooling when wet or still for long;
- exertion produces heat (a working pawn stays warm in cold air -- the existing activity level feeds M_activity).

## Built Oct 19 2026: the single node (F2 interim; Wes: "Ship single node now")
`UMOVitalsComponent::AdvanceCoreTemperature`: `C dTcore/dt = M - Q_loss`, C 245 kJ/K, Q_loss = 20 W/K x (1 - insulation) x (Tcore - Tfelt),
M = 100 W resting + shivering up to 200 W when the loss needs it (sweating sheds up to 400 W the other way); the core moves only by what
the body cannot answer. Numbers in `Docs/Medical_And_Survival.md` §4. Not built yet: the shell node, clo values, glucose for shivering,
exertion heat -- the decisions below still stand for those.

## Decisions for Wes
1. **Fidelity:** single node with real thermal mass (simplest, fixes the time scale) vs core + shell (needed for believable cold water and vasoconstriction). Recommendation: core + shell -- cold water is a headline hazard in this world (lakes, swimming, drowning already exist).
2. **Clothing data:** move items from a 0..1 "Warmth" scalar to clo values (+ a wet multiplier). DataTable migration through `Tools/ue_csv_utils.py`.
3. **Shivering cost:** shivering burns glucose / calories through the metabolism component (realism pillar) -- confirm.
4. **Tests that must hold:** the scenarios above as automation tests (rates, not just end states), clock scaling and step-size independence, recovery when warmed, save/load of core + skin temperature, and authoritative replication (the existing H12 rules).
