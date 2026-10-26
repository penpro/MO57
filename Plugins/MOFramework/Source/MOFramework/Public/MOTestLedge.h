// A test block for the traversal moves (MO.Test.SpawnLedge; Docs/Movement_And_Traversal.md): the engine cube scaled to a height and depth,
// tagged MOClimbable when it is a face to climb. Replicated, shape and all, so a joiner stands at the same block the host made for it: the
// prediction path (the joiner's move and the server's replay of it) needs both machines to trace the same thing. An AStaticMeshActor would
// not do: its scale and its tags do not replicate, so a joiner saw a 1 m plain cube where the host had a climbable 4 m face.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOTestLedge.generated.h"

class UStaticMeshComponent;

UCLASS(NotBlueprintable)
class MOFRAMEWORK_API AMOTestLedge : public AActor
{
	GENERATED_BODY()

public:
	AMOTestLedge();

	/** Server (or a lone machine): the block's size and whether it is climbable. */
	void Configure(float InHeightCm, float InDepthCm, bool bInClimbable);

	float GetHeightCm() const { return HeightCm; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static const FName TestLedgeTag;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Shape();

	UPROPERTY(VisibleAnywhere, Category="MO|Test")
	TObjectPtr<UStaticMeshComponent> Block;

private:
	void ApplyShape();

	UPROPERTY(ReplicatedUsing=OnRep_Shape)
	float HeightCm = 100.0f;

	UPROPERTY(ReplicatedUsing=OnRep_Shape)
	float DepthCm = 150.0f;

	UPROPERTY(ReplicatedUsing=OnRep_Shape)
	bool bClimbable = false;
};
