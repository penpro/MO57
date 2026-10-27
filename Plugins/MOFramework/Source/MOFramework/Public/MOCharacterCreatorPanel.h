/**
 * =============================================================================
 * MOCharacterCreatorPanel.h - New game: the first survivor's name and look, while the world loads behind
 * =============================================================================
 *
 * PURPOSE:
 * The character creator (Wes, Oct 15 2026): the name, skin tone, hair colour (melanin + redness), eye colour and -- Oct 25 2026 -- hair style, beard and
 * mustache (the MetaHuman groom library: UMOAppearanceSubsystem's catalog, None first) of the one starting survivor. It opens IN THE WORLD as soon as a
 * new game's first pawn has landed -- instead of the loading screen -- and frames that pawn with its own camera, so the vegetation and texture streaming
 * (the rest of UMOWorldReadinessSubsystem's cycle) happen behind it: "at least the first load in will be effectively instant". Oct 27 2026: the Base
 * picker (a MetaHuman from the Creator -- UMOSurvivorBaseSettings: its body, face and grooms, and the body the survivor starts with) and the build
 * sliders under it (bust, hips and butt, waist, thighs: the body shapes). The face is still a "coming soon" row.
 *
 * FLOW: AMOGameMode::OnPawnLandedSafely (new game only) hides the loading overlay and calls AMOPlayerController::ClientOpenCharacterCreator ->
 * UMOSystemMenuUIController::ShowCharacterCreator pushes this on Layer_Menu. Every change goes straight to the pawn (AMOMetaHumanCharacter::
 * SetAppearanceLook: replicated, saved). Begin renames the pawn (RequestSetPawnIdentity), gives the view back to it and closes; if the world is not
 * ready yet the loading overlay holds for the remainder.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOActivatableWidget.h"
#include "MOCharacterAppearance.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/StreamableManager.h"
#include "MOCharacterCreatorPanel.generated.h"

class UMOCommonButton;
class UBorder;
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
	UMOCharacterCreatorPanel(const FObjectInitializer& ObjectInitializer);

	const FMOCharacterAppearance& GetLook() const { return Look; }

	/** Turn the preview camera around the survivor (the drag on the world does this): degrees of yaw and pitch, pitch clamped to OrbitPitch. */
	UFUNCTION(BlueprintCallable, Category="MO|CharacterCreator")
	void OrbitPreview(float DeltaYawDeg, float DeltaPitchDeg);

	/** Move the preview camera nearer (negative) or further, clamped to PreviewZoomMin / MaxCm (the mouse wheel does this). */
	UFUNCTION(BlueprintCallable, Category="MO|CharacterCreator")
	void ZoomPreview(float DeltaCm);

	float GetOrbitYawDeg() const { return OrbitYawDeg; }

	/** Click-drag on the world orbits the camera around the survivor (Wes, Oct 25 2026: "i'd like to be able to click drag the world to pivot
	 *  the camera around the character to be able to look at the back/sides"): degrees per pixel dragged, the pitch range, the wheel's zoom range. */
	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	float OrbitDegreesPerPixel = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	FVector2D OrbitPitchRange = FVector2D(-15.0f, 40.0f);

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	float PreviewZoomMinCm = 110.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	float PreviewZoomMaxCm = 420.0f;

	/** Select an eye colour by preset id (tests, tools). */
	void SetEyeColor(FName PresetId);

	/** The styles a groom slot (Hair, Beard, Mustache) offers, in the creator's order: None ("Bald" for hair) first, then the library's. */
	TArray<FMOAppearanceAssetEntry> GetGroomChoices(EMOAppearanceCategory GroomSlot) const;

	/** Select a groom for a slot: a path from GetGroomChoices (FMOCharacterAppearance::NoGroom = none). Tests, tools. */
	void SetGroom(EMOAppearanceCategory GroomSlot, const FString& Path);

	/** Step a slot's groom through GetGroomChoices (wraps). */
	void StepGroom(EMOAppearanceCategory GroomSlot, int32 Delta);

	/** Select a survivor base by id (UMOSurvivorBaseSettings; Oct 27 2026): its meshes, its grooms and its body (the survivor starts with the body
	 *  fat and muscle the base was made with). Tests, tools; the Base picker steps through them. */
	void SetBase(FName BaseId);

	/** Step the base through the list (wraps). */
	void StepBase(int32 Delta);

	/** Set a build slider (a body-shape driver: Bust, Hips, Waist, Thighs; -1..1). */
	void SetBuild(FName Driver, float Value);

	/** The build sliders, in the panel's order: driver and widget. */
	static const TArray<FName>& GetBuildDrivers();

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

	/** Depth of field on the preview camera (PreviewFStop sets its aperture). OFF (Wes, Oct 16 2026): with Single Layer Water in view UE 5.8 drops groom
	 *  hair from the scene depth, so the blur ate the survivor's buns -- an engine bug (Docs/Engine_Patches.md #1). Turn back on once Epic fixes it. */
	UPROPERTY(EditDefaultsOnly, Category="MO|CharacterCreator")
	bool bPreviewDepthOfField = false;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	/** The creator is a step of the new game, not a menu to back out of: Escape does nothing here. */
	virtual bool NativeOnHandleBackAction() override { return true; }
	/** The drag that orbits the camera (it starts on OrbitArea, HandleOrbitAreaMouseDown, and holds the mouse until the button comes up). */
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UFUNCTION() void HandleSkinToneChanged(float Value);
	UFUNCTION() void HandleHairShadeChanged(float Value);
	UFUNCTION() void HandleHairRednessChanged(float Value);
	void HandleEyePrev() { StepEyeColor(-1); }
	void HandleEyeNext() { StepEyeColor(1); }
	void HandleHairStylePrev() { StepGroom(EMOAppearanceCategory::Hair, -1); }
	void HandleHairStyleNext() { StepGroom(EMOAppearanceCategory::Hair, 1); }
	void HandleBeardPrev() { StepGroom(EMOAppearanceCategory::Beard, -1); }
	void HandleBeardNext() { StepGroom(EMOAppearanceCategory::Beard, 1); }
	void HandleMustachePrev() { StepGroom(EMOAppearanceCategory::Mustache, -1); }
	void HandleMustacheNext() { StepGroom(EMOAppearanceCategory::Mustache, 1); }
	void HandleBasePrev() { StepBase(-1); }
	void HandleBaseNext() { StepBase(1); }
	UFUNCTION() void HandleBustChanged(float Value) { SetBuild(TEXT("Bust"), Value); }
	UFUNCTION() void HandleHipsChanged(float Value) { SetBuild(TEXT("Hips"), Value); }
	UFUNCTION() void HandleWaistChanged(float Value) { SetBuild(TEXT("Waist"), Value); }
	UFUNCTION() void HandleThighsChanged(float Value) { SetBuild(TEXT("Thighs"), Value); }
	/** The build slider of a driver (BustSlider ...), or null. */
	USlider* GetBuildSlider(FName Driver) const;
	/** Where Look's base sits in the list; a look with no base wears the pawn Blueprint's own meshes, the first base's (Femme). */
	int32 GetBaseIndex() const;
	void HandleRandomize();
	void HandleBegin();

	AMOMetaHumanCharacter* GetSurvivor() const;
	void StepEyeColor(int32 Delta);
	int32 GetEyeColorIndex() const;
	/** Where the slot's current groom sits in Choices: its path, or for an empty path (the Blueprint's own groom) the library style of the same
	 *  name the pawn wears (the survivor's updo braids), None when it wears nothing. INDEX_NONE: a groom the library does not have. */
	int32 GetGroomIndex(EMOAppearanceCategory GroomSlot, const TArray<FMOAppearanceAssetEntry>& Choices) const;
	FText GetGroomLabel(EMOAppearanceCategory GroomSlot) const;
	void RefreshControls();
	void RefreshWorldStatus();
	void ApplyLookToSurvivor();
	void FrameSurvivor();
	/** The preview camera at the orbit (OrbitYawDeg / OrbitPitchDeg around PreviewFocus, PreviewDistanceCm away), the survivor at PreviewScreenX
	 *  across the screen, pulled in front of any ground or rock between them. */
	void PlacePreviewCamera();
	void ReleaseCamera(float BlendSeconds);
	/** Start loading the styles either side of a slot's current one (the groom and its bindings), so stepping to them swaps at once. */
	void PreloadGroomNeighbours(EMOAppearanceCategory GroomSlot);

	UFUNCTION()
	FEventReply HandleOrbitAreaMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/** The screen behind the panel: a click there starts an orbit drag instead of reaching the world (which would also take the menu's focus). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> OrbitArea;

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

	/** The groom pickers: "<  Side swept fringe (medium)  >" rows like the eye colour's. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HairStyleText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> HairStylePrevButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> HairStyleNextButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BeardText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BeardPrevButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BeardNextButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MustacheText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> MustachePrevButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> MustacheNextButton;

	/** The base picker ("<  Lillian  >") and the build sliders under it (Oct 27 2026). */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BaseText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BasePrevButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UMOCommonButton> BaseNextButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> BustSlider;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> HipsSlider;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> WaistSlider;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<USlider> ThighsSlider;

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

	FVector PreviewFocus = FVector::ZeroVector;
	float PreviewBaseYawDeg = 0.0f;
	float OrbitYawDeg = 0.0f;
	float OrbitPitchDeg = 0.0f;
	float PreviewDistanceCm = 0.0f;
	bool bOrbitDragging = false;
	FVector2D LastDragPosition = FVector2D::ZeroVector;
	TMap<EMOAppearanceCategory, TSharedPtr<FStreamableHandle>> GroomPreloads;
};
