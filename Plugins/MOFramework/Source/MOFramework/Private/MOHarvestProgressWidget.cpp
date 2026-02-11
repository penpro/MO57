#include "MOHarvestProgressWidget.h"
#include "MOFramework.h"
#include "MOHarvestSubsystem.h"
#include "MOCraftingSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"
#include "TimerManager.h"
#include "Components/InstancedStaticMeshComponent.h"

void UMOHarvestProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure this widget doesn't steal focus from other UI elements
	SetIsFocusable(false);

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOHarvestProgressWidget::HandleCancelClicked);
	}
}

void UMOHarvestProgressWidget::NativeDestruct()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HarvestTimerHandle);
	}

	// Ensure we cancel if widget is destroyed while harvesting
	if (bIsHarvesting)
	{
		CancelHarvest();
	}

	Super::NativeDestruct();
}

void UMOHarvestProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsHarvesting)
	{
		// Calculate progress using GetRealTimeSeconds (continues even when game is paused)
		if (const UWorld* World = GetWorld())
		{
			ElapsedTime = static_cast<float>(World->GetRealTimeSeconds() - HarvestStartTime);
			CurrentProgress = FMath::Clamp(ElapsedTime / HarvestDuration, 0.0f, 1.0f);

			// Check for completion
			if (CurrentProgress >= 1.0f)
			{
				GetWorld()->GetTimerManager().ClearTimer(HarvestTimerHandle);
				CompleteHarvest();
				return;
			}

			const float TimeRemaining = FMath::Max(0.0f, HarvestDuration - ElapsedTime);
			UpdateDisplay(CurrentProgress, TimeRemaining, ActionDisplayName);
		}
	}
}

void UMOHarvestProgressWidget::TickHarvest()
{
	if (!bIsHarvesting)
	{
		return;
	}

	// Use GetRealTimeSeconds - continues even when game is paused
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ElapsedTime = static_cast<float>(World->GetRealTimeSeconds() - HarvestStartTime);
	CurrentProgress = FMath::Clamp(ElapsedTime / HarvestDuration, 0.0f, 1.0f);

	// Check for completion
	if (CurrentProgress >= 1.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(HarvestTimerHandle);
		CompleteHarvest();
	}
}

void UMOHarvestProgressWidget::StartHarvest(
	UInstancedStaticMeshComponent* InISMComponent,
	int32 InInstanceIndex,
	FName InRecipeId,
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

	RecipeId = InRecipeId;
	ActionDisplayName = InActionDisplayName;
	InventoryComponent = InInventory;
	SkillsComponent = InSkills;

	// Start the harvest via subsystem
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvestProgress] No harvest subsystem found"));
		return;
	}

	if (!HarvestSubsystem->BeginHarvest(InISMComponent, InInstanceIndex, InRecipeId, InInventory))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOHarvestProgress] Failed to begin harvest"));
		return;
	}

	// Get the duration from the subsystem's context
	const FMOHarvestContext& Context = HarvestSubsystem->GetCurrentContext();
	HarvestDuration = FMath::Max(0.1f, Context.TotalTime);

	ElapsedTime = 0.0f;
	CurrentProgress = 0.0f;
	bIsHarvesting = true;

	// Record start time using GetRealTimeSeconds (continues even when paused)
	if (const UWorld* World = GetWorld())
	{
		HarvestStartTime = World->GetRealTimeSeconds();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Started harvest '%s' (duration: %.1fs)"),
		*RecipeId.ToString(), HarvestDuration);

	// Start timer for display updates
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HarvestTimerHandle);
		World->GetTimerManager().SetTimer(
			HarvestTimerHandle,
			this,
			&UMOHarvestProgressWidget::TickHarvest,
			0.1f,  // Tick every 100ms for display updates
			true,  // Looping
			0.0f   // No delay
		);
	}

	// Initial display update
	UpdateDisplay(0.0f, HarvestDuration, ActionDisplayName);
}

void UMOHarvestProgressWidget::CancelHarvest()
{
	if (!bIsHarvesting)
	{
		return;
	}

	bIsHarvesting = false;

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HarvestTimerHandle);
	}

	// Cancel in subsystem
	if (UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>())
	{
		HarvestSubsystem->CancelHarvest();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Harvest cancelled for '%s'"), *RecipeId.ToString());

	// Broadcast empty result for cancellation
	FMOCraftResult EmptyResult;
	OnHarvestCompleted.Broadcast(false, EmptyResult);
	OnHarvestCancelled.Broadcast();
}

void UMOHarvestProgressWidget::CompleteHarvest()
{
	if (!bIsHarvesting)
	{
		return;
	}

	bIsHarvesting = false;

	UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Harvest complete for '%s'"), *RecipeId.ToString());

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

	// Broadcast completion
	OnHarvestCompleted.Broadcast(true, Result);
}

void UMOHarvestProgressWidget::HandleCancelClicked()
{
	CancelHarvest();
}

void UMOHarvestProgressWidget::UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& ActionName)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress);
	}

	if (ActionNameText)
	{
		ActionNameText->SetText(ActionName);
	}

	if (TimeRemainingText)
	{
		int32 SecondsRemaining = FMath::CeilToInt(TimeRemaining);
		TimeRemainingText->SetText(FText::Format(
			NSLOCTEXT("MO", "HarvestTimeRemaining", "{0}s remaining"),
			FText::AsNumber(SecondsRemaining)));
	}
}

void UMOHarvestProgressWidget::OnHarvestSuccess_Implementation(const FMOCraftResult& Result)
{
	// Default implementation - can be overridden in BP to show success feedback
	if (Result.ProducedItems.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOHarvestProgress] Player harvested %d item types"), Result.ProducedItems.Num());
	}
}
