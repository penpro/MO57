/**
 * MOWidgetEditorUtils.cpp - Editor utilities for widget blueprint manipulation
 */

#include "MOWidgetEditorUtils.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif

// Anchor presets lookup
namespace
{
	struct FAnchorPreset
	{
		FVector2D Min;
		FVector2D Max;
	};

	TMap<FString, FAnchorPreset> GetAnchorPresets()
	{
		TMap<FString, FAnchorPreset> Presets;
		Presets.Add(TEXT("TopLeft"),      {{0.0f, 0.0f}, {0.0f, 0.0f}});
		Presets.Add(TEXT("TopCenter"),    {{0.5f, 0.0f}, {0.5f, 0.0f}});
		Presets.Add(TEXT("TopRight"),     {{1.0f, 0.0f}, {1.0f, 0.0f}});
		Presets.Add(TEXT("CenterLeft"),   {{0.0f, 0.5f}, {0.0f, 0.5f}});
		Presets.Add(TEXT("Center"),       {{0.5f, 0.5f}, {0.5f, 0.5f}});
		Presets.Add(TEXT("CenterRight"),  {{1.0f, 0.5f}, {1.0f, 0.5f}});
		Presets.Add(TEXT("BottomLeft"),   {{0.0f, 1.0f}, {0.0f, 1.0f}});
		Presets.Add(TEXT("BottomCenter"), {{0.5f, 1.0f}, {0.5f, 1.0f}});
		Presets.Add(TEXT("BottomRight"),  {{1.0f, 1.0f}, {1.0f, 1.0f}});
		Presets.Add(TEXT("TopStretch"),   {{0.0f, 0.0f}, {1.0f, 0.0f}});
		Presets.Add(TEXT("BottomStretch"),{{0.0f, 1.0f}, {1.0f, 1.0f}});
		Presets.Add(TEXT("LeftStretch"),  {{0.0f, 0.0f}, {0.0f, 1.0f}});
		Presets.Add(TEXT("RightStretch"), {{1.0f, 0.0f}, {1.0f, 1.0f}});
		Presets.Add(TEXT("FullScreen"),   {{0.0f, 0.0f}, {1.0f, 1.0f}});
		return Presets;
	}
}

bool UMOWidgetEditorUtils::SetWidgetIsVariable(UWidget* Widget, bool bIsVariable)
{
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::SetWidgetIsVariable - Widget is null"));
		return false;
	}

#if WITH_EDITOR
	Widget->Modify();
	Widget->bIsVariable = bIsVariable;
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::SetWidgetIsVariable - Only available in editor builds"));
	return false;
#endif
}

bool UMOWidgetEditorUtils::GetWidgetIsVariable(UWidget* Widget)
{
	if (!Widget)
	{
		return false;
	}

#if WITH_EDITOR
	return Widget->bIsVariable;
#else
	return false;
#endif
}

bool UMOWidgetEditorUtils::SetWidgetIsVariableByName(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, bool bIsVariable)
{
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::SetWidgetIsVariableByName - WidgetBlueprint is null"));
		return false;
	}

#if WITH_EDITOR
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::SetWidgetIsVariableByName - WidgetTree is null"));
		return false;
	}

	UWidget* Widget = WidgetTree->FindWidget(WidgetName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::SetWidgetIsVariableByName - Widget '%s' not found"), *WidgetName.ToString());
		return false;
	}

	Widget->Modify();
	Widget->bIsVariable = bIsVariable;

	// Notify the blueprint system of structural changes
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::SetWidgetIsVariableByName - Only available in editor builds"));
	return false;
#endif
}

int32 UMOWidgetEditorUtils::BatchSetWidgetsAsVariables(UWidgetBlueprint* WidgetBlueprint, const TArray<FName>& WidgetNames, bool bIsVariable)
{
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::BatchSetWidgetsAsVariables - WidgetBlueprint is null"));
		return 0;
	}

#if WITH_EDITOR
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::BatchSetWidgetsAsVariables - WidgetTree is null"));
		return 0;
	}

	int32 ModifiedCount = 0;

	for (const FName& WidgetName : WidgetNames)
	{
		UWidget* Widget = WidgetTree->FindWidget(WidgetName);
		if (Widget)
		{
			Widget->Modify();
			Widget->bIsVariable = bIsVariable;
			ModifiedCount++;
			UE_LOG(LogTemp, Log, TEXT("MOWidgetEditorUtils: Set '%s' bIsVariable = %s"),
				*WidgetName.ToString(), bIsVariable ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils: Widget '%s' not found"), *WidgetName.ToString());
		}
	}

	if (ModifiedCount > 0)
	{
		// Notify the blueprint system of structural changes
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}

	return ModifiedCount;
#else
	UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::BatchSetWidgetsAsVariables - Only available in editor builds"));
	return 0;
#endif
}

namespace
{
	FString GetAlignmentString(EHorizontalAlignment Alignment)
	{
		switch (Alignment)
		{
		case HAlign_Left: return TEXT("Left");
		case HAlign_Center: return TEXT("Center");
		case HAlign_Right: return TEXT("Right");
		case HAlign_Fill: return TEXT("Fill");
		default: return TEXT("Unknown");
		}
	}

	FString GetAlignmentString(EVerticalAlignment Alignment)
	{
		switch (Alignment)
		{
		case VAlign_Top: return TEXT("Top");
		case VAlign_Center: return TEXT("Center");
		case VAlign_Bottom: return TEXT("Bottom");
		case VAlign_Fill: return TEXT("Fill");
		default: return TEXT("Unknown");
		}
	}

	void ExtractSlotInfo(UWidget* Widget, FMOWidgetLayoutInfo& OutInfo)
	{
		UPanelSlot* Slot = Widget->Slot;
		if (!Slot)
		{
			return;
		}

		OutInfo.SlotType = Slot->GetClass()->GetName();

		// Canvas Panel Slot
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			FAnchors Anchors = CanvasSlot->GetAnchors();
			OutInfo.AnchorMinX = Anchors.Minimum.X;
			OutInfo.AnchorMinY = Anchors.Minimum.Y;
			OutInfo.AnchorMaxX = Anchors.Maximum.X;
			OutInfo.AnchorMaxY = Anchors.Maximum.Y;

			FVector2D Position = CanvasSlot->GetPosition();
			FVector2D Size = CanvasSlot->GetSize();
			OutInfo.OffsetLeft = Position.X;
			OutInfo.OffsetTop = Position.Y;
			OutInfo.OffsetRight = Size.X;
			OutInfo.OffsetBottom = Size.Y;

			OutInfo.AlignmentX = CanvasSlot->GetAlignment().X;
			OutInfo.AlignmentY = CanvasSlot->GetAlignment().Y;
			OutInfo.ZOrder = CanvasSlot->GetZOrder();
		}
		// Horizontal Box Slot
		else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			OutInfo.HorizontalAlignment = GetAlignmentString(HBoxSlot->GetHorizontalAlignment());
			OutInfo.VerticalAlignment = GetAlignmentString(HBoxSlot->GetVerticalAlignment());
			FMargin Padding = HBoxSlot->GetPadding();
			OutInfo.PaddingLeft = Padding.Left;
			OutInfo.PaddingTop = Padding.Top;
			OutInfo.PaddingRight = Padding.Right;
			OutInfo.PaddingBottom = Padding.Bottom;
		}
		// Vertical Box Slot
		else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			OutInfo.HorizontalAlignment = GetAlignmentString(VBoxSlot->GetHorizontalAlignment());
			OutInfo.VerticalAlignment = GetAlignmentString(VBoxSlot->GetVerticalAlignment());
			FMargin Padding = VBoxSlot->GetPadding();
			OutInfo.PaddingLeft = Padding.Left;
			OutInfo.PaddingTop = Padding.Top;
			OutInfo.PaddingRight = Padding.Right;
			OutInfo.PaddingBottom = Padding.Bottom;
		}
		// Overlay Slot
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
		{
			OutInfo.HorizontalAlignment = GetAlignmentString(OverlaySlot->GetHorizontalAlignment());
			OutInfo.VerticalAlignment = GetAlignmentString(OverlaySlot->GetVerticalAlignment());
			FMargin Padding = OverlaySlot->GetPadding();
			OutInfo.PaddingLeft = Padding.Left;
			OutInfo.PaddingTop = Padding.Top;
			OutInfo.PaddingRight = Padding.Right;
			OutInfo.PaddingBottom = Padding.Bottom;
		}
	}

	void CollectWidgetInfo(UWidget* Widget, const FString& ParentName, int32 Depth, TArray<FMOWidgetLayoutInfo>& OutArray)
	{
		if (!Widget)
		{
			return;
		}

		FMOWidgetLayoutInfo Info;
		Info.Name = Widget->GetName();
		Info.WidgetType = Widget->GetClass()->GetName();
		Info.ParentName = ParentName;
		Info.Depth = Depth;
		Info.bIsVariable = Widget->bIsVariable;

		// Extract slot info
		ExtractSlotInfo(Widget, Info);

		// Check for SizeBox overrides
		if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
		{
			Info.bHasWidthOverride = SizeBox->GetWidthOverride() > 0;
			Info.bHasHeightOverride = SizeBox->GetHeightOverride() > 0;
			if (Info.bHasWidthOverride)
			{
				Info.WidthOverride = SizeBox->GetWidthOverride();
			}
			if (Info.bHasHeightOverride)
			{
				Info.HeightOverride = SizeBox->GetHeightOverride();
			}
		}

		OutArray.Add(Info);

		// Recurse into children
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				UWidget* Child = Panel->GetChildAt(i);
				CollectWidgetInfo(Child, Info.Name, Depth + 1, OutArray);
			}
		}
		else if (UContentWidget* Content = Cast<UContentWidget>(Widget))
		{
			UWidget* Child = Content->GetContent();
			if (Child)
			{
				CollectWidgetInfo(Child, Info.Name, Depth + 1, OutArray);
			}
		}
	}
}

TArray<FMOWidgetLayoutInfo> UMOWidgetEditorUtils::GetAllWidgetLayoutInfo(UWidgetBlueprint* WidgetBlueprint)
{
	TArray<FMOWidgetLayoutInfo> Result;

	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::GetAllWidgetLayoutInfo - WidgetBlueprint is null"));
		return Result;
	}

#if WITH_EDITOR
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::GetAllWidgetLayoutInfo - WidgetTree is null"));
		return Result;
	}

	UWidget* RootWidget = WidgetTree->RootWidget;
	if (RootWidget)
	{
		CollectWidgetInfo(RootWidget, TEXT(""), 0, Result);
	}

	return Result;
#else
	UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils::GetAllWidgetLayoutInfo - Only available in editor builds"));
	return Result;
#endif
}

FString UMOWidgetEditorUtils::GetRootWidgetName(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return FString();
	}

#if WITH_EDITOR
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return WidgetTree->RootWidget->GetName();
	}
#endif

	return FString();
}

FString UMOWidgetEditorUtils::GetRootWidgetType(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return FString();
	}

#if WITH_EDITOR
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return WidgetTree->RootWidget->GetClass()->GetName();
	}
#endif

	return FString();
}

// ========== Widget Modification ==========

bool UMOWidgetEditorUtils::SetWidgetAnchorsPreset(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, const FString& PresetName)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils: Widget '%s' not found"), *WidgetName.ToString());
		return false;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils: Widget '%s' is not in a CanvasPanel"), *WidgetName.ToString());
		return false;
	}

	static TMap<FString, FAnchorPreset> Presets = GetAnchorPresets();
	const FAnchorPreset* Preset = Presets.Find(PresetName);
	if (!Preset)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils: Unknown anchor preset '%s'"), *PresetName);
		return false;
	}

	Widget->Modify();
	FAnchors NewAnchors(Preset->Min.X, Preset->Min.Y, Preset->Max.X, Preset->Max.Y);
	CanvasSlot->SetAnchors(NewAnchors);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return true;
#else
	return false;
#endif
}

bool UMOWidgetEditorUtils::SetWidgetSize(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, float Width, float Height)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	if (!Widget)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		return false;
	}

	Widget->Modify();
	CanvasSlot->SetSize(FVector2D(Width, Height));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return true;
#else
	return false;
#endif
}

bool UMOWidgetEditorUtils::SetWidgetPosition(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, float OffsetX, float OffsetY)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	if (!Widget)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		return false;
	}

	Widget->Modify();
	CanvasSlot->SetPosition(FVector2D(OffsetX, OffsetY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return true;
#else
	return false;
#endif
}

bool UMOWidgetEditorUtils::SetWidgetAlignment(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, float AlignmentX, float AlignmentY)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	if (!Widget)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		return false;
	}

	Widget->Modify();
	CanvasSlot->SetAlignment(FVector2D(AlignmentX, AlignmentY));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return true;
#else
	return false;
#endif
}

bool UMOWidgetEditorUtils::SetWidgetZOrder(UWidgetBlueprint* WidgetBlueprint, FName WidgetName, int32 ZOrder)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	if (!Widget)
	{
		return false;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		return false;
	}

	Widget->Modify();
	CanvasSlot->SetZOrder(ZOrder);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return true;
#else
	return false;
#endif
}

bool UMOWidgetEditorUtils::RenameWidget(UWidgetBlueprint* WidgetBlueprint, FName OldName, FName NewName)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(OldName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils: Widget '%s' not found for rename"), *OldName.ToString());
		return false;
	}

	// Check if new name already exists
	if (WidgetBlueprint->WidgetTree->FindWidget(NewName))
	{
		UE_LOG(LogTemp, Warning, TEXT("MOWidgetEditorUtils: Widget '%s' already exists"), *NewName.ToString());
		return false;
	}

	Widget->Modify();
	Widget->Rename(*NewName.ToString());

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	UE_LOG(LogTemp, Log, TEXT("MOWidgetEditorUtils: Renamed '%s' to '%s'"), *OldName.ToString(), *NewName.ToString());
	return true;
#else
	return false;
#endif
}

// ========== Query Functions ==========

TArray<FString> UMOWidgetEditorUtils::GetWidgetsByType(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetTypeName)
{
	TArray<FString> Result;

#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return Result;
	}

	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetClass()->GetName().Contains(WidgetTypeName))
		{
			Result.Add(Widget->GetName());
		}
	});
#endif

	return Result;
}

TArray<FString> UMOWidgetEditorUtils::GetAllWidgetNames(UWidgetBlueprint* WidgetBlueprint)
{
	TArray<FString> Result;

#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return Result;
	}

	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget)
		{
			Result.Add(Widget->GetName());
		}
	});
#endif

	return Result;
}

bool UMOWidgetEditorUtils::DoesWidgetExist(UWidgetBlueprint* WidgetBlueprint, FName WidgetName)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}
	return WidgetBlueprint->WidgetTree->FindWidget(WidgetName) != nullptr;
#else
	return false;
#endif
}

// ========== Validation ==========

TArray<FMOWidgetValidationIssue> UMOWidgetEditorUtils::ValidateWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
	TArray<FMOWidgetValidationIssue> Issues;

#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		FMOWidgetValidationIssue Issue;
		Issue.Severity = TEXT("Error");
		Issue.Message = TEXT("Invalid widget blueprint or missing widget tree");
		Issues.Add(Issue);
		return Issues;
	}

	// Common BindWidget candidate names
	TSet<FString> BindWidgetCandidates = {
		TEXT("TitleText"), TEXT("MessageText"), TEXT("DescriptionText"),
		TEXT("ConfirmButton"), TEXT("CancelButton"), TEXT("CloseButton"), TEXT("BackButton"),
		TEXT("ProgressBar"), TEXT("TimeRemainingText"), TEXT("ActionNameText"),
		TEXT("ContentScrollBox"), TEXT("IconImage"), TEXT("BackgroundBorder")
	};

	// Naming convention patterns (PascalCase check)
	auto IsPascalCase = [](const FString& Name) -> bool
	{
		if (Name.IsEmpty()) return false;
		// First char should be uppercase
		if (!FChar::IsUpper(Name[0])) return false;
		// Should not have underscores (except _C suffix from generated names)
		if (Name.Contains(TEXT("_")) && !Name.EndsWith(TEXT("_C"))) return false;
		return true;
	};

	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget) return;

		FString WidgetName = Widget->GetName();
		FString WidgetType = Widget->GetClass()->GetName();

		// Check: BindWidget candidates not marked as variable
		if (BindWidgetCandidates.Contains(WidgetName) && !Widget->bIsVariable)
		{
			FMOWidgetValidationIssue Issue;
			Issue.Severity = TEXT("Warning");
			Issue.WidgetName = WidgetName;
			Issue.Message = FString::Printf(TEXT("'%s' is a common BindWidget name but not marked as variable"), *WidgetName);
			Issue.Suggestion = TEXT("Mark as variable in Designer or use BatchSetWidgetsAsVariables");
			Issues.Add(Issue);
		}

		// Check: Naming convention (should be PascalCase)
		if (!IsPascalCase(WidgetName))
		{
			FMOWidgetValidationIssue Issue;
			Issue.Severity = TEXT("Info");
			Issue.WidgetName = WidgetName;
			Issue.Message = FString::Printf(TEXT("'%s' does not follow PascalCase naming convention"), *WidgetName);
			Issue.Suggestion = TEXT("Consider renaming to PascalCase for consistency");
			Issues.Add(Issue);
		}

		// Check: Generic names that should be more specific
		TSet<FString> GenericNames = {TEXT("Text"), TEXT("Button"), TEXT("Image"), TEXT("Border"), TEXT("Panel")};
		if (GenericNames.Contains(WidgetName))
		{
			FMOWidgetValidationIssue Issue;
			Issue.Severity = TEXT("Info");
			Issue.WidgetName = WidgetName;
			Issue.Message = FString::Printf(TEXT("'%s' is a generic name that may cause confusion"), *WidgetName);
			Issue.Suggestion = TEXT("Consider a more descriptive name like 'TitleText' or 'SubmitButton'");
			Issues.Add(Issue);
		}

		// Check: Canvas panel widgets at 0,0 with TopLeft anchor (possibly unpositioned)
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			FVector2D Position = CanvasSlot->GetPosition();
			FAnchors Anchors = CanvasSlot->GetAnchors();

			// Check for default position with non-centered anchor
			if (Position.IsNearlyZero() &&
				Anchors.Minimum.X < 0.1f && Anchors.Minimum.Y < 0.1f &&
				Anchors.Maximum.X < 0.1f && Anchors.Maximum.Y < 0.1f)
			{
				// Might be intentional TopLeft, but flag it
				FMOWidgetValidationIssue Issue;
				Issue.Severity = TEXT("Info");
				Issue.WidgetName = WidgetName;
				Issue.Message = TEXT("Widget at default position (0,0) with TopLeft anchor");
				Issue.Suggestion = TEXT("Verify this is intentional positioning");
				Issues.Add(Issue);
			}
		}
	});

	// Blueprint-level checks
	FString BPName = WidgetBlueprint->GetName();

	// Check: Blueprint naming convention
	if (!BPName.StartsWith(TEXT("WBP_")))
	{
		FMOWidgetValidationIssue Issue;
		Issue.Severity = TEXT("Info");
		Issue.Message = FString::Printf(TEXT("Blueprint '%s' doesn't follow WBP_ prefix convention"), *BPName);
		Issue.Suggestion = TEXT("Consider renaming to WBP_YourWidgetName");
		Issues.Add(Issue);
	}
#endif

	return Issues;
}

// ========== Style Extraction ==========

TArray<FMOWidgetStyleInfo> UMOWidgetEditorUtils::ExtractWidgetStyles(UWidgetBlueprint* WidgetBlueprint)
{
	TArray<FMOWidgetStyleInfo> Styles;

#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return Styles;
	}

	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget) return;

		FMOWidgetStyleInfo StyleInfo;
		StyleInfo.WidgetName = Widget->GetName();
		StyleInfo.WidgetType = Widget->GetClass()->GetName();

		// Extract TextBlock styles
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			StyleInfo.bHasTextStyle = true;
			StyleInfo.TextColor = TextBlock->GetColorAndOpacity().GetSpecifiedColor();

			FSlateFontInfo FontInfo = TextBlock->GetFont();
			StyleInfo.FontSize = FontInfo.Size;
			if (FontInfo.FontObject)
			{
				StyleInfo.FontFamily = FontInfo.FontObject->GetName();
			}
		}

		// Extract Border styles
		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			StyleInfo.bHasBorderStyle = true;
			StyleInfo.BackgroundColor = Border->GetBrushColor();
		}

		// Extract Image styles
		if (UImage* Image = Cast<UImage>(Widget))
		{
			StyleInfo.bHasImageStyle = true;
			StyleInfo.ImageTint = Image->GetColorAndOpacity();
		}

		// Only add if we found style info
		if (StyleInfo.bHasTextStyle || StyleInfo.bHasBorderStyle || StyleInfo.bHasImageStyle)
		{
			Styles.Add(StyleInfo);
		}
	});
#endif

	return Styles;
}

// ========== Batch Operations ==========

int32 UMOWidgetEditorUtils::BatchRenameByPrefix(UWidgetBlueprint* WidgetBlueprint, const FString& OldPrefix, const FString& NewPrefix)
{
	int32 RenamedCount = 0;

#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return 0;
	}

	// Collect widgets to rename (can't modify during iteration)
	TArray<TPair<FName, FName>> RenameList;

	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget) return;

		FString WidgetName = Widget->GetName();
		if (WidgetName.StartsWith(OldPrefix))
		{
			FString NewName = NewPrefix + WidgetName.RightChop(OldPrefix.Len());
			RenameList.Add(TPair<FName, FName>(FName(*WidgetName), FName(*NewName)));
		}
	});

	// Perform renames
	for (const auto& Pair : RenameList)
	{
		if (RenameWidget(WidgetBlueprint, Pair.Key, Pair.Value))
		{
			RenamedCount++;
		}
	}

	if (RenamedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}
#endif

	return RenamedCount;
}

int32 UMOWidgetEditorUtils::BatchSetVariablesByPattern(UWidgetBlueprint* WidgetBlueprint, const FString& NamePattern, bool bIsVariable)
{
	int32 ModifiedCount = 0;

#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return 0;
	}

	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget) return;

		if (Widget->GetName().Contains(NamePattern))
		{
			if (Widget->bIsVariable != bIsVariable)
			{
				Widget->Modify();
				Widget->bIsVariable = bIsVariable;
				ModifiedCount++;
				UE_LOG(LogTemp, Log, TEXT("MOWidgetEditorUtils: Set '%s' bIsVariable = %s (matched '%s')"),
					*Widget->GetName(), bIsVariable ? TEXT("true") : TEXT("false"), *NamePattern);
			}
		}
	});

	if (ModifiedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}
#endif

	return ModifiedCount;
}
