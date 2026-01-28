#include "MOCommonButton.h"

UMOCommonButton::UMOCommonButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOCommonButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Call Blueprint event to update text during design time
	UpdateButtonText(ButtonLabel);
}

void UMOCommonButton::NativeConstruct()
{
	Super::NativeConstruct();
	// Call Blueprint event to update text at runtime
	UpdateButtonText(ButtonLabel);
}

void UMOCommonButton::SetButtonText(const FText& InText)
{
	ButtonLabel = InText;
	UpdateButtonText(ButtonLabel);
}
