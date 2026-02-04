#include "MONotificationComponent.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"

#include "MONotificationWidget.h"
#include "MOSkillEntryWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MOSkillDatabaseSettings.h"
#include "MOSkillsComponent.h"

UMONotificationComponent::UMONotificationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMONotificationComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMONotificationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NotificationTimerHandle);
		World->GetTimerManager().ClearTimer(SkillPopupTimerHandle);
	}

	// Remove notification widget
	UMONotificationWidget* Widget = CurrentNotificationWidget.Get();
	if (IsValid(Widget))
	{
		Widget->RemoveFromParent();
	}
	CurrentNotificationWidget.Reset();

	// Remove skill popup widget
	UMOSkillEntryWidget* SkillWidget = CurrentSkillPopupWidget.Get();
	if (IsValid(SkillWidget))
	{
		SkillWidget->RemoveFromParent();
	}
	CurrentSkillPopupWidget.Reset();

	// Remove skill popup container
	UUserWidget* PopupContainer = SkillPopupContainerWidget.Get();
	if (IsValid(PopupContainer))
	{
		PopupContainer->RemoveFromParent();
	}
	SkillPopupContainerWidget.Reset();

	// Clear queues
	NotificationQueue.Empty();
	SkillPopupQueue.Empty();

	Super::EndPlay(EndPlayReason);
}

APlayerController* UMONotificationComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UMONotificationComponent::ShowNotification(const FText& Message, float Duration)
{
	// Add to queue
	FQueuedNotification Notification;
	Notification.Message = Message;
	Notification.Duration = Duration;
	NotificationQueue.Add(Notification);

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Queued: %s (%.1fs)"), *Message.ToString(), Duration);

	// If no notification showing, start processing
	UMONotificationWidget* CurrentWidget = CurrentNotificationWidget.Get();
	if (!IsValid(CurrentWidget) || !CurrentWidget->IsInViewport())
	{
		ProcessNextNotification();
	}
}

void UMONotificationComponent::ShowSkillIncreaseNotification(FName SkillId, float XPAmount)
{
	// Get skill display name
	FText SkillName = FText::FromName(SkillId);

	// Capitalize the skill ID for display
	FString SkillString = SkillId.ToString();
	if (SkillString.Len() > 0)
	{
		SkillString[0] = FChar::ToUpper(SkillString[0]);
		SkillName = FText::FromString(SkillString);
	}

	FText Message = FText::Format(
		NSLOCTEXT("MO", "SkillIncrease", "Your skill in {0} increased (+{1} XP)"),
		SkillName,
		FText::AsNumber(FMath::RoundToInt(XPAmount)));

	ShowNotification(Message, 3.0f);
}

void UMONotificationComponent::ShowRecipeUnlockedNotification(FName RecipeId)
{
	// Get recipe display name from database
	FText RecipeName = UMORecipeDatabaseSettings::GetRecipeDisplayName(RecipeId);
	if (RecipeName.IsEmpty())
	{
		// Log warning - this shouldn't happen with valid recipe IDs
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] ShowRecipeUnlockedNotification called with invalid RecipeId: %s"), *RecipeId.ToString());

		// Fallback: format the recipe ID nicely (insert spaces before capitals)
		FString RecipeString = RecipeId.ToString();
		if (RecipeString.Len() > 0)
		{
			FString FormattedString;
			for (int32 i = 0; i < RecipeString.Len(); i++)
			{
				TCHAR Char = RecipeString[i];

				// Insert space before capitals (except first char)
				if (i > 0 && FChar::IsUpper(Char))
				{
					FormattedString.AppendChar(TEXT(' '));
				}

				// Capitalize first character
				if (i == 0)
				{
					Char = FChar::ToUpper(Char);
				}

				FormattedString.AppendChar(Char);
			}
			RecipeString = FormattedString;
		}
		RecipeName = FText::FromString(RecipeString);
	}

	FText Message = FText::Format(
		NSLOCTEXT("MO", "RecipeUnlocked", "You can now craft: {0}"),
		RecipeName);

	ShowNotification(Message, 4.0f);
}

void UMONotificationComponent::ShowKnowledgeLearnedNotification(FName KnowledgeId)
{
	// Format the knowledge ID nicely for display
	// Convert camelCase/PascalCase to "Title Case With Spaces"
	FString KnowledgeString = KnowledgeId.ToString();

	if (KnowledgeString.Len() > 0)
	{
		// Insert spaces before capital letters and capitalize first letter
		FString FormattedString;
		for (int32 i = 0; i < KnowledgeString.Len(); i++)
		{
			TCHAR Char = KnowledgeString[i];

			// Insert space before capitals (except first char)
			if (i > 0 && FChar::IsUpper(Char))
			{
				FormattedString.AppendChar(TEXT(' '));
			}

			// Capitalize first character
			if (i == 0)
			{
				Char = FChar::ToUpper(Char);
			}

			FormattedString.AppendChar(Char);
		}
		KnowledgeString = FormattedString;
	}

	FText Message = FText::Format(
		NSLOCTEXT("MO", "KnowledgeLearned", "You gained some knowledge of {0}"),
		FText::FromString(KnowledgeString));

	ShowNotification(Message, 3.5f);
}

void UMONotificationComponent::ClearAllNotifications()
{
	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NotificationTimerHandle);
		World->GetTimerManager().ClearTimer(SkillPopupTimerHandle);
	}

	// Remove current notification widget
	UMONotificationWidget* Widget = CurrentNotificationWidget.Get();
	if (IsValid(Widget) && Widget->IsInViewport())
	{
		Widget->RemoveFromParent();
	}

	// Remove current skill popup widget
	UMOSkillEntryWidget* SkillWidget = CurrentSkillPopupWidget.Get();
	if (IsValid(SkillWidget) && SkillWidget->IsInViewport())
	{
		SkillWidget->RemoveFromParent();
	}
	CurrentSkillPopupWidget.Reset();

	// Clear queues
	NotificationQueue.Empty();
	SkillPopupQueue.Empty();

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Cleared all notifications and popups"));
}

int32 UMONotificationComponent::GetQueuedNotificationCount() const
{
	int32 Count = NotificationQueue.Num();

	// Add 1 if currently showing
	UMONotificationWidget* Widget = CurrentNotificationWidget.Get();
	if (IsValid(Widget) && Widget->IsInViewport())
	{
		Count++;
	}

	return Count;
}

void UMONotificationComponent::ProcessNextNotification()
{
	if (NotificationQueue.Num() == 0)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	// Get next notification
	FQueuedNotification Next = NotificationQueue[0];
	NotificationQueue.RemoveAt(0);

	// Create or reuse notification widget
	UMONotificationWidget* NotificationWidget = CurrentNotificationWidget.Get();
	if (!IsValid(NotificationWidget))
	{
		TSubclassOf<UMONotificationWidget> WidgetClass = NotificationWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UMONotificationWidget::StaticClass();
		}

		NotificationWidget = CreateWidget<UMONotificationWidget>(PlayerController, WidgetClass);
		CurrentNotificationWidget = NotificationWidget;
	}

	if (!IsValid(NotificationWidget))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] Failed to create notification widget"));
		return;
	}

	// Ensure notification doesn't steal focus or block mouse clicks
	NotificationWidget->SetIsFocusable(false);
	NotificationWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Set message and show
	NotificationWidget->SetMessage(Next.Message);

	if (!NotificationWidget->IsInViewport())
	{
		NotificationWidget->AddToViewport(NotificationZOrder);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Showing: %s"), *Next.Message.ToString());

	// Set timer to hide and show next
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NotificationTimerHandle);
		World->GetTimerManager().SetTimer(
			NotificationTimerHandle,
			this,
			&UMONotificationComponent::HideCurrentNotification,
			Next.Duration,
			false
		);
	}
}

void UMONotificationComponent::HideCurrentNotification()
{
	// Hide current notification
	UMONotificationWidget* NotificationWidget = CurrentNotificationWidget.Get();
	if (IsValid(NotificationWidget) && NotificationWidget->IsInViewport())
	{
		NotificationWidget->RemoveFromParent();
	}

	// Process next in queue
	ProcessNextNotification();
}

void UMONotificationComponent::ShowSkillPopup(FName SkillId, float Duration)
{
	// Add to queue
	FQueuedSkillPopup Popup;
	Popup.Id = SkillId;
	Popup.bIsKnowledge = false;
	Popup.Duration = Duration;
	SkillPopupQueue.Add(Popup);

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Queued skill popup: %s (%.1fs)"), *SkillId.ToString(), Duration);

	// If no popup currently showing, process immediately
	UMOSkillEntryWidget* CurrentWidget = CurrentSkillPopupWidget.Get();
	if (!IsValid(CurrentWidget) || !CurrentWidget->IsInViewport())
	{
		ProcessNextSkillPopup();
	}
}

void UMONotificationComponent::ProcessNextSkillPopup()
{
	if (SkillPopupQueue.Num() == 0)
	{
		return;
	}

	FQueuedSkillPopup Next = SkillPopupQueue[0];
	SkillPopupQueue.RemoveAt(0);

	ShowSkillPopupInternal(Next.Id, Next.bIsKnowledge, Next.Duration);
}

void UMONotificationComponent::ShowSkillPopupInternal(FName SkillId, bool bIsKnowledge, float Duration)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!SkillNotificationWidgetClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] ShowSkillPopup called but SkillNotificationWidgetClass is not set"));
		// Try to process next in queue
		ProcessNextSkillPopup();
		return;
	}

	// Remove any existing popup widget
	UMOSkillEntryWidget* OldWidget = CurrentSkillPopupWidget.Get();
	if (IsValid(OldWidget) && OldWidget->IsInViewport())
	{
		OldWidget->RemoveFromParent();
	}
	CurrentSkillPopupWidget.Reset();

	// Create skill entry widget
	UMOSkillEntryWidget* SkillWidget = CreateWidget<UMOSkillEntryWidget>(PlayerController, SkillNotificationWidgetClass);
	if (!IsValid(SkillWidget))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] Failed to create skill popup widget"));
		// Try to process next in queue
		ProcessNextSkillPopup();
		return;
	}

	// Ensure popup doesn't steal focus or block mouse clicks
	SkillWidget->SetIsFocusable(false);
	SkillWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Build and set skill data (use appropriate builder for skill vs knowledge)
	FMOSkillDisplayData Data;
	if (bIsKnowledge)
	{
		Data = BuildKnowledgeDisplayData(SkillId);
	}
	else
	{
		Data = BuildSkillDisplayData(SkillId);
	}
	SkillWidget->SetupEntry(Data);

	// Add to viewport
	SkillWidget->AddToViewport(SkillPopupZOrder);

	// Position it - for now we let the widget handle its own positioning via anchors
	// The Blueprint widget should be configured to anchor bottom-center with appropriate offset
	CurrentSkillPopupWidget = SkillWidget;

	// Play flash animation
	SkillWidget->PlayFlashAnimation(Duration);

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Showing %s popup for: %s"),
		bIsKnowledge ? TEXT("knowledge") : TEXT("skill"),
		*SkillId.ToString());

	// Set timer to hide and show next
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SkillPopupTimerHandle);
		World->GetTimerManager().SetTimer(
			SkillPopupTimerHandle,
			this,
			&UMONotificationComponent::HideSkillPopup,
			Duration,
			false
		);
	}
}

void UMONotificationComponent::ShowKnowledgePopup(FName KnowledgeId, float Duration)
{
	// Add to queue (using same queue as skill popups, but marked as knowledge)
	FQueuedSkillPopup Popup;
	Popup.Id = KnowledgeId;
	Popup.bIsKnowledge = true;
	Popup.Duration = Duration;
	SkillPopupQueue.Add(Popup);

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Queued knowledge popup: %s (%.1fs)"), *KnowledgeId.ToString(), Duration);

	// If no popup currently showing, process immediately
	UMOSkillEntryWidget* CurrentWidget = CurrentSkillPopupWidget.Get();
	if (!IsValid(CurrentWidget) || !CurrentWidget->IsInViewport())
	{
		ProcessNextSkillPopup();
	}
}

void UMONotificationComponent::HideSkillPopup()
{
	UMOSkillEntryWidget* SkillWidget = CurrentSkillPopupWidget.Get();
	if (IsValid(SkillWidget) && SkillWidget->IsInViewport())
	{
		SkillWidget->RemoveFromParent();
	}
	CurrentSkillPopupWidget.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SkillPopupTimerHandle);
	}

	// Process next popup in queue
	ProcessNextSkillPopup();
}

FMOSkillDisplayData UMONotificationComponent::BuildSkillDisplayData(FName SkillId) const
{
	FMOSkillDisplayData Data;
	Data.SkillId = SkillId;

	// Get skill definition
	const FMOSkillDefinitionRow* SkillDef = UMOSkillDatabaseSettings::GetSkillDefinition(SkillId);
	if (SkillDef)
	{
		Data.DisplayName = SkillDef->DisplayName;
		Data.Description = SkillDef->Description;
		Data.HowToIncrease = SkillDef->HowToIncrease;
		Data.Category = SkillDef->Category;
		Data.MaxLevel = SkillDef->MaxLevel;
		Data.Icon = SkillDef->Icon;
	}
	else
	{
		Data.DisplayName = FText::FromName(SkillId);
	}

	// Try to get player progress from their skills component
	AActor* OwnerActor = GetOwner();
	if (APlayerController* PC = Cast<APlayerController>(OwnerActor))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UMOSkillsComponent* Skills = Pawn->FindComponentByClass<UMOSkillsComponent>())
			{
				FMOSkillProgress Progress;
				if (Skills->GetSkillProgress(SkillId, Progress))
				{
					Data.Level = Progress.Level;
					Data.CurrentXP = Progress.CurrentXP;
					Data.XPToNextLevel = Progress.XPToNextLevel;
					Data.LevelProgress = Progress.GetLevelProgress();
				}
			}
		}
	}

	return Data;
}

FMOSkillDisplayData UMONotificationComponent::BuildKnowledgeDisplayData(FName KnowledgeId) const
{
	FMOSkillDisplayData Data;
	Data.SkillId = KnowledgeId; // Reuse SkillId field

	// Format knowledge ID into readable name
	FString KnowledgeString = KnowledgeId.ToString();
	if (KnowledgeString.Len() > 0)
	{
		FString FormattedString;
		for (int32 i = 0; i < KnowledgeString.Len(); i++)
		{
			TCHAR Char = KnowledgeString[i];
			if (i > 0 && FChar::IsUpper(Char))
			{
				FormattedString.AppendChar(TEXT(' '));
			}
			if (i == 0)
			{
				Char = FChar::ToUpper(Char);
			}
			FormattedString.AppendChar(Char);
		}
		Data.DisplayName = FText::FromString(FormattedString);
	}
	else
	{
		Data.DisplayName = FText::FromName(KnowledgeId);
	}

	Data.Description = NSLOCTEXT("MO", "KnowledgeDesc", "Knowledge gained through study and inspection.");
	Data.HowToIncrease = NSLOCTEXT("MO", "KnowledgeHowTo", "Inspect items related to this topic.");
	Data.Category = EMOSkillCategory::Knowledge;
	Data.MaxLevel = 100; // Default, could be looked up from a knowledge database

	// Knowledge now uses the same XP/level system as skills
	// Try to get player progress from their skills component
	AActor* OwnerActor = GetOwner();
	if (APlayerController* PC = Cast<APlayerController>(OwnerActor))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UMOSkillsComponent* Skills = Pawn->FindComponentByClass<UMOSkillsComponent>())
			{
				FMOSkillProgress Progress;
				if (Skills->GetSkillProgress(KnowledgeId, Progress))
				{
					Data.Level = Progress.Level;
					Data.CurrentXP = Progress.CurrentXP;
					Data.XPToNextLevel = Progress.XPToNextLevel;
					Data.LevelProgress = Progress.GetLevelProgress();
				}
				else
				{
					// Knowledge not yet tracked, show level 0
					Data.Level = 0;
					Data.CurrentXP = 0.0f;
					Data.XPToNextLevel = 100.0f;
					Data.LevelProgress = 0.0f;
				}
			}
		}
	}

	return Data;
}
