/**
 * =============================================================================
 * MOStatusField.h - Single Stat Display Widget
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Reusable widget for displaying a single stat with title, value text, and
 * optional progress bar. Used in status panel for vitals, nutrition, fitness,
 * mental state, etc. Supports color thresholds for warning/critical states.
 *
 * DISPLAY ELEMENTS:
 * - TitleText: Label (e.g., "Heart Rate", "Hunger")
 * - ValueText: Value (e.g., "72 BPM", "98%")
 * - ValueBar: Optional progress bar for normalized values
 * - IconImage: Optional icon
 *
 * COLOR THRESHOLDS:
 * - HealthyColor: Above WarningThreshold (default green)
 * - WarningColor: Between Critical and Warning (default yellow)
 * - CriticalColor: Below CriticalThreshold (default red)
 * - bInvertThresholds: For stats where higher = worse (e.g., temperature)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] NORMALIZED VALUE: SetValue() with -1.0 hides progress bar.
 *   Pass valid 0.0-1.0 to show bar.
 *
 * [2024-02] FIELD ID: Use SetFieldId() for data binding. Query with
 *   GetFieldId() to match against data sources.
 *
 * [2024-02] COMMON UI: Inherits UCommonUserWidget, not UUserWidget.
 *
 * =============================================================================
 * RELATED FILES: MOStatusPanel.h, MOPlayerStatusWidget.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MOStatusField.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;

/**
 * Reusable status field widget for displaying a single stat with title, value, and optional progress bar.
 * Used in the status panel to display vitals, nutrition, fitness, mental state, etc.
 */
UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOStatusField : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Set the field's display values */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SetFieldData(const FText& Title, const FText& Value, float NormalizedValue = -1.0f);

	/** Set just the value (for frequent updates) */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SetValue(const FText& Value, float NormalizedValue = -1.0f);

	/** Set the color based on status thresholds */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SetStatusColor(FLinearColor Color);

	/** Convenience function to set color based on normalized value and thresholds */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SetStatusFromNormalizedValue(float NormalizedValue);

	/** Show or hide the progress bar */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SetProgressBarVisible(bool bVisible);

	/** Get the field identifier */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	FName GetFieldId() const { return FieldId; }

	/** Set the field identifier (used for binding to data sources) */
	UFUNCTION(BlueprintCallable, Category = "MO|Status")
	void SetFieldId(FName InFieldId) { FieldId = InFieldId; }

protected:
	virtual void NativeConstruct() override;

	/** Blueprint event when field data changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "MO|Status")
	void OnFieldDataChanged(const FText& Title, const FText& Value, float NormalizedValue);

protected:
	/** Title/label text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Value text (e.g., "72 BPM", "98%") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	/** Optional progress bar for normalized values */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ValueBar;

	/** Optional icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** Unique identifier for this field (used for data binding) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status")
	FName FieldId;

	/** Color thresholds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status|Colors")
	float WarningThreshold = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status|Colors")
	float CriticalThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status|Colors")
	FLinearColor HealthyColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status|Colors")
	FLinearColor WarningColor = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status|Colors")
	FLinearColor CriticalColor = FLinearColor::Red;

	/** Whether to invert the threshold logic (higher = worse, like temperature) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|Status|Colors")
	bool bInvertThresholds = false;
};
