/**
 * MOStatusEffectStripWidget.cpp - Generic moodle bar for HUD status indicators
 */

#include "MOStatusEffectStripWidget.h"
#include "MOStatusMoodleWidget.h"
#include "MOFramework.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

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

	MoodleBox->AddChildToHorizontalBox(NewEntry);
	NewEntry->SetMoodle(Moodle);
	ActiveMoodles.Add(Moodle.Id, NewEntry);

	UpdateVisibility();

	UE_LOG(LogMOFramework, Verbose,
		TEXT("[MOStatusStrip] Added moodle '%s' (now %d active)"),
		*Moodle.Id.ToString(), ActiveMoodles.Num());
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
		UpdateVisibility();

		UE_LOG(LogMOFramework, Verbose,
			TEXT("[MOStatusStrip] Removed moodle '%s' (%d remaining)"),
			*MoodleId.ToString(), ActiveMoodles.Num());
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
	UpdateVisibility();
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
