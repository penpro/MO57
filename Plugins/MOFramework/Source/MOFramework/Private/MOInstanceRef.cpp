#include "MOInstanceRef.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

FMOInstanceRef FMOInstanceRef::Make(const UInstancedStaticMeshComponent* ISM, int32 InstanceIndex)
{
	FMOInstanceRef Ref;
	FTransform Xf;
	if (IsValid(ISM) && ISM->GetStaticMesh() && ISM->GetInstanceTransform(InstanceIndex, Xf, /*bWorldSpace=*/true))
	{
		Ref.Mesh = ISM->GetStaticMesh();
		Ref.Location = Xf.GetLocation();
	}
	return Ref;
}

bool FMOInstanceRef::Resolve(const UWorld* World, UInstancedStaticMeshComponent*& OutISM, int32& OutInstanceIndex) const
{
	OutISM = nullptr;
	OutInstanceIndex = INDEX_NONE;
	if (!World || !Mesh)
	{
		return false;
	}
	double BestSq = FMath::Square(ToleranceCm);
	// Every instanced mesh component in memory, not every actor: a PCG world has thousands of actors and far fewer components of this one mesh. The world,
	// mesh and bounds tests reject almost all of them before an instance is looked at.
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* ISM = *It;
		if (!IsValid(ISM) || ISM->GetWorld() != World || ISM->GetStaticMesh() != Mesh || ISM->GetInstanceCount() == 0
			|| !ISM->Bounds.GetBox().ExpandBy(ToleranceCm).IsInside(Location))
		{
			continue;
		}
		for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
		{
			FTransform Xf;
			if (!ISM->GetInstanceTransform(i, Xf, /*bWorldSpace=*/true))
			{
				continue;
			}
			const double DistSq = FVector::DistSquared(Xf.GetLocation(), Location);
			if (DistSq <= BestSq)
			{
				BestSq = DistSq;
				OutISM = ISM;
				OutInstanceIndex = i;
			}
		}
	}
	return OutISM != nullptr;
}

FString FMOInstanceRef::ToString() const
{
	return FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f)"), *GetNameSafe(Mesh), Location.X, Location.Y, Location.Z);
}
