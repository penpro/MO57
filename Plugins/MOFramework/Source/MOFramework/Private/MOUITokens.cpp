#include "MOUITokens.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"
#include "MOFramework.h"

// ============================================================================
// FMOUITokenSet
// ============================================================================

bool FMOUITokenSet::ParseColor(const FString& Hex, FLinearColor& Out)
{
	FString S = Hex.TrimStartAndEnd();
	if (S.StartsWith(TEXT("#")))
	{
		S.RightChopInline(1);
	}
	if (S.Len() != 6 && S.Len() != 8)
	{
		return false;
	}
	for (const TCHAR C : S)
	{
		if (!FChar::IsHexDigit(C))
		{
			return false;
		}
	}
	Out = FLinearColor::FromSRGBColor(FColor::FromHex(S));   // RGB are sRGB; alpha stays linear
	return true;
}

bool FMOUITokenSet::ParseMargin(const FString& Text, FVector4& Out)
{
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() != 4)
	{
		return false;
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FString P = Parts[i].TrimStartAndEnd();
		if (!P.IsNumeric())
		{
			return false;
		}
		Out[i] = FCString::Atof(*P);
	}
	return true;
}

TArray<FString> FMOUITokenSet::FontFaceKeys() const
{
	TArray<FString> Keys;
	const FString Prefix = TEXT("font.face.");
	for (const TPair<FName, FString>& Pair : Values)
	{
		const FString Name = Pair.Key.ToString();
		if (!Name.StartsWith(Prefix))
		{
			continue;
		}
		const FString Rest = Name.RightChop(Prefix.Len());   // "<Family>.<Weight>"
		FString Family, Weight;
		if (Rest.Split(TEXT("."), &Family, &Weight, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && Weight.IsNumeric())
		{
			Keys.Add(Family + TEXT("|") + Weight);
		}
	}
	return Keys;
}

FString FMOUITokenSet::PickFontKey(const TArray<FString>& Keys, const FString& Family, int32 Weight)
{
	FString Best;
	int32 BestDelta = MAX_int32;
	int32 BestWeight = 0;
	for (const FString& Key : Keys)
	{
		FString KeyFamily, KeyWeight;
		if (!Key.Split(TEXT("|"), &KeyFamily, &KeyWeight) || KeyFamily != Family || !KeyWeight.IsNumeric())
		{
			continue;
		}
		const int32 W = FCString::Atoi(*KeyWeight);
		const int32 Delta = FMath::Abs(W - Weight);
		if (Delta < BestDelta || (Delta == BestDelta && W > BestWeight))
		{
			Best = Key;
			BestDelta = Delta;
			BestWeight = W;
		}
	}
	return Best;
}

bool FMOUITokenSet::IsScaledByTextScale(const FString& Token)
{
	if (Token.StartsWith(TEXT("type.")))
	{
		return Token.EndsWith(TEXT(".size")) || Token.EndsWith(TEXT(".umgSize")) || Token.EndsWith(TEXT(".lineHeight"));
	}
	return Token.StartsWith(TEXT("control.height.")) || Token == TEXT("row.height") || Token == TEXT("row.height.dense");
}

bool FMOUITokenSet::Resolve(FName Token, const FMOUIAccessibility& A, FString& Out, int32 Depth) const
{
	if (Depth > 4)
	{
		return false;   // a reference loop
	}
	FString Name = Token.ToString();

	if (A.bColorBlind && Name.StartsWith(TEXT("color.severity.")))
	{
		if (const FString* Swapped = Values.Find(FName(*(FString(TEXT("colorblind.")) + Name))))
		{
			Out = *Swapped;
			return true;
		}
	}
	if (A.bHighContrast)
	{
		if (Name == TEXT("opacity.hud.plate"))
		{
			Out = TEXT("0.95");
			return true;
		}
		if (Name == TEXT("color.text.secondary"))
		{
			Name = TEXT("color.text.primary");
		}
	}
	if (A.bReducedMotion && Name.StartsWith(TEXT("motion.")) && Name != TEXT("motion.fast"))
	{
		const FString* Raw = Values.Find(FName(*Name));
		if (Raw && Raw->IsNumeric())
		{
			Out = TEXT("0");   // motion.base / slow / pulse.critical (static double border) / instant
			return true;
		}
	}

	const FString* Value = Values.Find(FName(*Name));
	if (!Value)
	{
		return false;
	}
	if (Values.Contains(FName(**Value)))
	{
		return Resolve(FName(**Value), A, Out, Depth + 1);   // `text.world.outline.color = color.hud.outline`
	}
	Out = *Value;
	return true;
}

FLinearColor FMOUITokenSet::Color(FName Token, const FMOUIAccessibility& A, bool* bFound) const
{
	FString Raw;
	FLinearColor Result = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);   // loud: a missing token must be seen
	const bool bOk = Resolve(Token, A, Raw) && ParseColor(Raw, Result);
	if (bFound)
	{
		*bFound = bOk;
	}
	return Result;
}

float FMOUITokenSet::Number(FName Token, const FMOUIAccessibility& A, float Default, bool* bFound) const
{
	FString Raw;
	const bool bOk = Resolve(Token, A, Raw) && Raw.IsNumeric();
	if (bFound)
	{
		*bFound = bOk;
	}
	if (!bOk)
	{
		return Default;
	}
	float V = FCString::Atof(*Raw);
	if (IsScaledByTextScale(Token.ToString()))
	{
		V *= A.TextScale;
	}
	return V;
}

FString FMOUITokenSet::String(FName Token, const FMOUIAccessibility& A) const
{
	FString Raw;
	return Resolve(Token, A, Raw) ? Raw : FString();
}

// ============================================================================
// UMOUITokens (the runtime set, loaded from DT_UITokens)
// ============================================================================

namespace
{
	FMOUITokenSet GUITokenSet;
	FMOUIAccessibility GUITokenAccess;
	bool GUITokensLoaded = false;
	bool GUITokensWarnedNoTable = false;
	TSet<FName> GUITokensWarnedMissing;

	void EnsureUITokensLoaded()
	{
		if (GUITokensLoaded)
		{
			return;
		}
		const UMOUIThemeSettings* Settings = GetDefault<UMOUIThemeSettings>();
		UDataTable* Table = Settings ? Cast<UDataTable>(Settings->TokenTable.TryLoad()) : nullptr;
		if (!Table)
		{
			if (!GUITokensWarnedNoTable)
			{
				GUITokensWarnedNoTable = true;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOUITokens] no token table at '%s' (Project Settings > MO UI Theme): every token will read as the loud fallback"), Settings ? *Settings->TokenTable.ToString() : TEXT("?"));
			}
			return;
		}
		TMap<FName, FString> Map;
		Table->ForeachRow<FMOUITokenRow>(TEXT("MOUITokens"), [&Map](const FName& Name, const FMOUITokenRow& Row) { Map.Add(Name, Row.Value); });
		GUITokenSet.Set(Map);
		GUITokensLoaded = Map.Num() > 0;
		UE_LOG(LogMOFramework, Log, TEXT("[MOUITokens] loaded %d tokens from %s"), Map.Num(), *Table->GetPathName());
	}

	void WarnUITokenMissing(FName Token)
	{
		if (!GUITokensWarnedMissing.Contains(Token))
		{
			GUITokensWarnedMissing.Add(Token);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOUITokens] token '%s' is missing or has the wrong kind"), *Token.ToString());
		}
	}
}

FLinearColor UMOUITokens::Color(FName Token)
{
	EnsureUITokensLoaded();
	bool bFound = false;
	const FLinearColor C = GUITokenSet.Color(Token, GUITokenAccess, &bFound);
	if (!bFound)
	{
		WarnUITokenMissing(Token);
	}
	return C;
}

FSlateColor UMOUITokens::SlateColor(FName Token)
{
	return FSlateColor(Color(Token));
}

float UMOUITokens::Number(FName Token, float Default)
{
	EnsureUITokensLoaded();
	bool bFound = false;
	const float V = GUITokenSet.Number(Token, GUITokenAccess, Default, &bFound);
	if (!bFound)
	{
		WarnUITokenMissing(Token);
	}
	return V;
}

FString UMOUITokens::String(FName Token)
{
	EnsureUITokensLoaded();
	return GUITokenSet.String(Token, GUITokenAccess);
}

float UMOUITokens::Seconds(FName Token)
{
	return Number(Token, 0.0f) * 0.001f;
}

FSlateFontInfo UMOUITokens::Font(FName TypeToken)
{
	EnsureUITokensLoaded();
	const FString Base = FString(TEXT("type.")) + TypeToken.ToString();
	const FString Family = String(FName(*(Base + TEXT(".font"))));
	const int32 Weight = FMath::RoundToInt(Number(FName(*(Base + TEXT(".weight"))), 400.0f));
	const int32 Size = FMath::Max(1, FMath::RoundToInt(Number(FName(*(Base + TEXT(".umgSize"))), 14.0f)));

	UObject* FontObject = nullptr;
	FName Typeface = NAME_None;
	{
		const FString FamilyKey = Family.Replace(TEXT(" "), TEXT(""));
		const FString Key = FMOUITokenSet::PickFontKey(GUITokenSet.FontFaceKeys(), FamilyKey, Weight);   // the nearest weight this family has a face for
		FString KeyFamily, KeyWeight;
		if (Key.Split(TEXT("|"), &KeyFamily, &KeyWeight))
		{
			// one composite Font asset per family (typefaces named W<weight>), built by Content/Python/kit_import.py; the token names it
			const FString Path = GUITokenSet.String(FName(*FString::Printf(TEXT("font.asset.%s"), *KeyFamily)), GUITokenAccess);
			FontObject = Path.IsEmpty() ? nullptr : StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
			Typeface = FName(*(FString(TEXT("W")) + KeyWeight));
		}
	}
	FSlateFontInfo Info = FontObject ? FSlateFontInfo(FontObject, Size, Typeface) : FCoreStyle::GetDefaultFontStyle(Weight >= 600 ? FName("Bold") : FName("Regular"), Size);
	Info.LetterSpacing = FMath::RoundToInt(Number(FName(*(Base + TEXT(".letterSpacingUMG"))), 0.0f));
	return Info;
}

UTexture2D* UMOUITokens::Icon(FName IconName)
{
	const FString Path = FString::Printf(TEXT("/MOFramework/UI/Kit/Icons/T_%s.T_%s"), *IconName.ToString(), *IconName.ToString());
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn);
	if (!Tex)
	{
		WarnUITokenMissing(FName(*(FString(TEXT("icon:")) + IconName.ToString())));
	}
	return Tex;
}

void UMOUITokens::GetKitTexturePaths(TArray<FSoftObjectPath>& OutPaths)
{
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	for (const TCHAR* Folder : { TEXT("/MOFramework/UI/Kit/Icons"), TEXT("/MOFramework/UI/Kit/NineSlice") })
	{
		Assets.Reset();
		Registry.GetAssetsByPath(FName(Folder), Assets, /*bRecursive*/ false);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
			{
				OutPaths.Add(Asset.GetSoftObjectPath());
			}
		}
	}
}

UTexture2D* UMOUITokens::NineSlice(FName Piece)
{
	const FString Path = FString::Printf(TEXT("/MOFramework/UI/Kit/NineSlice/T_UI_%s.T_UI_%s"), *Piece.ToString(), *Piece.ToString());
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn);
	if (!Tex)
	{
		WarnUITokenMissing(FName(*(FString(TEXT("nineslice:")) + Piece.ToString())));
	}
	return Tex;
}

FMargin UMOUITokens::MarginToken(FName Token)
{
	EnsureUITokensLoaded();
	FVector4 V(0, 0, 0, 0);
	if (!FMOUITokenSet::ParseMargin(GUITokenSet.String(Token, GUITokenAccess), V))
	{
		WarnUITokenMissing(Token);
	}
	return FMargin(V.X, V.Y, V.Z, V.W);
}

FSlateBrush UMOUITokens::NineSliceBrush(FName Piece, FName ColorToken)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(NineSlice(Piece));
	const FString Draw = String(FName(*FString::Printf(TEXT("nineslice.%s.draw"), *Piece.ToString())));
	Brush.DrawAs = (Draw == TEXT("Image") || Draw == TEXT("Tile")) ? ESlateBrushDrawType::Image : ESlateBrushDrawType::Box;
	if (Draw == TEXT("Tile"))
	{
		Brush.Tiling = ESlateBrushTileType::Horizontal;
	}
	Brush.Margin = MarginToken(FName(*FString::Printf(TEXT("nineslice.%s.margin"), *Piece.ToString())));
	Brush.TintColor = ColorToken.IsNone() ? FSlateColor(FLinearColor::White) : FSlateColor(Color(ColorToken));
	return Brush;
}

void UMOUITokens::SetAccessibility(bool bColorBlind, bool bHighContrast, bool bReducedMotion, float TextScale)
{
	GUITokenAccess.bColorBlind = bColorBlind;
	GUITokenAccess.bHighContrast = bHighContrast;
	GUITokenAccess.bReducedMotion = bReducedMotion;
	GUITokenAccess.TextScale = FMath::Clamp(TextScale, 1.0f, 1.5f);
}

const FMOUIAccessibility& UMOUITokens::GetAccessibility()
{
	return GUITokenAccess;
}

void UMOUITokens::Reload()
{
	GUITokensLoaded = false;
	GUITokensWarnedNoTable = false;
	GUITokensWarnedMissing.Reset();
	GUITokenSet.Set(TMap<FName, FString>());
}

bool UMOUITokens::IsLoaded()
{
	EnsureUITokensLoaded();
	return GUITokensLoaded;
}
