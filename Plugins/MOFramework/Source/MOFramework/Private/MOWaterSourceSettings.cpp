#include "MOWaterSourceSettings.h"

#define LOCTEXT_NAMESPACE "MOWaterSources"

UMOWaterSourceSettings::UMOWaterSourceSettings()
{
	// Calibration (Docs/Water_And_Illness.md): backcountry trips drink ~10 L untreated; 0.5-1 % per litre gives the 5-10 % chance of being ill per trip that the
	// before / after trip studies support (5.7-24 % infected, under half symptomatic). Flowing water is safer than still; small still pools are worst. The sea's
	// problem is its salt, not its bugs.
	auto Add = [this](EMOWaterBodyKind Kind, const FText& Name, float Salinity, float Chance)
	{
		FMOWaterSourceProfile& P = Profiles.Add(Kind);
		P.DisplayName = Name;
		P.SalinityGPerL = Salinity;
		P.IllnessChancePerLitre = Chance;
	};
	Add(EMOWaterBodyKind::Sea, LOCTEXT("Sea", "Sea water"), 35.0f, 0.002f);
	Add(EMOWaterBodyKind::River, LOCTEXT("River", "River water"), 0.0f, 0.005f);
	Add(EMOWaterBodyKind::Lake, LOCTEXT("Lake", "Lake water"), 0.0f, 0.01f);
	Add(EMOWaterBodyKind::UplandLake, LOCTEXT("UplandLake", "Lake water"), 0.0f, 0.01f);
	Add(EMOWaterBodyKind::Pond, LOCTEXT("Pond", "Pond water"), 0.0f, 0.025f);
}

FMOWaterSourceProfile UMOWaterSourceSettings::GetProfile(EMOWaterBodyKind Kind)
{
	const UMOWaterSourceSettings* Settings = GetDefault<UMOWaterSourceSettings>();
	if (const FMOWaterSourceProfile* Found = Settings ? Settings->Profiles.Find(Kind) : nullptr)
	{
		return *Found;
	}
	FMOWaterSourceProfile Fallback;
	Fallback.DisplayName = LOCTEXT("Water", "Water");
	return Fallback;
}

#undef LOCTEXT_NAMESPACE
