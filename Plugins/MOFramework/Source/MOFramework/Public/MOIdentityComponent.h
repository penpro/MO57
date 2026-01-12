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
