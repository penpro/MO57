#include "MOKeyBindingManager.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MOPlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"

TMap<FName, FKey> FMOKeyBindingManager::DefaultBindingsCache;

FName FMOKeyBindingManager::MakeCacheKey(const UInputAction* Action, int32 SlotIndex)
{
	if (!Action)
	{
		return NAME_None;
	}
	return FName(*FString::Printf(TEXT("%s_%d"), *Action->GetFName().ToString(), SlotIndex));
}

FName FMOKeyBindingManager::GetActionId(const UInputAction* Action)
{
	return Action ? Action->GetFName() : NAME_None;
}

FKey FMOKeyBindingManager::GetDefaultBinding(const UInputAction* Action, const UInputMappingContext* Context, int32 SlotIndex)
{
	if (!Action || !Context)
	{
		return EKeys::Invalid;
	}

	// Check cache first
	const FName CacheKey = MakeCacheKey(Action, SlotIndex);
	if (const FKey* CachedKey = DefaultBindingsCache.Find(CacheKey))
	{
		return *CachedKey;
	}

	// Search the mapping context for this action
	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
	int32 CurrentSlot = 0;

	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (Mapping.Action == Action)
		{
			if (CurrentSlot == SlotIndex)
			{
				// Cache and return
				DefaultBindingsCache.Add(CacheKey, Mapping.Key);
				return Mapping.Key;
			}
			CurrentSlot++;
		}
	}

	return EKeys::Invalid;
}

FKey FMOKeyBindingManager::GetCurrentBinding(const UInputAction* Action, const UInputMappingContext* Context, int32 SlotIndex)
{
	if (!Action || !Context)
	{
		return EKeys::Invalid;
	}

	// Check for custom binding in settings
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		const FName ActionId = GetActionId(Action);
		if (const FMOKeyBindingEntry* CustomBinding = Settings->FindCustomBinding(ActionId, SlotIndex))
		{
			return CustomBinding->OverrideKey;
		}
	}

	// Fall back to default
	return GetDefaultBinding(Action, Context, SlotIndex);
}

bool FMOKeyBindingManager::SetMappingKey(UInputMappingContext* Context, const UInputAction* Action, int32 SlotIndex, FKey NewKey, FKey& OutOldKey)
{
	if (!Context || !Action)
	{
		return false;
	}
	// The SlotIndex-th mapping of this action, in the context's order
	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
	int32 CurrentSlot = 0;
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		if (Mappings[Index].Action != Action)
		{
			continue;
		}
		if (CurrentSlot++ == SlotIndex)
		{
			// Change ONLY the key, in place (audit H54). It used to UnmapKey + MapKey a bare {Action, Key}: the new mapping lost its modifiers and
			// triggers -- IMC_MODefault's W / S carry a Swizzle (S also a Negate) and A a Negate, so a rebound movement key moved the pawn sideways
			// or backwards -- and landed at the END of the list, shifting every later slot index. Callers pass the controller's own transient copy
			// (AMOPlayerController::EnsureRuntimeInputContexts), never the project asset.
			OutOldKey = Mappings[Index].Key;
			Context->GetMapping(Index).Key = NewKey;
			return true;
		}
	}
	return false;
}

bool FMOKeyBindingManager::ApplyBinding(UInputAction* Action, FKey NewKey, UInputMappingContext* Context, APlayerController* PC, int32 SlotIndex)
{
	if (!Action || !Context || !PC || !NewKey.IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] ApplyBinding failed - invalid parameters"));
		return false;
	}

	// Get the enhanced input subsystem
	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] ApplyBinding failed - no local player"));
		return false;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] ApplyBinding failed - no enhanced input subsystem"));
		return false;
	}

	FKey OldKey;
	if (!SetMappingKey(Context, Action, SlotIndex, NewKey, OldKey))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] ApplyBinding failed - action not found in context at slot %d"), SlotIndex);
		return false;
	}

	// Rebuild the active mappings (keeps every context's priority; the remove + re-add at priority 0 that stood here demoted the building context).
	Subsystem->RequestRebuildControlMappings();

	UE_LOG(LogMOFramework, Log, TEXT("[KeyBindingManager] Applied binding: %s -> %s (was %s)"),
		*Action->GetFName().ToString(), *NewKey.ToString(), *OldKey.ToString());

	return true;
}

void FMOKeyBindingManager::ApplyAllBindingsFromSettings(AMOPlayerController* PC)
{
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] ApplyAllBindingsFromSettings - no player controller"));
		return;
	}

	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] ApplyAllBindingsFromSettings - no settings"));
		return;
	}

	const TArray<FMOKeyBindingEntry>& CustomBindings = Settings->CustomKeyBindings;
	if (CustomBindings.Num() == 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[KeyBindingManager] No custom bindings to apply"));
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[KeyBindingManager] Applying %d custom key bindings"), CustomBindings.Num());

	// Build a map of action name -> UInputAction* from the player controller
	TMap<FName, UInputAction*> ActionMap;

	// Movement actions
	if (PC->MoveAction) ActionMap.Add(PC->MoveAction->GetFName(), PC->MoveAction);
	if (PC->LookAction) ActionMap.Add(PC->LookAction->GetFName(), PC->LookAction);
	if (PC->JumpAction) ActionMap.Add(PC->JumpAction->GetFName(), PC->JumpAction);
	if (PC->HustleAction) ActionMap.Add(PC->HustleAction->GetFName(), PC->HustleAction);
	if (PC->CrouchAction) ActionMap.Add(PC->CrouchAction->GetFName(), PC->CrouchAction);

	// Action actions
	if (PC->InteractAction) ActionMap.Add(PC->InteractAction->GetFName(), PC->InteractAction);
	if (PC->PrimaryAction) ActionMap.Add(PC->PrimaryAction->GetFName(), PC->PrimaryAction);
	if (PC->SecondaryAction) ActionMap.Add(PC->SecondaryAction->GetFName(), PC->SecondaryAction);

	// UI actions
	if (PC->InventoryAction) ActionMap.Add(PC->InventoryAction->GetFName(), PC->InventoryAction);
	if (PC->CraftAction) ActionMap.Add(PC->CraftAction->GetFName(), PC->CraftAction);
	if (PC->PlayerStatusAction) ActionMap.Add(PC->PlayerStatusAction->GetFName(), PC->PlayerStatusAction);
	if (PC->SkillsAction) ActionMap.Add(PC->SkillsAction->GetFName(), PC->SkillsAction);
	if (PC->BuildAction) ActionMap.Add(PC->BuildAction->GetFName(), PC->BuildAction);
	if (PC->BackMenuAction) ActionMap.Add(PC->BackMenuAction->GetFName(), PC->BackMenuAction);
	if (PC->PossessAction) ActionMap.Add(PC->PossessAction->GetFName(), PC->PossessAction);

	// Building actions
	if (PC->PlaceBuildingAction) ActionMap.Add(PC->PlaceBuildingAction->GetFName(), PC->PlaceBuildingAction);
	if (PC->RotateBuildingCWAction) ActionMap.Add(PC->RotateBuildingCWAction->GetFName(), PC->RotateBuildingCWAction);
	if (PC->RotateBuildingCCWAction) ActionMap.Add(PC->RotateBuildingCCWAction->GetFName(), PC->RotateBuildingCCWAction);
	if (PC->CancelPlacementAction) ActionMap.Add(PC->CancelPlacementAction->GetFName(), PC->CancelPlacementAction);

	// Apply each custom binding
	for (const FMOKeyBindingEntry& Binding : CustomBindings)
	{
		if (!Binding.IsValid())
		{
			continue;
		}

		UInputAction** ActionPtr = ActionMap.Find(Binding.ActionId);
		if (!ActionPtr || !*ActionPtr)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[KeyBindingManager] Unknown action: %s"), *Binding.ActionId.ToString());
			continue;
		}

		// Determine which context this action belongs to
		// Most actions are in PawnControlContext
		UInputMappingContext* Context = PC->PawnControlContext;

		// Building actions are in BaseBuildingContext
		if (Binding.ActionId.ToString().Contains(TEXT("Building")) ||
			Binding.ActionId.ToString().Contains(TEXT("Placement")))
		{
			Context = PC->BaseBuildingContext;
		}

		if (Context)
		{
			ApplyBinding(*ActionPtr, Binding.OverrideKey, Context, PC, Binding.SlotIndex);
		}
	}
}

void FMOKeyBindingManager::RestoreAllDefaults(AMOPlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[KeyBindingManager] Restoring all bindings to defaults"));

	// Iterate through cached defaults and restore them
	for (const auto& CachedEntry : DefaultBindingsCache)
	{
		// Parse the cache key to get ActionId
		FString KeyString = CachedEntry.Key.ToString();
		int32 UnderscoreIndex;
		if (!KeyString.FindLastChar('_', UnderscoreIndex))
		{
			continue;
		}

		FName ActionId = FName(*KeyString.Left(UnderscoreIndex));
		FKey DefaultKey = CachedEntry.Value;

		// Build action map
		TMap<FName, UInputAction*> ActionMap;
		if (PC->MoveAction) ActionMap.Add(PC->MoveAction->GetFName(), PC->MoveAction);
		if (PC->LookAction) ActionMap.Add(PC->LookAction->GetFName(), PC->LookAction);
		if (PC->JumpAction) ActionMap.Add(PC->JumpAction->GetFName(), PC->JumpAction);
		if (PC->HustleAction) ActionMap.Add(PC->HustleAction->GetFName(), PC->HustleAction);
		if (PC->CrouchAction) ActionMap.Add(PC->CrouchAction->GetFName(), PC->CrouchAction);
		if (PC->InteractAction) ActionMap.Add(PC->InteractAction->GetFName(), PC->InteractAction);
		if (PC->PrimaryAction) ActionMap.Add(PC->PrimaryAction->GetFName(), PC->PrimaryAction);
		if (PC->SecondaryAction) ActionMap.Add(PC->SecondaryAction->GetFName(), PC->SecondaryAction);
		if (PC->InventoryAction) ActionMap.Add(PC->InventoryAction->GetFName(), PC->InventoryAction);
		if (PC->CraftAction) ActionMap.Add(PC->CraftAction->GetFName(), PC->CraftAction);
		if (PC->PlayerStatusAction) ActionMap.Add(PC->PlayerStatusAction->GetFName(), PC->PlayerStatusAction);
		if (PC->SkillsAction) ActionMap.Add(PC->SkillsAction->GetFName(), PC->SkillsAction);
		if (PC->BuildAction) ActionMap.Add(PC->BuildAction->GetFName(), PC->BuildAction);
		if (PC->BackMenuAction) ActionMap.Add(PC->BackMenuAction->GetFName(), PC->BackMenuAction);
		if (PC->PossessAction) ActionMap.Add(PC->PossessAction->GetFName(), PC->PossessAction);
		if (PC->PlaceBuildingAction) ActionMap.Add(PC->PlaceBuildingAction->GetFName(), PC->PlaceBuildingAction);
		if (PC->RotateBuildingCWAction) ActionMap.Add(PC->RotateBuildingCWAction->GetFName(), PC->RotateBuildingCWAction);
		if (PC->RotateBuildingCCWAction) ActionMap.Add(PC->RotateBuildingCCWAction->GetFName(), PC->RotateBuildingCCWAction);
		if (PC->CancelPlacementAction) ActionMap.Add(PC->CancelPlacementAction->GetFName(), PC->CancelPlacementAction);

		UInputAction** ActionPtr = ActionMap.Find(ActionId);
		if (!ActionPtr || !*ActionPtr)
		{
			continue;
		}

		// Determine context
		UInputMappingContext* Context = PC->PawnControlContext;
		if (ActionId.ToString().Contains(TEXT("Building")) || ActionId.ToString().Contains(TEXT("Placement")))
		{
			Context = PC->BaseBuildingContext;
		}

		if (Context && DefaultKey.IsValid())
		{
			ApplyBinding(*ActionPtr, DefaultKey, Context, PC, 0);
		}
	}
}
