#include "MOModalBackground.h"
#include "Components/Border.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

UMOModalBackground::UMOModalBackground(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

TSharedRef<SWidget> UMOModalBackground::RebuildWidget()
{
	// Full-screen dimming border. HitTestInvisible so clicks pass through to the menu
	// behind/above us. Click-outside-to-close is intentionally disabled because the
	// modal background sits in front of menus and was consuming every menu click.
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.Padding(0)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Visibility(EVisibility::HitTestInvisible);
}

FReply UMOModalBackground::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Disabled — see RebuildWidget comment. Click-outside-to-close should be a
	// per-menu opt-in once CommonUI's layer system is properly handling input routing.
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
