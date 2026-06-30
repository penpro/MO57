// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBuffer.h"
#include "VoxelGraphQuery.h"
#include "VoxelBufferAccessor.h"
#include "VoxelRuntimePinValue.h"
#include "VoxelFunctionLibrary.generated.h"

struct FVoxelNode_UFunction;

struct VOXELGRAPH_API FVoxelFunctionLibraryRegistry
{
public:
	using FGetClassFuncPtr = UClass* (*)();
	using FNativeFuncPtr = void (*)(UVoxelFunctionLibrary& Context, TConstVoxelArrayView<FVoxelRuntimePinValue*> Values);

public:
	static FNativeFuncPtr FindFunction(const UFunction& Function);
};

struct VOXELGRAPH_API FVoxelFunctionRegistration
{
	const FVoxelFunctionLibraryRegistry::FGetClassFuncPtr GetClassPtr;
	const TCHAR* const Name;
	const FVoxelFunctionLibraryRegistry::FNativeFuncPtr NativeFunction;
	FVoxelFunctionRegistration* Next;

	FVoxelFunctionRegistration(
		FVoxelFunctionLibraryRegistry::FGetClassFuncPtr GetClassPtr,
		const TCHAR* Name,
		FVoxelFunctionLibraryRegistry::FNativeFuncPtr NativeFunction);
};

UCLASS()
class VOXELGRAPH_API UVoxelFunctionLibrary
	: public UObject
	, public FVoxelBufferInitializers
{
	GENERATED_BODY()

public:
	FVoxelGraphQuery Query;

	void RaiseBufferError() const;
	TSharedRef<FVoxelMessageToken> CreateMessageToken() const;

private:
	const FVoxelNode_UFunction* PrivateNode = nullptr;

public:
	static FVoxelPinType MakeType(const FProperty& Property);

	struct FCachedFunction
	{
		const UFunction& Function;
		const FVoxelFunctionLibraryRegistry::FNativeFuncPtr NativeCall;
		explicit FCachedFunction(const UFunction& Function);
	};
	static void Call(
		const FVoxelNode_UFunction& Node,
		const FCachedFunction& CachedFunction,
		FVoxelGraphQuery InQuery,
		TConstVoxelArrayView<FVoxelRuntimePinValue*> Values);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<auto Function>
struct TVoxelFunctionExec
{
private:
	using Info = TVoxelFunctionInfo<decltype(Function)>;
	using ClassType = typename Info::Class;
	using ReturnType = typename Info::ReturnType;
	using ArgsType = typename Info::ArgTypes;

	template<typename... Args, std::size_t... Is>
	static void CallImpl(
		ClassType& FunctionLibrary,
		TConstVoxelArrayView<FVoxelRuntimePinValue*> Values,
		TVoxelTypes<Args...>,
		std::index_sequence<Is...>)
	{
		const auto UnpackArg = [&]<typename Arg>(FVoxelRuntimePinValue* Value) -> decltype(auto)
		{
			using DecayedArg = std::decay_t<Arg>;
			if constexpr (std::is_same_v<DecayedArg, FVoxelRuntimePinValue>)
			{
				return *Value;
			}
			else if constexpr (std::is_enum_v<DecayedArg>)
			{
				// Enums are stored as uint8 in FVoxelRuntimePinValue
				return static_cast<DecayedArg>(Value->Get<uint8>());
			}
			else if constexpr (std::is_lvalue_reference_v<Arg> && !std::is_const_v<std::remove_reference_t<Arg>>)
			{
				return ConstCast(Value->Get<DecayedArg>());
			}
			else
			{
				return Value->Get<DecayedArg>();
			}
		};

		if constexpr (std::is_void_v<ReturnType>)
		{
			(FunctionLibrary.*Function)(UnpackArg.template operator()<Args>(Values[Is])...);
		}
		else if constexpr (std::is_same_v<ReturnType, FVoxelRuntimePinValue>)
		{
			ConstCast(*Values[sizeof...(Args)]) = (FunctionLibrary.*Function)(UnpackArg.template operator()<Args>(Values[Is])...);
		}
		else if constexpr (std::is_enum_v<ReturnType>)
		{
			ConstCast(Values[sizeof...(Args)]->Get<uint8>()) = static_cast<uint8>((FunctionLibrary.*Function)(UnpackArg.template operator()<Args>(Values[Is])...));
		}
		else
		{
			ConstCast(Values[sizeof...(Args)]->Get<ReturnType>()) = (FunctionLibrary.*Function)(UnpackArg.template operator()<Args>(Values[Is])...);
		}
	}

	template<typename... Args>
	static void Dispatch(
		UVoxelFunctionLibrary& Base,
		TConstVoxelArrayView<FVoxelRuntimePinValue*> Values,
		TVoxelTypes<Args...> ArgsTag)
	{
		CallImpl(static_cast<ClassType&>(Base), Values, ArgsTag, std::index_sequence_for<Args...>{});
	}

public:
	static void Exec(
		UVoxelFunctionLibrary& Base,
		TConstVoxelArrayView<FVoxelRuntimePinValue*> Values)
	{
		Dispatch(Base, Values, ArgsType{});
	}
};

#define VOXEL_REGISTER_FUNCTION(Class, Function) \
	static const FVoxelFunctionRegistration VOXEL_APPEND_LINE(VoxelFunctionRegistration_##Function)( \
		Class::StaticClass, \
		TEXT(#Function), \
		&TVoxelFunctionExec<&Class::Function>::Exec)