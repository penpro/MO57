#include "MONotificationComponent.h"
#include "MOUIManagerComponent.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"

#include "MONotificationWidget.h"
#include "MOSkillToastWidget.h"
#include "MORecipeDatabaseSettings.h"
#include "MOSkillDatabaseSettings.h"
#include "MOSkillsComponent.h"
#include "MOPlayerController.h"

UMONotificationComponent::UMONotificationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SkillToastWidgetClass = TSoftClassPtr<UMOSkillToastWidget>(FSoftObjectPath(TEXT("/MOFramework/UI/WBP_SkillToast.WBP_SkillToast_C")));
}

void UMONotificationComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMONotificationComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
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
	UMOSkillToastWidget* SkillWidget = CurrentSkillPopupWidget.Get();
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

AMOPlayerController* UMONotificationComponent::GetRemoteOwningPlayerController() const
{
	APlayerController* PC = GetOwningPlayerController();
	return (PC && !PC->IsLocalController() && PC->GetNetMode() != NM_Client) ? Cast<AMOPlayerController>(PC) : nullptr;
}

APlayerController* UMONotificationComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UMONotificationComponent::ShowNotification(const FText& Message, float Duration, EMONotificationType Type)
{
	// The server's copy of a co-op player's controller has no screen: the message goes to that player, into their own notification component. Until
	// Oct 18 2026 it was queued here and never shown (and the queue only grew) -- what the server does for a joiner (a discovery, a harvest) said nothing.
	if (AMOPlayerController* RemotePC = GetRemoteOwningPlayerController())
	{
		RemotePC->ClientShowNotification(Message, Duration, Type);
		return;
	}
	ShowNotificationHere(Message, Duration, Type);
}

void UMONotificationComponent::ShowNotificationHere(const FText& Message, float Duration, EMONotificationType Type)
{
	// Add to queue
	FQueuedNotification Notification;
	Notification.Message = Message;
	Notification.Duration = Duration;
	Notification.Type = Type;
	NotificationQueue.Add(Notification);

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Queued: %s (%.1fs, type=%d)"), *Message.ToString(), Duration, static_cast<int32>(Type));

	// If no notification showing, start processing
	UMONotificationWidget* CurrentWidget = CurrentNotificationWidget.Get();
	if (!IsValid(CurrentWidget) || !CurrentWidget->IsInViewport())
	{
		ProcessNextNotification();
	}
}

void UMONotificationComponent::ShowInfoNotification(const FText& Message, float Duration)
{
	ShowNotification(Message, Duration, EMONotificationType::Info);
}

void UMONotificationComponent::ShowSuccessNotification(const FText& Message, float Duration)
{
	ShowNotification(Message, Duration, EMONotificationType::Success);
}

void UMONotificationComponent::ShowWarningNotification(const FText& Message, float Duration)
{
	ShowNotification(Message, Duration, EMONotificationType::Warning);
}

void UMONotificationComponent::ShowErrorNotification(const FText& Message, float Duration)
{
	ShowNotification(Message, Duration, EMONotificationType::Error);
}

void UMONotificationComponent::ShowItemPickupNotification(const FText& ItemName, int32 Quantity)
{
	FText Message;
	if (Quantity > 1)
	{
		Message = FText::Format(
			NSLOCTEXT("MO", "PickedUpMultiple", "Picked up {0} x{1}"),
			ItemName,
			FText::AsNumber(Quantity));
	}
	else
	{
		Message = FText::Format(
			NSLOCTEXT("MO", "PickedUpSingle", "Picked up {0}"),
			ItemName);
	}

	ShowNotification(Message, 2.0f, EMONotificationType::ItemPickup);
}

void UMONotificationComponent::ShowInventoryFullNotification()
{
	ShowNotification(
		NSLOCTEXT("MO", "InventoryFull", "Inventory is full!"),
		2.5f,
		EMONotificationType::Warning);
}

void UMONotificationComponent::ShowSkillIncreaseNotification(FName SkillId, float XPAmount)
{
	const FText SkillName = UMOSkillDatabaseSettings::GetSkillOrKnowledgeDisplayName(SkillId);

	FText Message = FText::Format(
		NSLOCTEXT("MO", "SkillIncrease", "Your skill in {0} increased (+{1} XP)"),
		SkillName,
		FText::AsNumber(FMath::RoundToInt(XPAmount)));

	ShowNotification(Message, 3.0f, EMONotificationType::SkillGain);
}

void UMONotificationComponent::ShowRecipeUnlockedNotification(FName RecipeId)
{
	// Get recipe display name from database
	FText RecipeName = UMORecipeDatabaseSettings::GetRecipeDisplayName(RecipeId);
	if (RecipeName.IsEmpty())
	{
		// Log warning - this shouldn't happen with valid recipe IDs
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] ShowRecipeUnlockedNotification called with invalid RecipeId: %s"), *RecipeId.ToString());

		RecipeName = UMOSkillDatabaseSettings::HumanizeId(RecipeId);
	}

	// A harvest recipe (Harvest Bark: "strip bark from a tree") never appears in the crafting menu -- it is an action on the source in the world. Saying "you can
	// now craft" sent the player looking for it in the crafting list (playtest 2026-10-14 #23: "it says I unlocked strip bark but it's not showing as a recipe").
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	FText Message = (Recipe && Recipe->bIsHarvestRecipe)
		? FText::Format(NSLOCTEXT("MO", "HarvestUnlocked", "New harvest action: {0} (interact with the source in the world to use it)"), RecipeName)
		: FText::Format(NSLOCTEXT("MO", "RecipeUnlocked", "You can now craft: {0}"), RecipeName);

	ShowNotification(Message, 4.0f, EMONotificationType::Success);
}

void UMONotificationComponent::ShowKnowledgeLearnedNotification(FName KnowledgeId)
{
	FText Message = FText::Format(
		NSLOCTEXT("MO", "KnowledgeLearned", "You gained some knowledge of {0}"),
		UMOSkillDatabaseSettings::GetSkillOrKnowledgeDisplayName(KnowledgeId));

	ShowNotification(Message, 3.5f, EMONotificationType::SkillGain);
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
	UMOSkillToastWidget* SkillWidget = CurrentSkillPopupWidget.Get();
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

	// Set up notification with message and type
	NotificationWidget->SetupNotification(Next.Message, Next.Type);

	if (!NotificationWidget->IsInViewport())
	{
		NotificationWidget->AddToViewport(NotificationZOrder);
		UMOUIManagerComponent::RegisterHUDElement(NotificationWidget, EMOHUDElementKind::Feedback);
	}

	// Play show animation
	NotificationWidget->PlayShowAnimation();

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Showing: %s (type=%d)"), *Next.Message.ToString(), static_cast<int32>(Next.Type));

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
	// Hide current notification with animation
	UMONotificationWidget* NotificationWidget = CurrentNotificationWidget.Get();
	if (IsValid(NotificationWidget) && NotificationWidget->IsInViewport())
	{
		// Bind to hide animation completion
		NotificationWidget->OnNotificationHidden.RemoveDynamic(this, &UMONotificationComponent::OnNotificationHideComplete);
		NotificationWidget->OnNotificationHidden.AddDynamic(this, &UMONotificationComponent::OnNotificationHideComplete);

		// Start hide animation (default implementation immediately calls OnHideAnimationComplete)
		NotificationWidget->PlayHideAnimation();
	}
	else
	{
		// No widget to hide, just process next
		ProcessNextNotification();
	}
}

void UMONotificationComponent::OnNotificationHideComplete()
{
	// Remove widget from viewport after animation
	UMONotificationWidget* NotificationWidget = CurrentNotificationWidget.Get();
	if (IsValid(NotificationWidget))
	{
		NotificationWidget->OnNotificationHidden.RemoveDynamic(this, &UMONotificationComponent::OnNotificationHideComplete);
		NotificationWidget->RemoveFromParent();
	}

	// Process next notification in queue
	ProcessNextNotification();
}

void UMONotificationComponent::ShowSkillPopup(FName SkillId, float Duration)
{
	ShowSkillToast(SkillId, /*bIsKnowledge=*/false, 0.0f, INDEX_NONE, INDEX_NONE, Duration);
}

void UMONotificationComponent::ShowSkillToast(FName Id, bool bIsKnowledge, float XPGained, int32 LevelBefore, int32 LevelAfter, float Duration)
{
	if (AMOPlayerController* RemotePC = GetRemoteOwningPlayerController())
	{
		RemotePC->ClientShowSkillPopup(Id, bIsKnowledge, Duration, XPGained, LevelBefore, LevelAfter);
		return;
	}
	ShowSkillPopupHere(Id, bIsKnowledge, Duration, XPGained, LevelBefore, LevelAfter);
}

void UMONotificationComponent::ShowSkillPopupHere(FName SkillId, bool bIsKnowledge, float Duration, float XPGained, int32 LevelBefore, int32 LevelAfter)
{
	// Add to queue
	FQueuedSkillPopup Popup;
	Popup.Id = SkillId;
	Popup.bIsKnowledge = bIsKnowledge;
	Popup.Duration = Duration;
	Popup.XPGained = XPGained;
	Popup.LevelBefore = LevelBefore;
	Popup.LevelAfter = LevelAfter;
	SkillPopupQueue.Add(Popup);

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Queued %s popup: %s (%.1fs)"), bIsKnowledge ? TEXT("knowledge") : TEXT("skill"), *SkillId.ToString(), Duration);

	// If no popup currently showing, process immediately
	UMOSkillToastWidget* CurrentWidget = CurrentSkillPopupWidget.Get();
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

	ShowSkillPopupInternal(Next);
}

void UMONotificationComponent::ShowSkillPopupInternal(const FQueuedSkillPopup& Popup)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	UClass* ToastClass = SkillToastWidgetClass.LoadSynchronous();
	if (!ToastClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] No skill toast class (%s): the %s toast for %s is not shown"),
			*SkillToastWidgetClass.ToString(), Popup.bIsKnowledge ? TEXT("knowledge") : TEXT("skill"), *Popup.Id.ToString());
		ProcessNextSkillPopup();
		return;
	}

	UMOSkillToastWidget* OldWidget = CurrentSkillPopupWidget.Get();
	if (IsValid(OldWidget) && OldWidget->IsInViewport())
	{
		OldWidget->RemoveFromParent();
	}
	CurrentSkillPopupWidget.Reset();

	UMOSkillToastWidget* Toast = CreateWidget<UMOSkillToastWidget>(PlayerController, ToastClass);
	if (!IsValid(Toast))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MONotification] Failed to create the skill toast"));
		ProcessNextSkillPopup();
		return;
	}

	// Never steals focus or a click; the tree places its plate (under the text notification), so it goes on as a full-screen layer.
	Toast->SetIsFocusable(false);
	Toast->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FMOSkillToastData Data = BuildToastData(Popup);
	Toast->SetupToast(Data);
	Toast->AddToViewport(SkillPopupZOrder);
	UMOUIManagerComponent::RegisterHUDElement(Toast, EMOHUDElementKind::Info);
	CurrentSkillPopupWidget = Toast;

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Skill toast: \"%s\" / \"%s\""), *UMOSkillToastWidget::Headline(Data).ToString(),
		*UMOSkillToastWidget::Detail(Data).ToString());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SkillPopupTimerHandle);
		World->GetTimerManager().SetTimer(SkillPopupTimerHandle, this, &UMONotificationComponent::HideSkillPopup, Popup.Duration, false);
	}
}

void UMONotificationComponent::ShowKnowledgePopup(FName KnowledgeId, float Duration)
{
	ShowSkillToast(KnowledgeId, /*bIsKnowledge=*/true, 0.0f, INDEX_NONE, INDEX_NONE, Duration);
}

void UMONotificationComponent::HideSkillPopup()
{
	UMOSkillToastWidget* SkillWidget = CurrentSkillPopupWidget.Get();
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

FMOSkillToastData UMONotificationComponent::BuildToastData(const FQueuedSkillPopup& Popup) const
{
	FMOSkillToastData Data;
	Data.Id = Popup.Id;
	Data.bIsKnowledge = Popup.bIsKnowledge;
	Data.DisplayName = UMOSkillDatabaseSettings::GetSkillOrKnowledgeDisplayName(Popup.Id);
	Data.XPGained = Popup.XPGained;
	Data.LevelBefore = Popup.LevelBefore;
	Data.LevelAfter = Popup.LevelAfter;
	if (const FMOSkillDefinitionRow* SkillDef = UMOSkillDatabaseSettings::GetSkillDefinition(Popup.Id))
	{
		Data.MaxLevel = SkillDef->MaxLevel;
	}
	// Knowledge rides on the skills component like a skill (its own levels and XP).
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UMOSkillsComponent* Skills = Pawn ? Pawn->FindComponentByClass<UMOSkillsComponent>() : nullptr;
	FMOSkillProgress Progress;
	if (Skills && Skills->GetSkillProgress(Popup.Id, Progress))
	{
		Data.Level = Progress.Level;
		Data.CurrentXP = Progress.CurrentXP;
		Data.XPToNextLevel = Progress.XPToNextLevel;
	}
	else if (Popup.LevelAfter != INDEX_NONE)
	{
		Data.Level = Popup.LevelAfter;
	}
	return Data;
}
