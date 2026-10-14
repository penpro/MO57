/**
 * =============================================================================
 * MOWorldFeatureActor.h - Generic world feature (POI) actor: one PCG composition chosen from variants
 * =============================================================================
 *
 * A world feature whose whole composition is a PCG graph needs no Blueprint of its own: point the DT_WorldFeatures row at this class and set
 * Variants in ActorProperties, e.g. Variants = ("/Game/WorldFeatures/Cliffs/PCG_LargeCliff_01b.PCG_LargeCliff_01b", ...).
 *
 * The site seed (UMOWorldFeatureSubsystem writes it into every PCG component before FinishSpawning) picks the variant, so every co-op machine
 * builds the same one at the same site. The graph builds around the actor's own point (Get Actor Data: single point) -- the actor transform is
 * the site's location, yaw and scale.
 *
 * The box is the composition's bounds only: PCG will not generate on an actor without primitive bounds. It has no collision and never renders.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOWorldFeatureActor.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;

UCLASS(Blueprintable)
class MOFRAMEWORK_API AMOWorldFeatureActor : public AActor
{
	GENERATED_BODY()

public:
	AMOWorldFeatureActor();

	/** The compositions this feature can be. One is built per site, chosen by the site seed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Feature")
	TArray<TSoftObjectPtr<UPCGGraphInterface>> Variants;

	/** Half-size of the composition's bounds (cm, before the site scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Feature")
	FVector BoundsExtent = FVector(3000.0, 3000.0, 3000.0);

	/** Index into Variants for a seed: the same on every machine. INDEX_NONE without variants. */
	static int32 ChooseVariant(int32 Seed, int32 NumVariants);

	UFUNCTION(BlueprintPure, Category="World Feature")
	int32 GetChosenVariant() const { return ChosenVariant; }

	UPCGComponent* GetComposition() const { return Composition; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category="World Feature")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category="World Feature")
	TObjectPtr<UPCGComponent> Composition;

	int32 ChosenVariant = INDEX_NONE;
};
