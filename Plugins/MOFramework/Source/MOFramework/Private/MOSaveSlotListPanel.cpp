#include "MOSaveSlotListPanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSaveSlotEntry.h"
#include "MOPersistenceSubsystem.h"
#include "MOConfirmationDialog.h"
#include "MOTextInputDialog.h"
#include "MOGameUIManagerSubsystem.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "TimerManager.h"

void UMOSaveSlotListPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOSaveSlotListPanel::HandleBackClicked);
	}

	RefreshSaveList();
}

void UMOSaveSlotListPanel::NativeDestruct()
{
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}

	for (UMOSaveSlotEntry* Entry : SlotEntryWidgets)
	{
		if (Entry)
		{
			Entry->OnSlotSelected.RemoveDynamic(this, &UMOSaveSlotListPanel::HandleEntrySelected);
			Entry->OnRenameRequested.RemoveDynamic(this, &UMOSaveSlotListPanel::HandleEntryRenameRequested);
			Entry->OnDeleteRequested.RemoveDynamic(this, &UMOSaveSlotListPanel::HandleEntryDeleteRequested);
		}
	}

	Super::NativeDestruct();
}

UWidget* UMOSaveSlotListPanel::NativeGetDesiredFocusTarget() const
{
	if (SlotEntryWidgets.Num() > 0 && IsValid(SlotEntryWidgets[0]))
	{
		return SlotEntryWidgets[0];
	}
	if (BackButton)
	{
		return BackButton;
	}
	return nullptr;
}

void UMOSaveSlotListPanel::RefreshSaveList()
{
	CachedSaves = QuerySaveMetadata();
	PopulateSaveList();
	OnSaveListUpdated(CachedSaves);

	// SYSTEMATIC: every refresh DESTROYS all slot entry widgets via
	// PopulateSaveList → ClearSaveList, then CreateWidgets new ones. If
	// any of those entries held Slate keyboard focus — which is the
	// normal state after a rename modal closes and
	// ClaimFocusForReactivation routes focus to SlotEntryWidgets[0] —
	// that widget is now destroyed, and Slate falls back to SViewport.
	// Symptom: the player has to click the panel again before Tab/Escape
	// take effect.
	//
	// Fix: on the next tick (after the new entry widgets have been laid
	// out and have valid SWidgets), set focus to the panel's desired
	// target. NativeGetDesiredFocusTarget returns SlotEntryWidgets[0]
	// when non-empty, BackButton otherwise — both focusable.
	//
	// Important: This is the focus-after-rebuild concern, separate from
	// the modal-close concern. Modal-close input-mode stomping is fixed
	// at UMOActivatableWidget::NativeOnDeactivated. A previous edit
	// removed the deferred refocus thinking the two were the same; they
	// aren't. Rename and delete from any caller path destroys focused
	// entries, so Slate needs the next-tick refocus regardless of how
	// RefreshSaveList was triggered.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UMOSaveSlotListPanel> WeakSelf(this);
		World->GetTimerManager().SetTimerForNextTick([WeakSelf]()
		{
			UMOSaveSlotListPanel* Self = WeakSelf.Get();
			if (!Self || !Self->IsActivated())
			{
				return;
			}
			if (UWidget* Target = Self->NativeGetDesiredFocusTarget())
			{
				Target->SetFocus();
			}
		});
	}
}

bool UMOSaveSlotListPanel::ConfirmRename(const FString& SlotName, const FText& NewDisplayName)
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UMOPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
	if (!Persistence)
	{
		return false;
	}
	const bool bOk = Persistence->RenameSaveSlot(SlotName, NewDisplayName);
	if (bOk)
	{
		RefreshSaveList();
	}
	return bOk;
}

bool UMOSaveSlotListPanel::ConfirmDelete(const FString& SlotName)
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UMOPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
	if (!Persistence)
	{
		return false;
	}
	const bool bOk = Persistence->DeleteSaveSlot(SlotName);
	if (bOk)
	{
		RefreshSaveList();
	}
	return bOk;
}

TArray<FMOSaveMetadata> UMOSaveSlotListPanel::QuerySaveMetadata() const
{
	TArray<FMOSaveMetadata> Result;

	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	UMOPersistenceSubsystem* Persistence = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
	if (!Persistence)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSaveSlotListPanel] No persistence subsystem"));
		return Result;
	}

	// World-filter target: empty string means "show everything" (used by main
	// menu Load panel where you want all worlds visible). Otherwise filter to
	// the current world's identifier so in-game save / load doesn't surface
	// saves from a different world.
	const FString FilterWorld = bFilterToCurrentWorld ? Persistence->GetCurrentWorldIdentifier() : FString();

	const TArray<FString> SlotNames = Persistence->GetAllSaveSlots();
	for (const FString& SlotName : SlotNames)
	{
		FMOSaveMetadata Meta;
		if (Persistence->GetSaveSlotMetadata(SlotName, Meta))
		{
			// Filter by world identifier if both sides have one available
			// (legacy saves without a stored WorldName are kept; we only
			// filter when we can confidently determine mismatch).
			if (!FilterWorld.IsEmpty() && !Meta.WorldName.IsEmpty() && Meta.WorldName != FilterWorld)
			{
				continue;
			}
			Result.Add(Meta);
		}
		else
		{
			// Legacy/corrupted save — fall back to whatever metadata we can
			// derive from the file name + filesystem timestamp.
			Meta.SlotName = SlotName;
			Meta.DisplayName = FText::FromString(SlotName);
			Meta.WorldName = FilterWorld;

			const FString SavePath = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".sav"));
			Meta.Timestamp = FPaths::FileExists(SavePath)
				? IFileManager::Get().GetTimeStamp(*SavePath)
				: FDateTime::Now();
			Meta.bIsAutosave = SlotName.Contains(TEXT("AutoSave"));

			if (!FilterWorld.IsEmpty() && !SlotName.Contains(FilterWorld))
			{
				continue;
			}
			Result.Add(Meta);
		}
	}

	// Newest first.
	Result.Sort([](const FMOSaveMetadata& A, const FMOSaveMetadata& B)
	{
		return A.Timestamp > B.Timestamp;
	});

	return Result;
}

void UMOSaveSlotListPanel::PopulateSaveList()
{
	ClearSaveList();

	if (!SaveSlotsScrollBox)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSaveSlotListPanel] SaveSlotsScrollBox is NULL - check BindWidget in WBP"));
		return;
	}
	if (!SaveSlotEntryClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSaveSlotListPanel] SaveSlotEntryClass is not set - configure in subclass Blueprint defaults"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSaveSlotListPanel] No owning player controller"));
		return;
	}

	for (const FMOSaveMetadata& Meta : CachedSaves)
	{
		UMOSaveSlotEntry* Entry = CreateWidget<UMOSaveSlotEntry>(PC, SaveSlotEntryClass);
		if (!Entry)
		{
			continue;
		}

		Entry->InitializeFromMetadata(Meta);
		Entry->OnSlotSelected.AddDynamic(this, &UMOSaveSlotListPanel::HandleEntrySelected);
		Entry->OnRenameRequested.AddDynamic(this, &UMOSaveSlotListPanel::HandleEntryRenameRequested);
		Entry->OnDeleteRequested.AddDynamic(this, &UMOSaveSlotListPanel::HandleEntryDeleteRequested);

		SaveSlotsScrollBox->AddChild(Entry);
		SlotEntryWidgets.Add(Entry);
	}
}

void UMOSaveSlotListPanel::ClearSaveList()
{
	if (SaveSlotsScrollBox)
	{
		SaveSlotsScrollBox->ClearChildren();
	}
	SlotEntryWidgets.Empty();
}

void UMOSaveSlotListPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOSaveSlotListPanel::HandleEntrySelected(const FString& SlotName)
{
	// Dispatch to subclass — Save panel triggers save flow; Load panel
	// triggers load flow.
	HandleSlotSelected(SlotName);
}

void UMOSaveSlotListPanel::HandleEntryRenameRequested(const FString& SlotName)
{
	// Push via the canonical subsystem helper — it picks the right path
	// (in-game Modal layer vs main-menu viewport fallback) without us
	// branching here. Same lifecycle either way.
	if (TextInputDialogClass)
	{
		UCommonActivatableWidget* Pushed = UMOGameUIManagerSubsystem::PushModalWidget(this, TextInputDialogClass);
		if (UMOTextInputDialog* Dialog = Cast<UMOTextInputDialog>(Pushed))
		{
			PendingActionSlotName = SlotName;
			Dialog->ConfigureWithText(
				NSLOCTEXT("MOSaveSlot", "RenameTitle", "Rename Save"),
				NSLOCTEXT("MOSaveSlot", "RenameMessage", "Enter a new name for this save:"),
				GetDisplayNameForSlot(SlotName),
				/*MaxLength=*/64,
				NSLOCTEXT("MOSaveSlot", "RenameHint", "Save name"),
				NSLOCTEXT("MOSaveSlot", "RenameConfirm", "Rename"),
				NSLOCTEXT("MOSaveSlot", "RenameCancel", "Cancel"));
			Dialog->OnTextConfirmed.AddDynamic(this, &UMOSaveSlotListPanel::HandleRenameConfirmed);
			return;
		}
		else if (Pushed)
		{
			UE_LOG(LogMOFramework, Error,
				TEXT("[MOSaveSlotListPanel] Rename: pushed widget '%s' isn't a UMOTextInputDialog — check WBP parent class"),
				*Pushed->GetClass()->GetName());
		}
	}

	// No dialog class configured → broadcast for a custom consumer.
	OnSlotRenameRequested.Broadcast(SlotName);
}

void UMOSaveSlotListPanel::HandleEntryDeleteRequested(const FString& SlotName)
{
	if (ConfirmationDialogClass)
	{
		UCommonActivatableWidget* Pushed = UMOGameUIManagerSubsystem::PushModalWidget(this, ConfirmationDialogClass);
		if (UMOConfirmationDialog* Dialog = Cast<UMOConfirmationDialog>(Pushed))
		{
			PendingActionSlotName = SlotName;
			Dialog->Setup(
				NSLOCTEXT("MOSaveSlot", "DeleteTitle", "Delete Save"),
				NSLOCTEXT("MOSaveSlot", "DeleteMessage", "Delete this save permanently? This cannot be undone."),
				NSLOCTEXT("MOSaveSlot", "DeleteConfirm", "Delete"),
				NSLOCTEXT("MOSaveSlot", "DeleteCancel", "Cancel"));
			Dialog->OnConfirmed.AddDynamic(this, &UMOSaveSlotListPanel::HandleDeleteConfirmed);
			return;
		}
		else if (Pushed)
		{
			UE_LOG(LogMOFramework, Error,
				TEXT("[MOSaveSlotListPanel] Delete: pushed widget '%s' isn't a UMOConfirmationDialog — check WBP parent class"),
				*Pushed->GetClass()->GetName());
		}
	}

	OnSlotDeleteRequested.Broadcast(SlotName);
}

FText UMOSaveSlotListPanel::GetDisplayNameForSlot(const FString& SlotName) const
{
	for (const FMOSaveMetadata& Meta : CachedSaves)
	{
		if (Meta.SlotName == SlotName)
		{
			return Meta.DisplayName.IsEmpty() ? FText::FromString(SlotName) : Meta.DisplayName;
		}
	}
	return FText::FromString(SlotName);
}

void UMOSaveSlotListPanel::HandleDeleteConfirmed()
{
	const FString SlotName = PendingActionSlotName;
	PendingActionSlotName.Reset();
	if (!SlotName.IsEmpty())
	{
		ConfirmDelete(SlotName); // base method — deletes + auto-refresh
	}
}

void UMOSaveSlotListPanel::HandleRenameConfirmed(const FText& NewName)
{
	const FString SlotName = PendingActionSlotName;
	PendingActionSlotName.Reset();
	if (SlotName.IsEmpty())
	{
		return;
	}
	// Reject empty/whitespace-only names rather than letting the save get a
	// blank display name. Player can re-open the modal if they didn't mean
	// to confirm with empty text.
	const FString Trimmed = NewName.ToString().TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSaveSlotListPanel] Rename for '%s' ignored — empty name"), *SlotName);
		return;
	}
	ConfirmRename(SlotName, FText::FromString(Trimmed));
}

