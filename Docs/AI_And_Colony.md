# AI, survivors and the colony

> **Covers:** creature AI (prey and predators), survivor pawns (recruitment, jobs, the village loop), the colony manager's upkeep
> passes, possession of any pawn, and the planned colony management UI.
> **Current state (2026-10-26):** creatures are deer, pig, fox, boar and rabbit (prey) and wolf and bear (predator) on behavior trees, plus
> goat, sheep, cow and chicken built but not spawned (F43); every species after deer and wolf is data (`UMOCreatureAnimInstance`, no AnimBlueprint); a spawned group is one species of its own size, together; survivors run a C++ simple-job
> state machine in `AMOSurvivorController` (NO behavior-tree assets for survivors); the July village slice is live (craft jobs,
> quotas, skill decay / schools, relationships, seasons, courtship, real children, winter shelter, fire heat and refuel hauling);
> the colony UI is partly built (history, personality, colony manager, overview, portrait exist; colony bar, task assignment UI and
> alert tiers do not).
> **Entry points:** `AMOCreature` / `AMOPreyCreature` / `AMOPredatorCreature`, `AMOCreatureController`, `AMOSurvivorController`,
> `UMOSurvivorJobQueueComponent`, `UMORecruitmentComponent`, `UMOColonyManagerSubsystem` (`RunQuotaPass`, `RunHearthPass`,
> `RunExcavationPass`, `CanRecruitByStanding`), `UMOPersonalityComponent`, `UMOCharacterHistoryComponent`, `UMOPossessionSubsystem`,
> `UMOSpawnManagerSubsystem`. **Dev verbs:** `MO.Colony.Found|SpawnSurvivor|Recruit|PlaceBuilding|Stock|SetSkill|AssignJob|UI|SendTo|Status`,
> `MO.AI.DumpBlackboard <pawn>`, `MO.AI.SetKey`, `MO.AI.StressSpawn`, `MO.Spawn.Try`, `MO.Spawn.Survivors`, `MO.Possess.List|Take`,
> `MO.Test.SpawnCreature <Species> [DistanceCm] [SideCm]`, `MO.Test.SpawnGroup <Species> [DistanceCm]`.
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

- **A creature animation is a cue event:** `AMOCreature::PlayAnimationEverywhere(Anim, Rate)` (server) plays it there and sends
  `MulticastAnimCue` (the animation's PATH, rate, serial, stop; RELIABLE); each client applies the cues in the server's order.
  `StopAnimationEverywhere(Anim)` stops that one montage, only if it is still playing on the server: the bite's abort used to stop
  EVERY montage, a frame after the flinch a blow had just started. It was a replicated property until Oct 21 (a flinch then a bite in
  one net update reached a joiner as the bite alone). The path, not an object reference: a reference to an animation the joiner had
  not loaded arrived as None; a client loads it (high priority) and applies it then unless a newer cue came ("loading X first"), and
  loads what a cue or the carcass can name when the creature arrives (`AMOCreature::GatherClientAssets`: the hit reactions and the
  carcass table). No clock comparison: a client's estimate of the server's game time was 1.5-1.9 s off. Test a blow with
  `MO.Test.HitCreature [player] [damage] [class]` (a tick later): an `inflict_damage` from editor Python runs the flinch cue on the host only. A montage plays as itself, a sequence in the ABP's `DefaultSlot`. Before, the bite (`BTTask_CreatureAttack`) was a
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
- **A pack hunts and fights together** (Oct 20, N-096): a predator that is hurt, or takes a new quarry it perceives, calls the wolves of
  its kind within `PackAlertRange` (15 m): `AMOPredatorCreature::AlertPack` -> each one's `AMOCreatureController::NotifyPackThreat`,
  which turns it on the threat for `PackCallMemorySeconds` (20 s; its own senses take over once it sees it). It never re-calls, and
  never turns on its own kind. AlertPack existed but nothing called it, and it wrote blackboard keys the controller rewrote the next
  frame. Host-side test `_host_wolf_pack`.

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
- **One flee decision** (Oct 20): `AMOCreatureController::ShouldFlee` (the species row's `FleeThreshold`); `BTService_UpdateCreatureState` and
  `AMOPredatorCreature::ShouldFlee` ask it (each kept its own 0.3 before).
- **Fire-fear** (N-096, Oct 20): a creature will not close on a target within its species' `FireFearRadiusCm` of a flame (a carried torch or
  a campfire: the flames `UMOAmbientEnvironmentRegistry` knows, `AMOCreature::IsWaryOfFireAt`). `BTTask_ChaseTarget` stops about 6 m short
  and fails ("[MOCreature] Wolf_C_0 holds off from X: fire within N cm"); the tree wanders it and tries again, so it circles; and
  `BTTask_CreatureAttack` makes no bite within the radius of a flame. Wolf01 400 cm, Bear01 500; the deer 0 (prey flees threats anyway).
  **Unless attacked** (Wes, Oct 27: predators stay away from fires "unless attacked and brought into range of fires"): a predator the
  person attacked, or whose pack they attacked, while its anger lasts (`WoundedPanicSeconds`, the bear's 30 s; `AMOCreatureController::IsProvokedBy`) fights on through the fire
  (until Oct 27 a wolf hit once still held off). Proof: `nettest actions` `_host_predator_fire_rules` (a hunting bear holds off a torch;
  hit, it bites through the fire; control: the torch out, a fresh bear bites).
- **Aggro range** (Oct 27): a predator goes for someone it was not attacked by only within its species' `AggroRange` (Wolf01 20 m --
  in practice its 15 m sight -- Bear01 6 m: F46, Wes: a bear is a predator "just a smaller agro range"); prey flee whatever they see.
  Until Oct 27 no code read `AggroRange`: whatever a predator saw was quarry. Perception reports only CHANGES in what is seen, so a
  pursuer with no threat also looks at what it sees five times a second (`AMOCreatureController::ScanForQuarry`): someone seen far off
  and walking in is taken at the range, and a quarry forgotten in plain sight can be taken again. One gate for both:
  `MayTakeAsQuarry` (another kind of pawn, in the world, not given up, outside the start area, within the aggro range unless attacked).
  Proof: `Tools/check_creator_bases.py` (a bear spawned 10 m off, which sees the survivor, takes it only once within 6 m -- Oct 27 it
  wandered in and took it at 596 cm; every new quarry is logged: "[MOCreatureController] X takes Y as its quarry at N cm (aggro range M cm)").
- **The start area** (Wes, Oct 27: predators "stay away from the starting area, like 200m"): no predator spawns (both spawn paths),
  wanders (`BTTask_CreatureWander` turns it away from the site), takes a quarry, chases (its controller gives the chase up) or bites within
  `UMOStartSiteSettings::PredatorFreeRadiusCm` (200 m) of the start site -- the same "unless attacked" exception
  (`AMOCreature::KeepsOutOfStartAreaAt`). No placed site, no area (old saves, test worlds); `MO.AI.PredatorFreeAreaM <m>` overrides it
  (0 = none). And a survivor in the character creator is no creature's quarry (`AMOCharacter::IsInCharacterCreator`). Proof:
  `Tools/check_creator_bases.py` (a bear 6 m off in the creator and at the start leaves the survivor alone; hit, it fights back).
  While it holds off, its chase clock waits (`AMOCreatureController::UpdatePursuit`): circling a fire is not failing to gain, so when
  the flame goes out the still-angry creature closes in instead of giving up at once (Oct 22).
- Creature ABP state machines (locomotion / rest / sleep / death) and EQS query assets (`EQ_FindHarvestableItems`,
  `EQ_FindHarvestTargets`, `EQ_FindEscapeRoute`) still need Blueprint setup (C++ complete).
- **A new species is data (Oct 26; Wes: "let's get ... some other animals going").** `UMOCreatureAnimInstance` (native, no graph)
  blends the creature's `IdleAnimation` / `WalkAnimation` / `RunAnimation` by its speed (`FMOCreatureLocomotionBlend`: standing ->
  the walk at its own speed -> the run at its own, then the run faster up to `MaxRunPlayRate`; walk and run share one stride clock)
  and evaluates "DefaultSlot", so the cues (`PlayAnimationEverywhere`: hit reactions, a bite) play over it as in an AnimBlueprint.
  A species = a data-only Blueprint under `/MOFramework/MOBS/<Species>/BP_<Species>` (parent `BP_Prey` / `BP_Predator`; mesh,
  capsule, `AnimClass = MOCreatureAnimInstance`, the three cycles and their ground speeds measured from the pack's `_RM` twins) + a
  `DT_Creatures` row + a `DT_Carcasses` row + its class in the spawn category (`Config/DefaultGame.ini` `MOSpawnSettings`).
  Pig (`BP_Pig`, `Pig01`: walk 1.02 m/s / run 6.56 m/s cycles, a sounder of 3-6, ~90 kg: 27 meat, hide, bladder) and fox (`BP_Fox`,
  `Fox01`: 0.45 / 2.63 m/s cycles played up to 2.5x, alone, keen hearing 30 m, wary of fire, a pelt and 2 meat) are in the prey list.
  The crow is not: a bird needs flight (its own movement and AI). Deer and wolf keep their AnimBlueprints.
- **The Animals Full Pack species (Oct 26, N-221, wired on DARKTOWER).** PROTOFACTOR's pack is local-only (`Content/AnimalsFullPack`,
  gitignored; copy it from DARKSIDE). The Blueprints and their assets are tracked under `/MOFramework/MOBS/<Species>/`. Every mesh faces +Y
  with its pivot at the feet, so the mesh sits at yaw -90 and z = -capsule half-height. Cycle speeds are measured from each `_RM` twin's
  root track. Rows: `Docs/RowPatches/2026-10-26-animals-DT_Creatures.json` / `-DT_Carcasses.json`. Meat runs at the pig's ~0.3
  `RawMeat01` (0.5 kg) per kg of live weight.

  | Species | Blueprint / row | Cycles walk / run (m/s), max rate | Group | Weight -> meat | Spawn list |
  |---|---|---|---|---|---|
  | Wild boar | `BP_Boar` / `Boar01` | 0.83 / 6.25, 1.5x (runs 8) | 2-6 | ~80 kg -> 24, hide, bones | prey |
  | Brown bear | `BP_Bear` / `Bear01` (`AMOPredatorCreature`) | 2.14 / 4.22, 2.4x (charges 10) | 1 | ~250 kg -> 75, hide | predator |
  | Rabbit | `BP_Rabbit` / `Rabbit01` | 0.99 / 1.74 (gallop), 3x (runs 5.2) | 1-3 | ~1.8 kg -> 1, hide | prey |
  | Goat | `BP_Goat` / `Goat01` | 0.60 / 3.53, 1.7x | 3-8 | ~50 kg -> 15, hide, 2 horn | none (F43) |
  | Sheep | `BP_Sheep` / `Sheep01` | 0.49 / 3.00, 1.85x | 4-10 | ~60 kg -> 18, fleece, hide, 2 horn | none (F43) |
  | Cow | `BP_Cow` / `Cow01` | 1.00 / 6.18, 1.1x | 4-10 | ~450 kg -> 135, hide, 2 horn | none (F43) |
  | Chicken | `BP_Chicken` / `Chicken01` | 0.30 / 1.63, 2.5x | 3-8 | ~2 kg -> 1, 3 feathers | none (F43) |

  - **The bear's bite.** `BT_Predator`'s attack node plays the wolf's `AM_WolfBite`, which cannot play on another skeleton. So the bear runs
    `MOBS/Bear/BT_Bear`, a copy whose attack node plays `MOBS/Bear/AM_BearBite` (from `ANIM_Bear_BiteAttack4Legs`, DefaultSlot) at 40
    damage and 2 m. The attack task deals its own `BaseDamage`, not the row's. A new predator needs its own tree the same way.
  - **Verified (one -game instance, seed 777).** Each species loads with its mesh, the native anim instance and an `AMOCreatureController`,
    and stands on the ground. A blow plays its hit clip from the side it came, except the rabbit, whose pack has no hit clip. It then runs
    at its row's run speed with the legs cycling (12-68 cm of bone motion between samples), and a kill leaves its carcass. A hurt bear
    charges at 10 m/s and bites with `AM_BearBite`.
  - **Not wired, by design:** the African animals, the sea animals and the birds of the pack. The list and recommendations are in the
    N-221 handoff note.
- **A spawned group is one species, together (Oct 26).** The spawner picks the group's class once; its size is the species' own
  (`FMOCreatureDefinitionRow::Min / MaxGroupSize`: deer 2-5, pig 3-6, fox 1; 0 = the category's) and the members stand 3-12 m from
  the first (`UMOSpawnManagerSubsystem::SpawnGroupMemberNear`; the category's entity separation, hundreds of metres, is for groups).
  Before, every member was a random class and its own spawn point: a "group" was strangers far apart. `MOFramework.Spawn.GroupSizeIsTheSpecies`.
- Creature footsteps need foot bones (`ball_l` / `ball_r`) the skeletons lack (they step by ground covered).

## 2. Survivors and the village loop

- **Survivor AI = `AMOSurvivorController`'s C++ simple-job state machine** (bands: craft 10-15, refuel 20-23, excavate 30-33, treat 40-43).
  There are NO behavior-tree assets for survivors in Content: don't look for them.
- Jobs: `UMOSurvivorJobQueueComponent` guards every mutator (a model implementation to clone). The haul pattern (fetch into pack ->
  deliver -> station-specific handling) is `RefuelStation`; `ExcavateAndHaul` mirrors it (`Docs/Terraform_Excavation_Plan.md`).
- **Village slice (July 4-7, all gated green):** V0 one villager one real job; V1 settlement loop (feeding, mood, housing); V2 quotas
  -> skill decay / schools -> relationships -> seasons -> dynasty courtship / marriage / REAL children -> winter shelter; F1 fire
  units 1-4: fire is warmth (`IMOLocalHeatSource`), dries clothes (~11x), cold villagers seek lit hearths, the hearth pass issues
  RefuelStation jobs (firewood demand joined food demand). Station AddFuel is transactional (returns the accepted count).
- **The colony medic** (Oct 20, N-061): `RunMedicPass` runs first in every upkeep tick (10 s): anyone in the settlement -- villager, player or
  guest, never an animal -- with a wound bleeding at least `MedicBleedThresholdMlPerSecond` (0.2 mL/s) gets the nearest idle villager that has the skill and
  the supplies, one medic per patient, worst bleeder first; tending its own wound comes last. `FMOMedicalTreatmentActions::PickForMedic`
  chooses the worst bleeding wound and, of the treatments that slow bleeding and that the medic can do, the one of most total benefit (bleed +
  infection + healing), so a dressing beats a tourniquet. Supplies come from the medic's pack, else the nearest settlement store that holds
  them: the `TreatWound` job then fetches them first (states 42-43: walk to the store, take the shortfall; a tool is carried whole with its
  durability), walks to the patient (40, following it if it moves), and runs `FMOMedicalTreatmentActions::Begin` on its own timed action
  (41) -- the same server treatment, checks and duration a player's is. Done when the wound bleeds less. Test
  `MOFramework.Medical.Treatment.MedicPick`; host: `_host_colony_medic`. A bleeding player is told "<medic> is coming to dress your
  wound. Stay still." (the treatment needs the patient still); a failed attempt (can't reach, refused at the bedside, no better)
  leaves that patient for `MedicRetryAfterFailureSeconds` (60 s; it was re-sent every 10 s tick).
- **A pawn's age** (Oct 19): `FMOPersistedPawnRecord::DefaultAgeInDays(Guid)` (18-40 years, fixed by the GUID) wherever a record is made without
  one -- the save capture, the possession list's live-world rows, the game mode's spawn registration. Before, only game-mode spawns got an age;
  the player's own pawn, recruits and wild survivors read "0 years" in the Characters list. Gender stays "Unknown" until F15.
- **Recruitment** (Oct 10): `CanRecruitByStanding` judges trust built by shared game time (2 % per hour within 12 m) only for a colony
  of >= `RecruitGateMinRoster` (3); the first survivors just join. `BeginInteractionDetailed` returns an `EMORecruitmentOutcome` with
  the numbers, and `AMOPlayerController::ClientShowNotification` tells the player why. MO.Colony survivors persist (recruited -> captured).
- **The campfire ask and the survivor's words** (Oct 29, Wes: "'I got a fire started and that's as far as I could get.' ... 'If you can do
  better than me I'll join your village.' and prompts the 'build a camp fire' quest line or, if it's already done have the quest complete
  right away with like 'Ohh you already built one!?'"): every survivor now asks (`QuestChance` 1, the default `PossibleQuests` entry,
  `EMORecruitmentQuestType::NeedsBuilding`): a campfire must stand built by the players (`IsBuildingStanding`: any complete `BuildCampfire`
  but a survivor's own, tagged `UMOSpawnManagerSubsystem::SurvivorBeaconTag`; the world is the server's truth, so it works for a joiner
  too). The first E: her greeting, her camp line (one of each per survivor: `GreetingLines` / `CampLines`, "Oh my god, I thought I was
  going to be alone forever!" ...), the ask, in the dialogue popup (`Docs/UI.md` § What a survivor says), and the asked player's machine
  starts the campfire quest LINE (`ClientStartQuestLine` -> `UMOQuestSubsystem::StartQuestLine`: `Tutorial_BuildCampfire`, or while its
  prerequisites are not met the steps it waits on -- gathering sticks and bark -- each tracked; StartQuest alone "could not start" it).
  Met already: "Ohh, you already built one!?" and she joins on the spot (`EMORecruitmentOutcome::QuestAlreadyMet`). Back early: the
  reminder; back with it built: "You built one!" and she joins. The ask is for the first settlers: once the colony has
  `RecruitGateMinRoster` (3) a stranger drops it and the trust gate decides (V2.3; she greets you and says "I don't know your people yet",
  the notification gives the numbers). The ask's timeout (10 real hours, then she is lost) is unchanged. Test
  `MOFramework.Colony.SurvivorDialogue`.
- **A survivor near every player** (Oct 29, Wes: "i haven't seen one in ages and another player asked about them also saying he's never seen
  one ... let's make it so there's always one within 200m with the campfire going"). Why none were met: the survivor category spawned one in
  the first ~20 s (about 180 m off, a random direction: a campfire in the trees there is not seen), then one per ~11 minutes up to its cap,
  and never moved or despawned any -- a player who walked away from the start never came near another. Now every spawn check
  (`UMOSpawnManagerSubsystem::EnsureSurvivorNearEachPlayer`) makes sure each player has a wandering survivor within 200 m; one whose fire
  went out lights it again (she tends it); none -> one is spawned 80-180 m away, behind the player's view and out of every other player's
  where a spot allows, with her lit campfire. Past the category's cooldown and cap (at the cap the oldest survivor far from everyone goes
  first), and with its own distance from buildings (20 m): the category's, 200 m on the map's spawn settings actor, ruled out the ground
  round any camp -- beside a campfire the player had just built "no spot for one was found" for a minute (Oct 29). Settings
  `UMOSpawnSettings` § Survivor Near Players (`bKeepSurvivorNearPlayers`, `NearbySurvivorRadiusM` 200, `NearbySurvivorSpawnMinM` 80 / `MaxM`
  180, `NearbySurvivorMinFromBuildingsM` 20). Probe `MO.Spawn.Survivors`; test `MOFramework.Spawn.SurvivorNearPlayers`; in game
  `python Tools/check_survivor_reticle.py`.
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

**Built (Oct 20, N-095):** tier 1 as notifications to every player. `UMOColonyManagerSubsystem::RunAlertPass` (each upkeep tick) looks at
colonists nobody is playing: a new wound dealt by an animal ("Harper is being attacked by a wolf!"), a running failure clock ("... is
dying."), or bleeding at least `AlertBleedMlPerSecond` (1 mL/s: "... is bleeding badly."); `AlertPlayers` sends each at most once per
`AlertRepeatSeconds` (60) per colonist and kind. Not built: the persistent dot, the sound, tiers 2-4. Host-side test `_host_colony_alerts`.

## 5. Gotchas that bit (keep)

- `SetStayAtLocation` SUSPENDS job processing: call `AbortCurrentJob()` FIRST, or the in-flight job and its quota slot wedge forever.
- Teleporting a pawn to ~1 m of a building lands INSIDE its collision hull; depenetration shoves it out past e.g. a fire's heat radius.
  Gates stand 2.5 m out; AI nav projection is immune.
- Fast jobs start AND finish between gate polls: assert on evidence (stock deltas), not "current job" sampling.
- Un-recruited survivors don't run job move-legs: gates FOUND + RECRUIT first.
- Plain C++ interface bases can't be downcast under UE no-RTTI: iterate the concrete actor type.
