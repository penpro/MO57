#include "MOPawnEntryWidget.h"
#include "MOCommonButton.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"

void UMOPawnEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PossessButton)
	{
		PossessButton->OnClicked().RemoveAll(this);
		PossessButton->OnClicked().AddUObject(this, &UMOPawnEntryWidget::HandlePossessClicked);
	}
	if (EntryButton)
	{
		EntryButton->OnClicked().RemoveAll(this);
		EntryButton->OnClicked().AddUObject(this, &UMOPawnEntryWidget::HandlePossessClicked);
	}
}

void UMOPawnEntryWidget::NativeDestruct()
{
	// Clean up button bindings
	if (PossessButton)
	{
		PossessButton->OnClicked().RemoveAll(this);
	}
	if (EntryButton)
	{
		EntryButton->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMOPawnEntryWidget::InitializeEntry(const FMOPersistedPawnRecord& PawnRecord)
{
	CachedPawnGuid = PawnRecord.PawnGuid;
	bIsDeceased = PawnRecord.bIsDeceased;

	// Name
	if (NameText)
	{
		NameText->SetText(FText::FromString(PawnRecord.CharacterName.IsEmpty() ? TEXT("Unknown") : PawnRecord.CharacterName));
	}

	// Age - convert days to years for display
	if (AgeText)
	{
		int32 AgeYears = PawnRecord.AgeInDays / 365;
		AgeText->SetText(FText::Format(NSLOCTEXT("MO", "AgeFormat", "{0} years"), FText::AsNumber(AgeYears)));
	}

	// Gender
	if (GenderText)
	{
		GenderText->SetText(FText::FromString(PawnRecord.Gender.IsEmpty() ? TEXT("Unknown") : PawnRecord.Gender));
	}

	// Health bar
	if (HealthBar)
	{
		HealthBar->SetPercent(PawnRecord.HealthPercent);

		// Color based on health
		FLinearColor HealthColor;
		if (PawnRecord.bIsDeceased)
		{
			HealthColor = FLinearColor(0.3f, 0.3f, 0.3f); // Gray for deceased
		}
		else if (PawnRecord.HealthPercent > 0.6f)
		{
			HealthColor = FLinearColor::Green;
		}
		else if (PawnRecord.HealthPercent > 0.3f)
		{
			HealthColor = FLinearColor::Yellow;
		}
		else
		{
			HealthColor = FLinearColor::Red;
		}
		HealthBar->SetFillColorAndOpacity(HealthColor);
	}

	// Status
	if (StatusText)
	{
		FString Status = PawnRecord.StatusText;
		if (PawnRecord.bIsDeceased)
		{
			Status = TEXT("Deceased");
		}
		else if (Status.IsEmpty())
		{
			Status = TEXT("Healthy");
		}
		StatusText->SetText(FText::FromString(Status));
	}

	// Location
	if (LocationText)
	{
		FString Location = PawnRecord.LocationName;
		if (Location.IsEmpty())
		{
			// Derive from transform
			FVector Pos = PawnRecord.Transform.GetLocation();
			Location = FString::Printf(TEXT("%.0f, %.0f"), Pos.X, Pos.Y);
		}
		LocationText->SetText(FText::FromString(Location));
	}

	// Last played
	if (LastPlayedText)
	{
		if (PawnRecord.LastPlayedTime.GetTicks() > 0)
		{
			FTimespan TimeSince = FDateTime::Now() - PawnRecord.LastPlayedTime;
			FString TimeText;
			if (TimeSince.GetTotalDays() > 1)
			{
				TimeText = FString::Printf(TEXT("%.0f days ago"), TimeSince.GetTotalDays());
			}
			else if (TimeSince.GetTotalHours() > 1)
			{
				TimeText = FString::Printf(TEXT("%.0f hours ago"), TimeSince.GetTotalHours());
			}
			else
			{
				TimeText = TEXT("Recently");
			}
			LastPlayedText->SetText(FText::FromString(TimeText));
		}
		else
		{
			LastPlayedText->SetText(NSLOCTEXT("MO", "NeverPlayed", "Never played"));
		}
	}

	// Possess button state
	if (PossessButton)
	{
		PossessButton->SetIsEnabled(!PawnRecord.bIsDeceased);
	}

	// Dim the whole entry if deceased
	if (bIsDeceased)
	{
		SetRenderOpacity(0.5f);
	}

	// Notify Blueprint
	OnEntryInitialized(PawnRecord);
}

void UMOPawnEntryWidget::SetOccupancy(EMOPawnOccupancy InOccupancy, const FString& OccupantName)
{
	Occupancy = InOccupancy;
	FText Line;
	if (InOccupancy == EMOPawnOccupancy::You)
	{
		Line = NSLOCTEXT("MO", "PossessCurrent", "Current survivor");
	}
	else if (InOccupancy == EMOPawnOccupancy::OtherPlayer)
	{
		Line = OccupantName.IsEmpty()
			? NSLOCTEXT("MO", "PossessOtherPlayer", "Played by another player")
			: FText::Format(NSLOCTEXT("MO", "PossessPlayedBy", "Played by {0}"), FText::FromString(OccupantName));
	}
	if (OccupantText)
	{
		OccupantText->SetText(Line);
		OccupantText->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	else if (StatusText && !Line.IsEmpty())
	{
		StatusText->SetText(Line);
	}
	if (PossessButton)
	{
		PossessButton->SetIsEnabled(InOccupancy == EMOPawnOccupancy::Free && !bIsDeceased);
		if (InOccupancy != EMOPawnOccupancy::Free)
		{
			PossessButton->SetButtonText(InOccupancy == EMOPawnOccupancy::You ? NSLOCTEXT("MO", "PossessPlaying", "Playing") : NSLOCTEXT("MO", "PossessTaken", "Taken"));
		}
	}
}

void UMOPawnEntryWidget::HandlePossessClicked()
{
	if (CachedPawnGuid.IsValid() && !bIsDeceased && Occupancy == EMOPawnOccupancy::Free)
	{
		OnPossessClicked.Broadcast(CachedPawnGuid);
	}
}
