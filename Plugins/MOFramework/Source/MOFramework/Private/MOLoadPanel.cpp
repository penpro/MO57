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

	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UMOLoadPanel::HandleBackClicked);
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
	CachedSaves.Empty();

	// Get persistence subsystem (GameInstanceSubsystem)
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (!GameInstance)
	{
		PopulateSaveList();
		OnSaveListUpdated(CachedSaves);
		return;
	}

	UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
	if (!Persistence)
	{
		PopulateSaveList();
		OnSaveListUpdated(CachedSaves);
		return;
	}

	const FString CurrentWorldId = bFilterToCurrentWorld ? Persistence->GetCurrentWorldIdentifier() : FString();
	TArray<FString> SaveSlots = Persistence->GetAllSaveSlots();

	for (const FString& SlotName : SaveSlots)
	{
		// Filter to current world if enabled
		if (bFilterToCurrentWorld && !SlotName.Contains(CurrentWorldId))
		{
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
	}

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
	OnLoadRequested.Broadcast(SlotName);
}

void UMOLoadPanel::PopulateSaveList()
{
	ClearSaveList();

	if (!SaveSlotsScrollBox || !SaveSlotEntryClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOLoadPanel] Missing SaveSlotsScrollBox or SaveSlotEntryClass"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
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
		Entry->OnSlotSelected.AddDynamic(this, &UMOLoadPanel::HandleSlotSelected);

		SaveSlotsScrollBox->AddChild(Entry);
		SlotEntryWidgets.Add(Entry);
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
