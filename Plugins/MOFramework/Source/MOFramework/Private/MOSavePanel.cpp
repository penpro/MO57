#include "MOSavePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOPersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UMOSavePanel::NativeConstruct()
{
	Super::NativeConstruct(); // base wires Back button + RefreshSaveList

	if (NewSaveButton)
	{
		NewSaveButton->OnClicked().RemoveAll(this);
		NewSaveButton->OnClicked().AddUObject(this, &UMOSavePanel::HandleNewSaveClicked);
	}
}

void UMOSavePanel::NativeDestruct()
{
	if (NewSaveButton)
	{
		NewSaveButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

UWidget* UMOSavePanel::NativeGetDesiredFocusTarget() const
{
	// Prefer base focus (first slot if any). Fall back to NewSaveButton —
	// makes sense for empty save lists where the player wants to create one.
	if (UWidget* BaseTarget = Super::NativeGetDesiredFocusTarget())
	{
		return BaseTarget;
	}
	return NewSaveButton;
}

void UMOSavePanel::CreateNewSave()
{
	// Slot name template: {WorldId}_Save_{Timestamp}. World ID prefix lets us
	// filter saves by world; timestamp makes each slot unique.
	FString WorldId;
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (UMOPersistenceSubsystem* Persistence = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			WorldId = Persistence->GetCurrentWorldIdentifier();
		}
	}

	const FString NewSlotName = FString::Printf(TEXT("%s_Save_%s"),
		WorldId.IsEmpty() ? TEXT("World") : *WorldId,
		*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	SaveToSlot(NewSlotName);
}

void UMOSavePanel::SaveToSlot(const FString& SlotName)
{
	OnSaveRequested.Broadcast(SlotName);
}

void UMOSavePanel::HandleSlotSelected(const FString& SlotName)
{
	// In the Save panel, picking an existing slot means "overwrite this".
	// Caller is responsible for confirming overwrite via UMOConfirmationBase.
	SaveToSlot(SlotName);
}

void UMOSavePanel::HandleNewSaveClicked()
{
	CreateNewSave();
}
