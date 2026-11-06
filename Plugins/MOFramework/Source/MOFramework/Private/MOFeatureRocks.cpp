#include "MOFeatureRocks.h"
#include "MOSpeciesPlacement.h"
#include "MOFramework.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	// Angular, broken scans. The round boulders (wgosajs, wc2hbfr) read as eggs stuck on a wall (seen in a ravine, Nov 6: every mesh tried in place).
	const TCHAR* const RockMeshPaths[] = {
		TEXT("/Game/Fab/Megascans/3D/Beach_Boulder_ukopfbks/Medium/ukopfbks_tier_2/StaticMeshes/ukopfbks_tier_2.ukopfbks_tier_2"),
		TEXT("/Game/Fab/Megascans/3D/Beach_Rock_wd5mbhd/Medium/wd5mbhd_tier_2/StaticMeshes/wd5mbhd_tier_2.wd5mbhd_tier_2"),
		TEXT("/Game/Megascans/3D_Assets/NordicBeachRocks/SM_NordicBeachRocks_01.SM_NordicBeachRocks_01"),
	};
}

void MOFeatureRocks::LoadMeshes(TArray<UStaticMesh*>& OutMeshes, TArray<FMeshSize>& OutSizes)
{
	OutMeshes.Reset();
	OutSizes.Reset();
	for (const TCHAR* Path : RockMeshPaths)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path))
		{
			const FBoxSphereBounds Bounds = FMOSpeciesPlacement::SeatBounds(Mesh);
			OutMeshes.Add(Mesh);
			OutSizes.Add({ Bounds.Origin, Bounds.BoxExtent });
		}
	}
	if (OutMeshes.IsEmpty())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOFeatureRocks] none of the rock meshes loaded"));
	}
}

FTransform MOFeatureRocks::MakeInstance(const FMeshSize& Size, const FVector& Center, const FRotator& Rotation, double HalfWidth, double HalfHeight)
{
	const double Across = HalfWidth / FMath::Max(1.0, FMath::Max(Size.Extent.X, Size.Extent.Y));
	const double Along = HalfHeight / FMath::Max(1.0, Size.Extent.Z);
	const FVector Scale(Across, Across, Along);
	// The mesh's middle, not its pivot, goes to Center: a scan's pivot can be anywhere.
	return FTransform(Rotation, Center - Rotation.RotateVector(Size.Origin * Scale), Scale);
}

AActor* MOFeatureRocks::SpawnHolder(UWorld* World, const TArray<UStaticMesh*>& Meshes, const TArray<TArray<FTransform>>& ByMesh)
{
	bool bAny = false;
	for (const TArray<FTransform>& Instances : ByMesh)
	{
		bAny |= !Instances.IsEmpty();
	}
	if (!World || !bAny)
	{
		return nullptr;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
	if (!Holder)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("FeatureRocksRoot"), RF_Transient);
	Root->SetMobility(EComponentMobility::Static);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	for (int32 M = 0; M < Meshes.Num() && M < ByMesh.Num(); ++M)
	{
		if (ByMesh[M].IsEmpty())
		{
			continue;
		}
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Holder, NAME_None, RF_Transient);
		ISM->SetStaticMesh(Meshes[M]);
		ISM->SetMobility(EComponentMobility::Static);
		ISM->SetCollisionProfileName(TEXT("BlockAll"));
		ISM->SetCanEverAffectNavigation(false);
		ISM->bAffectDynamicIndirectLighting = false;   // hardware Lumen lights its hits where they land (N-333); a surface cache for these would be wasted
		ISM->bAffectDistanceFieldLighting = false;
		ISM->SetEvaluateWorldPositionOffset(false);
		ISM->ComponentTags.Add(TEXT("MOClimbable"));
		ISM->SetupAttachment(Root);
		ISM->RegisterComponent();
		ISM->AddInstances(ByMesh[M], /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
	}
	return Holder;
}
