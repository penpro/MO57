#include "MOSavePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSaveSlotEntry.h"
#include "MOPersistenceSubsystem.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UMOSavePanel::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] NativeConstruct called"));
	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] NewSaveButton: %s, BackButton: %s, ScrollBox: %s, EntryClass: %s"),
		NewSaveButton ? TEXT("OK") : TEXT("NULL"),
		BackButton ? TEXT("OK") : TEXT("NULL"),
		SaveSlotsScrollBox ? TEXT("OK") : TEXT("NULL"),
		SaveSlotEntryClass ? *SaveSlotEntryClass->GetName() : TEXT("NULL"));

	if (NewSaveButton)
	{
		NewSaveButton->OnClicked().RemoveAll(this);
		NewSaveButton->OnClicked().AddUObject(this, &UMOSavePanel::HandleNewSaveClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] NewSaveButton bound"));
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOSavePanel::HandleBackClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] BackButton bound"));
	}

	RefreshSaveList();
}

UWidget* UMOSavePanel::NativeGetDesiredFocusTarget() const
{
	// Focus first save slot if any exist, otherwise the New Save button
	if (SlotEntryWidgets.Num() > 0 && IsValid(SlotEntryWidgets[0]))
	{
		return SlotEntryWidgets[0];
	}
	if (NewSaveButton)
	{
		return NewSaveButton;
	}
	return nullptr;
}

void UMOSavePanel::RefreshSaveList()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] RefreshSaveList called"));
	CachedSaves = GetCurrentWorldSaves();
	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] Found %d saves for current world"), CachedSaves.Num());
	PopulateSaveList();
	OnSaveListUpdated(CachedSaves);
}

TArray<FMOSaveMetadata> UMOSavePanel::GetCurrentWorldSaves() const
{
	TArray<FMOSaveMetadata> Result;

	// Get persistence subsystem (GameInstanceSubsystem)
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (!GameInstance)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] GameInstance is NULL"));
		return Result;
	}

	UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
	if (!Persistence)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] Persistence subsystem is NULL"));
		return Result;
	}

	// Get current world identifier
	const FString CurrentWorldId = Persistence->GetCurrentWorldIdentifier();
	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] Current world ID: '%s'"), *CurrentWorldId);

	// Get all saves for this world
	TArray<FString> SaveSlots = Persistence->GetAllSaveSlots();
	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] Total save slots found: %d"), SaveSlots.Num());

	for (const FString& SlotName : SaveSlots)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel]   Checking slot: %s"), *SlotName);

		// Filter to current world only (if world ID is available)
		// Show all saves if world ID is empty or slot matches world
		if (!CurrentWorldId.IsEmpty() && !SlotName.Contains(CurrentWorldId))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel]   Skipping (doesn't match world ID '%s')"), *CurrentWorldId);
			continue;
		}

		FMOSaveMetadata Meta;
		Meta.SlotName = SlotName;
		Meta.DisplayName = FText::FromString(SlotName);
		Meta.WorldName = CurrentWorldId;

		// TODO: Load actual metadata from save file
		// For now, use slot name and get file timestamp
		const FString SavePath = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".sav"));
		if (FPaths::FileExists(SavePath))
		{
			Meta.Timestamp = IFileManager::Get().GetTimeStamp(*SavePath);
		}
		else
		{
			Meta.Timestamp = FDateTime::Now();
		}

		Meta.bIsAutosave = SlotName.Contains(TEXT("Autosave"));

		Result.Add(Meta);
		UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel]   Added save: %s"), *SlotName);
	}

	// Sort by timestamp (newest first)
	Result.Sort([](const FMOSaveMetadata& A, const FMOSaveMetadata& B)
	{
		return A.Timestamp > B.Timestamp;
	});

	return Result;
}

void UMOSavePanel::CreateNewSave()
{
	// Generate new slot name with world identifier and timestamp
	FString WorldId;
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (GameInstance)
	{
		UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
		if (Persistence)
		{
			WorldId = Persistence->GetCurrentWorldIdentifier();
		}
	}

	const FString NewSlotName = FString::Printf(TEXT("%s_Save_%s"),
		WorldId.IsEmpty() ? TEXT("World") : *WorldId,
		*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] CreateNewSave: %s"), *NewSlotName);
	SaveToSlot(NewSlotName);
}

void UMOSavePanel::SaveToSlot(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] SaveToSlot: %s (delegate bound: %s)"),
		*SlotName,
		OnSaveRequested.IsBound() ? TEXT("YES") : TEXT("NO"));
	// Broadcast save request - UIManager will handle confirmation and actual save
	OnSaveRequested.Broadcast(SlotName);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] SaveToSlot broadcast complete"));
}

void UMOSavePanel::PopulateSaveList()
{
	ClearSaveList();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] PopulateSaveList: %d saves to display"), CachedSaves.Num());

	if (!SaveSlotsScrollBox)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] SaveSlotsScrollBox is NULL - check BindWidget in WBP"));
		return;
	}

	if (!SaveSlotEntryClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] SaveSlotEntryClass is not set - configure in SavePanel Blueprint defaults"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] No owning player controller"));
		return;
	}

	for (const FMOSaveMetadata& Meta : CachedSaves)
	{
		UMOSaveSlotEntry* Entry = CreateWidget<UMOSaveSlotEntry>(PC, SaveSlotEntryClass);
		if (!Entry)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] Failed to create entry widget for slot: %s"), *Meta.SlotName);
			continue;
		}

		Entry->InitializeFromMetadata(Meta);
		Entry->OnSlotSelected.AddDynamic(this, &UMOSavePanel::HandleSlotSelected);

		SaveSlotsScrollBox->AddChild(Entry);
		SlotEntryWidgets.Add(Entry);
		UE_LOG(LogMOFramework, Log, TEXT("[MOSavePanel] Added entry for slot: %s"), *Meta.SlotName);
	}
}

void UMOSavePanel::ClearSaveList()
{
	if (SaveSlotsScrollBox)
	{
		SaveSlotsScrollBox->ClearChildren();
	}
	SlotEntryWidgets.Empty();
}

void UMOSavePanel::HandleNewSaveClicked()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] *** NEW SAVE BUTTON CLICKED ***"));
	CreateNewSave();
}

void UMOSavePanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOSavePanel::HandleSlotSelected(const FString& SlotName)
{
	SaveToSlot(SlotName);
}
