#include "MOSessionListEntry.h"
#include "Components/TextBlock.h"

void UMOSessionListEntry::SetSessionInfo(const FMOFoundSessionInfo& InSessionInfo)
{
	SessionInfo = InSessionInfo;
	SetEntryId(FName(*FString::FromInt(InSessionInfo.ResultIndex)));

	if (SessionNameText)
	{
		SessionNameText->SetText(FText::FromString(InSessionInfo.DisplayName));
	}

	if (SessionDetailText)
	{
		const FString PingText = InSessionInfo.PingMs >= 0
			? FString::Printf(TEXT("%d ms"), InSessionInfo.PingMs)
			: TEXT("? ms");

		SessionDetailText->SetText(FText::FromString(FString::Printf(
			TEXT("%d/%d players - %s%s"),
			InSessionInfo.CurrentPlayers, InSessionInfo.MaxPlayers, *PingText,
			InSessionInfo.bIsFull ? TEXT(" - FULL") : TEXT(""))));
	}

	SetEntryEnabled(!InSessionInfo.bIsFull);
}
