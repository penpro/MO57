# AI, survivors and the colony

> **Covers:** creature AI (prey and predators), survivor pawns (recruitment, jobs, the village loop), the colony manager's upkeep
> passes, possession of any pawn, and the planned colony management UI.
> **Current state (2026-10-17):** creatures are deer (prey) and wolf (predator) on behavior trees; survivors run a C++ simple-job
> state machine in `AMOSurvivorController` (NO behavior-tree assets for survivors); the July village slice is live (craft jobs,
> quotas, skill decay / schools, relationships, seasons, courtship, real children, winter shelter, fire heat and refuel hauling);
> the colony UI is partly built (history, personality, colony manager, overview, portrait exist; colony bar, task assignment UI and
> alert tiers do not).
> **Entry points:** `AMOCreature` / `AMOPreyCreature` / `AMOPredatorCreature`, `AMOCreatureController`, `AMOSurvivorController`,
> `UMOSurvivorJobQueueComponent`, `UMORecruitmentComponent`, `UMOColonyManagerSubsystem` (`RunQuotaPass`, `RunHearthPass`,
> `RunExcavationPass`, `CanRecruitByStanding`), `UMOPersonalityComponent`, `UMOCharacterHistoryComponent`, `UMOPossessionSubsystem`,
> `UMOSpawnManagerSubsystem`. **Dev verbs:** `MO.Colony.Found|SpawnSurvivor|Recruit|PlaceBuilding|Stock|SetSkill|AssignJob|UI|SendTo|Status`,
> `MO.AI.DumpBlackboard <pawn>`, `MO.AI.SetKey`, `MO.AI.StressSpawn`, `MO.Spawn.Try`, `MO.Possess.List|Take`.
> **Tests:** `MOFramework.Colony.*` (incl. `Colony.Recruit.GateMath`, `Colony.Excavation.*`), `MOFramework.Creature.*` (hearing,
> pursuit, footfalls) and `MOFramework.Combat.Damage.*` (fist vs vital parts, instigator, armor penetration), live gates `Content/Python/test_village_*.py`,
> `test_fire_heat.py`. **Open items:** `Docs/BACKLOG.md` § AI, colony, combat. **North star:** `Docs/MedievalDynasty_Village_Reference.md`;
> colony design: `Docs/MO57_Colony_Management_Design.docx`.

## 1. Creatures

### Creature AI architecture (TECHNICAL_REFERENCE, May 2026, verbatim)

### Class Hierarchy

```
AMOCharacter → AMOCreature → AMOPreyCreature (deer: flee-first)
                           → AMOPredatorCreature (wolf: hunt, flee when wounded)
```

Single controller: `AMOCreatureController` with `UAIPerceptionComponent` (sight + hearing).

### Behavior Trees

- **BT_Prey**: Death → Flee (ShouldFlee AND HasTarget) → Threatened (fight if cornered) → Idle (wander/graze)
- **BT_Predator**: Death → Flee (wounded) → Combat (chase/attack) → Hunt (lost target) → Idle/Patrol
- *As read from the assets (Oct 18 2026):* **BT_Deer** / BT_Prey = a selector of `HasTarget` (abort Both) -> `FleeFromThreat`
  (legs of 20-30 m, ThreatActorKey = TargetActor) and `CreatureWander` (MaxDistanceFromHome 40 m): a deer flees whenever its
  controller holds a threat; health plays no part. BT_Predator = `HasTarget` -> `ChaseTarget` / `IsInAttackRange` -> `CreatureAttack`
  (AM_WolfBite), else wander.

### Animation on every machine (Oct 20 2026, N-169)

- **A creature animation is a replicated cue:** `AMOCreature::PlayAnimationEverywhere(Anim, Rate)` (server) plays it there and sets `AnimCue`
  (animation, rate, server time, serial); each client plays it in `OnRep_AnimCue`, skipping a cue more than 1 s old (a join, the creature
  coming into relevance). A montage plays as itself, a sequence in the ABP's `DefaultSlot`. Before, the bite (`BTTask_CreatureAttack`) was a
  `Montage_Play` on the server's AnimInstance: a joiner saw a wolf glide up and hurt it with no bite. Any new creature animation goes through here.
- **Hit reactions:** a blow with an attacker (`UMOAnatomyComponent`'s instigator; none = the environment, no flinch) plays the definition's
  `HitReactFront / Left / Right / Back` for the side it came from (`AMOCreature::ClassifyHitSide`: Front / Back within 45 deg of the nose, flat;
  missing sides fall back to Front). Not while a montage plays (mid-bite: the bite's damage lands on the task's own timer, so a flinch would
  only hide it) and once per 0.3 s (one blow, several parts). Data: `DT_Creatures` Wolf01 -> `ANIM_Wolf_GetHitFront / Left / Right` (0.67 s);
  Deer_Doe has none: **`ABP_DeerDoe` has no `DefaultSlot` node** (wolf's has), so a deer would show no montage at all -- add the slot in the graph
  editor (not scriptable; N-096), then its row. Proof: `nettest actions` `_client_sees_creature_flinch` (the joiner's log: "anim cue N on this
  client: ANIM_Wolf_GetHitLeft", then AM_WolfBite); `MOFramework.Creature.HitSide`.
- **A killed wolf leaves a carcass** (`DT_Carcasses` "Wolf": the deer's parts at a ~40 kg animal's yields; DECISIONS Oct 20). Before, it
  ragdolled and left nothing.

### Senses, wounds and the chase (Oct 18 2026)

- **Hearing** works from Oct 18: every creature sense uses `AMOCreatureController::SenseAllAffiliations()`. The hearing sense compares
  raw team ids and the engine's default solver calls NoTeam vs NoTeam Friendly, so with friendlies off (as before) no creature ever
  heard a footstep. MO57 has no teams; `OnTargetPerceptionUpdated` decides what is a threat (same class ignored).
- **Footfalls** (`AMOCharacter::GetMovementNoiseLoudness`, every 0.3 s while moving, server): the loudness follows the pawn's ACTUAL
  speed through its own gaits (walk 0.3 at WalkSpeed, jog 0.6, sprint 1.0; below the walk proportionally quieter; crouched x0.5), heard
  within HearingRange x loudness. A deer (HearingRange 20 m) hears a sprinting player or a running wolf at 20 m, a walk at 6 m, a
  crouched creep at ~1.5 m. A wolf used to be stuck at the walk value (its gait state is player-only).
- **A wound names its cause**: every hit carries its attacker (`FMOCombatHitInfo::Instigator`, the wolf's bite passes the wolf) into
  `UMOAnatomyComponent::InflictDamage(..., DamageInstigator)`, kept as `GetLastDamageInstigator()`. A creature's death names it as the
  killer (was always None).
- **A wounded animal runs** (`AMOCreatureController::NotifyDamaged`, from `AMOCreature::HandleBodyPartDamaged`): whoever hurt it is
  its threat for `WoundedPanicSeconds` (definition, 45 s) whether or not it was ever seen or heard, so the BT's flee legs chain: a deer
  runs ~340 m at 7.5 m/s (measured Oct 18: a punched deer ended ~220 m from the player, the legs zig-zag), then wanders. A predator
  hurt the same way turns on its attacker.
- **A predator's chase is leashed** (`UpdatePursuit` / `JudgeChase`, 5 Hz): it gives up past `ChaseLeashRadius` (200 m) from home (its
  den: where it spawned) or after `ChaseNoGainSeconds` (20 s) without closing in by 2 m, then ignores that quarry for
  `ChaseGiveUpCooldownSeconds` (120 s) unless it is hurt by it; wander then walks it home. Wolf and deer both run 750 cm/s, so a healthy
  deer that hears the wolf in time escapes.
- The four numbers are `FMOCreatureDefinitionRow` fields (defaults apply to rows that do not set them; `bIsPredator` turns the leash on).

### Melee damage (Oct 18 2026)

- **The blow**: `UMOCombatComponent::ProcessHit` -> `BaseDamage` x distance falloff (1.0 at the body, 0.7 at full range) -> the target's
  `ReceiveAttack` -> `UMOCombatMedicalHelpers::ApplyCombatDamage` -> `InflictDamage(part, BaseDamage x CriticalMultiplier, wound type of
  the damage category)` -> the part loses that x the wound's `BaseDamageMultiplier` (DT_Wounds: Blunt 0.8). A creature dies when a vital
  part (`AMOCreature::IsVitalPart`: head, brain, torso, heart, spine C/T, liver, stomach) reaches 0 HP (DT_BodyParts: torso 200, head
  150, liver 60, brain / stomach 50, heart 30; one table for people and animals).
- **The fist** is the class default (`UnarmedAttackProfile`: 5 Blunt, range 100 cm): about 4 HP off a part per blow. BP_MOMetaHuman_Female
  (every survivor) overrode it with 500 from a Feb 26 debug session (3452e7cb), so one punch destroyed a deer's torso; reset Oct 18.
  `MOFramework.Combat.Damage.BareHandCannotDestroyVitalPart` checks every survivor pawn's own fist against the vital parts.
- **ArmorPenetration** only matters through armor ("0 = stopped by any armor, 1 = ignores armor"): `GetFinalDamage` no longer multiplies
  by it (no hit carries target armor yet). It did: a wooden spear's 10 at 0.1 landed 1, a fifth of a punch.
- **The weapon in the hand**: the item in the right-hand equipment slot IS the main-hand weapon (`SyncMainHandFromEquipment`, server, on
  every right-hand change and at BeginPlay): an item with `bIsWeapon` + `WeaponProfileId` (DT_Weapons) is equipped and drawn; anything
  else fights bare. Nothing called `EquipMainHandWeapon` before, so every spear fought with the fist; the five spear rows in DT_Items
  were also not weapons (`bIsWeapon` false, slot None) until Oct 18. A weapon wears only when a blow lands, on the held item's own
  durability (a worn-out item leaves the hand; an item that keeps none, like the spears, wears the profile's MaxDurability counter).
  Measured Oct 18 (solo instance, deer frozen in front): fist 3.9 -> 3.1 HP off the torso; wooden spear light thrust 8.3 -> 10.0 HP
  (Puncture x1.2) with a bleeding puncture wound; before the reset the fist did 386.8 -> 309.4 and killed it.

### AI Known Issues

- Hardcoded blackboard key strings (M1) — use `FBlackboardKeySelector`
- Magic numbers for thresholds (M2) — extract to creature definition or config
- `IsCornered()` is placeholder (M3) — should use `EnvQueryTest_EscapeRoute`
- `FindPackMembers()` uses `GetAllActorsOfClass` (H6) — needs spatial registry
- Activity types not unified between player/creature (M6)

### BT Tasks (11) and Services (3)

Tasks: CreatureAttack, CreatureRest, CreatureWander, FleeFromThreat, ChaseTarget, SurvivorForage, SurvivorGather, SurvivorGoHome + 3 more
Services: CreatureActivity, UpdateCreatureState, SurvivorJobProcessor

**As built since (Oct 2026):**
- **Perception was never registered** until Oct 13: a `UAIPerceptionComponent` with no senses at OnRegister is never registered by the
  engine; call `RequestStimuliListenerUpdate()` before `ConfigureSense`. `AMOCreatureController` also primes blackboard values in Tick the
  first frame the asset exists (pushing them in OnPossess, before the BT bound the blackboard, logged bogus errors). Verify with
  `MO.AI.StressSpawn 3 1200 2000` + `MO.AI.DumpBlackboard Deer|Wolf`.
- **Runtime assets preload:** `AMOCharacter/AMOAIController::GatherRuntimeAssets` (virtual, CDO-callable); the spawn manager async-loads
  them at the first spawn check. Pattern for any soft asset resolved at spawn / possession / death: declare it there, never
  `LoadSynchronous` at the spawn.
- **Fallback spawns** use `MOSpawnClearance::TraceVoxelGround` + `IsUnderOpenWater` + `IsStandingSpotClear` (no sea-floor deer).
- **Sleep is real-clock:** frozen mobs stay asleep until the recheck timer fires (`Docs/ENGINEERING_PRINCIPLES.md` #11 example).
- The flee decision is computed twice (`BTService_UpdateCreatureState` and `AMOCreatureController::ShouldFlee`); unify before adding
  inputs such as fire-fear (`Docs/Light_Sources.md`).
- Creature ABP state machines (locomotion / rest / sleep / death) and EQS query assets (`EQ_FindHarvestableItems`,
  `EQ_FindHarvestTargets`, `EQ_FindEscapeRoute`) still need Blueprint setup (C++ complete).
- Only deer and wolf exist; the animal pack has fox / pig / crow assets but new fauna needs ABP work. Creature footsteps need foot
  bones (`ball_l` / `ball_r`) the skeletons lack.

## 2. Survivors and the village loop

- **Survivor AI = `AMOSurvivorController`'s C++ simple-job state machine** (bands: craft 10-15, refuel 20-23, excavate 30-33).
  There are NO behavior-tree assets for survivors in Content: don't look for them.
- Jobs: `UMOSurvivorJobQueueComponent` guards every mutator (a model implementation to clone). The haul pattern (fetch into pack ->
  deliver -> station-specific handling) is `RefuelStation`; `ExcavateAndHaul` mirrors it (`Docs/Terraform_Excavation_Plan.md`).
- **Village slice (July 4-7, all gated green):** V0 one villager one real job; V1 settlement loop (feeding, mood, housing); V2 quotas
  -> skill decay / schools -> relationships -> seasons -> dynasty courtship / marriage / REAL children -> winter shelter; F1 fire
  units 1-4: fire is warmth (`IMOLocalHeatSource`), dries clothes (~11x), cold villagers seek lit hearths, the hearth pass issues
  RefuelStation jobs (firewood demand joined food demand). Station AddFuel is transactional (returns the accepted count).
- **A pawn's age** (Oct 19): `FMOPersistedPawnRecord::DefaultAgeInDays(Guid)` (18-40 years, fixed by the GUID) wherever a record is made without
  one -- the save capture, the possession list's live-world rows, the game mode's spawn registration. Before, only game-mode spawns got an age;
  the player's own pawn, recruits and wild survivors read "0 years" in the Characters list. Gender stays "Unknown" until F15.
- **Recruitment** (Oct 10): `CanRecruitByStanding` judges trust built by shared game time (2 % per hour within 12 m) only for a colony
  of >= `RecruitGateMinRoster` (3); the first survivors just join. `BeginInteractionDetailed` returns an `EMORecruitmentOutcome` with
  the numbers, and `AMOPlayerController::ClientShowNotification` tells the player why. No quest appears because no survivor has
  `PossibleQuests` configured (a data task). MO.Colony survivors persist (recruited -> captured).
- **Delegate tedium to pawns** is the killer feature (`Docs/ENGINEERING_PRINCIPLES.md`): every system the player can do should be
  plumbed to be pawn-assignable.

### Job Assignment (UMOSurvivorJobQueueComponent)

```cpp
FGuid EnqueueJob(EMOSurvivorJobType, int32 RepeatCount = 1);
FGuid EnqueueJobAtLocation(EMOSurvivorJobType, FVector, int32 RepeatCount = 1);
FGuid EnqueueJobWithTarget(EMOSurvivorJobType, AActor*, int32 RepeatCount = 1);
bool CancelJob(const FGuid& JobId);
void CancelAllJobs();
```

**A dead survivor stops** (Oct 19): `AMOCharacter::EnterDeadState` (server) cancels its job queue and removes its AI controller; `UnPossessed`
never gives a corpse a new AI. Before, a dead survivor's controller kept working its queue.

**A failed job says why** (Oct 18, N-090): a failed job leaves the queue at once, so it used to vanish with nothing said.
`CompleteCurrentJobWithReason(bSuccess, Why)` records `LastFailure` (job name + reason, replicated; a success clears it), and every player
gets one notice ("Harper couldn't gather sticks: found nothing to gather nearby"), at most once a minute per survivor and message. The
task menu shows it while the survivor is idle ("Idle. Couldn't refuel station."). The controller's `FailSimpleJob(Why)` tags the
meaningful sites (`MOSurvivorJobWhy`: nothing to gather, couldn't get there, station / storage gone, ingredients not in storage, craft
cancelled, nothing left to dig, and the crafting rules' own refusal for a craft, which names a missing tool); internal failures report
none. Test `MOFramework.Colony.JobFailureKeepsItsReason`; host + joiner: the joiner's order failed, both players got the notice, the
joiner's task menu said why.

### Colony Membership (UMORecruitmentComponent)

`EMORecruitmentState::Recruited` = colony member. `IsPossessable()` returns true for recruited pawns.

## 3. Possession

- Player can possess any pawn they control (`IsPossessable()` = recruited); idle pawns run on AI. The possession menu never calls
  `Possess()` (authority-only): `UMOPossessionComponent::Request*` -> controller RPCs -> `UMOPossessionSubsystem`
  (`IsPawnAvailableTo` is the one definition of "available"). The menu shows occupancy ("Current survivor" / "Played by <Steam name>",
  `EMOPawnOccupancy`; Wes, Oct 15). Co-op details: `Docs/Coop_And_Networking.md` §2.
- Known coupling: `MOPossessionSubsystem` requires `UMOIdentityComponent` + `UMOInventoryComponent` (TECHNICAL_REFERENCE, coupling HIGH).
- **"While you were away"** (N-091, Oct 20): a pawn's `UMOCharacterHistoryComponent` stamps the game time when a player leaves it
  (`HandleControllerChange`, from `AMOCharacter::NotifyControllerChanged` on the server, read before the engine overwrites
  `PreviousController`); when a player takes it back, that player gets one notice: "While you were away (2 h 10 min):" and the pawn's
  history entries since, the newest four (`BuildAwaySummary`, pure). Nothing recorded since = no notice. Survivor jobs now write their
  result into the history ("Finished: Gather Sticks", "Gave up: Chop Tree (why)"), next to the colony's activity / mood / relationship
  entries. The stamp is not saved (a reload starts fresh). Proof: `MOFramework.Colony.AwaySummary`; `nettest actions`
  `_host_takes_departed_pawn`.

## 4. Colony management (planned, partly built)

PROJECT_STATUS (Oct 13 freshness note): `UMOCharacterHistoryComponent`, `UMOPersonalityComponent`, `UMOColonyManagerSubsystem`,
`UMOColonyOverviewWidget`, `UMOColonyPortrait` exist; the colony bar (4B), task assignment without possession UI (6B) and the alert
tier system (7B) do not. `UMOCharacterCardWidget` and `UMOTaskAssignmentWidget` below are planned names, not classes.

### Colony Management System (Planned) (CLAUDE.md until 2026-10-17, verbatim)

*Full design in `Docs/MO57_Colony_Management_Design.docx`, implementation in `Docs/Archive/MO57_Master_Plan.md`*

**Core Components**:
| Component | Purpose |
|-----------|---------|
| `UMOColonyManagerSubsystem` | Alert queue, character enumeration, task delegation |
| `UMOPersonalityComponent` | Character personality traits (Conscientiousness, Sociability, Stability) |
| `UMOCharacterHistoryComponent` | Event log, relationship tracking, mood/activity summaries |
| `UMORecruitmentComponent` | Tracks recruitment state (existing - determines colony membership) |
| `UMOSurvivorJobQueueComponent` | Job queue management (existing - task assignment API) |

**Colony UI Widgets**:
| Widget | Layer | Purpose |
|--------|-------|---------|
| `UMOColonyBarWidget` | HUD | Persistent character strip with portraits |
| `UMOColonyOverviewWidget` | Menu | Full colony management screen (replaces/extends possession menu) |
| `UMOCharacterCardWidget` | - | Character detail view (mood, history, relationships, skills) |
| `UMOColonyPortrait` | - | Reusable portrait with mood expression, activity, alert state |
| `UMOTaskAssignmentWidget` | - | Task picker and job stack management |

**Alert Tiers**:
| Tier | Name | Examples | Display |
|------|------|----------|---------|
| 1 | Critical | Health <15%, combat while away | Pulsing red border, sound, cannot dismiss |
| 2 | Urgent | Health <40%, idle >30min | Orange dot on portrait |
| 3 | Notable | Task complete, skill gained | Colony log only |
| 4 | Log | Routine activities | Character history only |

**Personality System (UMOPersonalityComponent)**:
- **Conscientiousness**: Diligent (methodical, slower, higher quality) vs Adaptable (quick, shortcuts, variable)
- **Sociability**: Social (performs better near others) vs Reserved (performs better alone)
- **Stability**: Stable (consistent mood) vs Volatile (strong mood swings, high ceiling/floor)

**Key Design Principles**:
1. Colony overview is a "window into community" not just a "management interface"
2. Characters should feel like people with opinions, not interchangeable labor
3. Trust is earned through demonstrated reliability
4. Alert tiering prevents notification fatigue while surfacing critical issues
5. Task assignment works without possession via `UMOSurvivorJobQueueComponent`

### Colony management design (TECHNICAL_REFERENCE, May 2026, verbatim)

### Personality System (UMOPersonalityComponent)

Three axes, float -1.0 to 1.0:
- **Conscientiousness**: Diligent ↔ Adaptable
- **Sociability**: Social ↔ Reserved
- **Stability**: Stable ↔ Volatile

### Alert Tiers

| Tier | Name | Examples | Display |
|------|------|----------|---------|
| 1 | Critical | Health <15%, combat while away | Pulsing red, sound, cannot dismiss |
| 2 | Urgent | Health <40%, idle >30min | Persistent orange dot |
| 3 | Notable | Task complete, skill gained | Colony log only |
| 4 | Log | Routine activities | History only |

## 5. Gotchas that bit (keep)

- `SetStayAtLocation` SUSPENDS job processing: call `AbortCurrentJob()` FIRST, or the in-flight job and its quota slot wedge forever.
- Teleporting a pawn to ~1 m of a building lands INSIDE its collision hull; depenetration shoves it out past e.g. a fire's heat radius.
  Gates stand 2.5 m out; AI nav projection is immune.
- Fast jobs start AND finish between gate polls: assert on evidence (stock deltas), not "current job" sampling.
- Un-recruited survivors don't run job move-legs: gates FOUND + RECRUIT first.
- Plain C++ interface bases can't be downcast under UE no-RTTI: iterate the concrete actor type.
