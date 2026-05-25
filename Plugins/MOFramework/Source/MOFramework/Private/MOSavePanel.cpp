#include "MOSavePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOIdentityComponent.h"
#include "MOPersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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
	// Preferred slot name: "<CharacterName>-NN" so saves are recognizable in
	// the load list ("Alex Smith-03" vs the old "World_Save_20260524_124056").
	// Fall back to the WorldId + timestamp pattern if we can't find a pawn /
	// character name (e.g. spectator before a pawn is possessed).
	auto SanitizeForFilename = [](FString In) -> FString
	{
		In = In.Replace(TEXT(" "), TEXT("_"));
		In = In.Replace(TEXT("/"), TEXT("_"));
		In = In.Replace(TEXT("\\"), TEXT("_"));
		In = In.Replace(TEXT(":"), TEXT("_"));
		In = In.Replace(TEXT("*"), TEXT("_"));
		In = In.Replace(TEXT("?"), TEXT("_"));
		In = In.Replace(TEXT("\""), TEXT("_"));
		In = In.Replace(TEXT("<"), TEXT("_"));
		In = In.Replace(TEXT(">"), TEXT("_"));
		In = In.Replace(TEXT("|"), TEXT("_"));
		return In;
	};

	// Try to read the locally-controlled pawn's character name.
	FString CharacterName;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UMOIdentityComponent* IdComp = Pawn->FindComponentByClass<UMOIdentityComponent>())
			{
				CharacterName = IdComp->DisplayName.ToString();
			}
		}
	}

	FString NewSlotName;
	if (!CharacterName.IsEmpty())
	{
		const FString SanitizedName = SanitizeForFilename(CharacterName);
		FString Candidate = FString::Printf(TEXT("%s-01"), *SanitizedName);
		int32 Iter = 2;
		while (UGameplayStatics::DoesSaveGameExist(Candidate, 0) && Iter < 100)
		{
			Candidate = FString::Printf(TEXT("%s-%02d"), *SanitizedName, Iter++);
		}
		NewSlotName = Candidate;

		UE_LOG(LogMOFramework, Log,
			TEXT("[MOSavePanel] New save slot derived from character '%s' -> '%s'"),
			*CharacterName, *NewSlotName);
	}
	else
	{
		// Legacy fallback.
		FString WorldId;
		if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
		{
			if (UMOPersistenceSubsystem* Persistence = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				WorldId = Persistence->GetCurrentWorldIdentifier();
			}
		}
		NewSlotName = FString::Printf(TEXT("%s_Save_%s"),
			WorldId.IsEmpty() ? TEXT("World") : *WorldId,
			*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOSavePanel] No character name available — falling back to legacy slot name '%s'"),
			*NewSlotName);
	}

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
