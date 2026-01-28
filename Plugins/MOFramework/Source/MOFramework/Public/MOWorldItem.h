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
class USphereComponent;

UCLASS()
class MOFRAMEWORK_API AMOWorldItem : public AActor
{
	GENERATED_BODY()

public:
	AMOWorldItem();

	virtual void Tick(float DeltaTime) override;

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

	/** Apply visuals from the item definition DataTable to the world mesh.
	 *  Call this after setting ItemComponent->ItemDefinitionId to update the mesh. */
	UFUNCTION(BlueprintCallable, Category="MO|Item|Definition")
	bool ApplyItemDefinitionToWorldMesh();

	/** Enable physics for dropped items. Physics will be disabled when the item comes to rest or timeout expires. */
	UFUNCTION(BlueprintCallable, Category="MO|Item|Drop")
	void EnableDropPhysics();

	/** Maximum time physics will be enabled after dropping (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Item|Drop")
	float DropPhysicsTimeout = 3.0f;

	/** Velocity threshold below which the item is considered "at rest". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="MO|Item|Drop")
	float RestVelocityThreshold = 5.0f;

protected:
	// Override interaction handling
	UFUNCTION()
	bool OnHandleInteract(AController* InteractorController);
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Called when drop physics completes - settles item on ground and disables physics. */
	void SettleOnGround();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** DataTable that defines how ItemDefinitionId maps to display name, mesh, icon, etc.
		If empty, the plugin Project Settings database is used. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Item|Definition")
	TSoftObjectPtr<UDataTable> ItemDefinitionsDataTable;

	UFUNCTION()
	void HandleItemDefinitionIdChanged(FName NewItemDefinitionId);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UStaticMeshComponent> ItemMesh;

	/** Collision sphere for interaction traces - easier to hit than the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Radius of the interaction collision sphere. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Interaction")
	float InteractionSphereRadius = 30.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOItemComponent> ItemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInteractableComponent> InteractableComponent;

private:
	/** Track if we're currently in drop physics mode. */
	bool bDropPhysicsActive = false;

	/** Time when drop physics was enabled. */
	float DropPhysicsStartTime = 0.0f;
};
