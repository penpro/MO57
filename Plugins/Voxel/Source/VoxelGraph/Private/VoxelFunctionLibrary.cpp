// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelFunctionLibrary.h"
#include "VoxelBuffer.h"
#include "VoxelGenericStructBuffer.h"
#include "VoxelNodeStats.h"
#include "Nodes/VoxelNode_UFunction.h"

static TVoxelMap<const UFunction*, FVoxelFunctionLibraryRegistry::FNativeFuncPtr> GVoxelFunctionToNativeCall;
static FVoxelFunctionRegistration* GVoxelFirstPendingFunctionRegistration = nullptr;

FVoxelFunctionRegistration::FVoxelFunctionRegistration(
	const FVoxelFunctionLibraryRegistry::FGetClassFuncPtr GetClassPtr,
	const TCHAR* Name,
	const FVoxelFunctionLibraryRegistry::FNativeFuncPtr NativeFunction)
	: GetClassPtr(GetClassPtr)
	, Name(Name)
	, NativeFunction(NativeFunction)
	, Next(GVoxelFirstPendingFunctionRegistration)
{
	GVoxelFirstPendingFunctionRegistration = this;
}

FVoxelFunctionLibraryRegistry::FNativeFuncPtr FVoxelFunctionLibraryRegistry::FindFunction(const UFunction& Function)
{
	for (const FVoxelFunctionRegistration* Registration = GVoxelFirstPendingFunctionRegistration; Registration; Registration = Registration->Next)
	{
		const UClass* Class = Registration->GetClassPtr();
		if (!ensure(Class))
		{
			continue;
		}

		const UFunction* PendingFunction = Class->FindFunctionByName(Registration->Name);
		if (!ensure(PendingFunction))
		{
			continue;
		}

		GVoxelFunctionToNativeCall.Add_EnsureNew(PendingFunction, Registration->NativeFunction);
	}
	GVoxelFirstPendingFunctionRegistration = nullptr;

	return GVoxelFunctionToNativeCall.FindRef(&Function);
}

#if !UE_BUILD_SHIPPING
VOXEL_RUN_ON_STARTUP_GAME()
{
	int32 NumMissing = 0;
	for (const UClass* Class : GetDerivedClasses<UVoxelFunctionLibrary>())
	{
		for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const UFunction* Function = *It;
			if (FVoxelFunctionLibraryRegistry::FindFunction(*Function))
			{
				continue;
			}

			LOG_VOXEL(Error, "Missing VOXEL_REGISTER_FUNCTION(%s, %s); in %s.cpp", *(FString(Class->GetPrefixCPP()) + Class->GetName()), *Function->GetName(), *Class->GetName());
			NumMissing++;
		}
	}
	ensureMsgf(NumMissing == 0, TEXT("%d UVoxelFunctionLibrary UFUNCTIONs are missing VOXEL_REGISTER_FUNCTION (see log)"), NumMissing);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelFunctionLibrary::RaiseBufferError() const
{
	PrivateNode->RaiseBufferError();
}

TSharedRef<FVoxelMessageToken> UVoxelFunctionLibrary::CreateMessageToken() const
{
	return PrivateNode->GetNodeRef().CreateMessageToken();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelPinType UVoxelFunctionLibrary::MakeType(const FProperty& Property)
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == StaticStructFast<FVoxelRuntimePinValue>())
		{
			return FVoxelPinType::MakeWildcard();
		}
		if (StructProperty->Struct->IsChildOf(StaticStructFast<FVoxelBuffer>()))
		{
			return FVoxelBuffer::FindInnerType(StructProperty->Struct).GetBufferType();
		}
	}

	return FVoxelPinType(Property);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelFunctionLibrary::FCachedFunction::FCachedFunction(const UFunction& Function)
	: Function(Function)
	, NativeCall(FVoxelFunctionLibraryRegistry::FindFunction(Function))
{
}

void UVoxelFunctionLibrary::Call(
	const FVoxelNode_UFunction& Node,
	const FCachedFunction& CachedFunction,
	const FVoxelGraphQuery InQuery,
	const TConstVoxelArrayView<FVoxelRuntimePinValue*> Values)
{
	FVoxelNodeStatScope StatScope(Node, 0);

	int32 Num = 1;
	if (StatScope.IsEnabled() ||
		AreVoxelStatsEnabled())
	{
		for (const FVoxelRuntimePinValue* Value : Values)
		{
			if (!Value ||
				!Value->IsValid())
			{
				continue;
			}

			if (Value->CanBeCastedTo<FVoxelBuffer>())
			{
				Num = FMath::Max(Num, Value->Get<FVoxelBuffer>().Num_Slow());
			}
		}

		StatScope.SetCount(Num);
	}

	{
		VOXEL_SCOPE_COUNTER_FORMAT("%s Num=%d", *CachedFunction.Function.GetName(), Num);
		VOXEL_SCOPE_COUNTER_FNAME(CachedFunction.Function.GetFName());

		TVoxelTypeCompatibleBytes<UVoxelFunctionLibrary> FunctionLibraryBytes;
#if VOXEL_DEBUG
		FMemory::Memzero(FunctionLibraryBytes);
#endif

		UVoxelFunctionLibrary& FunctionLibrary = FunctionLibraryBytes.GetValue();
		FunctionLibrary.Query = InQuery;
		FunctionLibrary.PrivateNode = &Node;

		if (!ensure(CachedFunction.NativeCall))
		{
			return;
		}

		CachedFunction.NativeCall(FunctionLibrary, Values);
	}

	if (!StatScope.IsEnabled() ||
		!CachedFunction.Function.GetReturnProperty())
	{
		return;
	}

	if (!ensure(Values.Num() > 0))
	{
		return;
	}

	const FVoxelRuntimePinValue* ReturnValue = Values.Last();
	if (!ReturnValue->IsBuffer())
	{
		return;
	}

	StatScope.SetCount(FMath::Max(Num, ReturnValue->Get<FVoxelBuffer>().Num_Slow()));
}