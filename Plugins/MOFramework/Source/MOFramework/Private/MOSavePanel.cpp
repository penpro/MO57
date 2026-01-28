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

	if (NewSaveButton)
	{
		NewSaveButton->OnClicked().AddUObject(this, &UMOSavePanel::HandleNewSaveClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UMOSavePanel::HandleBackClicked);
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
	CachedSaves = GetCurrentWorldSaves();
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
		return Result;
	}

	UMOPersistenceSubsystem* Persistence = GameInstance->GetSubsystem<UMOPersistenceSubsystem>();
	if (!Persistence)
	{
		return Result;
	}

	// Get current world identifier
	const FString CurrentWorldId = Persistence->GetCurrentWorldIdentifier();

	// Get all saves for this world
	TArray<FString> SaveSlots = Persistence->GetAllSaveSlots();

	for (const FString& SlotName : SaveSlots)
	{
		// Filter to current world only
		if (!SlotName.Contains(CurrentWorldId))
		{
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
	// Generate new slot name with timestamp
	const FString NewSlotName = FString::Printf(TEXT("Save_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	SaveToSlot(NewSlotName);
}

void UMOSavePanel::SaveToSlot(const FString& SlotName)
{
	// Broadcast save request - UIManager will handle confirmation and actual save
	OnSaveRequested.Broadcast(SlotName);
}

void UMOSavePanel::PopulateSaveList()
{
	ClearSaveList();

	if (!SaveSlotsScrollBox || !SaveSlotEntryClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSavePanel] Missing SaveSlotsScrollBox or SaveSlotEntryClass"));
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
		Entry->OnSlotSelected.AddDynamic(this, &UMOSavePanel::HandleSlotSelected);

		SaveSlotsScrollBox->AddChild(Entry);
		SlotEntryWidgets.Add(Entry);
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
