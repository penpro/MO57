/**
 * =============================================================================
 * MOWorldCreationWidgets.h - The New Game panel's world-creation rows: one knob row per FMOWorldKnob, one card per startable biome
 * =============================================================================
 *
 * Built at runtime by UMONewGamePanel from FMOWorldCreation::GetKnobs() and DT_Biomes (rows with a PreviewImage), so a new knob or a new biome
 * needs no widget work. Blueprint setup (Content/Python/ui_specs/kit_mainmenu.py builds both):
 *   WBP_WorldKnobRow  (parent UMOWorldKnobRow):  LabelText, ValueSlider, ValueText; optional HintText
 *   WBP_BiomeCard     (parent UMOBiomeCard):     PreviewImage, SelectButton (the MOButton WBP, labelled with the biome); optional SelectedFrame
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MOWorldCreationTypes.h"
#include "MOWorldCreationWidgets.generated.h"

class UImage;
class USlider;
class UTextBlock;
class UTexture2D;
class UMOCommonButton;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMOWorldKnobChanged, FName /*KnobId*/, float /*Value*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMOBiomeCardPicked, FName /*BiomeId (None = the coast)*/);

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOWorldKnobRow : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Knob at Value (the slider runs over the knob's own range in its own steps). */
	void Configure(const FMOWorldKnob& InKnob, float Value);

	/** Moves the slider without broadcasting (the panel's Defaults button). */
	void SetValue(float Value);

	FName GetKnobId() const { return Knob.Id; }

	/** The player moved the slider: the knob's id and its snapped value. */
	FMOWorldKnobChanged OnKnobChanged;

	/** "x1.25": the panel's text for a value; a switch knob (FMOWorldKnob::bSwitch) reads "Off" / "On". */
	static FText FormatValue(float Value, bool bSwitch = false);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USlider> ValueSlider;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

private:
	UFUNCTION()
	void HandleSliderChanged(float Value);

	FMOWorldKnob Knob;
	bool bSettingValue = false;
};

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOBiomeCard : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** BiomeId None = the coast (the default start). Image may be null (the card shows its frame only). */
	void Configure(FName InBiomeId, const FText& Name, UTexture2D* Image);

	void SetSelected(bool bSelected);

	FName GetBiomeId() const { return BiomeId; }

	FMOBiomeCardPicked OnPicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UMOCommonButton> SelectButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> SelectedFrame;

private:
	void HandleClicked();

	FName BiomeId;
};
