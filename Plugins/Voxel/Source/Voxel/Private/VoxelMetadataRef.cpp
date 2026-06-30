// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMetadataRef.h"
#include "VoxelBuffer.h"
#include "VoxelMetadata.h"
#include "VoxelObjectPinType.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"

const FName FVoxelMetadataRef::GuidTagName = "VoxelMetadataGuid";

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
	FGuid Guid;
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
		Guid = Metadata->Guid;
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
	TVoxelMap<FGuid, int32> GuidToIndex_RequiresLock;
	TVoxelMap<FGuid, FSoftObjectPath> GuidToUnloadedPath_RequiresLock;

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

		UVoxelMetadata* Metadata = WeakMetadata.Resolve();
		if (!ensure(Metadata))
		{
			return -1;
		}

		if (!Metadata->Guid.IsValid())
		{
			Metadata->Guid = FGuid::NewGuid();
			Metadata->MarkPackageDirty();
			if (!GIsEditor)
			{
				VOXEL_MESSAGE(
					Error,
					"Metadata {0} does not have a valid GUID.",
					Metadata->GetName());
			}
		}

		MetadatasToKeepAlive.Add(Metadata);

		const int32 Index = Metadatas.Add(FVoxelMetadataImpl
		{
			WeakMetadata
		});

		Metadatas[Index].Update();

		MetadataToIndex_RequiresLock.Add_EnsureNew(WeakMetadata, Index);

		if (const int32* ExistingIndexPtr = GuidToIndex_RequiresLock.Find(Metadata->Guid))
		{
			VOXEL_MESSAGE(
				Error,
				"Metadata {0} GUID is colliding with metadata asset {1}. Regenerate the GUID in one of the assets.",
				Metadata->GetFName(),
				Metadatas[*ExistingIndexPtr].Name);
		}
		else
		{
			GuidToIndex_RequiresLock.Add_CheckNew(Metadata->Guid, Index);
		}

		GuidToUnloadedPath_RequiresLock.Remove(Metadata->Guid);

		return Index;
	}

	void RegisterFromAssetData(const FAssetData& AssetData)
	{
		VOXEL_FUNCTION_COUNTER();
		checkUObjectAccess();

		const UClass* AssetClass = AssetData.GetClass();
		if (!AssetClass ||
			!AssetClass->IsChildOf(UVoxelMetadata::StaticClass()))
		{
			return;
		}

		FString GuidString;
		if (!AssetData.GetTagValue(FVoxelMetadataRef::GuidTagName, GuidString))
		{
			VOXEL_MESSAGE(
				Warning,
				"Metadata {0} has no GUID asset registry tag. Re-save it to fix.",
				AssetData.AssetName);
			return;
		}

		FGuid Guid;
		if (!ensure(FGuid::Parse(GuidString, Guid)) ||
			!ensure(Guid.IsValid()))
		{
			return;
		}

		VOXEL_SCOPE_WRITE_LOCK(CriticalSection);

		if (GuidToIndex_RequiresLock.Contains(Guid))
		{
			return;
		}

		GuidToUnloadedPath_RequiresLock.FindOrAdd(Guid) = AssetData.ToSoftObjectPath();
	}

	void PopulateFromAssetRegistry()
	{
		VOXEL_FUNCTION_COUNTER();
		check(IsInGameThread());

		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

		FARFilter Filter;
		Filter.ClassPaths.Add(UVoxelMetadata::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		for (const FAssetData& AssetData : Assets)
		{
			RegisterFromAssetData(AssetData);
		}
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

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if (AssetRegistry.IsLoadingAssets())
		{
			AssetRegistry.OnFilesLoaded().AddLambda([this]
			{
				PopulateFromAssetRegistry();
			});
		}
		else
		{
			PopulateFromAssetRegistry();
		}

		AssetRegistry.OnAssetAdded().AddLambda([this](const FAssetData& AssetData)
		{
			RegisterFromAssetData(AssetData);
		});
		AssetRegistry.OnAssetUpdatedOnDisk().AddLambda([this](const FAssetData& AssetData)
		{
			RegisterFromAssetData(AssetData);
		});
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

FString FVoxelMetadataRef::GetName() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().Name.ToString();
}

FGuid FVoxelMetadataRef::GetGuid() const
{
	if (!IsValid())
	{
		return {};
	}

	return GetImpl().Guid;
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

void operator<<(FArchive& Ar, FVoxelMetadataRef& MetadataRef)
{
	if (Ar.GetArchiveName().StartsWith("FVoxel"))
	{
		FGuid Guid;

		if (Ar.IsSaving())
		{
			Guid = MetadataRef.GetGuid();
		}

		Ar << Guid;

		if (Ar.IsLoading())
		{
			if (!FVoxelMetadataRef::FindFromGuid(
				Guid,
				MetadataRef))
			{
				Ar.SetError();
			}
		}

		return;
	}

	UVoxelMetadata* Metadata = nullptr;

	if (Ar.IsSaving())
	{
		Metadata = MetadataRef.GetMetadata().Resolve();
		ensureVoxelSlow(Metadata);
	}

	Ar << Metadata;

	if (Ar.IsLoading())
	{
		MetadataRef = FVoxelMetadataRef(Metadata);
	}
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

bool FVoxelMetadataRef::FindFromGuid(
	const FGuid Guid,
	FVoxelMetadataRef& OutMetadata)
{
	if (!ensure(Guid.IsValid()))
	{
		return true;
	}

	VOXEL_SCOPE_READ_LOCK(GVoxelMetadataSingleton->CriticalSection);

	if (const int32* IndexPtr = GVoxelMetadataSingleton->GuidToIndex_RequiresLock.Find(Guid))
	{
		OutMetadata.PrivateIndex = *IndexPtr;
		return true;
	}

	if (const FSoftObjectPath* UnloadedPath = GVoxelMetadataSingleton->GuidToUnloadedPath_RequiresLock.Find(Guid))
	{
		VOXEL_MESSAGE(
			Error,
			"Metadata {0} is referenced by a save but is not loaded. Hard-reference it from your GameInstance (or any always-loaded object) so it loads before the save is applied.",
			UnloadedPath->ToString());
	}
	else
	{
		VOXEL_MESSAGE(
			Error,
			"Failed to find metadata with GUID {0}. The asset may have been deleted, or it has not been re-saved since the GUID asset registry tag was added.",
			Guid.ToString());
	}
	return false;
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