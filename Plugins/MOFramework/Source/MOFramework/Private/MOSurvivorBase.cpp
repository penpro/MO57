#include "MOSurvivorBase.h"
#include "MOCharacterAppearance.h"

void FMOSurvivorBase::ApplyToLook(FMOCharacterAppearance& Look) const
{
	Look.BaseId = Id;
	Look.HairAssetPath = Hair;
	Look.EyebrowsAssetPath = Eyebrows;
	Look.EyelashesAssetPath = Eyelashes;
	Look.BeardAssetPath = Beard.IsEmpty() ? FMOCharacterAppearance::NoGroom : Beard;
	Look.MustacheAssetPath = Mustache.IsEmpty() ? FMOCharacterAppearance::NoGroom : Mustache;
}

const TArray<FMOSurvivorBase>& UMOSurvivorBaseSettings::GetBases()
{
	return GetDefault<UMOSurvivorBaseSettings>()->Bases;
}

const FMOSurvivorBase* UMOSurvivorBaseSettings::Find(FName Id)
{
	return Id.IsNone() ? nullptr : GetBases().FindByPredicate([Id](const FMOSurvivorBase& Base) { return Base.Id == Id; });
}
