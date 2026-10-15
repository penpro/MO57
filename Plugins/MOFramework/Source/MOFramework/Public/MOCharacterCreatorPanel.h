/**
 * =============================================================================
 * MOCharacterCreatorPanel.h - New game: the first survivor's name and look, while the world loads behind
 * =============================================================================
 *
 * PURPOSE:
 * The character creator (Wes, Oct 15 2026): the name, skin tone, hair colour (melanin + redness) and eye colour of the one starting survivor. It opens IN THE
 * WORLD as soon as a new game's first pawn has landed -- instead of the loading screen -- and frames that pawn with its own camera, so the vegetation and
 * texture streaming (the rest of UMOWorldReadinessSubsystem's cycle) happen behind it: "at least the first load in will be effectively instant". Hair
 * style, face and body are "coming soon" rows: nothing is built to change them yet.
 *
 * FLOW: AMOGameMode::OnPawnLandedSafely (new game only) hides the loading overlay and calls AMOPlayerController::ClientOpenCharacterCreator ->
 * UMOSystemMenuUIController::ShowCharacterCreator pushes this on Layer_Menu. Slider changes go straight to the pawn (AMOMetaHumanCharacter::
 * SetAppearanceColors: replicated, saved). Begin renames the pawn (RequestSetPawnIdentity), gives the view back to it and closes; if the world is not
 * ready yet the loading overlay holds for the remainder.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOCharacterAppearance.h"
#include "MOCharacterCreatorPanel.generated.h"

class UMOCommonButton;
class UEditableTextBox;
class USlider;
class UTextBlock;
class ACameraActor;
class AMOMetaHumanCharacter;

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOCharacterCreatorPanel : public UMOActivatableWidget
{
	GENERATED_BODY()

public:
	const FMOCharacterAppearance& GetLook() const { return Look; }

	/** Select an eye colour by preset id (tests, tools). */
	void SetEyeColor(FName PresetId);

	/** Preview camera: distance from the survivor, the height it looks at (chest / face) above the feet, field of view, where on the screen
	 *  (0..1 across) the survivor stands -- right of the panel -- and the aperture that softens the world still loading behind them. */
	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	float PreviewCameraDistanceCm = 260.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	float PreviewFocusHeightCm = 140.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	float PreviewFieldOfView = 34.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PreviewScreenX = 0.72f;

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator", meta=(ClampMin="0.7"))
	float PreviewFStop = 2.0f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	/** The creator is a step of the new game, not a menu to back out of: Escape does nothing here. */
	virtual bool NativeOnHandleBackAction() override { return true; }

private:
	UFUNCTION() void HandleSkinToneChanged(float Value);
	UFUNCTION() void HandleHairShadeChanged(float Value);
	UFUNCTION() void HandleHairRednessChanged(float Value);
	void HandleEyePrev() { StepEyeColor(-1); }
	void HandleEyeNext() { StepEyeColor(1); }
	void HandleRandomize();
	void HandleBegin();

	AMOMetaHumanCharacter* GetSurvivor() const;
	void StepEyeColor(int32 Delta);
	int32 GetEyeColorIndex() const;
	void RefreshControls();
	void RefreshWorldStatus();
	void ApplyLookToSurvivor();
	void FrameSurvivor();
	void ReleaseCamera(float BlendSeconds);

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UEditableTextBox> NameInputBox;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USlider> SkinToneSlider;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USlider> HairShadeSlider;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USlider> HairRednessSlider;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EyeColorText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> EyePrevButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> EyeNextButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> RandomizeButton;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> BeginButton;

	/** "The world is taking shape -- 64 %" while UMOWorldReadinessSubsystem is still tracking; "The world is ready" after. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WorldStatusText;

	FMOCharacterAppearance Look;
	TWeakObjectPtr<ACameraActor> PreviewCamera;
	FTimerHandle WorldStatusTimer;
};
