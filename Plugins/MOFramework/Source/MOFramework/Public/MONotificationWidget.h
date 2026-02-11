#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MONotificationWidget.generated.h"

class UTextBlock;
class UBorder;
class UImage;

/**
 * Notification type for styling different message categories.
 */
UENUM(BlueprintType)
enum class EMONotificationType : uint8
{
	Info		UMETA(DisplayName = "Info"),
	Success		UMETA(DisplayName = "Success"),
	Warning		UMETA(DisplayName = "Warning"),
	Error		UMETA(DisplayName = "Error"),
	ItemPickup	UMETA(DisplayName = "Item Pickup"),
	SkillGain	UMETA(DisplayName = "Skill Gain"),
	Custom		UMETA(DisplayName = "Custom")
};

/**
 * Notification widget for displaying temporary messages.
 *
 * Create a Blueprint widget based on this class and add the following widgets:
 * - MessageText (TextBlock) - Required: The notification message
 * - BackgroundBorder (Border) - Optional: Background for styling
 * - IconImage (Image) - Optional: Icon display
 *
 * The Blueprint can implement animations via PlayShowAnimation/PlayHideAnimation.
 */
UCLASS()
class MOFRAMEWORK_API UMONotificationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Core API ---

	/**
	 * Set up the notification with message and optional type/icon.
	 * Call this before adding to viewport.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void SetupNotification(const FText& Message, EMONotificationType Type = EMONotificationType::Info, UTexture2D* Icon = nullptr);

	/** Set just the message text. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void SetMessage(const FText& Message);

	/** Set the notification type (updates colors). */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void SetNotificationType(EMONotificationType Type);

	/** Set a custom icon. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void SetIcon(UTexture2D* Icon);

	/** Get the current notification type. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Notification")
	EMONotificationType GetNotificationType() const { return CurrentType; }

	// --- Color Customization ---

	/** Set the text color directly. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void SetTextColor(FLinearColor Color);

	/** Set the background color/opacity. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void SetBackgroundColor(FLinearColor Color);

	/** Get the default color for a notification type. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Notification")
	static FLinearColor GetColorForType(EMONotificationType Type);

	// --- Animation Hooks (Blueprint Implementable) ---

	/** Called when the notification should animate in. Override in Blueprint to add custom animation. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Notification")
	void PlayShowAnimation();

	/** Called when the notification should animate out. Override in Blueprint to add custom animation. */
	UFUNCTION(BlueprintNativeEvent, Category="MO|UI|Notification")
	void PlayHideAnimation();

	/** Called when notification is fully hidden (after animation). Call from Blueprint when hide animation completes. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void OnHideAnimationComplete();

	/** Delegate broadcast when hide animation completes. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNotificationHidden);
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Notification")
	FOnNotificationHidden OnNotificationHidden;

protected:
	virtual void NativeConstruct() override;

	/** Apply colors based on notification type. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Notification")
	void ApplyTypeColors();

	// --- Widget Bindings ---

	/** Text block for the notification message. Required. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	/** Background border for styling. Optional. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> BackgroundBorder;

	/** Icon image display. Optional. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// --- Type Colors (Configurable in Blueprint) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor InfoColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);  // Blue

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor SuccessColor = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);  // Green

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);  // Yellow/Gold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor ErrorColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // Red

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor ItemPickupColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);  // Light Gray

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor SkillGainColor = FLinearColor(0.6f, 0.4f, 1.0f, 1.0f);  // Purple

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Notification|Colors")
	FLinearColor BackgroundColorBase = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);  // Semi-transparent black

private:
	/** Current notification type. */
	EMONotificationType CurrentType = EMONotificationType::Info;

	/** Pending message to set after construct. */
	FText PendingMessage;
	bool bHasPendingMessage = false;

	/** Pending type to set after construct. */
	EMONotificationType PendingType = EMONotificationType::Info;
	bool bHasPendingType = false;
};
