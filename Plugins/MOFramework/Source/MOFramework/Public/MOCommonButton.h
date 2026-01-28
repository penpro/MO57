#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "MOCommonButton.generated.h"

/**
 * Base button class for MOFramework UI.
 * Extends CommonButtonBase with text label support.
 *
 * Blueprint Setup:
 * 1. Create WBP_MOCommonButton based on this class
 * 2. The button style controls the visual appearance
 * 3. Override "UpdateButtonText" event to set your text widget
 * 4. Set ButtonLabel per-instance in the designer
 *
 * The text is NOT managed via BindWidget due to CommonUI's internal structure.
 * Instead, override the UpdateButtonText event in Blueprint to update your text.
 */
UCLASS()
class MOFRAMEWORK_API UMOCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UMOCommonButton(const FObjectInitializer& ObjectInitializer);

	/** Set the button's text label. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Button")
	void SetButtonText(const FText& InText);

	/** Get the button's current text label. */
	UFUNCTION(BlueprintPure, Category="MO|UI|Button")
	FText GetButtonText() const { return ButtonLabel; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	/**
	 * Called when the button text needs to be updated.
	 * Override this in Blueprint to set your TextBlock's text.
	 *
	 * Example Blueprint implementation:
	 * - Get your TextBlock reference
	 * - Call SetText with the NewText parameter
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="MO|UI|Button")
	void UpdateButtonText(const FText& NewText);

protected:
	/** The text label displayed on the button. Set this per-instance in the designer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|UI|Button", meta=(ExposeOnSpawn=true))
	FText ButtonLabel;
};
