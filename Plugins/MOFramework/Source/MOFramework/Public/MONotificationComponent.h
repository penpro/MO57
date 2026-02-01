#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MONotificationComponent.generated.h"

class UMONotificationWidget;

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

	/** Widget class for notifications. If not set, uses default UMONotificationWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Notifications", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UMONotificationWidget> NotificationWidgetClass;

	/** Z-order for notification widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Notifications", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 NotificationZOrder = 250;

	// --- Internal State ---

	struct FQueuedNotification
	{
		FText Message;
		float Duration;
	};

	/** Queued notification messages. */
	TArray<FQueuedNotification> NotificationQueue;

	/** Current notification widget. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMONotificationWidget> CurrentNotificationWidget;

	/** Timer for notification display. */
	FTimerHandle NotificationTimerHandle;

	// --- Internal Methods ---

	/** Get owning player controller (null-safe). */
	APlayerController* GetOwningPlayerController() const;

	/** Process the next notification in queue. */
	void ProcessNextNotification();

	/** Hide current notification and process next. */
	void HideCurrentNotification();
};
