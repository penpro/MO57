// The HUD toast for a skill or knowledge change ("Woodworking +2 XP", "New knowledge: Wood Properties"), shown by UMONotificationComponent's
// skill-popup queue. Until Oct 20 2026 that queue reused the Skills panel's row widget (WBP_SkillEntry) as a floating popup: no plate of its own,
// faint grey sub-text, a near-empty black bar, at 20% / 80% of the screen right behind the pawn, and it never said what had changed -- the XP and
// level change were dropped and a knowledge id showed raw ("WoodProperties"). Wes: "the ui for the skill-ups/knowledge-ups sucks and is hard to read".
// The tree is WBP_SkillToast (Content/Python/ui_specs/kit_hud.py: a full-screen canvas that places its plate under the text notification).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOSkillToastWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UBorder;
class UImage;

/** What changed: the grant (XP, level before and after) and the progress it left. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSkillToastData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	bool bIsKnowledge = false;

	/** XP this grant added (0: unknown, the toast then shows the standing only). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	float XPGained = 0.0f;

	/** Level before and after the grant (INDEX_NONE: unknown). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	int32 LevelBefore = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	int32 LevelAfter = INDEX_NONE;

	/** The standing now: level, XP into it, XP the next level needs. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	float CurrentXP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	float XPToNextLevel = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Skills|UI")
	int32 MaxLevel = 100;

	bool LeveledUp() const { return LevelBefore != INDEX_NONE && LevelAfter > LevelBefore; }
	float LevelProgress() const { return XPToNextLevel > 0.0f ? FMath::Clamp(CurrentXP / XPToNextLevel, 0.0f, 1.0f) : 1.0f; }
};

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOSkillToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="MO|Skills|UI")
	void SetupToast(const FMOSkillToastData& InData);

	/** "Woodworking +2 XP" / "Woodworking reached level 1" / "New knowledge: Wood Properties" / "Wood Properties knowledge +40 XP". */
	static FText Headline(const FMOSkillToastData& Data);

	/** "Level 0 - 2 / 100 XP to level 1", or "Level 100 (mastered)". */
	static FText Detail(const FMOSkillToastData& Data);

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> HeadlineText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UProgressBar> XPProgressBar;

	/** The leading bar: the accent for a skill, the knowledge colour for knowledge. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> AccentBar;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> ToastIcon;
};
