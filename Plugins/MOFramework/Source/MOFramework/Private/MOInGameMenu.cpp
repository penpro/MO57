#include "MOInGameMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSavePanel.h"
#include "MOLoadPanel.h"
#include "MOOptionsPanel.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"

void UMOInGameMenu::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonEvents();

	// Start with no focus panel open
	if (FocusWindowSwitcher)
	{
		FocusWindowSwitcher->SetActiveWidgetIndex(PanelIndex_None);
	}
	CurrentPanelIndex = PanelIndex_None;
}

UWidget* UMOInGameMenu::NativeGetDesiredFocusTarget() const
{
	// If a focus panel is open, let it handle focus
	if (CurrentPanelIndex != PanelIndex_None && FocusWindowSwitcher)
	{
		UWidget* ActiveWidget = FocusWindowSwitcher->GetActiveWidget();
		if (ActiveWidget)
		{
			return ActiveWidget;
		}
	}

	// Otherwise focus the first button
	if (OptionsButton)
	{
		return OptionsButton;
	}

	return nullptr;
}

FReply UMOInGameMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Tab or Escape closes focus panel first, then menu
	if (InKeyEvent.GetKey() == EKeys::Tab || InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (IsFocusPanelOpen())
		{
			CloseFocusPanel();
			return FReply::Handled();
		}
		else
		{
			RequestClose();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMOInGameMenu::RequestClose()
{
	OnRequestClose.Broadcast();
}

void UMOInGameMenu::ShowOptionsPanel()
{
	SwitchToPanel(PanelIndex_Options);
}

void UMOInGameMenu::ShowSavePanel()
{
	SwitchToPanel(PanelIndex_Save);

	// Refresh save list when opening
	if (SavePanel)
	{
		SavePanel->RefreshSaveList();
	}
}

void UMOInGameMenu::ShowLoadPanel()
{
	SwitchToPanel(PanelIndex_Load);

	// Refresh load list when opening
	if (LoadPanel)
	{
		LoadPanel->RefreshSaveList();
	}
}

void UMOInGameMenu::CloseFocusPanel()
{
	SwitchToPanel(PanelIndex_None);

	// Return focus to button list
	if (OptionsButton)
	{
		OptionsButton->SetFocus();
	}
}

bool UMOInGameMenu::IsFocusPanelOpen() const
{
	return CurrentPanelIndex != PanelIndex_None;
}

void UMOInGameMenu::BindButtonEvents()
{
	if (OptionsButton)
	{
		OptionsButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleOptionsClicked);
	}
	if (SaveButton)
	{
		SaveButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleSaveClicked);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleLoadClicked);
	}
	if (ExitToMainMenuButton)
	{
		ExitToMainMenuButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleExitToMainMenuClicked);
	}
	if (ExitGameButton)
	{
		ExitGameButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleExitGameClicked);
	}

	// Bind panel close requests
	if (OptionsPanel)
	{
		OptionsPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
	}
	if (SavePanel)
	{
		SavePanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
	}
	if (LoadPanel)
	{
		LoadPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
	}
}

void UMOInGameMenu::SwitchToPanel(int32 PanelIndex)
{
	if (FocusWindowSwitcher)
	{
		FocusWindowSwitcher->SetActiveWidgetIndex(PanelIndex);
	}
	CurrentPanelIndex = PanelIndex;
}

void UMOInGameMenu::HandleOptionsClicked()
{
	ShowOptionsPanel();
}

void UMOInGameMenu::HandleSaveClicked()
{
	ShowSavePanel();
}

void UMOInGameMenu::HandleLoadClicked()
{
	ShowLoadPanel();
}

void UMOInGameMenu::HandleExitToMainMenuClicked()
{
	// This should show a confirmation dialog first
	// For now, broadcast directly - UIManager will handle confirmation
	OnExitToMainMenu.Broadcast();
}

void UMOInGameMenu::HandleExitGameClicked()
{
	// This should show a confirmation dialog first
	// For now, broadcast directly - UIManager will handle confirmation
	OnExitGame.Broadcast();
}

void UMOInGameMenu::HandlePanelRequestClose()
{
	CloseFocusPanel();
}
