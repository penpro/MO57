/**
 * =============================================================================
 * MOContainerActor.h - Buildable Storage Container with Inventory
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Buildable container actor providing storage slots. Extends MOBuildableActor
 * with inventory functionality. Interaction opens container UI for item transfer.
 * Can serve as material source for nearby crafting operations.
 *
 * KEY RESPONSIBILITIES:
 * 1. Provide storage inventory via UMOInventoryComponent
 * 2. Handle container open/close audio feedback
 * 3. Implement IMOInventoryHolderInterface for inventory queries
 * 4. Implement IMOMaterialSourceInterface for crafting material gathering
 * 5. Save/load container contents with building persistence
 *
 * ARCHITECTURE NOTES:
 * - Inherits from AMOBuildableActor for construction phases
 * - ContainerInventory component created in constructor
 * - SlotCount configured from recipe data in InitializeBuilding()
 * - Two audio components for open/close sounds (soft refs for lazy loading)
 *
 * INTERACTION FLOW:
 * Player interacts with complete container
 * -> OnCompleteInteracted_Implementation() -> Open container UI
 * -> PlayOpenSound() -> Transfer items -> Close UI -> PlayCloseSound()
 *
 * CRAFTING INTEGRATION:
 * IMOMaterialSourceInterface allows crafting system to pull materials:
 * CraftingSubsystem->GatherMaterials() -> Container->CanProvideMaterial()
 * -> Container->GatherMaterial() removes from inventory
 *
 * CRITICAL PATTERNS:
 * 1. Slot Count Configuration:
 *    InitializeBuilding() reads SlotCount from recipe's OutputSlotCount
 *    Sets ContainerInventory->SetSlotCountAuthority()
 *
 * 2. Save/Load:
 *    BuildSaveData() -> Inventory.BuildSaveData() into PersistedBuildingRecord
 *    ApplySaveData() -> Inventory.ApplySaveDataAuthority() from record
 *
 * 3. Material Source Priority:
 *    GetMaterialSourcePriority_Implementation() returns order in gather queue
 *    Lower = gathered first. Containers typically return 10.
 *
 * KNOWN PITFALLS:
 * 1. AUDIO SOFT REFS: OpenSound/CloseSound are TSoftObjectPtr.
 *    Use LoadSynchronous() before playing. Don't call in tight loops.
 *
 * 2. CONSTRUCTION STATE: Container UI only available when IsComplete().
 *    Don't call GetContainerInventory() during construction phase.
 *
 * 3. SLOT COUNT TIMING: SlotCount set in InitializeBuilding(), not constructor.
 *    If spawning manually without recipe, set SlotCount explicitly.
 *
 * 4. REPLICATION: ContainerInventory uses FastArraySerializer.
 *    Inventory changes replicate automatically. Don't duplicate logic.
 *
 * RELATED FILES:
 * - MOBuildableActor.h - Base class for construction phases
 * - MOInventoryComponent.h - Storage component
 * - MOInventoryUIController.h - Opens container UI
 * - MOCraftingSubsystem.h - Material gathering interface
 * - MORecipeDefinitionRow.h - OutputSlotCount configuration
 *
 * TESTING CHECKLIST:
 * [ ] Container interaction opens inventory UI
 * [ ] Open/close sounds play correctly
 * [ ] SlotCount matches recipe configuration
 * [ ] Items persist across save/load
 * [ ] Crafting can pull materials from container
 * [ ] Container UI shows correct slot count
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOContainerActor.generated.h"

class UMOInventoryComponent;
class UAudioComponent;

/**
 * A buildable container actor that provides storage slots.
 * When complete, interaction opens an inventory UI.
 */
UCLASS()
class MOFRAMEWORK_API AMOContainerActor : public AMOBuildableActor,
	public IMOInventoryHolderInterface,
	public IMOMaterialSourceInterface
{
	GENERATED_BODY()

public:
	AMOContainerActor();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Inventory component for storage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Container")
	TObjectPtr<UMOInventoryComponent> ContainerInventory;

	/** Sound component for opening container. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Container|Audio")
	TObjectPtr<UAudioComponent> OpenSoundComponent;

	/** Sound component for closing container. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Container|Audio")
	TObjectPtr<UAudioComponent> CloseSoundComponent;

	// ============================================================================
	// AUDIO CONFIGURATION
	// ============================================================================

	/** Sound to play when opening this container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Container|Audio")
	TSoftObjectPtr<USoundBase> OpenSound;

	/** Sound to play when closing this container. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Container|Audio")
	TSoftObjectPtr<USoundBase> CloseSound;

	/** Volume multiplier for container sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Container|Audio", meta=(ClampMin="0.0", ClampMax="2.0"))
	float SoundVolume = 1.0f;

	// ============================================================================
	// AUDIO CONTROL
	// ============================================================================

	/** Play the container open sound. */
	UFUNCTION(BlueprintCallable, Category="MO|Container|Audio")
	void PlayOpenSound();

	/** Play the container close sound. */
	UFUNCTION(BlueprintCallable, Category="MO|Container|Audio")
	void PlayCloseSound();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Number of storage slots. Set from recipe data on initialize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Container")
	int32 SlotCount = 8;

	// ============================================================================
	// ACCESSORS
	// ============================================================================

	/** Get the container inventory component. */
	UFUNCTION(BlueprintPure, Category="MO|Container")
	UMOInventoryComponent* GetContainerInventory() const { return ContainerInventory; }

	// ============================================================================
	// IMOInventoryHolderInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOInventoryComponent* GetInventory_Implementation() const override;
	virtual bool HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const override;
	virtual int32 GetInventoryItemCount_Implementation(FName ItemDefinitionId) const override;

	// ============================================================================
	// IMOMaterialSourceInterface IMPLEMENTATION
	// ============================================================================

	virtual bool CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const override;
	virtual int32 GatherMaterial_Implementation(FName MaterialId, int32 Quantity) override;
	virtual int32 GetMaterialSourcePriority_Implementation() const override;

protected:
	virtual void BeginPlay() override;

	/** Override to open container UI when complete. */
	virtual void OnCompleteInteracted_Implementation(AController* Controller) override;

	/** Initialize slot count from recipe data. */
	virtual void InitializeBuilding(FName InRecipeId) override;

	/** Save container inventory data. */
	virtual void BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const override;

	/** Restore container inventory data. */
	virtual void ApplySaveData(const FMOPersistedBuildingRecord& InRecord) override;
};
