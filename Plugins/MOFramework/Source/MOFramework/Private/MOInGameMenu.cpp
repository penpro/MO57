#include "MOInGameMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSavePanel.h"
#include "MOLoadPanel.h"
#include "MOOptionsPanel.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"

void UMOInGameMenu::NativeDestruct()
{
	// Clean up button bindings to prevent dangling delegates
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
	}
	if (SaveButton)
	{
		SaveButton->OnClicked().RemoveAll(this);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked().RemoveAll(this);
	}
	if (ExitToMainMenuButton)
	{
		ExitToMainMenuButton->OnClicked().RemoveAll(this);
	}
	if (ExitGameButton)
	{
		ExitGameButton->OnClicked().RemoveAll(this);
	}

	// Clean up panel delegate bindings
	if (OptionsPanel)
	{
		OptionsPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
	}
	if (SavePanel)
	{
		SavePanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		SavePanel->OnSaveRequested.RemoveDynamic(this, &UMOInGameMenu::HandleSavePanelSaveRequested);
	}
	if (LoadPanel)
	{
		LoadPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.RemoveDynamic(this, &UMOInGameMenu::HandleLoadPanelLoadRequested);
	}

	Super::NativeDestruct();
}

void UMOInGameMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Try to find panels by type if BindWidgetOptional didn't find them
	if (FocusWindowSwitcher)
	{
		const int32 NumWidgets = FocusWindowSwitcher->GetNumWidgets();

		for (int32 i = 0; i < NumWidgets; ++i)
		{
			UWidget* Widget = FocusWindowSwitcher->GetWidgetAtIndex(i);
			if (!Widget)
			{
				continue;
			}

			// Try to find panels by type if BindWidgetOptional failed
			if (!SavePanel)
			{
				SavePanel = Cast<UMOSavePanel>(Widget);
			}
			if (!LoadPanel)
			{
				LoadPanel = Cast<UMOLoadPanel>(Widget);
			}
			if (!OptionsPanel)
			{
				OptionsPanel = Cast<UMOOptionsPanel>(Widget);
			}
		}
	}

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

void UMOInGameMenu::RefreshSavePanelList()
{
	if (SavePanel)
	{
		SavePanel->RefreshSaveList();
	}
}

void UMOInGameMenu::RefreshLoadPanelList()
{
	if (LoadPanel)
	{
		LoadPanel->RefreshSaveList();
	}
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
	// Remove any existing bindings from this object first to prevent duplicates
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
		OptionsButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleOptionsClicked);
	}

	if (SaveButton)
	{
		SaveButton->OnClicked().RemoveAll(this);
		SaveButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleSaveClicked);
	}

	if (LoadButton)
	{
		LoadButton->OnClicked().RemoveAll(this);
		LoadButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleLoadClicked);
	}

	if (ExitToMainMenuButton)
	{
		ExitToMainMenuButton->OnClicked().RemoveAll(this);
		ExitToMainMenuButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleExitToMainMenuClicked);
	}

	if (ExitGameButton)
	{
		ExitGameButton->OnClicked().RemoveAll(this);
		ExitGameButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleExitGameClicked);
	}

	// Bind panel close requests
	if (OptionsPanel)
	{
		OptionsPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		OptionsPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
	}

	if (SavePanel)
	{
		SavePanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		SavePanel->OnSaveRequested.RemoveDynamic(this, &UMOInGameMenu::HandleSavePanelSaveRequested);
		SavePanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		SavePanel->OnSaveRequested.AddDynamic(this, &UMOInGameMenu::HandleSavePanelSaveRequested);
	}

	if (LoadPanel)
	{
		LoadPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.RemoveDynamic(this, &UMOInGameMenu::HandleLoadPanelLoadRequested);
		LoadPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.AddDynamic(this, &UMOInGameMenu::HandleLoadPanelLoadRequested);
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
	OnExitToMainMenu.Broadcast();
}

void UMOInGameMenu::HandleExitGameClicked()
{
	OnExitGame.Broadcast();
}

void UMOInGameMenu::HandlePanelRequestClose()
{
	CloseFocusPanel();
}

void UMOInGameMenu::HandleSavePanelSaveRequested(const FString& SlotName)
{
	OnSaveRequested.Broadcast(SlotName);
}

void UMOInGameMenu::HandleLoadPanelLoadRequested(const FString& SlotName)
{
	OnLoadRequested.Broadcast(SlotName);
}
