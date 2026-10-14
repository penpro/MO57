// Key rebinding keeps what the mapping does (audit H54). ApplyBinding used to UnmapKey + MapKey a bare {Action, Key}: the new mapping lost its modifiers
// (IMC_MODefault's W / S carry a Swizzle, S and A a Negate: a rebound movement key moved the pawn sideways or backwards) and went to the end of the list,
// shifting every later slot index. FMOKeyBindingManager::SetMappingKey is the in-place edit ApplyBinding now uses.
// Headless-safe: works on a transient copy of the real context.

#include "Misc/AutomationTest.h"
#include "MOKeyBindingManager.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOKeyBinding_SetMappingKeyKeepsModifiers,
	"MOFramework.Input.RebindKeepsModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOKeyBinding_SetMappingKeyKeepsModifiers::RunTest(const FString& Parameters)
{
	UInputMappingContext* Asset = LoadObject<UInputMappingContext>(nullptr, TEXT("/MOFramework/Input/IMC_MODefault.IMC_MODefault"));
	if (!TestNotNull(TEXT("IMC_MODefault loads"), Asset))
	{
		return false;
	}
	// The movement action's mapping with the most modifiers (S: Swizzle + Negate) and its slot among that action's mappings.
	const UInputAction* Move = nullptr;
	int32 Slot = INDEX_NONE, Index = INDEX_NONE, BestModifiers = 0;
	{
		TMap<const UInputAction*, int32> SlotOfAction;
		const TArray<FEnhancedActionKeyMapping>& Mappings = Asset->GetMappings();
		for (int32 I = 0; I < Mappings.Num(); ++I)
		{
			int32& NextSlot = SlotOfAction.FindOrAdd(Mappings[I].Action);
			if (Mappings[I].Key.IsValid() && !Mappings[I].Key.IsGamepadKey() && Mappings[I].Modifiers.Num() > BestModifiers)
			{
				BestModifiers = Mappings[I].Modifiers.Num();
				Move = Mappings[I].Action;
				Slot = NextSlot;
				Index = I;
			}
			++NextSlot;
		}
	}
	if (!TestTrue(TEXT("the context has a keyboard mapping with modifiers (the movement keys)"), Move != nullptr && BestModifiers >= 2))
	{
		return false;
	}
	const FKey AssetKeyBefore = Asset->GetMappings()[Index].Key;
	const int32 CountBefore = Asset->GetMappings().Num();

	UInputMappingContext* Copy = DuplicateObject<UInputMappingContext>(Asset, GetTransientPackage());
	FKey OldKey;
	TestTrue(TEXT("SetMappingKey finds the slot"), FMOKeyBindingManager::SetMappingKey(Copy, Move, Slot, EKeys::K, OldKey));
	const FEnhancedActionKeyMapping& After = Copy->GetMappings()[Index];
	TestTrue(TEXT("the old key is reported"), OldKey == AssetKeyBefore);
	TestTrue(TEXT("the mapping at the same index now has the new key"), After.Key == EKeys::K);
	TestTrue(TEXT("... and the same action"), After.Action == Move);
	TestEqual(TEXT("... and KEEPS its modifiers (swizzle / negate)"), After.Modifiers.Num(), BestModifiers);
	TestEqual(TEXT("no mapping was added or removed"), Copy->GetMappings().Num(), CountBefore);
	TestTrue(TEXT("the project asset is untouched"), Asset->GetMappings()[Index].Key == AssetKeyBefore);

	// A slot the action does not have is refused, changing nothing.
	FKey Unused;
	TestFalse(TEXT("a missing slot is refused"), FMOKeyBindingManager::SetMappingKey(Copy, Move, 999, EKeys::J, Unused));

	// CONTROL: the old way (UnmapKey + MapKey) on another copy loses the modifiers -- so the check above can fail.
	UInputMappingContext* OldWay = DuplicateObject<UInputMappingContext>(Asset, GetTransientPackage());
	OldWay->UnmapKey(Move, AssetKeyBefore);
	const FEnhancedActionKeyMapping& Remapped = OldWay->MapKey(Move, EKeys::K);
	TestEqual(TEXT("CONTROL: UnmapKey + MapKey leaves the new mapping without modifiers"), Remapped.Modifiers.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
