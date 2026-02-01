#include "MOInspectionProgressWidget.h"
#include "MOFramework.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"

void UMOInspectionProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &UMOInspectionProgressWidget::HandleCancelClicked);
	}
}

void UMOInspectionProgressWidget::NativeDestruct()
{
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

	if (!bIsInspecting)
	{
		return;
	}

	// Advance elapsed time
	ElapsedTime += InDeltaTime;
	CurrentProgress = FMath::Clamp(ElapsedTime / InspectionDuration, 0.0f, 1.0f);

	float TimeRemaining = FMath::Max(0.0f, InspectionDuration - ElapsedTime);

	// Update display
	UpdateDisplay(CurrentProgress, TimeRemaining, ItemDisplayName);

	// Check for completion
	if (CurrentProgress >= 1.0f)
	{
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
	InspectionDuration = FMath::Max(0.1f, InInspectionDuration);

	ElapsedTime = 0.0f;
	CurrentProgress = 0.0f;
	bIsInspecting = true;

	UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Started inspection of '%s' (duration: %.1fs)"),
		*ItemDefinitionId.ToString(), InspectionDuration);

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

		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Inspection result: Success=%s, NewKnowledge=%d, FirstInspection=%s"),
			Result.bSuccess ? TEXT("true") : TEXT("false"),
			Result.NewKnowledge.Num(),
			Result.bFirstInspection ? TEXT("true") : TEXT("false"));

		// Log new knowledge learned
		for (const FName& KnowledgeId : Result.NewKnowledge)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Learned new knowledge: %s"), *KnowledgeId.ToString());
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
	if (Result.NewKnowledge.Num() > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] Player learned %d new knowledge entries"), Result.NewKnowledge.Num());
	}
}
