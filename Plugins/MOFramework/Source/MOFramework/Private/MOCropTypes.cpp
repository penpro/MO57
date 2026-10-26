#include "MOCropTypes.h"

EMOCropStage FMOCropGrowth::StageAt(const FMOCropDefinitionRow& Crop, double DaysGrown)
{
	if (!FMath::IsFinite(DaysGrown) || DaysGrown < 0.0)
	{
		return EMOCropStage::Fallow;
	}
	const double Ripe = FMath::Max(static_cast<double>(Crop.DaysToRipe), 0.0);
	const double Sprout = FMath::Min(static_cast<double>(FMath::Max(Crop.DaysToSprout, 0.0f)), Ripe);
	if (DaysGrown < Sprout)
	{
		return EMOCropStage::Sown;
	}
	if (DaysGrown < Ripe)
	{
		// The first fifth after the green shows is "sprouted"; the rest "growing".
		return DaysGrown < Sprout + (Ripe - Sprout) * 0.2 ? EMOCropStage::Sprouted : EMOCropStage::Growing;
	}
	return DaysGrown < Ripe + FMath::Max(static_cast<double>(Crop.DaysRipe), 0.0) ? EMOCropStage::Ripe : EMOCropStage::Overripe;
}

float FMOCropGrowth::GrowthFraction(const FMOCropDefinitionRow& Crop, double DaysGrown)
{
	if (!FMath::IsFinite(DaysGrown) || DaysGrown <= 0.0)
	{
		return 0.0f;
	}
	const double Ripe = FMath::Max(static_cast<double>(Crop.DaysToRipe), UE_KINDA_SMALL_NUMBER);
	return static_cast<float>(FMath::Clamp(DaysGrown / Ripe, 0.0, 1.0));
}

float FMOCropGrowth::HarvestableFraction(const FMOCropDefinitionRow& Crop, double DaysGrown)
{
	const EMOCropStage Stage = StageAt(Crop, DaysGrown);
	if (Stage == EMOCropStage::Ripe)
	{
		return 1.0f;
	}
	if (Stage != EMOCropStage::Overripe)
	{
		return 0.0f;
	}
	const double PastRipe = DaysGrown - FMath::Max(Crop.DaysToRipe, 0.0f) - FMath::Max(Crop.DaysRipe, 0.0f);
	const double Lost = FMath::Max(static_cast<double>(Crop.DaysOverripeToLost), UE_KINDA_SMALL_NUMBER);
	return static_cast<float>(FMath::Clamp(1.0 - PastRipe / Lost, 0.0, 1.0));
}
