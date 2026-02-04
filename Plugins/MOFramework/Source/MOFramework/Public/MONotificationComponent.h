#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MONotificationComponent.generated.h"

class UMONotificationWidget;
class UMOSkillEntryWidget;
struct FMOSkillDisplayData;

/**
 * Component that manages queued notification display.
 * Attach to PlayerController alongside UMOUIManagerComponent.
 *
 * Notifications are queued and displayed one at a time with configurable duration.
 * Supports skill increase and recipe unlock notifications with formatted messages.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMONotificationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMONotificationComponent();

	// --- Public API ---

	/** Show a notification message for a duration. Queues if another is showing. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowNotification(const FText& Message, float Duration = 3.0f);

	/** Show skill increase notification with formatted message. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowSkillIncreaseNotification(FName SkillId, float XPAmount);

	/** Show recipe unlocked notification with formatted message. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowRecipeUnlockedNotification(FName RecipeId);

	/** Show knowledge learned notification with formatted message. */
	UFUNCTION(BlueprintCallable, Category="MO|Notifications")
	void ShowKnowledgeLearnedNotification(FName KnowledgeId);

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
