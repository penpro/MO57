/**
 * MOBuildingQueueWidget.cpp - building adapter over the shared queue renderer
 * (migration Stage 3). See the header for the compatibility contract and the
 * liveness/cancel-semantics notes.
 */

#include "MOBuildingQueueWidget.h"
#include "MOFramework.h"
#include "MOBuildProgressComponent.h"
#include "MOBuildingQueueEntryWidget.h"
#include "MOQueueRowWidgetBase.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "MOIdentityComponent.h"
#include "MOUIUtils.h"
#include "GameFramework/Actor.h"

UMOBuildingQueueWidget::UMOBuildingQueueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOBuildingQueueWidget::InitializeQueue(UMOBuildProgressComponent* InProgressComponent)
{
	// Unbind from previous component (source-swap safe, legacy discipline).
	if (UMOBuildProgressComponent* OldComponent = ProgressComponent.Get())
	{
		OldComponent->OnConstructionStateChanged.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionStateChanged);
		OldComponent->OnConstructionProgress.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionProgress);
		OldComponent->OnConstructionCompleted.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionCompleted);
	}

	ProgressComponent = InProgressComponent;

	// Stable row id for this construction: the buildable's identity GUID when
	// present, else one GUID minted per BOUND COMPONENT (never per refresh —
	// the legacy per-refresh NewGuid could not round-trip a cancel intent).
	StableRowId.Invalidate();
	if (InProgressComponent)
	{
		if (const AActor* Owner = InProgressComponent->GetOwner())
		{
			if (const UMOIdentityComponent* Identity = Owner->FindComponentByClass<UMOIdentityComponent>())
			{
				StableRowId = Identity->GetGuid();
			}
		}
		if (!StableRowId.IsValid())
		{
			StableRowId = FGuid::NewGuid();
		}

		InProgressComponent->OnConstructionStateChanged.AddDynamic(this, &UMOBuildingQueueWidget::HandleConstructionStateChanged);
		InProgressComponent->OnConstructionProgress.AddDynamic(this, &UMOBuildingQueueWidget::HandleConstructionProgress);
		InProgressComponent->OnConstructionCompleted.AddDynamic(this, &UMOBuildingQueueWidget::HandleConstructionCompleted);
	}

	SyncRowWidgetClass();
	RefreshRows();
}

void UMOBuildingQueueWidget::RefreshQueue()
{
	SyncRowWidgetClass();
	RefreshRows();
}

void UMOBuildingQueueWidget::UpdateProgress()
{
	UpdateProgressDisplay();
}

bool UMOBuildingQueueWidget::IsQueueEmpty() const
{
	return !HasActiveConstruction();
}

int32 UMOBuildingQueueWidget::GetQueueLength() const
{
	return HasActiveConstruction() ? 1 : 0;
}

float UMOBuildingQueueWidget::GetCurrentProgress() const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	return Component ? Component->GetProgress() : 0.0f;
}

FText UMOBuildingQueueWidget::GetTimeRemainingText() const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component)
	{
		return FText::GetEmpty();
	}
	return UMOUIUtils::FormatDurationAsText(Component->GetTimeRemaining());
}

EMOQueueRowState UMOBuildingQueueWidget::RowStateForBuildState(EMOBuildState BuildState)
{
	switch (BuildState)
	{
	case EMOBuildState::Constructing:
		return EMOQueueRowState::Active;
	case EMOBuildState::Paused:
		return EMOQueueRowState::Paused;
	default:
		// Ghost/Complete never produce a row; callers gate on HasActiveConstruction.
		return EMOQueueRowState::Queued;
	}
}

bool UMOBuildingQueueWidget::HasQueueSource_Implementation() const
{
	return ProgressComponent.IsValid();
}

void UMOBuildingQueueWidget::BuildDisplayRows_Implementation(TArray<FMOQueueDisplayRow>& OutRows) const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component || !HasActiveConstruction())
	{
		return;
	}

	const EMOBuildState State = Component->GetState();
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Component->GetRecipeId());

	FMOQueueDisplayRow Row;
	Row.RowId = StableRowId;
	Row.SourceId = Component->GetRecipeId();
	Row.Progress = Component->GetProgress();
	Row.RemainingSeconds = FMath::Max(0.0f, Component->GetTimeRemaining());
	Row.State = RowStateForBuildState(State);
	Row.bCancellable = true;

	if (Recipe)
	{
		Row.Title = Recipe->DisplayName;
		Row.Icon = Recipe->Icon;
	}
	else
	{
		Row.Title = FText::FromName(Row.SourceId);
	}

	// Part display: "current part / total parts" (legacy format).
	Row.CountCurrent = Component->GetCurrentPartIndex() + 1;
	Row.CountTotal = Recipe ? Recipe->BuildParts.Num() : 1;

	OutRows.Add(Row);
}

void UMOBuildingQueueWidget::GetHeaderDisplay_Implementation(FMOQueueHeaderDisplay& OutHeader) const
{
	// Building header mirrors its single row, but reads FRESH component state so
	// the tick path notices completion/pause between rebuilds.
	OutHeader = FMOQueueHeaderDisplay();
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component || !HasActiveConstruction())
	{
		return;
	}

	OutHeader.bHasRows = true;
	OutHeader.Progress = Component->GetProgress();
	OutHeader.RemainingSeconds = FMath::Max(0.0f, Component->GetTimeRemaining());
	const TArray<FMOQueueDisplayRow>& Rows = GetLastBuiltRows();
	if (Rows.Num() > 0)
	{
		OutHeader.ActiveTitle = Rows[0].Title;
	}
}

bool UMOBuildingQueueWidget::GetActiveRowLiveProgress_Implementation(float& OutProgress, float& OutRemainingSeconds) const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component || !HasActiveConstruction())
	{
		return false;
	}
	OutProgress = Component->GetProgress();
	OutRemainingSeconds = FMath::Max(0.0f, Component->GetTimeRemaining());
	return true;
}

void UMOBuildingQueueWidget::ExecuteCancelRow_Implementation(const FGuid& RowId)
{
	// Verify the intent targets THIS construction (the stable id fix makes this
	// check meaningful; the legacy path ignored the id entirely).
	if (RowId != StableRowId)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOBuildingQueue] Cancel intent for unknown row %s (current %s) - ignored"),
			*RowId.ToString(EGuidFormats::Short), *StableRowId.ToString(EGuidFormats::Short));
		return;
	}
	ExecuteCancelAll();
}

void UMOBuildingQueueWidget::ExecuteCancelAll_Implementation()
{
	// Single-operation domain: cancel-one == cancel-all. Full material refund as
	// world drops, ghost survives (legacy semantics — see the header's fork note).
	if (UMOBuildProgressComponent* Component = ProgressComponent.Get())
	{
		Component->CancelConstruction(/*bRefundMaterials=*/true);
	}
}

void UMOBuildingQueueWidget::NativeDestruct()
{
	if (UMOBuildProgressComponent* Component = ProgressComponent.Get())
	{
		Component->OnConstructionStateChanged.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionStateChanged);
		Component->OnConstructionProgress.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionProgress);
		Component->OnConstructionCompleted.RemoveDynamic(this, &UMOBuildingQueueWidget::HandleConstructionCompleted);
	}

	Super::NativeDestruct();
}

void UMOBuildingQueueWidget::NotifyRowsRefreshed(int32 RowCount)
{
	OnQueueUpdated(RowCount);
}

void UMOBuildingQueueWidget::NotifyProgressUpdated(float Progress, const FText& TimeRemaining)
{
	OnProgressUpdated(Progress, TimeRemaining);
}

void UMOBuildingQueueWidget::OnRowWidgetBound(UMOQueueRowWidgetBase* RowWidget, const FMOQueueDisplayRow& InRow)
{
	// Keep the legacy BP-visible display struct in sync on the compat entry,
	// including the real EMOBuildState the neutral row abstracts away.
	if (UMOBuildingQueueEntryWidget* LegacyEntry = Cast<UMOBuildingQueueEntryWidget>(RowWidget))
	{
		FMOBuildQueueEntryDisplayData LegacyData;
		LegacyData.EntryId = InRow.RowId;
		LegacyData.RecipeId = InRow.SourceId;
		LegacyData.RecipeName = InRow.Title;
		LegacyData.Icon = InRow.Icon;
		LegacyData.CountText = InRow.CountText;
		LegacyData.Progress = InRow.Progress;
		LegacyData.TimeRemainingText = InRow.TimeRemainingText;
		LegacyData.bIsActive = (InRow.State == EMOQueueRowState::Active);
		if (UMOBuildProgressComponent* Component = ProgressComponent.Get())
		{
			LegacyData.State = Component->GetState();
		}
		LegacyEntry->SetLegacyEntryData(LegacyData);
	}
}

void UMOBuildingQueueWidget::HandleConstructionStateChanged(EMOBuildState NewState)
{
	RefreshRows();
}

void UMOBuildingQueueWidget::HandleConstructionProgress(float Progress)
{
	// Progress is handled by tick-based updates for smoother display.
}

void UMOBuildingQueueWidget::HandleConstructionCompleted()
{
	RefreshRows();
}

bool UMOBuildingQueueWidget::HasActiveConstruction() const
{
	UMOBuildProgressComponent* Component = ProgressComponent.Get();
	if (!Component)
	{
		return false;
	}
	const EMOBuildState State = Component->GetState();
	return State == EMOBuildState::Constructing || State == EMOBuildState::Paused;
}

void UMOBuildingQueueWidget::SyncRowWidgetClass()
{
	if (QueueEntryWidgetClass)
	{
		RowWidgetClass = QueueEntryWidgetClass;
	}
}
