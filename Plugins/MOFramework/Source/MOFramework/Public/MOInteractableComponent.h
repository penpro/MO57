#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInteractableComponent.generated.h"

UENUM(BlueprintType)
enum class EMOInteractionVerb : uint8
{
	Use     UMETA(DisplayName="Use"),
	Pickup  UMETA(DisplayName="Pickup"),
	Open    UMETA(DisplayName="Open"),
	Talk    UMETA(DisplayName="Talk"),
	Custom  UMETA(DisplayName="Custom")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FMOInteractableInteractedSignature,
	AController*, InteractorController,
	EMOInteractionVerb, Verb
);

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInteractableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction")
	EMOInteractionVerb Verb = EMOInteractionVerb::Use;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction", meta=(ClampMin="0.0"))
	float MaxInteractionDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction")
	bool bDestroyOwnerOnInteract = false;

	UPROPERTY(BlueprintAssignable, Category="MO|Interaction")
	FMOInteractableInteractedSignature OnInteracted;

	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool CanInteract(AController* InteractorController) const;

	/** Called by the subsystem on the server after validation. */
	bool ExecuteInteraction(AController* InteractorController);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintNativeEvent, Category="MO|Interaction")
	bool K2_OnInteract(AController* InteractorController);
	virtual bool K2_OnInteract_Implementation(AController* InteractorController);
};
