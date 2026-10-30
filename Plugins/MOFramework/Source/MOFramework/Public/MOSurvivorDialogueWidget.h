/**
 * =============================================================================
 * MOSurvivorDialogueWidget.h - What a survivor says, on screen
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Wes, Oct 29 2026: "when you interact with a survivor have a dialogue pop up to the effect of 'Omg I thought I was going to be alone forever!'".
 * A speech plate low in the middle of the screen: the speaker's name over their lines (UMORecruitmentComponent::GetDialogueFor). It comes up at
 * once, stays for as long as the lines take to read (ReadSeconds), then fades. It takes no input and pauses nothing (Docs/PAUSE_POLICY.md):
 * talking to someone never takes the player's hands. A new line replaces the one on screen.
 *
 * The tree is built in C++ (theme tokens: the HUD plate, type.body / body.strong), like the breath meter; a Widget Blueprint subclass may
 * supply its own tree with TextBlocks named SpeakerText and LinesText.
 *
 * RELATED FILES: MOUIManagerComponent.h (ShowSurvivorDialogue), MOPlayerController.h (ClientShowSurvivorDialogue), MORecruitmentComponent.h
 * LAST UPDATED: 2026-10-29
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOSurvivorDialogueWidget.generated.h"

class UTextBlock;

UCLASS()
class MOFRAMEWORK_API UMOSurvivorDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Say Lines (one per row) as SpeakerName: up at once, held for ReadSeconds(Lines), then faded out. */
	void ShowLines(const FText& SpeakerName, const FText& Lines);

	/** How long a reader needs for Text (seconds): a floor plus a per-character rate, capped. */
	static float ReadSeconds(const FText& Text);

	/** True while the lines are on screen (held or fading). */
	bool IsShowing() const { return HoldRemaining > 0.0f || Alpha > 0.01f; }

	FText GetSpeakerText() const;
	FText GetLinesText() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Dialogue", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(BlueprintReadOnly, Category="MO|UI|Dialogue", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LinesText;

private:
	float HoldRemaining = 0.0f;
	float Alpha = 0.0f;
};
