#include "MOSpeciesPlacement.h"
#include "MOResourceNodeDefinitionRow.h"
#include "Engine/CollisionProfile.h"

FName FMOSpeciesPlacement::CollisionProfile(EMOSpeciesCollision SpeciesCollision, const FMOResourceNodeDefinitionRow* Resource, FName DecorativeProfile)
{
	// The species' own say wins over the resource type's default (see EMOSpeciesCollision): saplings are harvestable trees you can still walk through.
	if (SpeciesCollision == EMOSpeciesCollision::Passable)
	{
		return TEXT("OverlapAllDynamic");
	}
	if (SpeciesCollision == EMOSpeciesCollision::Solid)
	{
		return TEXT("BlockAll");
	}
	return Resource ? Resource->GetPlacementCollisionProfile() : DecorativeProfile;
}

float FMOSpeciesPlacement::BurialOffsetCm(const FMOBiomeSpeciesEntry& Species, const FBoxSphereBounds& MeshBounds, float Scale)
{
	const float HeightCm = MeshBounds.BoxExtent.Z * 2.0f * Scale;
	const bool bRestsOnGround = Species.bAlignToSurfaceNormal || Species.bFaceDownhill;   // rock that rests on the surface, not a plant growing out of it
	const float SinkFraction = Species.SinkFraction >= 0.0f ? Species.SinkFraction : (bRestsOnGround ? 0.25f : 0.0f);
	float Offset = -(SinkFraction * HeightCm + Species.SinkMinCm);
	if (bRestsOnGround)
	{
		Offset -= (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * Scale;   // pivot to bottom: negative bottom (a centred pivot) lifts the mesh onto the surface
	}
	return Offset;
}

FQuat FMOSpeciesPlacement::FaceDownhillRotation(const FMOBiomeSpeciesEntry& Species, const FVector& SurfaceNormal, float JitterRad, float FallbackYawRad)
{
	// A surface normal leans toward downhill (ground rising along +X has the normal (-sin, 0, cos)): its horizontal part IS the downhill direction.
	const FVector2D Downhill(SurfaceNormal.X, SurfaceNormal.Y);
	if (Downhill.SizeSquared() < 1.0e-6f)
	{
		return FQuat(FVector::UpVector, FallbackYawRad);
	}
	const float TargetYaw = FMath::Atan2(Downhill.Y, Downhill.X);
	return FQuat(FVector::UpVector, TargetYaw - FMath::DegreesToRadians(Species.FaceYawDeg) + JitterRad);
}

float FMOSpeciesPlacement::FaceDropCm(const FMOBiomeSpeciesEntry& Species, const FBoxSphereBounds& MeshBounds, float Scale, float SlopeRad)
{
	if (!Species.bFaceDownhill || SlopeRad <= 0.0f)
	{
		return 0.0f;
	}
	// How far the mesh reaches from its pivot toward its face (bounds are axis-aligned in mesh space), then how far the ground falls over that reach.
	const float FaceRad = FMath::DegreesToRadians(Species.FaceYawDeg);
	const FVector Face(FMath::Cos(FaceRad), FMath::Sin(FaceRad), 0.0f);
	const float Reach = (FVector::DotProduct(MeshBounds.Origin, Face) + FMath::Abs(Face.X) * MeshBounds.BoxExtent.X + FMath::Abs(Face.Y) * MeshBounds.BoxExtent.Y) * Scale;
	return FMath::Max(0.0f, Reach) * FMath::Tan(FMath::Min(SlopeRad, FMath::DegreesToRadians(70.0f)));
}

bool FMOSpeciesPlacement::ProfileBlocks(FName Profile)
{
	return Profile != UCollisionProfile::NoCollision_ProfileName && Profile != FName(TEXT("OverlapAllDynamic")) && Profile != FName(TEXT("OverlapAll"))
		&& Profile != FName(TEXT("NoCollision"));
}

EMOSpeciesContact FMOSpeciesPlacement::ExpectedContact(const FMOBiomeSpeciesEntry& Species, const FMOResourceNodeDefinitionRow* Resource, FName Profile)
{
	if (!ProfileBlocks(Profile))
	{
		return EMOSpeciesContact::WalkThrough;
	}
	const bool bUpright = !Species.bAlignToSurfaceNormal && !Species.bFaceDownhill;   // a downhill-facing cliff is rock you can stand on, not a plant
	if ((Resource && (Resource->ResourceType == EMOResourceType::Tree || Resource->ResourceType == EMOResourceType::Bush)) || (!Resource && bUpright))
	{
		return EMOSpeciesContact::BlocksWalk;
	}
	return EMOSpeciesContact::Standable;
}

const TCHAR* FMOSpeciesPlacement::ContactName(EMOSpeciesContact Contact)
{
	switch (Contact)
	{
	case EMOSpeciesContact::WalkThrough: return TEXT("walk-through");
	case EMOSpeciesContact::BlocksWalk: return TEXT("blocks-walk");
	case EMOSpeciesContact::Standable: return TEXT("standable");
	}
	return TEXT("?");
}
