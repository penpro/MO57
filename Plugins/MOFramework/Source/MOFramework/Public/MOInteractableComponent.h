/**
 * =============================================================================
 * MOInteractableComponent.h - Actor Interaction Target Component
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Makes actors interactable by players. Receives interaction requests from
 * MOInteractorComponent and dispatches to owner via delegates. Supports both
 * primary (E key) and secondary (right-click) interactions.
 *
 * KEY RESPONSIBILITIES:
 * 1. Receive interaction requests from MOInteractorComponent
 * 2. Validate interaction via CanInteract()
 * 3. Dispatch to owner via HandleInteract/HandleSecondaryInteract
 * 4. Support C++ delegate binding (OnHandleInteract)
 * 5. Support Blueprint event binding (OnInteracted)
 *
 * ARCHITECTURE NOTES:
 * - Server-authoritative: ServerInteract() is the entry point
 * - Two delegation paths: C++ delegate and BlueprintNativeEvent
 * - C++ delegate checked first, then HandleInteract_Implementation
 * - bDestroyOwnerOnInteract is simple default for testing
 *
 * INTERACTION FLOW:
 * Player presses interact -> MOInteractorComponent::TryInteract()
 * -> ServerRequestInteract RPC -> MOInteractableComponent::ServerInteract()
 * -> CanInteract() check -> HandleInteract() -> C++ delegate OR BP event
 * -> OnInteracted broadcast (for observers)
 *
 * CRITICAL PATTERNS:
 * 1. C++ Owner Binding:
 *    InteractableComp->OnHandleInteract.BindUObject(this, &ThisClass::HandleIt);
 *    Return true to indicate "handled", false to continue chain
 *
 * 2. Blueprint Override:
 *    Override HandleInteract event in BP
 *    Call parent if you want default behavior
 *
 * 3. Enable/Disable:
 *    SetCanInteract(false) prevents all interaction
 *    Check IsInteractionEnabled() before showing prompts
 *
 * KNOWN PITFALLS:
 * 1. DELEGATE ORDER: C++ delegate (OnHandleInteract) fires BEFORE
 *    BlueprintNativeEvent. If C++ returns true, BP won't fire.
 *
 * 2. SERVER ONLY: ServerInteract() should only be called on server.
 *    MOInteractorComponent handles the RPC - don't call directly.
 *
 * 3. OWNERSHIP: Component must be on actor you want to interact with.
 *    MOInteractorComponent traces for actors WITH this component.
 *
 * 4. SECONDARY INTERACT: Right-click flows through separate path.
 *    Both OnHandleSecondaryInteract AND HandleSecondaryInteract exist.
 *
 * RELATED FILES:
 * - MOInteractorComponent.h - Traces for and triggers interactions
 * - MOInteractionSubsystem.h - Server-side validation and rate limiting
 * - MOWorldItem.h - Uses this for pickup interaction
 * - MOCraftingStationActor.h - Uses this for station interaction
 *
 * TESTING CHECKLIST:
 * [ ] Primary interact triggers HandleInteract
 * [ ] Secondary interact triggers HandleSecondaryInteract
 * [ ] OnInteracted fires after successful interaction
 * [ ] SetCanInteract(false) blocks interaction
 * [ ] C++ delegate binding works
 * [ ] Blueprint override works
 * [ ] bDestroyOwnerOnInteract destroys actor
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOInteractEvent, AActor*, InteractableActor, AController*, InteractorController);
DECLARE_DELEGATE_RetVal_OneParam(bool, FMOHandleInteractDelegate, AController*);
DECLARE_DELEGATE_RetVal_OneParam(bool, FMOHandleSecondaryInteractDelegate, AController*);

UCLASS(Blueprintable, BlueprintType, ClassGroup=(MO), meta=(BlueprintSpawnableComponent))

class MOFRAMEWORK_API UMOInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInteractableComponent();

	// Simple default behavior for early testing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interactable")
	bool bDestroyOwnerOnInteract = false;

	// Event for Blueprints to react without coupling (fires AFTER interaction handled).
	UPROPERTY(BlueprintAssignable, Category="MO|Interactable")
	FMOInteractEvent OnInteracted;

	// C++ delegate for owners to handle interaction (called FROM HandleInteract).
	// If bound and returns true, the interaction is considered handled.
	FMOHandleInteractDelegate OnHandleInteract;

	// C++ delegate for owners to handle secondary interaction (right-click).
	// If bound and returns true, the secondary interaction is considered handled.
	FMOHandleSecondaryInteractDelegate OnHandleSecondaryInteract;

	// Event for Blueprints to react to secondary interact.
	UPROPERTY(BlueprintAssignable, Category="MO|Interactable")
	FMOInteractEvent OnSecondaryInteracted;

	// Lightweight validation hook.
	UFUNCTION(BlueprintCallable, Category="MO|Interactable")
	bool CanInteract(AController* InteractorController) const;

	// Enable or disable interaction with this component.
	UFUNCTION(BlueprintCallable, Category="MO|Interactable")
	void SetCanInteract(bool bCanInteract) { bInteractionEnabled = bCanInteract; }

	// Check if interaction is currently enabled.
	UFUNCTION(BlueprintPure, Category="MO|Interactable")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	// Server-authoritative interaction entry point.
	UFUNCTION(BlueprintCallable, Category="MO|Interactable")
	bool ServerInteract(AController* InteractorController);

	// Server-authoritative secondary interaction entry point (right-click).
	UFUNCTION(BlueprintCallable, Category="MO|Interactable")
	bool ServerSecondaryInteract(AController* InteractorController);

protected:
	// Override in Blueprint or C++ to implement behavior.
	UFUNCTION(BlueprintNativeEvent, Category="MO|Interactable")
	bool HandleInteract(AController* InteractorController);
	virtual bool HandleInteract_Implementation(AController* InteractorController);

	// Override in Blueprint or C++ to implement secondary interaction behavior.
	UFUNCTION(BlueprintNativeEvent, Category="MO|Interactable")
	bool HandleSecondaryInteract(AController* InteractorController);
	virtual bool HandleSecondaryInteract_Implementation(AController* InteractorController);

private:
	// Whether interaction is currently enabled.
	UPROPERTY()
	bool bInteractionEnabled = true;
};
