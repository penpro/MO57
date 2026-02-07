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
