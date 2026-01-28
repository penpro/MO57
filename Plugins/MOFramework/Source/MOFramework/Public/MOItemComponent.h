#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOItemComponent.generated.h"

class UMOIdentityComponent;
class UMOInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOItemDefinitionIdChangedSignature, FName, NewItemDefinitionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOWorldItemActiveChangedSignature, bool, bIsActive);


UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOItemComponent();

	// Static item identifier - references row in item database DataTable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_ItemDefinitionId, Category="MO|Item", meta=(GetOptions="GetItemDefinitionOptions"))
	FName ItemDefinitionId = NAME_None;

	// Returns all available ItemDefinitionIds from the configured DataTable (for dropdown).
	UFUNCTION()
	TArray<FName> GetItemDefinitionOptions() const;

	// Current quantity represented by this world item instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Item", meta=(ClampMin="1"))
	int32 Quantity = 1;

	// For stacking rules. If <= 1 then not stackable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Item", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	// Replicated world presence state so clients see picked-up items disappear without destroying the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_WorldItemActive, Category="MO|Item")
	bool bWorldItemActive = true;
	
	UPROPERTY(BlueprintAssignable, Category="MO|Item")
	FMOItemDefinitionIdChangedSignature OnItemDefinitionIdChanged;

	UPROPERTY(BlueprintAssignable, Category="MO|Item")
	FMOWorldItemActiveChangedSignature OnWorldItemActiveChanged;

	UFUNCTION(BlueprintPure, Category="MO|Item")
	bool IsWorldItemActive() const { return bWorldItemActive; }

	// Called on the server (typically from MOInteractableComponent::HandleInteract) to give item to inventory.
	UFUNCTION(BlueprintCallable, Category="MO|Item")
	bool GiveToInteractorInventory(AController* InteractorController);

	// Toggle active state in world (server sets, clients follow via replication).
	UFUNCTION(BlueprintCallable, Category="MO|Item")
	void SetWorldItemActive(bool bNewWorldItemActive);

	UFUNCTION()
	void OnRep_ItemDefinitionId();

	

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UMOIdentityComponent* FindIdentityComponent() const;
	UMOInventoryComponent* FindInventoryComponentForController(AController* InteractorController) const;

	void ApplyWorldItemActiveState();

	UFUNCTION()
	void OnRep_WorldItemActive();
};
