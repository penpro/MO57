/**
 * MOHarvestProgressWidget.cpp - Harvest Progress Widget Implementation
 */

#include "MOHarvestProgressWidget.h"
#include "MOFramework.h"
#include "MOHarvestSubsystem.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/InstancedStaticMeshComponent.h"

void UMOHarvestProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure this widget doesn't steal focus from other UI elements
	SetIsFocusable(false);
}

void UMOHarvestProgressWidget::NativeDestruct()
{
	// Ensure we cancel if widget is destroyed while harvesting
	if (IsProgressActive())
	{
		CancelHarvest();
	}

	Super::NativeDestruct();
}

void UMOHarvestProgressWidget::StartHarvest(
	UInstancedStaticMeshComponent* InISMComponent,
	int32 InInstanceIndex,
	FName InActionId,
	const FText& InActionDisplayName,
	UMOInventoryComponent* InInventory,
	UMOSkillsComponent* InSkills
)
{
	if (!IsValid(InISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvestProgress] StartHarvest: Invalid ISM component"));
		return;
	}

	// Store domain-specific state
	HarvestActionId = InActionId;
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;

	// Start the harvest via subsystem
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvestProgress] No harvest subsystem found"));
		return;
	}

	if (!HarvestSubsystem->BeginHarvest(InISMComponent, InInstanceIndex, InActionId, InInventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvestProgress] Failed to begin harvest"));
		return;
	}

	// Get the duration from the subsystem's context
	const FMOHarvestContext& Context = HarvestSubsystem->GetCurrentContext();
	const float HarvestDuration = FMath::Max(0.1f, Context.TotalTime);

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Started harvest action '%s' (duration: %.1fs)"),
		*InActionId.ToString(), HarvestDuration);

	// Use base class to start the timed progress
	StartProgress(InActionDisplayName, HarvestDuration);
}

void UMOHarvestProgressWidget::CancelHarvest()
{
	if (!IsProgressActive())
	{
		return;
	}

	// Cancel in subsystem
	if (UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>())
	{
		HarvestSubsystem->CancelHarvest();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Harvest cancelled for action '%s'"), *HarvestActionId.ToString());

	// Broadcast empty result for cancellation
	FMOCraftResult EmptyResult;
	OnHarvestCompleted.Broadcast(false, EmptyResult);
	OnHarvestCancelled.Broadcast();

	// Use base class to cancel progress (broadcasts OnCancelled)
	CancelProgress();
}

void UMOHarvestProgressWidget::OnProgressSuccess_Implementation()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Harvest complete for action '%s'"), *HarvestActionId.ToString());

	// Complete the harvest via subsystem
	FMOCraftResult Result;
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (HarvestSubsystem)
	{
		Result = HarvestSubsystem->CompleteHarvest(InventoryComponent.Get(), SkillsComponent.Get());

		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Harvest result: Success=%s, ProducedItems=%d"),
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			Result.ProducedItems.Num());

		// Log produced items
		for (const auto& Pair : Result.ProducedItems)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress]   Produced: %s x%d"),
				*Pair.Key.ToString(), Pair.Value);
		}

		// Log XP granted
		for (const auto& Pair : Result.XPGranted)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress]   XP: %s +%.0f"),
				*Pair.Key.ToString(), Pair.Value);
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvestProgress] No harvest subsystem available for completion"));
		Result.bSuccess = false;
	}

	// Call blueprint event
	OnHarvestSuccess(Result);

	// Broadcast domain-specific completion
	OnHarvestCompleted.Broadcast(Result.bSuccess, Result);
}

void UMOHarvestProgressWidget::UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& InActionName)
{
	// Update progress bar
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress);
	}

	// Update action name
	if (ActionNameText)
	{
		ActionNameText->SetText(InActionName);
	}

	// Use harvest-specific time format
	if (TimeRemainingText)
	{
		const int32 SecondsRemaining = FMath::CeilToInt(TimeRemaining);
		TimeRemainingText->SetText(FText::Format(
			NSLOCTEXT("MO", "HarvestTimeRemaining", "{0}s remaining"),
			FText::AsNumber(SecondsRemaining)));
	}

	// Note: Completion is handled by base class in NativeTick -> OnProgressSuccess
}

void UMOHarvestProgressWidget::UpdateHarvestDisplay_Implementation(float Progress, float TimeRemaining, const FText& InActionName)
{
	// Just delegate to UpdateDisplay
	UpdateDisplay_Implementation(Progress, TimeRemaining, InActionName);
}

void UMOHarvestProgressWidget::OnHarvestSuccess_Implementation(const FMOCraftResult& Result)
{
	// Default implementation - can be overridden in BP to show success feedback
	if (Result.ProducedItems.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Player harvested %d item types"), Result.ProducedItems.Num());
	}
}
