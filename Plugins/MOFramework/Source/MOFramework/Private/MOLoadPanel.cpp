#include "MOLoadPanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOSaveSlotEntry.h"
#include "MOPersistenceSubsystem.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UMOLoadPanel::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] NativeConstruct called"));
	UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] BackButton: %s, ScrollBox: %s, EntryClass: %s"),
		BackButton ? TEXT("OK") : TEXT("NULL"),
		SaveSlotsScrollBox ? TEXT("OK") : TEXT("NULL"),
		SaveSlotEntryClass ? *SaveSlotEntryClass->GetName() : TEXT("NOT SET - Configure in Blueprint!"));

	if (!SaveSlotEntryClass)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOLoadPanel] SaveSlotEntryClass is NOT SET! Go to WBP_LoadPanel Blueprint defaults and set SaveSlotEntryClass to your WBP_SaveSlotEntry."));
	}

	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMOLoadPanel::HandleBackClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] BackButton bound"));
	}

	RefreshSaveList();
}

UWidget* UMOLoadPanel::NativeGetDesiredFocusTarget() const
{
	// Focus first save slot if any exist
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

void UMOLoadPanel::RefreshSaveList()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] RefreshSaveList called"));
	CachedSaves.Empty();

	// Get persistence subsystem (GameInstanceSubsystem)
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (!GameInstance)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] GameInstance is NULL"));
		PopulateSaveList();
		OnSaveListUpdated(CachedSaves);
		return;
	}

	UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
	if (!Persistence)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] Persistence subsystem is NULL"));
		PopulateSaveList();
		OnSaveListUpdated(CachedSaves);
		return;
	}

	const FString CurrentWorldId = bFilterToCurrentWorld ? Persistence->GetCurrentWorldIdentifier() : FString();
	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] Filter to world: %s, World ID: '%s'"),
		bFilterToCurrentWorld ? TEXT("YES") : TEXT("NO"), *CurrentWorldId);

	TArray<FString> SaveSlots = Persistence->GetAllSaveSlots();
	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] Total save slots found: %d"), SaveSlots.Num());

	for (const FString& SlotName : SaveSlots)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel]   Checking slot: %s"), *SlotName);

		// Filter to current world if enabled
		if (bFilterToCurrentWorld && !CurrentWorldId.IsEmpty() && !SlotName.Contains(CurrentWorldId))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel]   Skipping (doesn't match world ID)"));
			continue;
		}

		FMOSaveMetadata Meta;
		Meta.SlotName = SlotName;
		Meta.DisplayName = FText::FromString(SlotName);

		// Extract world name from slot name if possible
		Meta.WorldName = CurrentWorldId;

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

		CachedSaves.Add(Meta);
		UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel]   Added save: %s"), *SlotName);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] Found %d saves for display"), CachedSaves.Num());

	// Sort by timestamp (newest first)
	CachedSaves.Sort([](const FMOSaveMetadata& A, const FMOSaveMetadata& B)
	{
		return A.Timestamp > B.Timestamp;
	});

	PopulateSaveList();
	OnSaveListUpdated(CachedSaves);
}

void UMOLoadPanel::SetFilterToCurrentWorld(bool bFilter)
{
	bFilterToCurrentWorld = bFilter;
}

void UMOLoadPanel::LoadFromSlot(const FString& SlotName)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] LoadFromSlot broadcasting: %s"), *SlotName);
	OnLoadRequested.Broadcast(SlotName);
}

void UMOLoadPanel::PopulateSaveList()
{
	ClearSaveList();

	UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] PopulateSaveList: %d saves to display"), CachedSaves.Num());

	if (!SaveSlotsScrollBox)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] SaveSlotsScrollBox is NULL - check BindWidget in WBP"));
		return;
	}

	if (!SaveSlotEntryClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] SaveSlotEntryClass is not set - configure in LoadPanel Blueprint defaults"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] No owning player controller"));
		return;
	}

	for (const FMOSaveMetadata& Meta : CachedSaves)
	{
		UMOSaveSlotEntry* Entry = CreateWidget<UMOSaveSlotEntry>(PC, SaveSlotEntryClass);
		if (!Entry)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] Failed to create entry widget for slot: %s"), *Meta.SlotName);
			continue;
		}

		Entry->InitializeFromMetadata(Meta);
		Entry->OnSlotSelected.AddDynamic(this, &UMOLoadPanel::HandleSlotSelected);

		SaveSlotsScrollBox->AddChild(Entry);
		SlotEntryWidgets.Add(Entry);
		UE_LOG(LogMOFramework, Log, TEXT("[MOLoadPanel] Added entry for slot: %s"), *Meta.SlotName);
	}
}

void UMOLoadPanel::ClearSaveList()
{
	if (SaveSlotsScrollBox)
	{
		SaveSlotsScrollBox->ClearChildren();
	}
	SlotEntryWidgets.Empty();
}

void UMOLoadPanel::HandleBackClicked()
{
	OnRequestClose.Broadcast();
}

void UMOLoadPanel::HandleSlotSelected(const FString& SlotName)
{
	LoadFromSlot(SlotName);
}
