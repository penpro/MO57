#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "MOOptionsPanel.generated.h"

class UCommonButtonBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOOptionsPanelRequestCloseSignature);

/**
 * Options/Settings panel for the in-game menu.
 *
 * This is a placeholder base class. Override in Blueprint to add
 * your specific options UI (audio, video, controls, etc).
 */
UCLASS()
class MOFRAMEWORK_API UMOOptionsPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Apply current settings. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Options")
	virtual void ApplySettings();

	/** Reset settings to defaults. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Options")
	virtual void ResetToDefaults();

	/** Called when panel requests to close. */
	UPROPERTY(BlueprintAssignable, Category="MO|UI|Options")
	FMOOptionsPanelRequestCloseSignature OnRequestClose;

protected:
	virtual void NativeConstruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Called when settings should be refreshed from current values. */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|Options")
	void OnRefreshSettings();

private:
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleResetClicked();
	UFUNCTION() void HandleBackClicked();

private:
	// ============================================================
	// BIND WIDGETS - Create these in your WBP_OptionsPanel
	// ============================================================

	/** Apply settings button. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ApplyButton;

	/** Reset to defaults button. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ResetButton;

	/** Back/Close button. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonButtonBase> BackButton;
};
