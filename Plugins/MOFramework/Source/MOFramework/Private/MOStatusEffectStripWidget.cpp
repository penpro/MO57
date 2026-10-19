/**
 * MOStatusEffectStripWidget.cpp - Generic moodle bar for HUD status indicators
 */

#include "MOStatusEffectStripWidget.h"
#include "MOStatusMoodleWidget.h"
#include "MOFramework.h"
#include "Algo/Reverse.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UMOStatusEffectStripWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateVisibility();
}

void UMOStatusEffectStripWidget::NativeDestruct()
{
	ActiveMoodles.Empty();
	Super::NativeDestruct();
}

void UMOStatusEffectStripWidget::AddOrUpdateMoodle(const FMOStatusMoodle& Moodle)
{
	if (Moodle.Id.IsNone())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOStatusStrip] AddOrUpdateMoodle refused — empty Id"));
		return;
	}

	if (!MoodleBox)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOStatusStrip] AddOrUpdateMoodle('%s') — MoodleBox not bound; check WBP_StatusEffectStrip"),
			*Moodle.Id.ToString());
		return;
	}

	// Look for an existing entry first — update-in-place avoids the
	// remove-and-recreate flicker that periodic sources (e.g. wet-state
	// auto-refresh) would otherwise hit every poll.
	if (TObjectPtr<UMOStatusMoodleWidget>* ExistingPtr = ActiveMoodles.Find(Moodle.Id))
	{
		if (UMOStatusMoodleWidget* Existing = ExistingPtr->Get())
		{
			Existing->SetMoodle(Moodle);
			RebuildOrder();   // the severity may have changed: the order follows it
			return;
		}
		// Stale entry — drop it and fall through to creation.
		ActiveMoodles.Remove(Moodle.Id);
	}

	if (!MoodleEntryClass)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOStatusStrip] MoodleEntryClass not set — set TSubclassOf to WBP_StatusMoodle in BP defaults"));
		return;
	}

	UMOStatusMoodleWidget* NewEntry = CreateWidget<UMOStatusMoodleWidget>(this, MoodleEntryClass);
	if (!NewEntry)
	{
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOStatusStrip] Failed to create moodle entry widget for '%s'"),
			*Moodle.Id.ToString());
		return;
	}

	NewEntry->SetExpanded(bExpanded);
	NewEntry->SetMoodle(Moodle);
	ActiveMoodles.Add(Moodle.Id, NewEntry);
	RebuildOrder();   // places the entry (and re-sorts) instead of appending

	UpdateVisibility();

	// DEBUG (MOStatusStrip, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose,
	// 	TEXT("[MOStatusStrip] Added moodle '%s' (now %d active)"),
	// 	*Moodle.Id.ToString(), ActiveMoodles.Num());
}

void UMOStatusEffectStripWidget::RemoveMoodle(FName MoodleId)
{
	if (TObjectPtr<UMOStatusMoodleWidget>* ExistingPtr = ActiveMoodles.Find(MoodleId))
	{
		if (UMOStatusMoodleWidget* Existing = ExistingPtr->Get())
		{
			Existing->RemoveFromParent();
		}
		ActiveMoodles.Remove(MoodleId);
		RebuildOrder();
		UpdateVisibility();

		// DEBUG (MOStatusStrip, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose,
		// 	TEXT("[MOStatusStrip] Removed moodle '%s' (%d remaining)"),
		// 	*MoodleId.ToString(), ActiveMoodles.Num());
	}
}

void UMOStatusEffectStripWidget::SetExpanded(bool bInExpanded)
{
	if (bExpanded == bInExpanded)
	{
		return;
	}
	bExpanded = bInExpanded;
	for (const TPair<FName, TObjectPtr<UMOStatusMoodleWidget>>& Pair : ActiveMoodles)
	{
		if (UMOStatusMoodleWidget* W = Pair.Value)
		{
			W->SetExpanded(bExpanded);
		}
	}
}

void UMOStatusEffectStripWidget::ClearAllMoodles()
{
	for (auto& Pair : ActiveMoodles)
	{
		if (UMOStatusMoodleWidget* W = Pair.Value)
		{
			W->RemoveFromParent();
		}
	}
	ActiveMoodles.Empty();
	RebuildOrder();
	UpdateVisibility();
}

namespace
{
	int32 MoodleSeverityRank(EMOStatusMoodleSeverity Severity)
	{
		switch (Severity)
		{
		case EMOStatusMoodleSeverity::Critical: return 0;
		case EMOStatusMoodleSeverity::Warning:  return 1;
		case EMOStatusMoodleSeverity::Info:     return 2;
		case EMOStatusMoodleSeverity::Buff:     return 3;
		default:                                return 4;
		}
	}
}

void UMOStatusEffectStripWidget::RebuildOrder()
{
	if (!MoodleBox)
	{
		return;
	}
	TArray<UMOStatusMoodleWidget*> Sorted;
	for (const TPair<FName, TObjectPtr<UMOStatusMoodleWidget>>& Pair : ActiveMoodles)
	{
		if (UMOStatusMoodleWidget* W = Pair.Value)
		{
			Sorted.Add(W);
		}
	}
	Sorted.Sort([](const UMOStatusMoodleWidget& A, const UMOStatusMoodleWidget& B)
	{
		const int32 RA = MoodleSeverityRank(A.GetMoodle().Severity);
		const int32 RB = MoodleSeverityRank(B.GetMoodle().Severity);
		return RA != RB ? RA < RB : A.GetMoodle().Id.LexicalLess(B.GetMoodle().Id);
	});

	const int32 Shown = FMath::Min(Sorted.Num(), FMath::Max(1, MaxVisibleMoodles));
	const int32 Folded = Sorted.Num() - Shown;

	TArray<UWidget*> Order;
	for (int32 i = 0; i < Shown; ++i)
	{
		Order.Add(Sorted[i]);
	}
	if (OverflowPill)
	{
		if (Folded > 0)
		{
			if (OverflowText)
			{
				OverflowText->SetText(FText::Format(NSLOCTEXT("MO", "MoodleOverflow", "+{0} more"), FText::AsNumber(Folded)));
			}
			OverflowPill->SetVisibility(ESlateVisibility::HitTestInvisible);
			Order.Add(OverflowPill);
		}
		else
		{
			OverflowPill->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (bStackUpward)
	{
		Algo::Reverse(Order);   // the worst entry ends up nearest the corner the strip is anchored to
	}

	MoodleBox->ClearChildren();
	for (UWidget* Entry : Order)
	{
		if (UVerticalBoxSlot* BoxSlot = MoodleBox->AddChildToVerticalBox(Entry))
		{
			BoxSlot->SetPadding(FMargin(0.0f, 4.0f));   // 8 px between pills
			BoxSlot->SetHorizontalAlignment(HAlign_Left);
		}
	}
}

bool UMOStatusEffectStripWidget::HasMoodle(FName MoodleId) const
{
	return ActiveMoodles.Contains(MoodleId);
}

void UMOStatusEffectStripWidget::UpdateVisibility()
{
	if (!bAutoCollapseWhenEmpty) return;

	SetVisibility(ActiveMoodles.Num() > 0
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
}
