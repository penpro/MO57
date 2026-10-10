#include "MOTutorialHintWidget.h"
#include "MOInputHintFormatter.h"
#include "MOQuestSubsystem.h"
#include "MOFramework.h"

#include "CommonTextBlock.h"
#include "CommonInputModeTypes.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

UMOTutorialHintWidget::UMOTutorialHintWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Passive overlay — never closes on outside click and doesn't grab input.
	bClosesOnOutsideClick = false;
}

TOptional<FUIInputConfig> UMOTutorialHintWidget::GetDesiredInputConfig() const
{
	// Stay in game input mode — the popup is purely informational and must
	// not steal mouse/keyboard from gameplay. Returning the GameOnly config
	// explicitly tells CommonUI not to swap to UI mode for this widget.
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently);
}

void UMOTutorialHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (UGameInstance* GI = PC->GetGameInstance())
		{
			if (UMOQuestSubsystem* Quest = GI->GetSubsystem<UMOQuestSubsystem>())
			{
				BoundSubsystem = Quest;
				Quest->OnTutorialHintChanged.RemoveDynamic(this, &UMOTutorialHintWidget::HandleTutorialHintChanged);
				Quest->OnTutorialHintChanged.AddDynamic(this, &UMOTutorialHintWidget::HandleTutorialHintChanged);
			}
		}
	}

	// Start hidden until a hint is available — caller (Blueprint) is expected
	// to construct deactivated, but be defensive and refresh once.
	HandleTutorialHintChanged();
}

void UMOTutorialHintWidget::NativeDestruct()
{
	if (UMOQuestSubsystem* Quest = BoundSubsystem.Get())
	{
		Quest->OnTutorialHintChanged.RemoveDynamic(this, &UMOTutorialHintWidget::HandleTutorialHintChanged);
	}
	BoundSubsystem.Reset();

	Super::NativeDestruct();
}

ESlateVisibility UMOTutorialHintWidget::ComputeVisibility(bool bInHaveHint, bool bInSuppressedByMenu)
{
	return (bInHaveHint && !bInSuppressedByMenu) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

void UMOTutorialHintWidget::SetSuppressedByMenu(bool bSuppress)
{
	if (bSuppressedByMenu == bSuppress)
	{
		return;
	}
	bSuppressedByMenu = bSuppress;
	SetVisibility(ComputeVisibility(bHaveHint, bSuppressedByMenu));
	UE_LOG(LogMOFramework, Verbose, TEXT("[MOTutorialHintWidget] %s (hint active: %d)"), bSuppress ? TEXT("hidden: a menu is open") : TEXT("shown again: no menu open"), bHaveHint ? 1 : 0);
}

void UMOTutorialHintWidget::HandleTutorialHintChanged()
{
	UMOQuestSubsystem* Quest = BoundSubsystem.Get();
	if (!Quest)
	{
		return;
	}

	FName QuestId, ObjectiveId;
	FText HintTitle, HintBody;
	const bool bGotHint = Quest->GetActiveTutorialHint(QuestId, ObjectiveId, HintTitle, HintBody);
	bHaveHint = bGotHint;

	if (!bGotHint)
	{
		// Nothing to show — deactivate AND collapse. We use both because:
		//   - DeactivateWidget triggers any CommonUI Deactivated animations the
		//     WBP has bound (fade out).
		//   - SetVisibility(Collapsed) guarantees the widget disappears even
		//     when the BP has no animations bound yet, so this works on day 1.
		// When the BP later wires fade animations, the visibility change is
		// what the animation can interpolate from.
		CurrentObjectiveId = NAME_None;
		if (IsActivated())
		{
			DeactivateWidget();
		}
		SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOTutorialHintWidget] No active tutorial hint — collapsing"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	const FText ResolvedTitle = FMOInputHintFormatter::Format(HintTitle, PC);
	const FText ResolvedBody = FMOInputHintFormatter::Format(HintBody, PC);

	if (TitleText)
	{
		TitleText->SetText(ResolvedTitle);
	}
	if (BodyText)
	{
		BodyText->SetText(ResolvedBody);
	}

	CurrentObjectiveId = ObjectiveId;

	// Make sure we're visible (counters any prior Collapsed) -- unless a menu is open, in which case the banner stays hidden until it closes -- and
	// activated (so CommonUI animations fire and input config applies).
	SetVisibility(ComputeVisibility(bHaveHint, bSuppressedByMenu));
	if (!IsActivated())
	{
		ActivateWidget();
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOTutorialHintWidget] Showing hint %s.%s: %s"),
		*QuestId.ToString(), *ObjectiveId.ToString(), *ResolvedTitle.ToString());
}
