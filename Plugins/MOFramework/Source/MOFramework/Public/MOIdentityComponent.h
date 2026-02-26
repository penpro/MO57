/**
 * =============================================================================
 * MOIdentityComponent.h - GUID-Based Actor Identity for Persistence
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Provides stable GUID identity for actors across save/load cycles.
 * Essential for persistence - GUIDs survive level transitions, serialization,
 * and PIE/runtime transitions. Foundation of the identity registry system.
 *
 * KEY RESPONSIBILITIES:
 * 1. Generate and store stable FGuid for actor identity
 * 2. Register/unregister with MOIdentityRegistrySubsystem
 * 3. Replicate GUID to clients for multiplayer consistency
 * 4. Support editor-placed instances with persistent GUIDs
 * 5. Broadcast events when GUID becomes available or owner destroyed
 *
 * ARCHITECTURE NOTES:
 * - StableGuid is EditInstanceOnly to survive editor sessions
 * - GUID auto-generated in BeginPlay if not set
 * - Registration with IdentityRegistrySubsystem happens in BeginPlay
 * - Supports both runtime-spawned and editor-placed actors
 *
 * GUID LIFECYCLE:
 * Editor-placed: OnRegister() -> EnsureGuidForEditorInstanceIfMissing()
 * Runtime-spawned: BeginPlay() -> EnsureGuidForAuthorityIfMissing()
 * -> RegisterWithSubsystem() -> OnGuidAvailable broadcast
 *
 * CRITICAL PATTERNS:
 * 1. Getting GUID:
 *    GetGuid() returns current (may be invalid early)
 *    GetOrCreateGuid() ensures valid GUID exists
 *
 * 2. Setting GUID (load/spawn):
 *    SetGuid() sets and broadcasts OnGuidAvailable
 *    Used during persistence restore
 *
 * 3. Editor Instances:
 *    EditInstanceOnly + SaveGame ensures GUID persists
 *    PostEditImport() handles copy/duplicate operations
 *
 * KNOWN PITFALLS:
 * 1. TIMING: GUID may not be valid until after BeginPlay.
 *    Use OnGuidAvailable delegate if you need GUID early.
 *
 * 2. DUPLICATION: Copy-pasting actors in editor must regenerate GUID.
 *    PostEditImport() handles this, but verify in new scenarios.
 *
 * 3. REPLICATION: StableGuid replicates via OnRep_StableGuid.
 *    Clients receive GUID after server generates it. Account for delay.
 *
 * 4. REGISTRY SYNC: Actor must register with IdentityRegistrySubsystem.
 *    If registration fails, GUID lookups won't find this actor.
 *
 * RELATED FILES:
 * - MOIdentityRegistrySubsystem.h - GUID-to-Actor registry
 * - MOIdentifiableInterface.h - Interface for identity queries
 * - MOPersistenceSubsystem.h - Uses GUIDs for save/load
 * - MOInventoryComponent.h - Items identified by GUID
 *
 * TESTING CHECKLIST:
 * [ ] Runtime-spawned actors get valid GUID in BeginPlay
 * [ ] Editor-placed actors preserve GUID across sessions
 * [ ] Copy/paste generates new GUID
 * [ ] OnGuidAvailable fires when GUID becomes valid
 * [ ] GUID replicates to clients
 * [ ] IdentityRegistry can find actor by GUID
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOIdentityComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOIdentityGuidSignature, const FGuid&, StableGuid);

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOIdentityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOIdentityComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="MO|Identity", meta=(ExposeOnSpawn="true"))
	FText DisplayName;

	/**
	 * Stable GUID used as the canonical identity key across save/load.
	 *
	 * IMPORTANT:
	 * EditInstanceOnly is intentional so Blueprint-added component instances in a level
	 * can persist this value across editor sessions.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_StableGuid, SaveGame, AdvancedDisplay, Category="MO|Identity")
	FGuid StableGuid;

	UPROPERTY(BlueprintAssignable, Category="MO|Identity")
	FMOIdentityGuidSignature OnGuidAvailable;

	UPROPERTY(BlueprintAssignable, Category="MO|Identity")
	FMOIdentityGuidSignature OnOwnerDestroyedWithGuid;

	UFUNCTION(BlueprintPure, Category="MO|Identity")
	bool HasValidGuid() const { return StableGuid.IsValid(); }

	UFUNCTION(BlueprintPure, Category="MO|Identity")
	const FGuid& GetGuid() const { return StableGuid; }

	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	void SetDisplayName(const FText& InDisplayName) { DisplayName = InDisplayName; }

	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	void SetGuid(const FGuid& InGuid);

	UFUNCTION(BlueprintCallable, Category="MO|Identity")
	FGuid GetOrCreateGuid();

	UFUNCTION(CallInEditor, BlueprintCallable, Category="MO|Identity")
	void RegenerateGuid();

protected:
	virtual void BeginPlay() override;
	virtual void OnComponentCreated() override;
	virtual void OnRegister() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif

private:
	void EnsureGuidForAuthorityIfMissing(bool bBroadcast);
	void EnsureGuidForEditorInstanceIfMissing(bool bBroadcast);

	UFUNCTION()
	void OnRep_StableGuid();

	UFUNCTION()
	void HandleOwnerDestroyed(AActor* DestroyedActor);
};
