// A flame carried in the hand: a lit torch or lamp held by a pawn (player or survivor). Caves are dark (no sky light reaches a gallery), and the night is dark;
// fire is the only light a primitive survivor has, and it has to be carried, fed and kept dry.
//
// What burns is the item in the hand (UMOLightSourceSettings has a profile for it). It is lit from a burning fire (FMOFireActions::LightCarried, a timed
// action) and goes out when it leaves the hand, goes under water, or burns down: one point of the item's durability per BurnSecondsPerDurability of game time,
// so a half-burnt torch put away and taken out again carries on from where it was. The light itself is a point light in candelas at the far end of the held
// mesh, warm (~1900 K), flickering, brighter while moving (a torch starves of air held still). Docs/Light_Sources.md.
//
// Server-authoritative: the lit item (its GUID) and the hand replicate; every machine with a renderer makes its own light from them.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOEquipmentComponent.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOCarriedLightComponent.generated.h"

class UPointLightComponent;
class UNiagaraComponent;
class UStaticMeshComponent;
class UStaticMesh;
struct FMOLightSourceProfile;

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCarriedLightComponent : public UActorComponent, public IMOLocalLightSource
{
	GENERATED_BODY()

public:
	UMOCarriedLightComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The unlit light source in the pawn's hands that could be lit now (right hand first), and which hand. False when none (or it is burnt out). */
	bool FindUnlitHeld(EMOEquipmentSlot& OutSlot, FMOEquippedItem& OutItem) const;

	/** Lights the light source in the hands (server; the caller has checked for a flame to light it from). False when there is none to light. */
	bool LightHeld();

	/** Puts it out (server). Why goes to the log and, for a player, to a notification when it matters (burnt out, doused). */
	void Extinguish(const TCHAR* Why, bool bTellOwner = false);

	UFUNCTION(BlueprintPure, Category="MO|Light")
	bool IsLit() const { return LitItemGuid.IsValid(); }

	/** The item that is burning (NAME_None when nothing is). */
	UFUNCTION(BlueprintPure, Category="MO|Light")
	FName GetLitItemId() const;

	/** The light's current luminous intensity (cd), flicker and airflow included; 0 when unlit or on a machine that renders nothing. For tests and dev verbs. */
	float GetCurrentIntensityCd() const { return CurrentIntensityCd; }

	// IMOLocalLightSource (registered while this machine renders the flame): the eye adapts to it.
	virtual FVector GetLightLocation() const override;
	virtual float GetLuminousIntensityCd() const override { return CurrentIntensityCd; }

	/**
	 * Where along a held mesh the flame sits, in the mesh component's space: the end of its longest bounding-box axis farther from the grip (the component's
	 * origin, at the hand socket), Position (0 grip .. 1 far end) of the way out. Pure, for the tests.
	 */
	static FVector FlameLocalPosition(const FBox& LocalBounds, float Position);

	/** Burn bookkeeping (pure, for the tests): adds GameSeconds to Accumulator and returns the whole durability points that burned; the remainder stays. */
	static int32 ConsumeBurn(float& Accumulator, float GameSeconds, float SecondsPerPoint);

protected:
	UFUNCTION()
	void OnRep_Lit();

	UFUNCTION()
	void HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem);

private:
	/** The burning item; invalid = unlit. */
	UPROPERTY(ReplicatedUsing=OnRep_Lit)
	FGuid LitItemGuid;

	UPROPERTY(Replicated)
	EMOEquipmentSlot LitSlot = EMOEquipmentSlot::RightHand;

	/** The burning item's id (replicated with the GUID so a client has the profile without the equipment row having arrived). */
	UPROPERTY(Replicated)
	FName LitItemId;

	/** The local light (any machine with a renderer); null when unlit. */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlameLight;

	/** The visible flame (the profile's FlameEffect), beside the light; null when unlit or the profile has none. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FlameEffect;

	/** The mesh the flame was placed on, to re-place it when the held mesh changes. */
	TWeakObjectPtr<const UStaticMesh> PlacedOnMesh;

	/** Server: game seconds burned since the last durability point. */
	float BurnAccumulator = 0.0f;

	/** Server: durability points burned since lighting, while the equipped item cannot be worn (CL-0034, pending UMOEquipmentComponent::ReduceEquippedDurability). */
	int32 PendingBurnPoints = 0;

	/** Smoothed airflow (a flame takes a moment to flare up or die down). */
	float AirflowSmoothed = 0.6f;
	float FlickerTime = 0.0f;
	float CurrentIntensityCd = 0.0f;

	UMOEquipmentComponent* GetEquipment() const;
	UStaticMeshComponent* GetHeldMesh(EMOEquipmentSlot Slot) const;
	const FMOLightSourceProfile* GetLitProfile() const;

	/** Makes, moves or removes the local light to match the replicated state (any machine). */
	void RefreshFlame();
	void PlaceFlame(const FMOLightSourceProfile& Profile);
	void DestroyFlame();
	void UpdateFlameIntensity(const FMOLightSourceProfile& Profile, float DeltaTime);

	/** Server, while lit: burns durability on the game clock and puts the flame out under water. */
	void TickBurnAuthority(const FMOLightSourceProfile& Profile, float DeltaTime);
};
