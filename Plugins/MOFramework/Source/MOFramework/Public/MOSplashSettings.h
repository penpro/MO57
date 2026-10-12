#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOSplashSettings.generated.h"

class UTexture2D;
class UMOSplashWidget;

/** One splash card: a mark (and/or a caption) that fades in, holds, fades out. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSplashCard
{
	GENERATED_BODY()

	/** The mark, white or coloured on transparent. Empty = caption only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> Logo;

	/** Text under the mark (the studio name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash")
	FText Caption;

	/** Seconds fully visible (the fades come on top of it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash", meta = (ClampMin = "0.2", ClampMax = "10.0"))
	float HoldSeconds = 2.4f;

	/** Rendered size of the mark in UI units (1080p reference). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Splash", meta = (ClampMin = "16.0", ClampMax = "1080.0"))
	float LogoSize = 360.0f;
};

/**
 * Project Settings > MO Splash Screens. The cards play before the intro video on the main menu map, over the live world (see UMOSplashWidget).
 *
 * To add "Made with Unreal Engine": import the OFFICIAL logo from Epic's branding page as a Texture2D and add a card that points at it. Epic's guidelines govern how
 * it may be shown (do not redraw or recolour it).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "MO Splash Screens"))
class MOFRAMEWORK_API UMOSplashSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMOSplashSettings();

	/** Play the cards before the intro video (first launch of the session). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Splash")
	bool bEnabled = true;

	/** The cards, in order. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Splash")
	TArray<FMOSplashCard> Cards;

	/** Fade in and fade out length of every card. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Splash", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float FadeSeconds = 0.7f;

	/** The widget that plays the cards. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Splash", meta = (AllowedClasses = "/Script/Engine.Blueprint"))
	TSoftClassPtr<UMOSplashWidget> WidgetClass;

	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
};
