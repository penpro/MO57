#include "MOInGameMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOMainMenuGameMode.h"
#include "MOBugReportPanel.h"
#include "MOCommunitySettings.h"
#include "MONotificationComponent.h"
#include "MOPlayerController.h"
#include "MOSavePanel.h"
#include "MOLoadPanel.h"
#include "MOOptionsPanel.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

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
	if (BugReportButton)
	{
		BugReportButton->OnClicked().RemoveAll(this);
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
	if (BugReportPanel)
	{
		BugReportPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
	}

	Super::NativeDestruct();
}

void UMOInGameMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Construct-entry diagnostic (demoted to Verbose -- routine event, L6-family log hygiene).
	const UWorld* DiagWorld = GetWorld();
	const AGameModeBase* DiagGM = DiagWorld ? DiagWorld->GetAuthGameMode() : nullptr;
	UE_LOG(LogMOFramework, Verbose,
		TEXT("[MOInGameMenu] NativeConstruct '%s'  bClosesOnOutsideClick=%s  World=%s  GameMode=%s (class=%s)"),
		*GetName(),
		bClosesOnOutsideClick ? TEXT("TRUE") : TEXT("false"),
		DiagWorld ? *DiagWorld->GetName() : TEXT("(null)"),
		DiagGM ? *DiagGM->GetName() : TEXT("(null)"),
		DiagGM ? *DiagGM->GetClass()->GetName() : TEXT("(null)"));

	// Context-dependent close policy:
	// - Pause menu in gameplay: click-outside should dismiss (game behind it)
	// - Main menu: nothing behind — click-outside leaves the player with no UI
	if (UWorld* World = GetWorld())
	{
		const AGameModeBase* GM = World->GetAuthGameMode();
		const bool bIsMainMenuWorld = GM && GM->IsA<AMOMainMenuGameMode>();
		if (bIsMainMenuWorld)
		{
			bClosesOnOutsideClick = false;
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOInGameMenu] '%s' detected MAIN-MENU world — bClosesOnOutsideClick now=%s"),
				*GetName(),
				bClosesOnOutsideClick ? TEXT("TRUE") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOInGameMenu] '%s' not in main-menu world — leaving bClosesOnOutsideClick=%s (GM was %s)"),
				*GetName(),
				bClosesOnOutsideClick ? TEXT("TRUE") : TEXT("false"),
				GM ? *GM->GetClass()->GetName() : TEXT("(null)"));
		}
	}

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
			if (!BugReportPanel)
			{
				BugReportPanel = Cast<UMOBugReportPanel>(Widget);
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

bool UMOInGameMenu::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	// Back out of an open focus panel (Save / Load / Options) first.
	if (IsFocusPanelOpen())
	{
		CloseFocusPanel();
		return true;
	}

	// No panel open — close the whole menu (modal base routes RequestClose).
	return Super::NativeOnCloseKeyRequested(InKeyEvent);
}

void UMOInGameMenu::RequestClose()
{
	// Call base class which broadcasts OnRequestClose
	Super::RequestClose();

	// Also broadcast legacy delegate for backward compatibility
	OnLegacyRequestClose.Broadcast();
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

	if (BugReportButton)
	{
		BugReportButton->SetButtonText(NSLOCTEXT("MOInGameMenu", "BugReportButton", "Bug Report"));
		BugReportButton->OnClicked().RemoveAll(this);
		BugReportButton->OnClicked().AddUObject(this, &UMOInGameMenu::HandleBugReportClicked);
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

	if (BugReportPanel)
	{
		BugReportPanel->OnRequestClose.RemoveDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
		BugReportPanel->OnRequestClose.AddDynamic(this, &UMOInGameMenu::HandlePanelRequestClose);
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

void UMOInGameMenu::ShowBugReportPanel()
{
	// The index comes from the widget, never from a constant: the switcher's child order is whatever the designer left it as (the main menu once opened
	// the wrong panel for exactly this reason -- see Docs/UI_TOOLING.md).
	const int32 Index = (BugReportPanel && FocusWindowSwitcher) ? FocusWindowSwitcher->GetChildIndex(BugReportPanel) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInGameMenu] ShowBugReportPanel: no bug report panel in the focus window switcher"));
		return;
	}
	BugReportPanel->PrepareForDisplay(); // fresh status and a new screenshot of the game behind the menu; the typed text is kept
	SwitchToPanel(Index);
}

void UMOInGameMenu::HandleBugReportClicked()
{
	if (BugReportPanel && FocusWindowSwitcher && FocusWindowSwitcher->GetChildIndex(BugReportPanel) != INDEX_NONE)
	{
		ShowBugReportPanel();
		return;
	}

	// This widget has no bug report form (an older WBP_MOInGameMenu): send the player to the community link instead, and say so --
	// a click that does nothing visible reads as a broken button.
	FText Message;
	const bool bOpened = UMOCommunitySettings::OpenBugReportLink(Message);
	if (const AMOPlayerController* PC = Cast<AMOPlayerController>(GetOwningPlayer()))
	{
		if (UMONotificationComponent* Notes = PC->GetNotificationComponent())
		{
			bOpened ? Notes->ShowInfoNotification(Message, 6.0f) : Notes->ShowWarningNotification(Message, 8.0f);
		}
	}
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
