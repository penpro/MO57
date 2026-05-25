/**
 * MOHUDRootWidget.cpp - Composite HUD root container
 */

#include "MOHUDRootWidget.h"
#include "MOFramework.h"
#include "MOThermalComfortWidget.h"
#include "MOStatusEffectStripWidget.h"
#include "MOWindDirectionWidget.h"
#include "Blueprint/WidgetTree.h"

UMOHUDRootWidget::UMOHUDRootWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// HUD is the root surface — nothing exists behind it. Clicks landing on
	// "empty" parts of the HUD shouldn't trigger any kind of dismiss path.
	bClosesOnOutsideClick = false;

	// Always-visible passive container — don't take focus, don't be in the
	// tab cycle. Sub-widgets (e.g. button-bearing indicators) handle their
	// own focus needs.
	SetIsFocusable(false);
}

TOptional<FUIInputConfig> UMOHUDRootWidget::GetDesiredInputConfig() const
{
	// Empty config — defers to whatever activatable widget sits on a higher
	// layer (Game / Menu / Modal). Without this, the HUD's mere presence on
	// Layer_HUD would force Game-mode input even when a menu is open.
	return TOptional<FUIInputConfig>();
}

void UMOHUDRootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Two pieces of info, so we can tell the failure modes apart:
	//   1. BindWidgetOptional pointer — null = "child isn't a named variable
	//      in WBP_HUDRoot's tree" (either not placed, OR placed without
	//      'Is Variable' ticked, OR placed with wrong type)
	//   2. Walk WidgetTree for any UMOThermalComfortWidget descendant — finds
	//      the widget regardless of variable status. Null here = "no widget
	//      of that class exists in the tree at all" (definitely not placed,
	//      or placed with wrong parent class)
	UMOThermalComfortWidget* FoundInTree = nullptr;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&FoundInTree](UWidget* W)
		{
			if (!FoundInTree)
			{
				if (UMOThermalComfortWidget* Casted = Cast<UMOThermalComfortWidget>(W))
				{
					FoundInTree = Casted;
				}
			}
		});
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot] NativeOnInitialized — children:"));
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot]   ThermalComfortIndicator: BindWidget=%s  TreeDescendant=%s"),
		ThermalComfortIndicator ? *ThermalComfortIndicator->GetName() : TEXT("NULL"),
		FoundInTree ? *FoundInTree->GetName() : TEXT("NULL"));
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot]   StatusStrip: BindWidget=%s"),
		StatusStrip ? *StatusStrip->GetName() : TEXT("NULL (add WBP_StatusEffectStrip to WBP_HUDRoot, name it 'StatusStrip', tick 'Is Variable')"));
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot]   WindIndicator: BindWidget=%s"),
		WindIndicator ? *WindIndicator->GetName() : TEXT("NULL (add WBP_WindDirection to WBP_HUDRoot, name it 'WindIndicator', tick 'Is Variable')"));

	// Self-heal: if the WBP designer placed the widget but forgot to tick
	// "Is Variable", the BindWidgetOptional pointer is null but the tree
	// has it. Wire it up so the rest of C++ can use ThermalComfortIndicator.
	// (Logs above keep the diagnostic visible so the designer still knows
	// to fix the WBP.)
	if (!ThermalComfortIndicator && FoundInTree)
	{
		ThermalComfortIndicator = FoundInTree;
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOHUDRoot]   ↳ Auto-wired ThermalComfortIndicator from tree (consider ticking 'Is Variable' on the widget in WBP_HUDRoot)"));
	}
}
