// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "VoxelBuffer.h"
#include "VoxelGraphQuery.h"
#include "VoxelBufferAccessor.h"
#include "VoxelRuntimePinValue.h"
#include "VoxelFunctionLibrary.generated.h"

struct FVoxelNode_UFunction;

UCLASS()
class VOXELGRAPH_API UVoxelFunctionLibrary
	: public UObject
	, public FVoxelBufferInitializers
{
	GENERATED_BODY()

public:
	using FVoxelNativeFuncPtr = void (*)(UVoxelFunctionLibrary& Context, TConstVoxelArrayView<FVoxelRuntimePinValue*> Values);
	static void RegisterFunction(const UClass* Class, FName FunctionName, FVoxelNativeFuncPtr NativeFunction);
	static FVoxelNativeFuncPtr FindFunction(const UFunction& Function);

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
		const FVoxelNativeFuncPtr NativeCall;
		explicit FCachedFunction(const UFunction& Function);
	};
	static void Call(
		const FVoxelNode_UFunction& Node,
		const FCachedFunction& CachedFunction,
		FVoxelGraphQuery InQuery,
		TConstVoxelArrayView<FVoxelRuntimePinValue*> Values);
};