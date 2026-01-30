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

	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] NativeConstruct called"));

	// Try to find panels by type if BindWidgetOptional didn't find them
	if (FocusWindowSwitcher)
	{
		const int32 NumWidgets = FocusWindowSwitcher->GetNumWidgets();
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] FocusWindowSwitcher has %d children"), NumWidgets);

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
				if (UMOSavePanel* FoundSavePanel = Cast<UMOSavePanel>(Widget))
				{
					SavePanel = FoundSavePanel;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] Found SavePanel by type at index %d (name: %s)"), i, *Widget->GetName());
				}
			}
			if (!LoadPanel)
			{
				if (UMOLoadPanel* FoundLoadPanel = Cast<UMOLoadPanel>(Widget))
				{
					LoadPanel = FoundLoadPanel;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] Found LoadPanel by type at index %d (name: %s)"), i, *Widget->GetName());
				}
			}
			if (!OptionsPanel)
			{
				if (UMOOptionsPanel* FoundOptionsPanel = Cast<UMOOptionsPanel>(Widget))
				{
					OptionsPanel = FoundOptionsPanel;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] Found OptionsPanel by type at index %d (name: %s)"), i, *Widget->GetName());
				}
			}
		}
	}

	BindButtonEvents();

	// Start with no focus panel open
	if (FocusWindowSwitcher)
	{
		FocusWindowSwitcher->SetActiveWidgetIndex(PanelIndex_None);

		// Log expected vs actual panel arrangement
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Expected panel order: 0=None/Empty, 1=Options, 2=Save, 3=Load"));
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] Panels found: Options=%s, Save=%s, Load=%s"),
			OptionsPanel ? TEXT("YES") : TEXT("NO"),
			SavePanel ? TEXT("YES") : TEXT("NO"),
			LoadPanel ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] FocusWindowSwitcher is NULL"));
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Save panel list refreshed"));
	}
}

void UMOInGameMenu::RefreshLoadPanelList()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] RefreshLoadPanelList called (LoadPanel: %s)"), LoadPanel ? TEXT("OK") : TEXT("NULL"));
	if (LoadPanel)
	{
		LoadPanel->RefreshSaveList();
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Load panel list refreshed"));
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
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] ShowLoadPanel called (LoadPanel: %s)"), LoadPanel ? TEXT("OK") : TEXT("NULL"));
	SwitchToPanel(PanelIndex_Load);

	// Refresh load list when opening
	if (LoadPanel)
	{
		LoadPanel->RefreshSaveList();
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInGameMenu] LoadPanel is NULL - cannot refresh save list"));
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
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] BindButtonEvents called"));

	// Remove any existing bindings from this object first to prevent duplicates
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
		OptionsButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleOptionsClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] OptionsButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] OptionsButton is NULL - check BindWidget name in WBP"));
	}

	if (SaveButton)
	{
		SaveButton->OnClicked().RemoveAll(this);
		SaveButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleSaveClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] SaveButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] SaveButton is NULL - check BindWidget name in WBP"));
	}

	if (LoadButton)
	{
		LoadButton->OnClicked().RemoveAll(this);
		LoadButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleLoadClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] LoadButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] LoadButton is NULL - check BindWidget name in WBP"));
	}

	if (ExitToMainMenuButton)
	{
		ExitToMainMenuButton->OnClicked().RemoveAll(this);
		ExitToMainMenuButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleExitToMainMenuClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] ExitToMainMenuButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] ExitToMainMenuButton is NULL - check BindWidget name in WBP"));
	}

	if (ExitGameButton)
	{
		ExitGameButton->OnClicked().RemoveAll(this);
		ExitGameButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleExitGameClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] ExitGameButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] ExitGameButton is NULL - check BindWidget name in WBP"));
	}

	// Bind panel close requests
	if (OptionsPanel)
	{
		// Remove any existing bindings first to prevent duplicates
		OptionsPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		OptionsPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] OptionsPanel bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] OptionsPanel is NULL"));
	}

	if (SavePanel)
	{
		// Remove any existing bindings first to prevent duplicates
		SavePanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		SavePanel->OnSaveRequested.RemoveDynamic(this, &UMOInGameMenu::HandleSavePanelSaveRequested);
		// Now add the bindings
		SavePanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		SavePanel->OnSaveRequested.AddDynamic(this, &UMOInGameMenu::HandleSavePanelSaveRequested);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] SavePanel bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInGameMenu] SavePanel is NULL - OnSaveRequested will NOT be received! Check BindWidgetOptional name in WBP."));
	}

	if (LoadPanel)
	{
		// Remove any existing bindings first to prevent duplicates
		LoadPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.RemoveDynamic(this, &UMOInGameMenu::HandleLoadPanelLoadRequested);
		// Now add the bindings
		LoadPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.AddDynamic(this, &UMOInGameMenu::HandleLoadPanelLoadRequested);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] LoadPanel bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInGameMenu] LoadPanel is NULL - OnLoadRequested will NOT be received! Check BindWidgetOptional name in WBP."));
	}
}

void UMOInGameMenu::SwitchToPanel(int32 PanelIndex)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] SwitchToPanel: %d (None=0, Options=1, Save=2, Load=3)"), PanelIndex);

	if (FocusWindowSwitcher)
	{
		const int32 NumWidgets = FocusWindowSwitcher->GetNumWidgets();
		UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Switcher has %d children"), NumWidgets);

		// Log what widget is at each index to help diagnose ordering issues
		for (int32 i = 0; i < NumWidgets; ++i)
		{
			UWidget* Widget = FocusWindowSwitcher->GetWidgetAtIndex(i);
			if (Widget)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu]   Index %d: %s (%s)"), i, *Widget->GetName(), *Widget->GetClass()->GetName());
			}
			else
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu]   Index %d: NULL"), i);
			}
		}

		FocusWindowSwitcher->SetActiveWidgetIndex(PanelIndex);
	}
	CurrentPanelIndex = PanelIndex;
}

void UMOInGameMenu::HandleOptionsClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Options button clicked"));
	ShowOptionsPanel();
}

void UMOInGameMenu::HandleSaveClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Save button clicked"));
	ShowSavePanel();
}

void UMOInGameMenu::HandleLoadClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Load button clicked"));
	ShowLoadPanel();
}

void UMOInGameMenu::HandleExitToMainMenuClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Exit to Main Menu button clicked"));
	OnExitToMainMenu.Broadcast();
}

void UMOInGameMenu::HandleExitGameClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Exit Game button clicked"));
	OnExitGame.Broadcast();
}

void UMOInGameMenu::HandlePanelRequestClose()
{
	CloseFocusPanel();
}

void UMOInGameMenu::HandleSavePanelSaveRequested(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] *** RECEIVED SAVE REQUEST: %s (forwarding delegate bound: %s) ***"),
		*SlotName,
		OnSaveRequested.IsBound() ? TEXT("YES") : TEXT("NO"));
	OnSaveRequested.Broadcast(SlotName);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] Save request forwarded"));
}

void UMOInGameMenu::HandleLoadPanelLoadRequested(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInGameMenu] Load requested for slot: %s"), *SlotName);
	OnLoadRequested.Broadcast(SlotName);
}
