// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "VoxelMetadataRef.h"
#include "VoxelBuffer.h"
#include "VoxelMetadata.h"
#include "VoxelObjectPinType.h"
#include "Buffer/VoxelBaseBuffers.h"

struct FVoxelMetadataRefObjectPinType : public FVoxelObjectPinType
{
	UScriptStruct* MetadataRefStruct = nullptr;
	UClass* MetadataClass = nullptr;

	//~ Begin FVoxelObjectPinType Interface
	virtual UScriptStruct* GetStruct() const override
	{
		return MetadataRefStruct;
	}
	virtual UClass* GetClass() const override
	{
		return MetadataClass;
	}
	virtual TVoxelObjectPtr<UObject> GetWeakObject(const FConstVoxelStructView Struct) const override
	{
		return Struct.Get<FVoxelMetadataRef>().GetMetadata();
	}
	virtual FVoxelInstancedStruct GetStruct(UObject* Object) const override
	{
		FVoxelInstancedStruct Result = FVoxelInstancedStruct(MetadataRefStruct);
		if (Object)
		{
			Result.Get<FVoxelMetadataRef>() = FVoxelMetadataRef(CastChecked<UVoxelMetadata>(Object));
		}
		return Result;
	}
	//~ End FVoxelObjectPinType Interface
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct FVoxelMetadataImpl
{
	TVoxelObjectPtr<UVoxelMetadata> WeakMetadata;
	FName Name;
	FVoxelPinType InnerType;
	FVoxelPinValue ExposedDefaultValue;
	FVoxelRuntimePinValue DefaultValue;
	TVoxelOptional<EVoxelMetadataMaterialType> MaterialType;
	FVoxelMetadataRefStatics Statics;

	void Update()
	{
		checkUObjectAccess();

		const UVoxelMetadata* Metadata = WeakMetadata.Resolve();
		if (!ensure(Metadata))
		{
			return;
		}

		const FVoxelPinValue NewDefaultValue = Metadata->GetDefaultValue();
		const FVoxelPinValue OldDefaultValue = MoveTemp(ExposedDefaultValue);

		Name = Metadata->GetFName();
		InnerType = Metadata->GetInnerType();
		ExposedDefaultValue = NewDefaultValue;
		DefaultValue = FVoxelPinType::MakeRuntimeValue(InnerType, NewDefaultValue, {});
		MaterialType = Metadata->GetMaterialType();
		Statics = Metadata->GetStatics();

		if (OldDefaultValue.IsValid() &&
			OldDefaultValue != NewDefaultValue)
		{
			Voxel::RefreshAll();
		}
	}
};

class FVoxelMetadataSingleton : public FVoxelSingleton
{
public:
	TVoxelChunkedArray<FVoxelMetadataImpl> Metadatas;

	FVoxelSharedCriticalSection CriticalSection;
	TVoxelMap<TVoxelObjectPtr<UVoxelMetadata>, int32> MetadataToIndex_RequiresLock;

	// Make sure metadatas are never GCed
	TVoxelArray<TObjectPtr<const UVoxelMetadata>> MetadatasToKeepAlive;

	int32 GetIndex(const TVoxelObjectPtr<UVoxelMetadata> WeakMetadata)
	{
		checkVoxelSlow(!WeakMetadata.IsExplicitlyNull());

		{
			VOXEL_SCOPE_READ_LOCK(CriticalSection);

			if (const int32* Index = MetadataToIndex_RequiresLock.Find(WeakMetadata))
			{
				return *Index;
			}
		}

		VOXEL_FUNCTION_COUNTER();
		VOXEL_SCOPE_WRITE_LOCK(CriticalSection);
		checkUObjectAccess();

		if (const int32* Index = MetadataToIndex_RequiresLock.Find(WeakMetadata))
		{
			return *Index;
		}

		const UVoxelMetadata* Metadata = WeakMetadata.Resolve();
		if (!ensure(Metadata))
		{
			return -1;
		}

		MetadatasToKeepAlive.Add(Metadata);

		const int32 Index = Metadatas.Add(FVoxelMetadataImpl
		{
			WeakMetadata
		});

		Metadatas[Index].Update();

		MetadataToIndex_RequiresLock.Add_EnsureNew(WeakMetadata, Index);
		return Index;
	}

	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		VOXEL_FUNCTION_COUNTER();

		TVoxelArray<TSubclassOf<UVoxelMetadata>> Classes = GetDerivedClasses<UVoxelMetadata>();
		Classes.Add(UVoxelMetadata::StaticClass());

		for (const TSubclassOf<UVoxelMetadata>& Class : Classes)
		{
			const TSharedRef<FVoxelMetadataRefObjectPinType> PinType = MakeShared<FVoxelMetadataRefObjectPinType>();
			PinType->MetadataRefStruct = Class.GetDefaultObject()->GetMetadataRefStruct();
			PinType->MetadataClass = Class;
			FVoxelObjectPinType::RegisterPinType(PinType);
		}
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		VOXEL_FUNCTION_COUNTER();

		Collector.AddReferencedObjects(MetadatasToKeepAlive);
	}
	//~ End FVoxelSingleton Interface
};
FVoxelMetadataSingleton* GVoxelMetadataSingleton = new FVoxelMetadataSingleton();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMetadataRef::FVoxelMetadataRef(const TVoxelObjectPtr<UVoxelMetadata> Metadata)
{
	if (Metadata.IsExplicitlyNull())
	{
		return;
	}

	PrivateIndex = GVoxelMetadataSingleton->GetIndex(Metadata);
}

FVoxelMetadataRef::FVoxelMetadataRef(UVoxelMetadata* Metadata)
	: FVoxelMetadataRef(MakeVoxelObjectPtr(Metadata))
{
}

FVoxelMetadataRef::FVoxelMetadataRef(const TObjectPtr<UVoxelMetadata>& Metadata)
	: FVoxelMetadataRef(MakeVoxelObjectPtr(Metadata))
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FName FVoxelMetadataRef::GetFName() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().Name;
}

FVoxelPinType FVoxelMetadataRef::GetInnerType() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().InnerType;
}

FVoxelRuntimePinValue FVoxelMetadataRef::GetDefaultValue() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().DefaultValue;
}

TVoxelOptional<EVoxelMetadataMaterialType> FVoxelMetadataRef::GetMaterialType() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().MaterialType;
}

TVoxelObjectPtr<UVoxelMetadata> FVoxelMetadataRef::GetMetadata() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().WeakMetadata;
}

TSharedRef<FVoxelBuffer> FVoxelMetadataRef::MakeDefaultBuffer(const int32 Num) const
{
	VOXEL_FUNCTION_COUNTER();
	check(IsValid());

	const TSharedRef<FVoxelBuffer> Buffer = FVoxelBuffer::MakeEmpty(GetInnerType());
	Buffer->Allocate(Num);
	Buffer->SetAllGeneric(GetDefaultValue());
	return Buffer;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMetadataRef::UpdateFromSourceObject() const
{
	if (!ensureVoxelSlow(IsValid()))
	{
		return;
	}

	return GetImpl().Update();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMetadataRef::Blend(
	const FVoxelBuffer& Value,
	const FVoxelFloatBuffer& Alpha,
	FVoxelBuffer& InOutResult) const
{
	checkVoxelSlow(IsValid());
	checkVoxelSlow(Value.IsConstant_Slow() || Value.Num_Slow() == InOutResult.Num_Slow());
	checkVoxelSlow(Alpha.IsConstant() || Alpha.Num() == InOutResult.Num_Slow());

	GetStatics().Blend(Value, Alpha, InOutResult);
}

void FVoxelMetadataRef::IndirectBlend(
	const TConstVoxelArrayView<int32> IndexToResult,
	const FVoxelBuffer& Value,
	const FVoxelFloatBuffer& Alpha,
	FVoxelBuffer& InOutResult) const
{
	checkVoxelSlow(IsValid());
	checkVoxelSlow(Value.IsConstant_Slow() || Value.Num_Slow() == IndexToResult.Num());
	checkVoxelSlow(Alpha.IsConstant() || Alpha.Num() == IndexToResult.Num());

	GetStatics().IndirectBlend(IndexToResult, Value, Alpha, InOutResult);
}

void FVoxelMetadataRef::IndirectBlend(
	const TVoxelOptional<FVoxelInt32Buffer>& IndexToResult,
	const FVoxelBuffer& Value,
	const FVoxelFloatBuffer& Alpha,
	FVoxelBuffer& InOutResult) const
{
	VOXEL_FUNCTION_COUNTER();

	if (Alpha.IsConstant())
	{
		const float Constant = FMath::Clamp(Alpha.GetConstant(), 0.f, 1.f);

		if (Constant == 0.f)
		{
			return;
		}

		if (Constant == 1.f)
		{
			InOutResult.IndirectCopyFrom(Value, IndexToResult);
			return;
		}
	}

	if (IndexToResult)
	{
		IndirectBlend(
			IndexToResult->View(),
			Value,
			Alpha,
			InOutResult);
	}
	else
	{
		Blend(
			Value,
			Alpha,
			InOutResult);
	}
}

void FVoxelMetadataRef::AddToPCG(
	UPCGMetadata& PCGMetadata,
	const TConstVoxelArrayView<FPCGPoint> Points,
	const FName Name,
	const FVoxelBuffer& Values) const
{
	checkVoxelSlow(IsValid());

	if (!ensure(Values.IsConstant_Slow() || Values.Num_Slow() == Points.Num()))
	{
		return;
	}

	GetStatics().AddToPCG(PCGMetadata, Points, Name, Values);
}

void FVoxelMetadataRef::WriteMaterialData(
	const FVoxelBuffer& Values,
	const TVoxelArrayView<uint8> OutBytes,
	const EVoxelMetadataMaterialType MaterialType) const
{
	checkVoxelSlow(IsValid());
	GetStatics().WriteMaterialData(Values, OutBytes, MaterialType);
}

FVoxelPinValue FVoxelMetadataRef::GetValue(
	const FVoxelBuffer& Buffer,
	const int32 Index) const
{
	checkVoxelSlow(IsValid());

	if (!ensure(Buffer.IsValidIndex_Slow(Index)))
	{
		return {};
	}

	return GetStatics().GetValue(Buffer, Index);
}

FVoxelFloatBuffer FVoxelMetadataRef::GetChannel(
	const FVoxelBuffer& Buffer,
	const EVoxelTextureChannel Channel) const
{
	checkVoxelSlow(IsValid());
	return GetStatics().GetChannel(Buffer, Channel);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelArray<FVoxelMetadataRef> FVoxelMetadataRef::GetUniqueValidRefs(const TConstVoxelArrayView<UVoxelMetadata*> Metadatas)
{
	TVoxelArray<FVoxelMetadataRef> Result;
	Result.Reserve(Metadatas.Num());

	for (UVoxelMetadata* Metadata : Metadatas)
	{
		if (Metadata)
		{
			Result.AddUnique(FVoxelMetadataRef(Metadata));
		}
	}

	return Result;
}

TVoxelArray<FVoxelMetadataRef> FVoxelMetadataRef::GetUniqueValidRefs(const TConstVoxelArrayView<TObjectPtr<UVoxelMetadata>> Metadatas)
{
	TVoxelArray<FVoxelMetadataRef> Result;
	Result.Reserve(Metadatas.Num());

	for (UVoxelMetadata* Metadata : Metadatas)
	{
		if (Metadata)
		{
			Result.AddUnique(FVoxelMetadataRef(Metadata));
		}
	}

	return Result;
}

TVoxelArray<FVoxelMetadataRef> FVoxelMetadataRef::GetUniqueValidRefs(const TConstVoxelArrayView<FVoxelMetadataRef> Refs)
{
	TVoxelArray<FVoxelMetadataRef> Result;
	Result.Reserve(Refs.Num());

	for (const FVoxelMetadataRef& Ref : Refs)
	{
		if (Ref.IsValid())
		{
			Result.AddUnique(Ref);
		}
	}

	return Result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FORCEINLINE FVoxelMetadataImpl& FVoxelMetadataRef::GetImpl() const
{
	return GVoxelMetadataSingleton->Metadatas[PrivateIndex];
}

FORCEINLINE const FVoxelMetadataRefStatics& FVoxelMetadataRef::GetStatics() const
{
	return GetImpl().Statics;
}