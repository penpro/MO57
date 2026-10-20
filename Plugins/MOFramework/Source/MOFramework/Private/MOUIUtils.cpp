#include "MOUIUtils.h"
#include "MOItemActions.h"
#include "MOCommonButton.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/UserWidget.h"
#include "MOUITokens.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Engine/DataTable.h"
#include "MORecipeDefinitionRow.h"
#include "MOItemDatabaseSettings.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"

// Color constants
const FLinearColor UMOUIUtils::ColorAvailable = FLinearColor::White;
const FLinearColor UMOUIUtils::ColorInsufficient = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
const FLinearColor UMOUIUtils::ColorDisabled = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
const FLinearColor UMOUIUtils::ColorTextInput = FLinearColor(0.03f, 0.03f, 0.03f, 1.0f);

namespace
{
	/** The engine's stock field: its brush is a core-style image with no resource object. A kit input well is a project texture. */
	bool IsStockField(const FEditableTextBoxStyle& Style)
	{
		return Style.BackgroundImageNormal.GetResourceObject() == nullptr;
	}

	/** The typed-text colours of a STOCK box style, all set to the dark input colour (shared by the single-line and multi-line boxes). */
	void MakeStyleReadable(FEditableTextBoxStyle& Style)
	{
		const FSlateColor Dark(UMOUIUtils::ColorTextInput);
		Style.SetForegroundColor(Dark);
		Style.SetFocusedForegroundColor(Dark); // the colour the typed text really has while the box is focused (SEditableTextBox::DetermineForegroundColor)
		Style.SetReadOnlyForegroundColor(Dark);
		Style.TextStyle.SetColorAndOpacity(Dark);
	}
}

void UMOUIUtils::ApplyReadableTextInputStyle(UEditableTextBox* TextBox)
{
	if (!TextBox)
	{
		return;
	}

	// Edit the box's own style IN PLACE (font, brushes and padding stay as authored), then re-point the Slate widget at it.
	//
	// NOT UEditableTextBox::SetWidgetStyle(Style): in UE 5.8 it copies the style into the member but hands Slate a pointer to its PARAMETER
	// (MyEditableTextBlock->SetStyle(&InStyle)), i.e. to the caller's local. The pointer dangles as soon as the caller returns and the next
	// text layout reads freed stack -- the packaged game crashed in FCachedTypefaceData the first time the console popup opened.
	if (!IsStockField(TextBox->WidgetStyle))
	{
		return;   // a kit input well: its light-on-dark colours are authored
	}
	MakeStyleReadable(TextBox->WidgetStyle);
	TextBox->SynchronizeProperties(); // SetStyle(&WidgetStyle): a pointer to the member, which lives as long as the widget
}

void UMOUIUtils::ApplyReadableMultiLineTextInputStyle(UMultiLineEditableTextBox* TextBox)
{
	if (!TextBox)
	{
		return;
	}
	// Same rule and in-place edit as above (UMultiLineEditableTextBox::SetTextStyle has the same pointer-to-parameter trap).
	if (!IsStockField(TextBox->WidgetStyle))
	{
		return;
	}
	MakeStyleReadable(TextBox->WidgetStyle);
	TextBox->SynchronizeProperties();
}

FText UMOUIUtils::GetBiomeDisplayNameAt(const UObject* WorldContext, const FVector& Location)
{
	const UMOWorldSeedSubsystem* Seeds = WorldContext ? UMOWorldSeedSubsystem::Get(WorldContext) : nullptr;
	if (!Seeds || !Seeds->HasActiveSeed() || !FMOWorldGenParamsProvider::HasPublished())
	{
		return FText::GetEmpty();
	}
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	const int32 Seed = Seeds->GetActiveTerrainSeed();
	const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, Location.X, Location.Y, Seed, P->Tuning.VoxelSizeCm);
	FMOWorldGen::FBiomeWeight Weights[1];
	const int32 Biome = FMOWorldGen::SampleSurfaceBiomeWeights(*P, Location.X, Location.Y, Seed, S.HeightCm, S.Moisture, S.Temperature, Weights, 1) > 0 ? Weights[0].Biome : S.BiomeIndex;
	const FName Id = P->GetBiomeId(Biome);
	if (Id.IsNone())
	{
		return FText::GetEmpty();
	}
	const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
	const UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
	const FMOBiomeDefinitionRow* Row = Table ? Table->FindRow<FMOBiomeDefinitionRow>(Id, TEXT("MOUIUtils"), /*bWarnIfRowMissing=*/false) : nullptr;
	return Row && !Row->DisplayName.IsEmpty() ? Row->DisplayName : FText::FromString(FName::NameToDisplayString(Id.ToString(), false));
}

FText UMOUIUtils::FormatWorldPosition(const FVector& Location)
{
	auto Axis = [](double Cm, const TCHAR* Positive, const TCHAR* Negative)
	{
		const double Abs = FMath::Abs(Cm);
		const TCHAR* Dir = Cm >= 0.0 ? Positive : Negative;
		return Abs < 100000.0
			? FString::Printf(TEXT("%d m %s"), FMath::RoundToInt(Abs / 100.0), Dir)
			: FString::Printf(TEXT("%.1f km %s"), Abs / 100000.0, Dir);
	};
	return FText::FromString(FString::Printf(TEXT("%s, %s"), *Axis(Location.X, TEXT("N"), TEXT("S")), *Axis(Location.Y, TEXT("E"), TEXT("W"))));
}

FText UMOUIUtils::FormatDurationAsText(float Seconds)
{
	if (Seconds <= 0.0f)
	{
		return NSLOCTEXT("MOUI", "Instant", "Instant");
	}

	if (Seconds >= 86400.0f) // 24 hours = 1 day
	{
		// Days format
		float Days = Seconds / 86400.0f;
		float RoundedDays = FMath::RoundToInt(Days * 10) / 10.0f;
		return FText::Format(NSLOCTEXT("MOUI", "TimeDays", "{0}d"), FText::AsNumber(RoundedDays));
	}
	else if (Seconds >= 3600.0f)
	{
		// Hours and minutes format
		int32 Hours = FMath::FloorToInt(Seconds / 3600.0f);
		int32 Minutes = FMath::FloorToInt(FMath::Fmod(Seconds, 3600.0f) / 60.0f);
		return FText::Format(NSLOCTEXT("MOUI", "TimeHoursMinutes", "{0}h {1}m"), FText::AsNumber(Hours), FText::AsNumber(Minutes));
	}
	else if (Seconds >= 60.0f)
	{
		// Minutes and seconds format
		int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
		int32 Secs = FMath::FloorToInt(FMath::Fmod(Seconds, 60.0f));
		return FText::Format(NSLOCTEXT("MOUI", "TimeMinutesSeconds", "{0}m {1}s"), FText::AsNumber(Minutes), FText::AsNumber(Secs));
	}
	else
	{
		// Seconds only format
		int32 RoundedSeconds = FMath::RoundToInt(Seconds);
		return FText::Format(NSLOCTEXT("MOUI", "TimeSeconds", "{0}s"), FText::AsNumber(RoundedSeconds));
	}
}

FText UMOUIUtils::FormatDurationAsTimeCode(float Seconds)
{
	if (Seconds <= 0.0f)
	{
		return FText::FromString(TEXT("0:00"));
	}

	int32 TotalSeconds = FMath::FloorToInt(Seconds);
	int32 Hours = TotalSeconds / 3600;
	int32 Minutes = (TotalSeconds % 3600) / 60;
	int32 Secs = TotalSeconds % 60;

	if (Hours > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, Secs));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Secs));
	}
}

FText UMOUIUtils::FormatQuantityDisplay(const FText& DisplayName, int32 Available, int32 Required)
{
	return FText::Format(
		NSLOCTEXT("MOUI", "QuantityFormat", "{0} ({1}/{2})"),
		DisplayName,
		FText::AsNumber(Available),
		FText::AsNumber(Required)
	);
}

FText UMOUIUtils::FormatOutputDisplay(const FText& DisplayName, int32 Quantity, float Chance)
{
	if (Chance < 1.0f && Chance > 0.0f)
	{
		// Show chance if not 100%
		return FText::Format(
			NSLOCTEXT("MOUI", "OutputFormatChance", "{0} x{1} ({2}%)"),
			DisplayName,
			FText::AsNumber(Quantity),
			FText::AsNumber(FMath::RoundToInt(Chance * 100))
		);
	}
	else if (Quantity > 1)
	{
		return FText::Format(
			NSLOCTEXT("MOUI", "OutputFormatQty", "{0} x{1}"),
			DisplayName,
			FText::AsNumber(Quantity)
		);
	}
	else
	{
		return DisplayName;
	}
}

FText UMOUIUtils::FormatActionDisplay(const FText& ActionName, int32 Quantity)
{
	return FText::Format(
		NSLOCTEXT("MOUI", "ActionFormat", "{0} x{1}"),
		ActionName,
		FText::AsNumber(Quantity)
	);
}

FText UMOUIUtils::FormatSkillRequirement(FName SkillId, int32 CurrentLevel, int32 RequiredLevel)
{
	return FText::Format(
		NSLOCTEXT("MOUI", "SkillReq", "{0}: {1}/{2}"),
		FText::FromName(SkillId),
		FText::AsNumber(CurrentLevel),
		FText::AsNumber(RequiredLevel)
	);
}

UTextBlock* UMOUIUtils::CreateSimpleTextWidget(UObject* Outer, const FText& Text, bool bHasEnough, int32 FontSize)
{
	if (!Outer)
	{
		return nullptr;
	}

	UTextBlock* TextWidget = NewObject<UTextBlock>(Outer);
	if (!TextWidget)
	{
		return nullptr;
	}

	TextWidget->SetText(Text);
	TextWidget->SetAutoWrapText(true);

	if (UMOUITokens::IsLoaded())
	{
		// Kit look: the body face; an ingredient we have is the ok colour, one we lack is the critical colour in the STRONG face (colour is never the only cue).
		TextWidget->SetFont(UMOUITokens::Font(bHasEnough ? FName(TEXT("body")) : FName(TEXT("body.strong"))));
		TextWidget->SetColorAndOpacity(UMOUITokens::SlateColor(bHasEnough ? FName(TEXT("color.severity.ok")) : FName(TEXT("color.severity.critical"))));
	}
	else
	{
		FSlateFontInfo FontInfo = TextWidget->GetFont();
		FontInfo.Size = FontSize;
		TextWidget->SetFont(FontInfo);
		TextWidget->SetColorAndOpacity(FSlateColor(bHasEnough ? ColorAvailable : ColorInsufficient));
	}

	return TextWidget;
}

UTexture2D* UMOUIUtils::LoadRecipeIcon(const FMORecipeDefinitionRow* Recipe)
{
	if (!Recipe)
	{
		return nullptr;
	}

	// First try recipe's own icon
	if (!Recipe->Icon.IsNull())
	{
		UTexture2D* IconTexture = Recipe->Icon.LoadSynchronous();
		if (IconTexture)
		{
			return IconTexture;
		}
	}

	// Fall back to first output item's icon
	if (Recipe->Outputs.Num() > 0)
	{
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(Recipe->Outputs[0].ItemDefinitionId, ItemDef))
		{
			return UMOItemDatabaseSettings::ResolveItemIcon(ItemDef, /*bLarge=*/false).LoadSynchronous();
		}
	}

	return nullptr;
}

UTexture2D* UMOUIUtils::LoadItemIconSmall(FName ItemDefId)
{
	FMOItemDefinitionRow ItemDef;
	if (UMOItemDatabaseSettings::GetItemDefinition(ItemDefId, ItemDef))
	{
		return UMOItemDatabaseSettings::ResolveItemIcon(ItemDef, /*bLarge=*/false).LoadSynchronous();
	}
	return nullptr;
}

void UMOUIUtils::RebuildActionButtons(UUserWidget* Owner, TArray<TObjectPtr<UMOCommonButton>>& InOutButtons, UPanelWidget* Panel, UWidget* After,
	const TArray<FMOItemActionOption>& Options, UMOCommonButton* StyleFrom, TFunction<void(FName)> OnClicked)
{
	// The same actions already on screen: update them in place, never pull a button out from under the cursor or the gamepad's focus.
	bool bSameActions = InOutButtons.Num() == Options.Num();
	for (int32 i = 0; bSameActions && i < Options.Num(); ++i)
	{
		const UMOCommonButton* Existing = InOutButtons[i];
		bSameActions = Existing && Existing->GetParent() == Panel && Existing->ActionId == Options[i].Id;
	}
	if (bSameActions)
	{
		for (int32 i = 0; i < Options.Num(); ++i)
		{
			UMOCommonButton* Button = InOutButtons[i];
			Button->SetButtonText(Options[i].Label);
			Button->SetIsEnabled(Options[i].bEnabled);
			Button->SetToolTipText(Options[i].bEnabled ? FText::GetEmpty() : Options[i].DisabledReason);
		}
		return;
	}
	for (UMOCommonButton* Button : InOutButtons)
	{
		if (Button)
		{
			Button->OnClicked().Clear();
			Button->RemoveFromParent();
		}
	}
	InOutButtons.Reset();
	static const TSoftClassPtr<UMOCommonButton> ButtonClassPath(FSoftObjectPath(TEXT("/MOFramework/UI/WBP_MOCommonButton.WBP_MOCommonButton_C")));
	UClass* ButtonClass = Owner && Panel && Options.Num() > 0 ? ButtonClassPath.LoadSynchronous() : nullptr;
	if (!ButtonClass)
	{
		return;
	}
	int32 InsertAt = After ? Panel->GetChildIndex(After) + 1 : Panel->GetChildrenCount();
	for (const FMOItemActionOption& Option : Options)
	{
		UMOCommonButton* Button = CreateWidget<UMOCommonButton>(Owner->GetOwningPlayer(), ButtonClass);
		if (!Button)
		{
			continue;
		}
		if (StyleFrom && StyleFrom->GetStyle())
		{
			Button->SetStyle(StyleFrom->GetStyle()->GetClass());   // the menu's own look
		}
		Button->SetButtonText(Option.Label);
		Button->SetIsEnabled(Option.bEnabled);
		if (!Option.bEnabled && !Option.DisabledReason.IsEmpty())
		{
			Button->SetToolTipText(Option.DisabledReason);
		}
		Button->ActionId = Option.Id;
		const FName Id = Option.Id;
		Button->OnClicked().AddWeakLambda(Owner, [OnClicked, Id]() { OnClicked(Id); });
		UPanelSlot* NewSlot = Panel->InsertChildAt(FMath::Clamp(InsertAt++, 0, Panel->GetChildrenCount()), Button);
		// The menu's own spacing too (the kit buttons sit 6 px apart): runtime buttons used to stack flush against each other.
		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(NewSlot))
		{
			if (const UVerticalBoxSlot* From = StyleFrom ? Cast<UVerticalBoxSlot>(StyleFrom->Slot) : nullptr)
			{
				VSlot->SetPadding(From->GetPadding());
			}
		}
		else if (UScrollBoxSlot* SSlot = Cast<UScrollBoxSlot>(NewSlot))
		{
			if (const UScrollBoxSlot* From = StyleFrom ? Cast<UScrollBoxSlot>(StyleFrom->Slot) : nullptr)
			{
				SSlot->SetPadding(From->GetPadding());
			}
		}
		InOutButtons.Add(Button);
	}
}
