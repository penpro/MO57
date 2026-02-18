# MOB AI Implementation Plan

## Overview

This plan outlines the implementation of a creature/mob AI system for MO57. The system leverages existing MOFramework components (combat, medical, threat assessment) and adds creature-specific behavior through Behavior Trees.

## Architecture

### Inheritance Hierarchy

```
AMOCharacter (existing)
    └── AMOCreature (new base class)
            ├── AMOPreyCreature (fleeing animals: deer, rabbit)
            │       └── Behavior: Flee-first, only fights when cornered
            │
            └── AMOPredatorCreature (wolves, bears)
                    └── Behavior: Hunt/attack, flee when severely wounded
```

### Controller Hierarchy

```
AMOAIController (existing)
    └── AMOCreatureController (new)
            └── Adds: AIPerceptionComponent, creature-specific Blackboard keys
```

### Why This Structure?

1. **AMOCreature** inherits all medical/combat systems from AMOCharacter
2. **Prey vs Predator** split allows different default behaviors
3. **Predators can use prey's flee logic** via composition (flee when injured)
4. **Single controller class** - behavior differences come from Behavior Trees, not controller code

---

## Phase 1: Foundation (Core Classes)

### 1.1 Create AMOCreature Base Class

**File:** `MOCreature.h/cpp`

```cpp
UCLASS()
class AMOCreature : public AMOCharacter
{
    GENERATED_BODY()

public:
    AMOCreature();

    // Creature definition from DataTable
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Creature")
    FDataTableRowHandle CreatureDefinition;

    // Override AI controller class
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature")
    TSubclassOf<AMOCreatureController> CreatureControllerClass;

    // Loot table for drops
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Creature")
    FDataTableRowHandle LootTable;

protected:
    virtual void BeginPlay() override;
    virtual void OnDeath();

    // Bind to anatomy component's death delegate
    UFUNCTION()
    void HandleCreatureDeath(AActor* Killer);
};
```

**Key Features:**
- Inherits all AMOCharacter components (combat, medical, inventory)
- DataTable-driven stats
- Death handling with loot spawning
- Default to AMOCreatureController

### 1.2 Create AMOCreatureController

**File:** `MOCreatureController.h/cpp`

```cpp
UCLASS()
class AMOCreatureController : public AMOAIController
{
    GENERATED_BODY()

public:
    AMOCreatureController();

    // Get current threat target
    UFUNCTION(BlueprintPure, Category="MO|AI")
    AActor* GetCurrentThreat() const;

    // Get health percentage (for flee decisions)
    UFUNCTION(BlueprintPure, Category="MO|AI")
    float GetHealthPercent() const;

    // Check if creature should flee
    UFUNCTION(BlueprintPure, Category="MO|AI")
    bool ShouldFlee() const;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void BeginPlay() override;

    // Perception component - detects players/threats
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|AI")
    TObjectPtr<UAIPerceptionComponent> PerceptionComponent;

    // Sight config
    UPROPERTY(EditDefaultsOnly, Category="MO|AI|Perception")
    float SightRadius = 1500.f;

    UPROPERTY(EditDefaultsOnly, Category="MO|AI|Perception")
    float LoseSightRadius = 2000.f;

    UPROPERTY(EditDefaultsOnly, Category="MO|AI|Perception")
    float PeripheralVisionAngle = 60.f;

    // Hearing config
    UPROPERTY(EditDefaultsOnly, Category="MO|AI|Perception")
    float HearingRange = 2000.f;

    // Flee threshold (health percent)
    UPROPERTY(EditDefaultsOnly, Category="MO|AI|Combat")
    float FleeHealthThreshold = 0.3f;

private:
    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    void SetupPerception();
};
```

**Key Features:**
- Extends existing MOAIController (keeps BT/Blackboard)
- Adds UAIPerceptionComponent for sight/hearing
- Exposes helper functions for BT nodes
- Configurable perception ranges

### 1.3 Create Creature DataTable Row

**File:** `MOCreatureDefinitionRow.h`

```cpp
USTRUCT(BlueprintType)
struct FMOCreatureDefinitionRow : public FTableRowBase
{
    GENERATED_BODY()

    // Display name
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    // Base health modifier (multiplied with anatomy component)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
    float HealthModifier = 1.0f;

    // Movement speed
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
    float WalkSpeed = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
    float RunSpeed = 600.f;

    // Combat stats
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
    float BaseDamage = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
    float AttackRange = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
    float AttackCooldown = 2.0f;

    // Behavior
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior")
    float AggroRange = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior")
    float FleeThreshold = 0.3f;  // Health % to start fleeing

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behavior")
    bool bIsPredator = false;

    // Behavior tree to use
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    TSoftObjectPtr<UBehaviorTree> BehaviorTree;

    // Loot
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
    TArray<FMOLootEntry> LootEntries;
};

USTRUCT(BlueprintType)
struct FMOLootEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ItemDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MinQuantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxQuantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DropChance = 1.0f;
};
```

---

## Phase 2: Behavior Trees

### 2.1 Blackboard Keys

Create `BB_Creature` Blackboard asset with:

| Key Name | Type | Description |
|----------|------|-------------|
| `TargetActor` | Object (AActor) | Current threat/target |
| `TargetLocation` | Vector | Last known target position |
| `HomeLocation` | Vector | Spawn point for returning |
| `FleeDestination` | Vector | EQS-found flee point |
| `HasTarget` | Bool | Is threat detected |
| `ShouldFlee` | Bool | Health below threshold |
| `IsInCombat` | Bool | Currently fighting |
| `HealthPercent` | Float | Current health % |
| `DistanceToTarget` | Float | Distance to threat |

### 2.2 BT_Prey (Fleeing Creature)

```
Root (Selector)
├── [1] Death Check
│   └── [Decorator: IsDead] → Die Sequence
│
├── [2] Flee Behavior (highest priority when scared)
│   └── [Decorator: ShouldFlee AND HasTarget]
│   └── Sequence
│       ├── Run EQS: FindFleeLocation
│       ├── Move To: FleeDestination (sprint)
│       └── Wait: 3-5 seconds
│
├── [3] Threatened Behavior
│   └── [Decorator: HasTarget AND NOT ShouldFlee]
│   └── Selector
│       ├── [Distance > FleeRange] → Flee
│       └── [Cornered] → Defensive Attack
│
└── [4] Idle Behavior
    └── Selector
        ├── Wander (random point in radius)
        └── Graze/Idle Animation
```

### 2.3 BT_Predator

```
Root (Selector)
├── [1] Death Check
│   └── [Decorator: IsDead] → Die Sequence
│
├── [2] Flee When Wounded (uses prey flee logic)
│   └── [Decorator: ShouldFlee]
│   └── Sequence
│       ├── Run EQS: FindFleeLocation
│       ├── Move To: FleeDestination (sprint)
│       └── Wait: 5 seconds
│       └── [Service: CheckIfSafe] → Return to Idle
│
├── [3] Combat Behavior
│   └── [Decorator: HasTarget AND NOT ShouldFlee]
│   └── Selector
│       ├── [InAttackRange] → Attack Sequence
│       │   ├── Face Target
│       │   ├── Play Attack Animation
│       │   ├── Apply Damage
│       │   └── Wait Cooldown
│       └── [OutOfRange] → Chase Sequence
│           ├── Move To: TargetActor
│           └── [Service: UpdateTargetLocation]
│
├── [4] Hunt Behavior (lost target)
│   └── [Decorator: NOT HasTarget AND HasLastKnownLocation]
│   └── Sequence
│       ├── Move To: TargetLocation
│       ├── Look Around
│       └── [Timeout] → Return Home
│
└── [5] Idle/Patrol
    └── Selector
        ├── Patrol Points
        └── Wander
```

### 2.4 Custom BT Tasks Needed

| Task | Purpose |
|------|---------|
| `BTTask_CreatureAttack` | Execute attack using MOCombatComponent |
| `BTTask_FleeFromThreat` | Run EQS query, move to safe location |
| `BTTask_ReturnHome` | Move back to spawn point |
| `BTTask_Wander` | Random movement in radius |
| `BTTask_PlayAnimation` | Trigger specific animations |

| Service | Purpose |
|---------|---------|
| `BTService_UpdateThreat` | Update Blackboard threat info |
| `BTService_CheckHealth` | Update ShouldFlee based on health |
| `BTService_UpdateDistance` | Calculate distance to target |

| Decorator | Purpose |
|-----------|---------|
| `BTDecorator_IsDead` | Check if creature is dead |
| `BTDecorator_HasTarget` | Check if target exists |
| `BTDecorator_ShouldFlee` | Check flee condition |
| `BTDecorator_InRange` | Check attack range |

### 2.5 EQS Query: FindFleeLocation

```
Generator: Points on Circle (radius 1500, spacing 300)
    └── Filter: PathExists
    └── Filter: NotInLOSFrom (TargetActor)
    └── Score: Distance from TargetActor (prefer farther)
    └── Score: Distance to cover objects (prefer close to trees/rocks)
```

---

## Phase 3: Child Classes

### 3.1 AMOPreyCreature

**File:** `MOPreyCreature.h/cpp`

```cpp
UCLASS()
class AMOPreyCreature : public AMOCreature
{
    GENERATED_BODY()

public:
    AMOPreyCreature();

protected:
    // Prey always prioritizes fleeing
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
    float FleeSpeedMultiplier = 1.5f;

    // Distance at which prey starts fleeing (before being attacked)
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
    float FleeDetectionRange = 800.f;

    // Default behavior tree for prey
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Prey")
    TSoftObjectPtr<UBehaviorTree> PreyBehaviorTree;
};
```

### 3.2 AMOPredatorCreature

**File:** `MOPredatorCreature.h/cpp`

```cpp
UCLASS()
class AMOPredatorCreature : public AMOCreature
{
    GENERATED_BODY()

public:
    AMOPredatorCreature();

    // Pack support
    UFUNCTION(BlueprintCallable, Category="MO|Creature|Predator")
    void AlertPack(AActor* Threat);

protected:
    // Predators fight first, flee when wounded
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
    float AggroRange = 1000.f;

    // Health threshold to switch to flee behavior
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
    float FleeHealthThreshold = 0.25f;

    // Pack coordination
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
    float PackAlertRange = 1500.f;

    // Default behavior tree for predators
    UPROPERTY(EditDefaultsOnly, Category="MO|Creature|Predator")
    TSoftObjectPtr<UBehaviorTree> PredatorBehaviorTree;
};
```

---

## Phase 4: Integration with Existing Systems

### 4.1 Combat Integration

Creatures use `MOCombatComponent` for attacks:

```cpp
void UBTTask_CreatureAttack::ExecuteTask(...)
{
    AMOCreature* Creature = Cast<AMOCreature>(OwnerPawn);
    UMOCombatComponent* Combat = Creature->FindComponentByClass<UMOCombatComponent>();

    if (Combat && Combat->CanAttack())
    {
        // Use light attack by default
        Combat->StartAttack(EMOAttackType::Light);
        return EBTNodeResult::InProgress;
    }
    return EBTNodeResult::Failed;
}
```

### 4.2 Medical Integration

Creature death via medical system:

```cpp
void AMOCreature::BeginPlay()
{
    Super::BeginPlay();

    // Bind to anatomy death
    if (UMOAnatomyComponent* Anatomy = FindComponentByClass<UMOAnatomyComponent>())
    {
        Anatomy->OnDeath.AddDynamic(this, &AMOCreature::HandleCreatureDeath);
    }
}

void AMOCreature::HandleCreatureDeath(AActor* Killer)
{
    // Stop AI
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
        AI->GetBrainComponent()->StopLogic(TEXT("Dead"));
    }

    // Spawn loot
    SpawnLoot();

    // Play death animation, then destroy after delay
    // ...
}
```

### 4.3 Threat Assessment Integration

Use existing `MOAdrenalineComponent` for threat:

```cpp
float AMOCreatureController::GetThreatLevel() const
{
    if (APawn* Pawn = GetPawn())
    {
        if (UMOAdrenalineComponent* Adrenaline = Pawn->FindComponentByClass<UMOAdrenalineComponent>())
        {
            return Adrenaline->GetCurrentThreatLevel();
        }
    }
    return 0.f;
}
```

---

## Phase 5: Implementation Order

### Week 1: Foundation
1. [ ] Create `MOCreature.h/cpp` base class
2. [ ] Create `MOCreatureController.h/cpp` with perception
3. [ ] Create `MOCreatureDefinitionRow.h` for DataTable
4. [ ] Create `BB_Creature` Blackboard asset
5. [ ] Test basic spawning and perception

### Week 2: Behavior Trees
1. [ ] Create `BTTask_CreatureAttack`
2. [ ] Create `BTTask_FleeFromThreat` with EQS
3. [ ] Create `BTService_UpdateThreat`
4. [ ] Create `BTService_CheckHealth`
5. [ ] Build `BT_Prey` behavior tree
6. [ ] Build `BT_Predator` behavior tree

### Week 3: Child Classes
1. [ ] Create `MOPreyCreature.h/cpp`
2. [ ] Create `MOPredatorCreature.h/cpp`
3. [ ] Create test creatures (deer, wolf)
4. [ ] Test flee behavior
5. [ ] Test combat behavior

### Week 4: Polish
1. [ ] Add loot spawning on death
2. [ ] Add pack coordination for predators
3. [ ] Performance optimization (tick rates)
4. [ ] Create initial creature DataTable entries
5. [ ] Test with multiple creatures

---

## Files to Create

| File | Type | Priority |
|------|------|----------|
| `MOCreature.h/cpp` | C++ Class | HIGH |
| `MOCreatureController.h/cpp` | C++ Class | HIGH |
| `MOCreatureDefinitionRow.h` | C++ Struct | HIGH |
| `MOPreyCreature.h/cpp` | C++ Class | MEDIUM |
| `MOPredatorCreature.h/cpp` | C++ Class | MEDIUM |
| `BTTask_CreatureAttack.h/cpp` | BT Task | HIGH |
| `BTTask_FleeFromThreat.h/cpp` | BT Task | HIGH |
| `BTService_UpdateThreat.h/cpp` | BT Service | MEDIUM |
| `BTService_CheckHealth.h/cpp` | BT Service | MEDIUM |
| `BB_Creature` | Blackboard Asset | HIGH |
| `BT_Prey` | Behavior Tree Asset | HIGH |
| `BT_Predator` | Behavior Tree Asset | HIGH |
| `EQS_FindFleeLocation` | EQS Query Asset | MEDIUM |
| `DT_Creatures` | DataTable Asset | LOW |

---

## Open Questions

1. **Animations**: Do we have creature animation assets? Need idle, walk, run, attack, death, hit react
2. **Skeletal Meshes**: What creatures are available in the project?
3. **Sounds**: Need growl, attack, pain, death sounds
4. **Navmesh**: Is NavMeshBoundsVolume set up in test levels?
5. **Spawn System**: How will creatures spawn? PCG? Spawn volumes?

---

## Performance Considerations

1. **Tick Rate**: Creatures beyond 50m use 0.5s tick
2. **Perception Updates**: Max 10 per frame via perception system
3. **EQS Caching**: Cache flee locations for 2 seconds
4. **BT Services**: Use 0.5s intervals, not every tick
5. **Max Creatures**: Target 50 active AI simultaneously
