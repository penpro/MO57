/**
 * MOQueueDisplayTypes.cpp - pure formatting for the neutral queue rows (Stage 3)
 * See MOQueueDisplayTypes.h. Keep this file free of component/world access.
 */

#include "MOQueueDisplayTypes.h"
#include "MOUIUtils.h"

namespace MOQueueDisplay
{

FText FormatCount(int32 Current, int32 Total)
{
	return FText::Format(
		NSLOCTEXT("MOQueue", "QueueRowCount", "{0}/{1}"),
		FText::AsNumber(Current),
		FText::AsNumber(Total));
}

FText FormatPercent(float Progress01)
{
	return FText::Format(
		NSLOCTEXT("MOQueue", "QueueProgressPercent", "{0}%"),
		FText::AsNumber(FMath::RoundToInt(FMath::Clamp(Progress01, 0.0f, 1.0f) * 100.0f)));
}

void FinalizeRowTexts(FMOQueueDisplayRow& Row)
{
	if (Row.CountText.IsEmpty() && Row.CountTotal > 0)
	{
		Row.CountText = FormatCount(Row.CountCurrent, Row.CountTotal);
	}
	if (Row.TimeRemainingText.IsEmpty() && Row.RemainingSeconds >= 0.0f)
	{
		Row.TimeRemainingText = UMOUIUtils::FormatDurationAsText(Row.RemainingSeconds);
	}
}

} // namespace MOQueueDisplay
