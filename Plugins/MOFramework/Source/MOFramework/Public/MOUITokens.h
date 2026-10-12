// UI design tokens ("Field Ledger" kit, DesignKit/MO57_UI_Kit): one named value per colour / size / duration, loaded from the DataTable DT_UITokens (the kit's tokens_datatable.csv).
//
// WHY: every screen used to carry its own colours and sizes (a red default tint stacked into one pink wall, 12 px grey text). Runtime code that needs a design value (a moodle's severity tint, a
// pulse duration, the plate behind world text) asks the token, never a literal; widget blueprints are generated from the same table by the `ui build` specs (Content/Python/ui_specs/kit_*.py).
//
// ACCESSIBILITY lives here, as token swaps (kit README):  colour-blind  -> color.severity.* read from the `colorblind.` block;  high contrast -> opacity.hud.plate 0.95 and secondary text = primary;
//   reduced motion -> motion.* = 0 except motion.fast;  text scale 1.0 / 1.25 / 1.5 -> type.*.size|umgSize|lineHeight, control.height.*, row.height multiplied.
//
// GAME THREAD ONLY (the cache is not locked).  A missing token is logged once and returns a loud magenta / the supplied default: a typo must be visible, not silently grey.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateColor.h"

#include "MOUITokens.generated.h"

class UFont;

/** One row of DT_UITokens (CSV columns: ---(row name), Value, Kind, Notes). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOUITokenRow : public FTableRowBase
{
	GENERATED_BODY()

	/** "#RRGGBB" / "#RRGGBBAA" (sRGB), a number, a list "[a, b]", or another token's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tokens")
	FString Value;

	/** Color | Number | String (documentation; parsing is by use). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tokens")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tokens")
	FString Notes;
};

/** The accessibility switches that change what a token resolves to. */
struct MOFRAMEWORK_API FMOUIAccessibility
{
	bool bColorBlind = false;
	bool bHighContrast = false;
	bool bReducedMotion = false;
	float TextScale = 1.0f;   // 1.0 / 1.25 / 1.5
};

/** Token name -> value, with the accessibility rules. No UObject: unit-testable (Tests/MOUITokensTests.cpp). */
class MOFRAMEWORK_API FMOUITokenSet
{
public:
	void Set(const TMap<FName, FString>& InValues) { Values = InValues; }
	bool Has(FName Token) const { return Values.Contains(Token); }
	int32 Num() const { return Values.Num(); }

	/** "<Family>|<Weight>" for every `font.face.<Family>.<Weight>` token (family without spaces). */
	TArray<FString> FontFaceKeys() const;

	/** @param bFound  false when the token (after reference resolution) does not exist or is not a colour. Out is then magenta. */
	FLinearColor Color(FName Token, const FMOUIAccessibility& A, bool* bFound = nullptr) const;
	float Number(FName Token, const FMOUIAccessibility& A, float Default = 0.0f, bool* bFound = nullptr) const;
	FString String(FName Token, const FMOUIAccessibility& A) const;

	/** "#RRGGBB" or "#RRGGBBAA", sRGB hex -> linear colour (alpha stays linear). */
	static bool ParseColor(const FString& Hex, FLinearColor& Out);

	/** "l,t,r,b" -> four floats. */
	static bool ParseMargin(const FString& Text, FVector4& Out);

	/** Among keys "<family>|<weight>" pick the face for (Family, Weight): exact, else the nearest weight of that family (heavier on a tie). Empty when the family has no key. */
	static FString PickFontKey(const TArray<FString>& Keys, const FString& Family, int32 Weight);

private:
	/** The raw string for a token after the accessibility swaps and following `token -> token` references (depth-limited). */
	bool Resolve(FName Token, const FMOUIAccessibility& A, FString& Out, int32 Depth = 0) const;
	static bool IsScaledByTextScale(const FString& Token);

	TMap<FName, FString> Values;
};

/** Project Settings > MO UI Theme: where the tokens and the fonts live. */
UCLASS(Config = Game, DefaultConfig, BlueprintType, meta = (DisplayName = "MO UI Theme"))
class MOFRAMEWORK_API UMOUIThemeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** DataTable of FMOUITokenRow (the kit's tokens_datatable.csv). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Tokens", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	FSoftObjectPath TokenTable;

	// Font faces are tokens too (`font.face.<Family>.<Weight>` = a FontFace asset path, family without spaces), so the typeface swap is a table change, not a setting.

	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
};

/** The token API for runtime code and Blueprints. */
UCLASS()
class MOFRAMEWORK_API UMOUITokens : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The kit icon `ic_moodle_cold`, `ic_sev_warning`, ... (white on transparent: tint it) from /MOFramework/UI/Kit/Icons/T_<name>. Null when the asset is missing (logged once). */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static class UTexture2D* Icon(FName IconName);

	/** A kit 9-slice mask `panel_fill`, `button_stroke`, ... from /MOFramework/UI/Kit/NineSlice/T_UI_<piece>. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static class UTexture2D* NineSlice(FName Piece);

	/** A box brush for a kit 9-slice piece: the texture, DrawAs Box (Image / Tile for the pieces that are), the slice margin from the token table, tinted by a colour token. */
	static FSlateBrush NineSliceBrush(FName Piece, FName ColorToken);

	/** "l,t,r,b" token as a margin (UMG 0..1 fractions for the 9-slice rows). */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static FMargin MarginToken(FName Token);

	/** Colour token as a linear colour, e.g. "color.severity.critical". */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static FLinearColor Color(FName Token);

	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static FSlateColor SlateColor(FName Token);

	/** Number token (px at 1080p, a duration in ms, an opacity), e.g. "space.4", "motion.base". */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static float Number(FName Token, float Default = 0.0f);

	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static FString String(FName Token);

	/** The font for a type token name without the "type." prefix: "body", "title", "numeric.large". Size = umgSize (px x 0.75) x text scale. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static FSlateFontInfo Font(FName TypeToken);

	/** Duration token in seconds (motion.*: ms -> s, 0 under reduced motion). */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Tokens")
	static float Seconds(FName Token);

	UFUNCTION(BlueprintCallable, Category = "MO|UI|Tokens")
	static void SetAccessibility(bool bColorBlind, bool bHighContrast, bool bReducedMotion, float TextScale);

	static const FMOUIAccessibility& GetAccessibility();

	/** Drop the cache (a re-imported table, the editor's PIE start). The next lookup reloads DT_UITokens. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Tokens")
	static void Reload();

	/** True once the table has been found and read (tests, startup checks). */
	static bool IsLoaded();
};
