#include "MOOptionsPanel.h"
#include "MOFramework.h"
#include "CommonButtonBase.h"

void UMOOptionsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (ApplyButton)
	{
		ApplyButton->OnClicked().RemoveAll(this);
		ApplyButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleApplyClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked().RemoveAll(this);
		ResetButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleResetClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOOptionsPanel::HandleBackClicked);
	}

	OnRefreshSettings();
}

UWidget* UMOOptionsPanel::NativeGetDesiredFocusTarget() const
{
	if (BackButton)
	{
		return BackButton;
	}
	return nullptr;
}

void UMOOptionsPanel::ApplySettings()
{
	// Base implementation - override in Blueprint for actual settings application
	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] ApplySettings called - override in Blueprint to implement"));
}

void UMOOptionsPanel::ResetToDefaults()
{
	// Base implementation - override in Blueprint for actual reset
	UE_LOG(LogMOFramework, Log, TEXT("[MOOptionsPanel] ResetToDefaults called - override in Blueprint to implement"));
	OnRefreshSettings();
}

void UMOOptionsPanel::HandleApplyClicked()
{
	ApplySettings();
}

void UMOOptionsPanel::HandleResetClicked()
{
	ResetToDefaults();
}

void UMOOptionsPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}
