#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOIdentifiableInterface.generated.h"

class UMOIdentityComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UMOIdentifiableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors with persistent identity (GUID tracking).
 * Used by the persistence system to save/load actors without FindComponentByClass.
 *
 * Implementers: AMOCharacter, AMOBuildableActor, AMOWorldItem
 */
class MOFRAMEWORK_API IMOIdentifiableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the identity component for this actor.
	 * @return The identity component, or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Identity")
	UMOIdentityComponent* GetIdentityComponent() const;

	/**
	 * Get the persistent GUID for this actor.
	 * @return Valid GUID if identifiable, invalid GUID otherwise
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Identity")
	FGuid GetPersistentGuid() const;

	/**
	 * Check if this actor has a valid persistent identity.
	 * @return True if the actor has a valid GUID
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Identity")
	bool HasValidIdentity() const;
};
