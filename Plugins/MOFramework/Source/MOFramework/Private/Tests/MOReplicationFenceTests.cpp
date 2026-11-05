#include "Misc/AutomationTest.h"
#include "MOCraftingSubsystem.h"
#include "MOCraftingTypes.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The maps and sets that would go over the wire through this property: the engine sends none of them and logs an error at each send. */
	void FindReplicatedMaps(const FProperty* Property, const FString& Path, TArray<FString>& OutMaps, TSet<const UStruct*>& Walking)
	{
		if (Property->HasAnyPropertyFlags(CPF_RepSkip))
		{
			return;
		}
		if (CastField<FMapProperty>(Property) || CastField<FSetProperty>(Property))
		{
			OutMaps.Add(Path);
			return;
		}
		if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FindReplicatedMaps(Array->Inner, Path + TEXT("[]"), OutMaps, Walking);
			return;
		}
		const FStructProperty* Struct = CastField<FStructProperty>(Property);
		if (!Struct || (Struct->Struct->StructFlags & STRUCT_NetSerializeNative) || Walking.Contains(Struct->Struct))
		{
			return;   // not a struct, or one whose own serializer decides, or one already on the way here
		}
		Walking.Add(Struct->Struct);
		for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
		{
			FindReplicatedMaps(*It, Path + TEXT(".") + It->GetName(), OutMaps, Walking);
		}
		Walking.Remove(Struct->Struct);
	}
}

// A map in a replicated struct sends nothing (N-308): every co-op log carried "LogProperty: Error: Replicated TMaps are not supported.", and UHT only refuses one directly on a class.
// Expected: none in any replicated property or RPC parameter of the three modules; failing before: FMOSurvivorCraftExecution::CompletedProducts and FMOCraftingQueueEntry::IngredientsPerCraft.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMONetNothingReplicatedIsAMap, "MOFramework.Net.NothingReplicatedIsAMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMONetNothingReplicatedIsAMap::RunTest(const FString& Parameters)
{
	TArray<FString> Maps;
	TSet<const UStruct*> Walking;
	int32 ReplicatedProperties = 0;
	int32 RemoteFunctions = 0;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		const UClass* Class = *ClassIt;
		if (!Class->HasAnyClassFlags(CLASS_Native) || !Class->GetOutermost()->GetName().StartsWith(TEXT("/Script/MOFramework")))
		{
			continue;
		}
		for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Net))
			{
				++ReplicatedProperties;
				FindReplicatedMaps(*It, Class->GetName() + TEXT(".") + It->GetName(), Maps, Walking);
			}
		}
		for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_Net))
			{
				++RemoteFunctions;
				for (TFieldIterator<FProperty> Param(*It); Param; ++Param)
				{
					FindReplicatedMaps(*Param, Class->GetName() + TEXT(".") + It->GetName() + TEXT("(") + Param->GetName() + TEXT(")"), Maps, Walking);
				}
			}
		}
	}
	TestTrue(TEXT("the walk saw the game's replicated properties (the control)"), ReplicatedProperties > 50);
	TestTrue(TEXT("and its RPCs (the control)"), RemoteFunctions > 20);
	for (const FString& Map : Maps)
	{
		AddError(FString::Printf(TEXT("%s holds a map, which the engine cannot replicate: NotReplicated if only the server needs it, an array if a client does"), *Map));
	}

	// CONTROL: the walk does find a map (FMOCraftResult is not replicated; it holds several), so the empty list above means something.
	TArray<FString> InCraftResult;
	for (TFieldIterator<FProperty> It(FMOCraftResult::StaticStruct()); It; ++It)
	{
		FindReplicatedMaps(*It, It->GetName(), InCraftResult, Walking);
	}
	TestTrue(TEXT("CONTROL: the walk finds the maps in FMOCraftResult"), InCraftResult.Num() > 0);

	// NotReplicated leaves a save alone: both still serialize with the save game's tagged properties.
	const FProperty* Ingredients = FMOCraftingQueueEntry::StaticStruct()->FindPropertyByName(TEXT("IngredientsPerCraft"));
	if (TestNotNull(TEXT("the queue entry has its ingredients"), Ingredients))
	{
		TestFalse(TEXT("they are not transient"), Ingredients->HasAnyPropertyFlags(CPF_Transient | CPF_SkipSerialization));
	}
	return true;
}

#endif
