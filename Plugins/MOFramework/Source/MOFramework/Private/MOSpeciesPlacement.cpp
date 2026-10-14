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
	const float SinkFraction = Species.SinkFraction >= 0.0f ? Species.SinkFraction : (Species.bAlignToSurfaceNormal ? 0.25f : 0.0f);
	float Offset = -(SinkFraction * HeightCm + Species.SinkMinCm);
	if (Species.bAlignToSurfaceNormal)
	{
		Offset -= (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * Scale;   // pivot to bottom: negative bottom (a centred pivot) lifts the mesh onto the surface
	}
	return Offset;
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
	const bool bUpright = !Species.bAlignToSurfaceNormal;
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
