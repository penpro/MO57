#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInteractEvent, AActor*, InteractableActor, AController*, InteractorController);

UCLASS(Blueprintable, BlueprintType, ClassGroup=(MO), meta=(BlueprintSpawnableComponent))

class MOFRAMEWORK_API UMOInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInteractableComponent();

	// Simple default behavior for early testing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interactable")
	bool bDestroyOwnerOnInteract = false;

	// Event for Blueprints to react without coupling.
	UPROPERTY(BlueprintAssignable, Category="MO|Interactable")
	FMOInteractEvent OnInteracted;

	// Lightweight validation hook.
	UFUNCTION(BlueprintCallable, Category="MO|Interactable")
	bool CanInteract(AController* InteractorController) const;

	// Server-authoritative interaction entry point.
	UFUNCTION(BlueprintCallable, Category="MO|Interactable")
	bool ServerInteract(AController* InteractorController);

protected:
	// Override in Blueprint or C++ to implement behavior.
	UFUNCTION(BlueprintNativeEvent, Category="MO|Interactable")
	bool HandleInteract(AController* InteractorController);
	virtual bool HandleInteract_Implementation(AController* InteractorController);
};
