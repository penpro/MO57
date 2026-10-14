#include "MOWorldFeatureActor.h"
#include "MOFramework.h"
#include "Components/BoxComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

AMOWorldFeatureActor::AMOWorldFeatureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetCanEverAffectNavigation(false);
	Bounds->SetHiddenInGame(true);
	Bounds->SetBoxExtent(BoundsExtent, /*bUpdateOverlaps=*/false);
	RootComponent = Bounds;

	Composition = CreateDefaultSubobject<UPCGComponent>(TEXT("Composition"));
	Composition->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;
}

int32 AMOWorldFeatureActor::ChooseVariant(int32 Seed, int32 NumVariants)
{
	return NumVariants > 0 ? static_cast<int32>(static_cast<uint32>(Seed) % static_cast<uint32>(NumVariants)) : INDEX_NONE;
}

void AMOWorldFeatureActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Bounds->SetBoxExtent(BoundsExtent, /*bUpdateOverlaps=*/false);
	ChosenVariant = ChooseVariant(Composition->Seed, Variants.Num());
	if (ChosenVariant == INDEX_NONE)
	{
		return;
	}
	UPCGGraphInterface* Graph = Variants[ChosenVariant].LoadSynchronous();
	if (!Graph)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldFeatures] %s: variant %d (%s) did not load"), *GetName(), ChosenVariant, *Variants[ChosenVariant].ToString());
		return;
	}
	// Before BeginPlay, so GenerateOnLoad builds this variant (and only this one).
	Composition->SetGraphLocal(Graph);
}
