#include "MOMainMenuWidget.h"
#include "MOFramework.h"
#include "MOBuildInfo.h"
#include "MOCommonButton.h"
#include "MONewGamePanel.h"
#include "MOLoadPanel.h"
#include "MOOptionsPanel.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

UMOMainMenuWidget::UMOMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);

	// Main menu is the root surface — nothing exists behind it to fall back to,
	// so clicking outside its content shouldn't dismiss it.
	bClosesOnOutsideClick = false;
}

void UMOMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Force false at runtime regardless of WBP class-default override. The
	// C++ constructor sets this to false, but Widget Blueprints can override
	// the value in their Class Defaults panel — and WBP_MOInGameMenu1 was
	// apparently doing exactly that, causing the entire main menu to dismiss
	// on outside-click. This per-instance write at NativeConstruct time
	// runs AFTER BP class defaults are applied to the CDO, so it always wins.
	bClosesOnOutsideClick = false;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOMainMenuWidget] NativeConstruct '%s' — forced bClosesOnOutsideClick=false"),
		*GetName());

	// Try to find panels by type if BindWidgetOptional didn't find them
	if (FocusWindowSwitcher)
	{
		const int32 NumWidgets = FocusWindowSwitcher->GetNumWidgets();
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] FocusWindowSwitcher has %d children"), NumWidgets);

		for (int32 i = 0; i < NumWidgets; ++i)
		{
			UWidget* Widget = FocusWindowSwitcher->GetWidgetAtIndex(i);
			if (!Widget)
			{
				continue;
			}

			// Try to find panels by type if BindWidgetOptional failed
			if (!NewGamePanel)
			{
				if (UMONewGamePanel* FoundNewGamePanel = Cast<UMONewGamePanel>(Widget))
				{
					NewGamePanel = FoundNewGamePanel;
					UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Found NewGamePanel by type at index %d"), i);
				}
			}
			if (!LoadPanel)
			{
				if (UMOLoadPanel* FoundLoadPanel = Cast<UMOLoadPanel>(Widget))
				{
					LoadPanel = FoundLoadPanel;
					UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Found LoadPanel by type at index %d"), i);
				}
			}
			if (!OptionsPanel)
			{
				if (UMOOptionsPanel* FoundOptionsPanel = Cast<UMOOptionsPanel>(Widget))
				{
					OptionsPanel = FoundOptionsPanel;
					UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Found OptionsPanel by type at index %d"), i);
				}
			}
		}
	}

	BindButtonEvents();

	// Populate the build info label (project version + commit hash + branch).
	// Optional widget — designers can omit it from the WBP. UMOBuildInfo
	// handles the "no git data" degradation internally.
	if (BuildInfoLabel)
	{
		BuildInfoLabel->SetText(UMOBuildInfo::GetDisplayLabel());
	}

	// Start with no focus panel open
	if (FocusWindowSwitcher)
	{
		FocusWindowSwitcher->SetActiveWidgetIndex(PanelIndex_None);
	}

	CurrentPanelIndex = PanelIndex_None;

	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Panels found: NewGame=%s, Load=%s, Options=%s"),
		NewGamePanel ? TEXT("YES") : TEXT("NO"),
		LoadPanel ? TEXT("YES") : TEXT("NO"),
		OptionsPanel ? TEXT("YES") : TEXT("NO"));
}

void UMOMainMenuWidget::NativeDestruct()
{
	// Clean up button bindings
	if (NewGameButton)
	{
		NewGameButton->OnClicked().RemoveAll(this);
	}
	if (LoadGameButton)
	{
		LoadGameButton->OnClicked().RemoveAll(this);
	}
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
	}
	if (ExitGameButton)
	{
		ExitGameButton->OnClicked().RemoveAll(this);
	}

	// Clean up panel delegate bindings
	if (NewGamePanel)
	{
		NewGamePanel->OnRequestClose.RemoveDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		NewGamePanel->OnStartGameRequested.RemoveDynamic(this, &UMOMainMenuWidget::HandleNewGamePanelStartRequested);
	}
	if (LoadPanel)
	{
		LoadPanel->OnRequestClose.RemoveDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.RemoveDynamic(this, &UMOMainMenuWidget::HandleLoadPanelLoadRequested);
	}
	if (OptionsPanel)
	{
		OptionsPanel->OnRequestClose.RemoveDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
	}

	Super::NativeDestruct();
}

UWidget* UMOMainMenuWidget::NativeGetDesiredFocusTarget() const
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

	// Otherwise focus the New Game button (first in list)
	if (NewGameButton)
	{
		return NewGameButton;
	}

	return nullptr;
}

bool UMOMainMenuWidget::NativeOnCloseKeyRequested(const FKeyEvent& InKeyEvent)
{
	// Back out of an open focus panel (New Game / Load / Options) first.
	if (IsFocusPanelOpen())
	{
		CloseFocusPanel();
		return true;
	}

	// At the bare title screen the close keys do nothing — there is nothing
	// behind the main menu. Letting the base deactivate it would soft-lock the
	// pawn-less level (FInputModeGameOnly + hidden cursor, no input surface).
	return true;
}

void UMOMainMenuWidget::ShowNewGamePanel()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] ShowNewGamePanel called"));
	SwitchToPanel(PanelIndex_NewGame);

	// Generate a fresh random seed when opening
	if (NewGamePanel)
	{
		NewGamePanel->GenerateRandomSeed();
	}
}

void UMOMainMenuWidget::ShowOptionsPanel()
{
	SwitchToPanel(PanelIndex_Options);
}

void UMOMainMenuWidget::ShowLoadPanel()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] ShowLoadPanel called"));
	SwitchToPanel(PanelIndex_Load);

	// Refresh load list when opening
	if (LoadPanel)
	{
		LoadPanel->RefreshSaveList();
	}
}

void UMOMainMenuWidget::CloseFocusPanel()
{
	SwitchToPanel(PanelIndex_None);

	// Return focus to button list
	if (NewGameButton)
	{
		NewGameButton->SetFocus();
	}
}

bool UMOMainMenuWidget::IsFocusPanelOpen() const
{
	return CurrentPanelIndex != PanelIndex_None;
}

void UMOMainMenuWidget::RefreshLoadPanelList()
{
	if (LoadPanel)
	{
		LoadPanel->RefreshSaveList();
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Load panel list refreshed"));
	}
}

void UMOMainMenuWidget::BindButtonEvents()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] BindButtonEvents called"));

	// New Game button
	if (NewGameButton)
	{
		NewGameButton->OnClicked().RemoveAll(this);
		NewGameButton->OnClicked().AddUObject(this, &UMOMainMenuWidget::HandleNewGameClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] NewGameButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuWidget] NewGameButton is NULL"));
	}

	// Load Game button
	if (LoadGameButton)
	{
		LoadGameButton->OnClicked().RemoveAll(this);
		LoadGameButton->OnClicked().AddUObject(this, &UMOMainMenuWidget::HandleLoadGameClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] LoadGameButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuWidget] LoadGameButton is NULL"));
	}

	// Options button
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
		OptionsButton->OnClicked().AddUObject(this, &UMOMainMenuWidget::HandleOptionsClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] OptionsButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuWidget] OptionsButton is NULL"));
	}

	// Exit Game button
	if (ExitGameButton)
	{
		ExitGameButton->OnClicked().RemoveAll(this);
		ExitGameButton->OnClicked().AddUObject(this, &UMOMainMenuWidget::HandleExitGameClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] ExitGameButton bound"));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMainMenuWidget] ExitGameButton is NULL"));
	}

	// Bind panel delegates
	if (NewGamePanel)
	{
		NewGamePanel->OnRequestClose.RemoveDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		NewGamePanel->OnStartGameRequested.RemoveDynamic(this, &UMOMainMenuWidget::HandleNewGamePanelStartRequested);
		NewGamePanel->OnRequestClose.AddDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		NewGamePanel->OnStartGameRequested.AddDynamic(this, &UMOMainMenuWidget::HandleNewGamePanelStartRequested);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] NewGamePanel bound"));
	}

	if (LoadPanel)
	{
		// Main menu shows ALL saves, not filtered to current world
		LoadPanel->SetFilterToCurrentWorld(false);
		// Refresh the list now that filter is disabled (NativeConstruct already ran with filter=true)
		LoadPanel->RefreshSaveList();

		LoadPanel->OnRequestClose.RemoveDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.RemoveDynamic(this, &UMOMainMenuWidget::HandleLoadPanelLoadRequested);
		LoadPanel->OnRequestClose.AddDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		LoadPanel->OnLoadRequested.AddDynamic(this, &UMOMainMenuWidget::HandleLoadPanelLoadRequested);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] LoadPanel bound (filter disabled for main menu)"));
	}

	if (OptionsPanel)
	{
		OptionsPanel->OnRequestClose.RemoveDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		OptionsPanel->OnRequestClose.AddDynamic(this, &UMOMainMenuWidget::HandlePanelRequestClose);
		UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] OptionsPanel bound"));
	}
}

void UMOMainMenuWidget::SwitchToPanel(int32 PanelIndex)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] SwitchToPanel: %d (None=0, NewGame=1, Load=2, Options=3)"), PanelIndex);

	if (FocusWindowSwitcher)
	{
		FocusWindowSwitcher->SetActiveWidgetIndex(PanelIndex);
	}
	CurrentPanelIndex = PanelIndex;
}

void UMOMainMenuWidget::HandleNewGameClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] New Game button clicked"));

	// If we have a NewGamePanel, show it for seed configuration
	// Otherwise fall back to immediate new game request
	if (NewGamePanel)
	{
		ShowNewGamePanel();
	}
	else
	{
		// No panel - proceed directly (backwards compatibility)
		OnNewGameRequested.Broadcast();
	}
}

void UMOMainMenuWidget::HandleLoadGameClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Load Game button clicked"));
	ShowLoadPanel();
}

void UMOMainMenuWidget::HandleOptionsClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Options button clicked"));
	ShowOptionsPanel();
}

void UMOMainMenuWidget::HandleExitGameClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Exit Game button clicked"));
	OnExitGameRequested.Broadcast();
}

void UMOMainMenuWidget::HandlePanelRequestClose()
{
	CloseFocusPanel();
}

void UMOMainMenuWidget::HandleLoadPanelLoadRequested(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] Load requested for slot: %s"), *SlotName);
	OnLoadGameRequested.Broadcast(SlotName);
}

void UMOMainMenuWidget::HandleNewGamePanelStartRequested()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOMainMenuWidget] New Game start requested from panel"));

	// Close the panel first
	CloseFocusPanel();

	// Broadcast the new game request - controller will handle level load
	OnNewGameRequested.Broadcast();
}
