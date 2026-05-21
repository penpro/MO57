/**
 * MOMenuWidgetBase.cpp - Compatibility Layer
 *
 * This class extends UMOMenuWidget for backward compatibility.
 * All functionality is provided by the base class.
 */

#include "MOMenuWidgetBase.h"

UMOMenuWidgetBase::UMOMenuWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// All configuration is handled by UMOMenuWidget base class
}

FReply UMOMenuWidgetBase::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Delegate to base class - UMOMenuWidget handles Escape/Tab
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
