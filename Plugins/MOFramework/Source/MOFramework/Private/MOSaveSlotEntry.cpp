#include "MOSaveSlotEntry.h"
#include "MOFramework.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UMOSaveSlotEntry::NativeConstruct()
{
	Super::NativeConstruct();
}

void UMOSaveSlotEntry::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnSlotSelected.Broadcast(Metadata.SlotName);
}

void UMOSaveSlotEntry::InitializeFromMetadata(const FMOSaveMetadata& InMetadata)
{
	Metadata = InMetadata;
	RefreshDisplay();
	OnMetadataUpdated(Metadata);
}

void UMOSaveSlotEntry::RefreshDisplay()
{
	if (SaveNameText)
	{
		SaveNameText->SetText(Metadata.DisplayName);
	}

	if (TimestampText)
	{
		// Format: "Jan 27, 2026 3:05 PM"
		const FString FormattedTime = Metadata.Timestamp.ToString(TEXT("%b %d, %Y %I:%M %p"));
		TimestampText->SetText(FText::FromString(FormattedTime));
	}

	if (PlayTimeText)
	{
		// Format: "2h 35m" or "35m" if less than an hour
		const int32 TotalMinutes = FMath::FloorToInt(Metadata.PlayTime.GetTotalMinutes());
		const int32 Hours = TotalMinutes / 60;
		const int32 Minutes = TotalMinutes % 60;

		FString PlayTimeStr;
		if (Hours > 0)
		{
			PlayTimeStr = FString::Printf(TEXT("%dh %dm"), Hours, Minutes);
		}
		else
		{
			PlayTimeStr = FString::Printf(TEXT("%dm"), Minutes);
		}
		PlayTimeText->SetText(FText::FromString(PlayTimeStr));
	}

	if (WorldNameText)
	{
		WorldNameText->SetText(FText::FromString(Metadata.WorldName));
	}

	if (CharacterInfoText)
	{
		CharacterInfoText->SetText(FText::FromString(Metadata.CharacterInfo));
	}

	if (AutosaveIndicator)
	{
		AutosaveIndicator->SetVisibility(Metadata.bIsAutosave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// TODO: Load screenshot thumbnail from ScreenshotPath if available
}
