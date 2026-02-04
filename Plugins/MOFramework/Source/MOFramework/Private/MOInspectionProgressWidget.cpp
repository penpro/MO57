#include "MOInspectionProgressWidget.h"
#include "MOFramework.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"
#include "TimerManager.h"

void UMOInspectionProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure this widget doesn't steal focus from other UI elements
	SetIsFocusable(false);

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOInspectionProgressWidget::HandleCancelClicked);
	}
}

void UMOInspectionProgressWidget::NativeDestruct()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InspectionTimerHandle);
	}

	// Ensure we cancel if widget is destroyed while inspecting
	if (bIsInspecting)
	{
		CancelInspection();
	}

	Super::NativeDestruct();
}

void UMOInspectionProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsInspecting)
	{
		// Calculate progress using GetRealTimeSeconds (continues even when game is paused)
		if (const UWorld* World = GetWorld())
		{
			ElapsedTime = static_cast<float>(World->GetRealTimeSeconds() - InspectionStartTime);
			CurrentProgress = FMath::Clamp(ElapsedTime / InspectionDuration, 0.0f, 1.0f);

			// Check for completion
			if (CurrentProgress >= 1.0f)
			{
				GetWorld()->GetTimerManager().ClearTimer(InspectionTimerHandle);
				CompleteInspection();
				return;
			}

			const float TimeRemaining = FMath::Max(0.0f, InspectionDuration - ElapsedTime);
			UpdateDisplay(CurrentProgress, TimeRemaining, ItemDisplayName);
		}
	}
}

void UMOInspectionProgressWidget::TickInspection()
{
	if (!bIsInspecting)
	{
		return;
	}

	// Use GetRealTimeSeconds - continues even when game is paused
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ElapsedTime = static_cast<float>(World->GetRealTimeSeconds() - InspectionStartTime);
	CurrentProgress = FMath::Clamp(ElapsedTime / InspectionDuration, 0.0f, 1.0f);

	// Check for completion
	if (CurrentProgress >= 1.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(InspectionTimerHandle);
		CompleteInspection();
	}
}

void UMOInspectionProgressWidget::StartInspection(
	FName InItemDefinitionId,
	const FText& InItemDisplayName,
	UMOKnowledgeComponent* InKnowledgeComponent,
	UMOSkillsComponent* InSkillsComponent,
	float InInspectionDuration)
{
	ItemDefinitionId = InItemDefinitionId;
	ItemDisplayName = InItemDisplayName;
	KnowledgeComponent = InKnowledgeComponent;
	SkillsComponent = InSkillsComponent;

	// Use debug duration if set, otherwise use the provided duration
	if (DebugInspectionDuration > 0.0f)
	{
		InspectionDuration = DebugInspectionDuration;
		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Using DEBUG duration override: %.1fs"), InspectionDuration);
	}
	else
	{
		InspectionDuration = FMath::Max(0.1f, InInspectionDuration);
	}

	ElapsedTime = 0.0f;
	CurrentProgress = 0.0f;
	bIsInspecting = true;

	// Record start time using GetRealTimeSeconds (continues even when paused)
	if (const UWorld* World = GetWorld())
	{
		InspectionStartTime = World->GetRealTimeSeconds();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Started inspection of '%s' (duration: %.1fs)"),
		*ItemDefinitionId.ToString(), InspectionDuration);

	// Start timer for display updates
	// Progress is calculated from wall-clock time, so it continues even when tabbed out
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InspectionTimerHandle);
		World->GetTimerManager().SetTimer(
			InspectionTimerHandle,
			this,
			&UMOInspectionProgressWidget::TickInspection,
			0.1f,  // Tick every 100ms for display updates
			true,  // Looping
			0.0f   // No delay
		);
	}

	// Initial display update
	UpdateDisplay(0.0f, InspectionDuration, ItemDisplayName);
}

void UMOInspectionProgressWidget::CancelInspection()
{
	if (!bIsInspecting)
	{
		return;
	}

	bIsInspecting = false;

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InspectionTimerHandle);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Inspection cancelled for '%s'"), *ItemDefinitionId.ToString());

	// Broadcast empty result for cancellation
	FMOInspectionResult EmptyResult;
	OnInspectionCompleted.Broadcast(false, EmptyResult);
	OnInspectionCancelled.Broadcast();
}

void UMOInspectionProgressWidget::CompleteInspection()
{
	if (!bIsInspecting)
	{
		return;
	}

	bIsInspecting = false;

	UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Inspection complete for '%s'"), *ItemDefinitionId.ToString());

	// Perform the actual inspection via knowledge component
	FMOInspectionResult Result;
	UMOKnowledgeComponent* Knowledge = KnowledgeComponent.Get();
	UMOSkillsComponent* Skills = SkillsComponent.Get();

	if (IsValid(Knowledge))
	{
		Result = Knowledge->InspectItem(ItemDefinitionId, Skills);

		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Inspection result: Success=%s, XPGrants=%d, FirstInspection=%s"),
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			Result.XPGrants.Num(),
			Result.bFirstInspection ? TEXT("true") : TEXT("false"));

		// Log XP granted to each skill/knowledge
		for (const FMOInspectionXPGrant& Grant : Result.XPGrants)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInspection]   %s '%s': +%.0f XP (level %d -> %d)"),
				Grant.bIsKnowledge ? TEXT("Knowledge") : TEXT("Skill"),
				*Grant.Id.ToString(),
				Grant.XPAmount,
				Grant.LevelBefore,
				Grant.LevelAfter);
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInspection] No KnowledgeComponent available for inspection"));
		Result.bSuccess = false;
	}

	// Call blueprint event
	OnInspectionSuccess(Result);

	// Broadcast completion
	OnInspectionCompleted.Broadcast(true, Result);
}

void UMOInspectionProgressWidget::HandleCancelClicked()
{
	CancelInspection();
}

void UMOInspectionProgressWidget::UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& ItemName)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress);
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(FText::Format(NSLOCTEXT("MO", "InspectingItem", "Inspecting: {0}"), ItemName));
	}

	if (TimeRemainingText)
	{
		int32 SecondsRemaining = FMath::CeilToInt(TimeRemaining);
		TimeRemainingText->SetText(FText::Format(
			NSLOCTEXT("MO", "TimeRemaining", "{0}s remaining"),
			FText::AsNumber(SecondsRemaining)));
	}
}

void UMOInspectionProgressWidget::OnInspectionSuccess_Implementation(const FMOInspectionResult& Result)
{
	// Default implementation - can be overridden in BP to show success feedback
	if (Result.XPGrants.Num() > 0)
	{
		int32 SkillCount = 0;
		int32 KnowledgeCount = 0;
		for (const FMOInspectionXPGrant& Grant : Result.XPGrants)
		{
			if (Grant.bIsKnowledge)
			{
				KnowledgeCount++;
			}
			else
			{
				SkillCount++;
			}
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Player gained XP in %d skills and %d knowledge entries"), SkillCount, KnowledgeCount);
	}
}
