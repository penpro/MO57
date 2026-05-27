/**
 * MOHUDRootWidget.cpp - Composite HUD root container
 */

#include "MOHUDRootWidget.h"
#include "MOFramework.h"
#include "MOStatusEffectStripWidget.h"
#include "MOWindDirectionWidget.h"

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

	// Report which BindWidgetOptional slots resolved — tells designers
	// at a glance whether each WBP child is wired correctly. NULL means
	// either "not placed" or "placed without 'Is Variable' ticked."
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot] NativeOnInitialized — children:"));
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot]   StatusStrip: BindWidget=%s"),
		StatusStrip ? *StatusStrip->GetName() : TEXT("NULL (add WBP_StatusEffectStrip to WBP_HUDRoot, name it 'StatusStrip', tick 'Is Variable')"));
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOHUDRoot]   WindIndicator: BindWidget=%s"),
		WindIndicator ? *WindIndicator->GetName() : TEXT("NULL (add WBP_WindDirection to WBP_HUDRoot, name it 'WindIndicator', tick 'Is Variable')"));
}
