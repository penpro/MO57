# PCG World Items Integration Plan

## Overview

Integrate Voxel plugin's PCG scatter system with MOFramework to enable:
- Rocks, sticks, and other harvestable items spawned via PCG on voxel terrain
- Player interaction to pick up PCG-spawned items
- Items removed from PCG layer and added to inventory
- Persistence of harvested items across save/load

---

## Architecture Options

### Option A: Hybrid ISM + Actor Spawning (Recommended)
- Use ISM (Instanced Static Mesh) for distant/visual representation
- Convert to actual actors when player gets close (within interaction range)
- Player interacts with actor, which removes both actor AND corresponding ISM instance
- Lower memory footprint, good performance

### Option B: Full Actor Spawning
- Spawn actual actors for each PCG point
- Simpler interaction (already compatible with MOFramework)
- Higher memory/performance cost
- Better for sparse high-value items

### Option C: Pure ISM with Raycast Detection
- Keep everything as ISM instances
- Custom raycast to detect which instance is being looked at
- Remove instance by index when harvested
- Most performant but requires custom interaction system

**Recommendation**: Option A for common items (rocks, sticks), Option B for rare items

---

## Implementation Plan

### Phase 1: PCG Graph Setup (Blueprint/Editor Work)

**Create Base PCG Graphs:**

1. **PG_WorldResources** - Master PCG graph for all world resources
   - Input: Voxel world surface points via `PCGVoxelSampler`
   - Output: Spawned resource instances

2. **Sampler Settings:**
   - Layer: Ground/terrain layer from voxel world
   - PointsPerSquaredMeter: 0.01-0.1 (sparse distribution)
   - Looseness: 0.8 (natural randomness)
   - Filter by surface type (grass, dirt, rock formations)

3. **Resource Distribution Nodes:**
   - Rocks: Higher density on rocky surfaces
   - Sticks: Higher density near trees/forest areas
   - Use PCG density nodes to control distribution

### Phase 2: MOFramework Integration Classes

#### 2.1 UMOPCGResourceComponent

New component that bridges PCG instances with MOFramework:

```cpp
// MOPCGResourceComponent.h
UCLASS(ClassGroup=(MOFramework), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOPCGResourceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // The item definition ID this resource yields when harvested
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
    FName ItemDefinitionId;

    // Quantity yielded (can be random range)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
    int32 MinQuantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|PCG")
    int32 MaxQuantity = 1;

    // Reference back to PCG system for removal
    UPROPERTY()
    int32 PCGInstanceIndex = INDEX_NONE;

    UPROPERTY()
    TWeakObjectPtr<UVoxelInstancedStampComponent> SourceStampComponent;

    // Called when resource is harvested
    UFUNCTION(BlueprintCallable, Category="MO|PCG")
    void Harvest(AActor* Harvester);

    // Remove from PCG layer (called internally by Harvest)
    void RemoveFromPCGLayer();
};
```

#### 2.2 AMOPCGResourceActor

Actor class for PCG-spawned resources:

```cpp
// MOPCGResourceActor.h
UCLASS()
class MOFRAMEWORK_API AMOPCGResourceActor : public AActor
{
    GENERATED_BODY()

public:
    AMOPCGResourceActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UMOPCGResourceComponent> ResourceComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UMOItemInteractableComponent> InteractableComponent;

    // Initialize from PCG data
    UFUNCTION(BlueprintCallable, Category="MO|PCG")
    void InitializeFromPCGPoint(
        UStaticMesh* Mesh,
        FName InItemDefinitionId,
        int32 InPCGInstanceIndex,
        UVoxelInstancedStampComponent* InSourceComponent
    );
};
```

#### 2.3 UMOPCGSpawnerSubsystem

World subsystem to manage PCG resource conversion:

```cpp
// MOPCGSpawnerSubsystem.h
UCLASS()
class MOFRAMEWORK_API UMOPCGSpawnerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // Configuration
    UPROPERTY(EditAnywhere, Category="MO|PCG")
    float ConversionRadius = 1500.0f;  // Distance at which ISM converts to actor

    UPROPERTY(EditAnywhere, Category="MO|PCG")
    float DespawnRadius = 2000.0f;  // Distance at which actors convert back to ISM

    // Track which instances have been converted to actors
    TMap<int32, TWeakObjectPtr<AMOPCGResourceActor>> InstanceToActorMap;

    // Track harvested instance indices (for persistence)
    TSet<int32> HarvestedInstances;

    // Called each tick to manage conversions
    void UpdateProximityConversions(const FVector& PlayerLocation);

    // Convert ISM instance to actor
    AMOPCGResourceActor* ConvertToActor(int32 InstanceIndex);

    // Convert actor back to ISM (if player moves away without harvesting)
    void ConvertToInstance(AMOPCGResourceActor* Actor);

    // Mark instance as harvested (prevents respawn)
    void MarkHarvested(int32 InstanceIndex);

    // Persistence
    void BuildSaveData(FMOPCGSaveData& OutData) const;
    void ApplySaveData(const FMOPCGSaveData& InData);
};
```

### Phase 3: Persistence Integration

#### 3.1 PCG Save Data Structure

```cpp
// Add to MOworldSaveGame.h
USTRUCT(BlueprintType)
struct FMOPCGSaveData
{
    GENERATED_BODY()

    // Set of harvested PCG instance indices (per PCG graph)
    UPROPERTY()
    TMap<FName, TArray<int32>> HarvestedInstancesByGraph;

    // Timestamp of last harvest (for potential respawn timers)
    UPROPERTY()
    TMap<int32, FDateTime> HarvestTimestamps;
};

// Add to UMOWorldSaveGame
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|PCG")
FMOPCGSaveData PCGResourceData;
```

#### 3.2 Persistence Subsystem Updates

Update `UMOPersistenceSubsystem` to capture/restore PCG state:

```cpp
// In CaptureWorldState
void CapturePCGResourceState(UWorld* World, UMOWorldSaveGame* SaveObject) const;

// In RestoreWorldState
void RestorePCGResourceState(UWorld* World, const FMOPCGSaveData& PCGData);
```

### Phase 4: Interaction Integration

#### 4.1 Harvest Interaction

The `UMOItemInteractableComponent` on PCG resources handles interaction:

```cpp
// In AMOPCGResourceActor constructor
InteractableComponent = CreateDefaultSubobject<UMOItemInteractableComponent>(TEXT("Interactable"));
InteractableComponent->InteractionPrompt = FText::FromString(TEXT("Pick Up"));
InteractableComponent->OnCompleteInteracted.AddDynamic(this, &AMOPCGResourceActor::HandleHarvested);
```

#### 4.2 Harvest Handler

```cpp
void AMOPCGResourceActor::HandleHarvested(AActor* Interactor)
{
    // Get inventory from interactor
    UMOInventoryComponent* Inventory = nullptr;
    if (APawn* Pawn = Cast<APawn>(Interactor))
    {
        Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
    }

    if (Inventory && ResourceComponent)
    {
        // Calculate quantity
        int32 Quantity = FMath::RandRange(
            ResourceComponent->MinQuantity,
            ResourceComponent->MaxQuantity
        );

        // Add to inventory
        Inventory->AddItem(ResourceComponent->ItemDefinitionId, Quantity);

        // Remove from PCG layer
        ResourceComponent->RemoveFromPCGLayer();

        // Destroy this actor
        Destroy();
    }
}
```

---

## File Structure

```
Plugins/MOFramework/Source/MOFramework/
├── Public/
│   ├── MOPCGResourceComponent.h
│   ├── MOPCGResourceActor.h
│   └── MOPCGSpawnerSubsystem.h
└── Private/
    ├── MOPCGResourceComponent.cpp
    ├── MOPCGResourceActor.cpp
    └── MOPCGSpawnerSubsystem.cpp

Content/
└── PCG/
    ├── PG_WorldResources.uasset        (Master PCG graph)
    ├── PG_Rocks.uasset                 (Rock distribution)
    ├── PG_Sticks.uasset                (Stick distribution)
    └── Resources/
        ├── BP_PCGRock.uasset           (Rock resource actor)
        └── BP_PCGStick.uasset          (Stick resource actor)
```

---

## Implementation Order

### Step 1: Core Components (C++)
1. Create `UMOPCGResourceComponent`
2. Create `AMOPCGResourceActor`
3. Add module dependency on VoxelPCG

### Step 2: Spawner Subsystem (C++)
4. Create `UMOPCGSpawnerSubsystem`
5. Implement proximity-based conversion

### Step 3: Persistence (C++)
6. Add `FMOPCGSaveData` to save game
7. Update persistence subsystem

### Step 4: Blueprint Setup (Editor)
8. Create resource actor blueprints (BP_PCGRock, BP_PCGStick)
9. Create PCG graphs for distribution
10. Configure PCGVoxelSampler nodes

### Step 5: Testing
11. Test basic spawn/harvest cycle
12. Test save/load persistence
13. Test proximity conversion performance

---

## Alternative: Simpler First Implementation

For a quicker prototype, skip the ISM-to-Actor conversion and just spawn actors directly:

1. Use `PCGSpawnActorWithVoxelGraph` to spawn `AMOPCGResourceActor` directly
2. Each actor has `UMOItemInteractableComponent` for pickup
3. On pickup, add to inventory and destroy actor
4. Save harvested locations to prevent respawn

This is less performant but much simpler to implement initially.

---

## Questions to Resolve

1. **Respawn behavior?** - Should harvested resources respawn after X time?
2. **Tool requirements?** - Do some resources require tools (pickaxe for rocks)?
3. **Visual feedback?** - Highlight when looking at harvestable resource?
4. **Sound effects?** - Audio on harvest?
5. **Particle effects?** - VFX on harvest?

---

## Dependencies

- VoxelPCG module (add to MOFramework.Build.cs)
- Existing MOFramework inventory system
- Existing MOFramework interaction system
