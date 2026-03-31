/**
 * MOInspectionProgressWidget.cpp - Inspection Progress Widget Implementation
 */

#include "MOInspectionProgressWidget.h"
#include "MOFramework.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UMOInspectionProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure this widget doesn't steal focus from other UI elements
	SetIsFocusable(false);
}

void UMOInspectionProgressWidget::NativeDestruct()
{
	// Ensure we cancel if widget is destroyed while inspecting
	if (IsProgressActive())
	{
		CancelInspection();
	}

	Super::NativeDestruct();
}

void UMOInspectionProgressWidget::StartInspection(
	FName InItemDefinitionId,
	const FText& InItemDisplayName,
	UMOKnowledgeComponent* InKnowledgeComponent,
	UMOSkillsComponent* InSkillsComponent,
	float InInspectionDuration)
{
	// Store domain-specific state
	ItemDefinitionId = InItemDefinitionId;
	ItemDisplayName = InItemDisplayName;
	KnowledgeComponent = InKnowledgeComponent;
	SkillsComponent = InSkillsComponent;

	// Use debug duration if set, otherwise use the provided duration
	float ActualDuration;
	if (DebugInspectionDuration > 0.0f)
	{
		ActualDuration = DebugInspectionDuration;
		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Using DEBUG duration override: %.1fs"), ActualDuration);
	}
	else
	{
		ActualDuration = FMath::Max(0.1f, InInspectionDuration);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Started inspection of '%s' (duration: %.1fs)"),
		*ItemDefinitionId.ToString(), ActualDuration);

	// Use base class to start the timed progress
	StartProgress(InItemDisplayName, ActualDuration);
}

void UMOInspectionProgressWidget::CancelInspection()
{
	if (!IsProgressActive())
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Inspection cancelled for '%s'"), *ItemDefinitionId.ToString());

	// Broadcast empty result for cancellation
	FMOInspectionResult EmptyResult;
	OnInspectionCompleted.Broadcast(false, EmptyResult);
	OnInspectionCancelled.Broadcast();

	// Use base class to cancel progress (broadcasts OnCancelled)
	CancelProgress();
}

void UMOInspectionProgressWidget::OnProgressSuccess_Implementation()
{
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

	// Broadcast domain-specific completion
	OnInspectionCompleted.Broadcast(Result.bSuccess, Result);
}

void UMOInspectionProgressWidget::UpdateDisplay_Implementation(float Progress, float TimeRemaining, const FText& InActionName)
{
	// Update progress bar
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Progress);
	}

	// Update action name with "Inspecting:" prefix
	if (ActionNameText)
	{
		ActionNameText->SetText(FText::Format(
			NSLOCTEXT("MO", "InspectingItem", "Inspecting: {0}"),
			InActionName));
	}

	// Update time remaining
	if (TimeRemainingText)
	{
		const int32 SecondsRemaining = FMath::CeilToInt(TimeRemaining);
		TimeRemainingText->SetText(FText::Format(
			NSLOCTEXT("MO", "TimeRemaining", "{0}s remaining"),
			FText::AsNumber(SecondsRemaining)));
	}

	// Note: Completion is handled by base class in NativeTick -> OnProgressSuccess
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
