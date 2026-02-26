/**
 * =============================================================================
 * MONotificationComponent.h - Queued Notification Display System
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Manages queued notification display on the player's screen. Supports text
 * notifications, skill XP popups, recipe unlock messages, and item pickup
 * feedback. Notifications are queued and displayed sequentially.
 *
 * KEY RESPONSIBILITIES:
 * 1. Queue and display text notifications with type-based styling
 * 2. Show skill increase popups with progress bar flash
 * 3. Show recipe/knowledge unlock notifications
 * 4. Show item pickup notifications
 * 5. Manage notification timing and transitions
 *
 * OWNERSHIP:
 * - Owner: AMOPlayerController (sibling to UMOUIManagerComponent)
 * - Lifespan: Exists for duration of PlayerController
 *
 * NOTIFICATION TYPES:
 * - Info (blue): General information messages
 * - Success (green): Successful operations
 * - Warning (yellow): Warnings and cautions
 * - Error (red): Errors and failures
 *
 * QUEUE SYSTEM:
 * - Text notifications queued in NotificationQueue
 * - Skill popups queued in SkillPopupQueue
 * - Separate queues allow both to display simultaneously
 * - Each queue processes one item at a time
 *
 * DISPLAY FLOW:
 * ShowNotification(Message, Duration, Type)
 * -> Add to NotificationQueue
 * -> If no current notification: ProcessNextNotification()
 * -> Create widget, show for Duration
 * -> Timer fires HideCurrentNotification()
 * -> OnNotificationHideComplete() -> ProcessNextNotification()
 *
 * SKILL POPUP FLOW:
 * ShowSkillPopup(SkillId, Duration)
 * -> Add to SkillPopupQueue
 * -> If no current popup: ProcessNextSkillPopup()
 * -> ShowSkillPopupInternal() creates widget
 * -> Timer fires HideSkillPopup()
 * -> ProcessNextSkillPopup() for next in queue
 *
 * CRITICAL PATTERNS:
 * 1. SEQUENTIAL DISPLAY: Only one notification at a time per queue
 * 2. DURATION TIMER: FTimerHandle controls display duration
 * 3. WIDGET REUSE: Widgets may be reused or recreated per notification
 *
 * KNOWN PITFALLS:
 * 1. WIDGET CLASS REQUIRED: SkillNotificationWidgetClass must be set for
 *    ShowSkillPopup/ShowKnowledgePopup to work.
 * 2. Z-ORDER OVERLAP: Skill popup Z-order (260) higher than text (250)
 * 3. CLEAR ALL: ClearAllNotifications clears queues AND hides current
 *
 * RELATED FILES:
 * - MOUIManagerComponent.h - Delegates to this for notifications
 * - MONotificationWidget.h - Text notification widget
 * - MOSkillEntryWidget.h - Skill popup widget
 * - MOSkillsComponent.h - Source of skill XP events
 * - MORecipeDiscoveryComponent.h - Source of recipe unlock events
 *
 * TESTING CHECKLIST:
 * [ ] Text notifications display with correct styling
 * [ ] Multiple notifications queue and display sequentially
 * [ ] Skill popups show with progress bar
 * [ ] Recipe unlock notifications display correctly
 * [ ] ClearAllNotifications stops current and clears queue
 * [ ] Notifications work after pawn possession change
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MONotificationWidget.h"
#include "MONotificationComponent.generated.h"

class UMONotificationWidget;
class UMOSkillEntryWidget;
struct FMOSkillDisplayData;
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMONotificationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMONotificationComponent();

	// --- Public API ---

	/** Show a notification message for a duration with optional type. Queues if another is showing. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowNotification(const FText& Message, float Duration = 3.0f, EMONotificationType Type = EMONotificationType::Info);

	/** Show an info notification (blue). */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowInfoNotification(const FText& Message, float Duration = 3.0f);

	/** Show a success notification (green). */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowSuccessNotification(const FText& Message, float Duration = 3.0f);

	/** Show a warning notification (yellow). */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowWarningNotification(const FText& Message, float Duration = 3.0f);

	/** Show an error notification (red). */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowErrorNotification(const FText& Message, float Duration = 3.0f);

	/** Show an item pickup notification. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowItemPickupNotification(const FText& ItemName, int32 Quantity = 1);

	/** Show skill increase notification with formatted message. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowSkillIncreaseNotification(FName SkillId, float XPAmount);

	/** Show recipe unlocked notification with formatted message. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowRecipeUnlockedNotification(FName RecipeId);

	/** Show knowledge learned notification with formatted message. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowKnowledgeLearnedNotification(FName KnowledgeId);

	/** Show inventory full warning. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowInventoryFullNotification();

	/**
	 * Show a skill entry popup widget with flashing progress bar.
	 * Used when skill/knowledge changes from inspection.
	 * @param SkillId The skill that changed
	 * @param Duration How long to show the popup (default 3 seconds)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowSkillPopup(FName SkillId, float Duration = 3.0f);

	/**
	 * Show a knowledge entry popup widget.
	 * @param KnowledgeId The knowledge that was learned
	 * @param Duration How long to show the popup (default 3 seconds)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowKnowledgePopup(FName KnowledgeId, float Duration = 3.0f);

	/** Clear all pending notifications and hide current one. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ClearAllNotifications();

	/** Get the number of notifications in the queue (including current). */
	UFUNCTION(BlueprintPure, Category="MO|Notifications")
	int32 GetQueuedNotificationCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Configuration ---

	/** Widget class for text notifications. If not set, uses default UMONotificationWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Notifications", meta=(AllowPrivateAccess="true", DisplayName="Text Notification Blueprint"))
	TSubclassOf<UMONotificationWidget> NotificationWidgetClass;

	/** Widget class for skill/knowledge popup notifications. Uses a SkillEntry widget. Required for ShowSkillPopup/ShowKnowledgePopup to work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Notifications", meta=(AllowPrivateAccess="true", DisplayName="Skill Notification Blueprint"))
	TSubclassOf<UMOSkillEntryWidget> SkillNotificationWidgetClass;

	/** Z-order for notification widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Notifications", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 NotificationZOrder = 250;

	/** Z-order for skill popup widgets (above notifications). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Notifications", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 SkillPopupZOrder = 260;

	// --- Internal State ---

	struct FQueuedNotification
	{
		FText Message;
		float Duration;
		EMONotificationType Type = EMONotificationType::Info;
	};

	struct FQueuedSkillPopup
	{
		FName Id;
		bool bIsKnowledge;
		float Duration;
	};

	/** Queued notification messages. */
	TArray<FQueuedNotification> NotificationQueue;

	/** Queued skill/knowledge popups. */
	TArray<FQueuedSkillPopup> SkillPopupQueue;

	/** Current notification widget. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMONotificationWidget> CurrentNotificationWidget;

	/** Current skill popup widget. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOSkillEntryWidget> CurrentSkillPopupWidget;

	/** Container widget for skill popup (handles positioning). */
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserWidget> SkillPopupContainerWidget;

	/** Timer for notification display. */
	FTimerHandle NotificationTimerHandle;

	/** Timer for skill popup display. */
	FTimerHandle SkillPopupTimerHandle;

	// --- Internal Methods ---

	/** Get owning player controller (null-safe). */
	APlayerController* GetOwningPlayerController() const;

	/** Process the next notification in queue. */
	void ProcessNextNotification();

	/** Hide current notification and process next. */
	void HideCurrentNotification();

	/** Called when notification hide animation completes. */
	UFUNCTION()
	void OnNotificationHideComplete();

	/** Hide current skill popup and process next in queue. */
	void HideSkillPopup();

	/** Process next skill popup in queue. */
	void ProcessNextSkillPopup();

	/** Internal method to actually show a skill popup (called by ProcessNextSkillPopup). */
	void ShowSkillPopupInternal(FName SkillId, bool bIsKnowledge, float Duration);

	/** Build skill display data for a skill ID. */
	FMOSkillDisplayData BuildSkillDisplayData(FName SkillId) const;

	/** Build skill display data for knowledge (reuses skill display data structure). */
	FMOSkillDisplayData BuildKnowledgeDisplayData(FName KnowledgeId) const;
};
