#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOInteractableComponent.h"
#include "MOWorldItem.generated.h"

class UDataTable;
struct FMOItemDefinitionRow;


class UMOIdentityComponent;
class UMOItemComponent;
class UMOInteractableComponent;
class UStaticMeshComponent;

UCLASS()
class MOFRAMEWORK_API AMOWorldItem : public AActor
{
	GENERATED_BODY()

public:
	AMOWorldItem();

	UFUNCTION(BlueprintPure, Category="MO")
	UMOIdentityComponent* GetIdentityComponent() const { return IdentityComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOItemComponent* GetItemComponent() const { return ItemComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInteractableComponent* GetInteractableComponent() const { return InteractableComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UStaticMeshComponent* GetItemMesh() const { return ItemMesh; }

	// Interaction behavior options
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bAddToInventoryOnInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bDestroyAfterPickup = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bHideOnPickup = true;

protected:
	// Override interaction handling
	UFUNCTION()
	bool OnHandleInteract(AController* InteractorController);

protected:

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** DataTable that defines how ItemDefinitionId maps to display name, mesh, icon, etc.
		If empty, the plugin Project Settings database is used. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Item|Definition")
	TSoftObjectPtr<UDataTable> ItemDefinitionsDataTable;

	UFUNCTION(BlueprintCallable, Category="MO|Item|Definition")
	bool ApplyItemDefinitionToWorldMesh();

	UFUNCTION()
	void HandleItemDefinitionIdChanged(FName NewItemDefinitionId);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UStaticMeshComponent> ItemMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOItemComponent> ItemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInteractableComponent> InteractableComponent;
};
