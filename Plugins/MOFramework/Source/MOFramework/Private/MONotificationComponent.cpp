#include "MONotificationComponent.h"
#include "MOFramework.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "MONotificationWidget.h"
#include "MORecipeDatabaseSettings.h"

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
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NotificationTimerHandle);
	}

	// Remove widget
	UMONotificationWidget* Widget = CurrentNotificationWidget.Get();
	if (IsValid(Widget))
	{
		Widget->RemoveFromParent();
	}
	CurrentNotificationWidget.Reset();

	// Clear queue
	NotificationQueue.Empty();

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
		// Fallback: format the recipe ID nicely
		FString RecipeString = RecipeId.ToString();
		if (RecipeString.Len() > 0)
		{
			RecipeString[0] = FChar::ToUpper(RecipeString[0]);
		}
		RecipeName = FText::FromString(RecipeString);
	}

	FText Message = FText::Format(
		NSLOCTEXT("MO", "RecipeUnlocked", "You can now craft: {0}"),
		RecipeName);

	ShowNotification(Message, 4.0f);
}

void UMONotificationComponent::ClearAllNotifications()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NotificationTimerHandle);
	}

	// Remove current widget
	UMONotificationWidget* Widget = CurrentNotificationWidget.Get();
	if (IsValid(Widget) && Widget->IsInViewport())
	{
		Widget->RemoveFromParent();
	}

	// Clear queue
	NotificationQueue.Empty();

	UE_LOG(LogMOFramework, Log, TEXT("[MONotification] Cleared all notifications"));
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
