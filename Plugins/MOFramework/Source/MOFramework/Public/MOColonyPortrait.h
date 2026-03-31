/**
 * =============================================================================
 * MOColonyPortrait.h - Colony Character Portrait Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Character portrait widget for colony UI. Shows character portrait/icon,
 * mood indicator, activity state, and alert status. Used in colony bar,
 * colony overview, and character cards.
 *
 * FEATURES:
 * - Portrait image (placeholder for now, mood-expressive later)
 * - Alert indicator (pulsing red for critical, orange dot for urgent)
 * - Activity indicator (working, resting, fighting, etc.)
 * - Selection state (for use in lists)
 * - Click handling (for possession, selection)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] ALERT PULSING: Critical alerts should pulse. Use animation or
 *   tick to update AlertBorder color. Current impl uses static color.
 *
 * [2026-03] PORTRAIT SOURCE: Using placeholder icons initially. When art
 *   is ready, SetPortraitFromCharacter() will need to resolve portraits.
 *
 * =============================================================================
 * RELATED FILES: MOColonyTypes.h, MOColonyBarWidget.h, MOColonyOverviewWidget.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOColonyTypes.h"
#include "MOColonyPortrait.generated.h"

class UImage;
class UBorder;
class UTextBlock;
class UMOCommonButton;
class UTexture2D;

/**
 * Delegate fired when portrait is clicked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOPortraitClickedDelegate, FGuid, CharacterGuid);

/**
 * Character portrait widget for colony UI.
 * See file header for features and pitfalls.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOColonyPortrait : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOColonyPortrait(const FObjectInitializer& ObjectInitializer);

	// ============================================================================
	// DATA BINDING
	// ============================================================================

	/**
	 * Set the character this portrait represents.
	 * @param InCharacterGuid Character's GUID
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetCharacterGuid(const FGuid& InCharacterGuid);

	/** Get the character GUID. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Colony")
	const FGuid& GetCharacterGuid() const { return CharacterGuid; }

	// ============================================================================
	// PORTRAIT DISPLAY
	// ============================================================================

	/** Set the portrait image. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetPortraitImage(UTexture2D* Portrait);

	/** Set portrait from soft reference. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetPortraitFromSoftPtr(TSoftObjectPtr<UTexture2D> PortraitPtr);

	// ============================================================================
	// STATE DISPLAY
	// ============================================================================

	/**
	 * Set the alert state (affects border color/animation).
	 * @param InAlertState Alert tier
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetAlertState(EAlertState InAlertState);

	/** Get current alert state. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Colony")
	EAlertState GetAlertState() const { return AlertState; }

	/**
	 * Set the activity state (for activity indicator).
	 * @param InActivityState Current activity
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetActivityState(EColonyActivityState InActivityState);

	/** Get current activity state. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Colony")
	EColonyActivityState GetActivityState() const { return ActivityState; }

	/**
	 * Set the character's name (for tooltip/display).
	 * @param InName Character name
	 */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetCharacterName(const FText& InName);

	/** Get character name. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Colony")
	const FText& GetCharacterName() const { return CharacterName; }

	// ============================================================================
	// SELECTION
	// ============================================================================

	/** Set whether this portrait is selected. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Colony")
	void SetSelected(bool bInSelected);

	/** Check if selected. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Colony")
	bool IsSelected() const { return bIsSelected; }

	// ============================================================================
	// DELEGATES
	// ============================================================================

	/** Broadcast when portrait is clicked. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Colony")
	FMOPortraitClickedDelegate OnPortraitClicked;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Color for no alert. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Colony|Style")
	FLinearColor NormalBorderColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);

	/** Color for Notable alert (Tier 3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Colony|Style")
	FLinearColor NotableBorderColor = FLinearColor(0.3f, 0.3f, 0.5f, 1.0f);

	/** Color for Urgent alert (Tier 2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Colony|Style")
	FLinearColor UrgentBorderColor = FLinearColor(1.0f, 0.6f, 0.0f, 1.0f);

	/** Color for Critical alert (Tier 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Colony|Style")
	FLinearColor CriticalBorderColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	/** Color when selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Colony|Style")
	FLinearColor SelectedBorderColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * Update visual display based on current state.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "MO|UI|Colony")
	void UpdateVisuals();

	/** Handle button click. */
	UFUNCTION()
	void HandleButtonClicked();

	// ============================================================================
	// WIDGET BINDINGS
	// ============================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> AlertBorder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ActivityIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMOCommonButton> ClickButton;

private:
	FGuid CharacterGuid;
	FText CharacterName;
	EAlertState AlertState = EAlertState::None;
	EColonyActivityState ActivityState = EColonyActivityState::Idle;
	bool bIsSelected = false;

	// For critical alert pulsing
	float PulseTime = 0.0f;
};
