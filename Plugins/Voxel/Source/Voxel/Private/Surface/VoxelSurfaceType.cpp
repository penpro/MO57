// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "Surface/VoxelSurfaceType.h"
#include "Surface/VoxelSurfaceTypeTable.h"
#include "Surface/VoxelSurfaceTypeAsset.h"
#include "Surface/VoxelSmartSurfaceType.h"
#include "Surface/VoxelSurfaceTypeInterface.h"
#include "VoxelInvalidationCallstack.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"

const FName FVoxelSurfaceType::GuidTagName = "VoxelSurfaceTypeGuid";

#if !UE_BUILD_SHIPPING
TVoxelArray<FVoxelObjectPtr> GVoxelDebugSurfaceTypes;
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TVoxelSurfaceTypeImpl
{
	TVoxelObjectPtr<T> WeakSurfaceType;
	FName Name;
	FGuid Guid;

	void Update()
	{
		checkUObjectAccess();

		if (WeakSurfaceType.IsExplicitlyNull())
		{
			return;
		}

		const T* SurfaceType = WeakSurfaceType.Resolve();
		if (!ensure(SurfaceType))
		{
			return;
		}

		Name = SurfaceType->GetFName();
		Guid = SurfaceType->Guid;
	}
};

class FVoxelSurfaceTypeManager : public FVoxelSingleton
{
public:
	FVoxelSharedCriticalSection CriticalSection;

	template<typename T>
	struct TStorage
	{
		TVoxelArray<TVoxelSurfaceTypeImpl<T>> Slots;
		TVoxelMap<TObjectPtr<T>, uint16> ObjectToIndex;
		TVoxelFixedBitArray<FVoxelSurfaceType::MaxIndex> Valid;
	};

	TStorage<UVoxelSurfaceTypeAsset> SurfaceTypeAssets_RequiresLock;
	TStorage<UVoxelSmartSurfaceType> SmartSurfaceTypes_RequiresLock;

	struct FSurfaceTypeData
	{
		FVoxelSurfaceType::EClass Type = {};
		uint16 Index = 0;
	};
	TVoxelMap<FGuid, FSurfaceTypeData> GuidToData_RequiresLock;
	TVoxelMap<FGuid, FSoftObjectPath> GuidToUnloadedPath_RequiresLock;

public:
	FVoxelSurfaceTypeManager()
	{
		SurfaceTypeAssets_RequiresLock.Slots.Add({});
		SurfaceTypeAssets_RequiresLock.Valid.Add(false);

		SmartSurfaceTypes_RequiresLock.Slots.Add({});
		SmartSurfaceTypes_RequiresLock.Valid.Add(false);
	}

	template<typename T>
	void RegisterObject(
		T* Asset,
		const FVoxelSurfaceType::EClass Type,
		TStorage<T>& Storage,
		uint16& OutIndex,
		bool& bOutRegister)
	{
		{
			VOXEL_SCOPE_READ_LOCK(CriticalSection);

			if (const uint16* IndexPtr = Storage.ObjectToIndex.Find(Asset))
			{
				OutIndex = *IndexPtr;
				return;
			}
		}

		VOXEL_SCOPE_WRITE_LOCK(CriticalSection);

		if (const uint16* IndexPtr = Storage.ObjectToIndex.Find(Asset))
		{
			OutIndex = *IndexPtr;
			return;
		}

		check(Storage.Slots.Num() < FVoxelSurfaceType::MaxIndex);
		OutIndex = uint16(Storage.Slots.Add(TVoxelSurfaceTypeImpl<T>{ Asset }));
		Storage.Slots[OutIndex].Update();

		Storage.ObjectToIndex.Add_EnsureNew(Asset, OutIndex);
		ensure(Storage.Valid.Add(true) == OutIndex);

		if (const FSurfaceTypeData* ExistingData = GuidToData_RequiresLock.Find(Asset->Guid))
		{
			FName OtherName;
			switch (ExistingData->Type)
			{
			default: check(false);
			case FVoxelSurfaceType::EClass::SurfaceTypeAsset: OtherName = SurfaceTypeAssets_RequiresLock.Slots[ExistingData->Index].Name; break;
			case FVoxelSurfaceType::EClass::SmartSurfaceType: OtherName = SmartSurfaceTypes_RequiresLock.Slots[ExistingData->Index].Name; break;
			}
			VOXEL_MESSAGE(
				Error,
				"Surface Type {0} GUID collides with Surface Type {1}. Regenerate the GUID in one of the assets.",
				Asset->GetFName(),
				OtherName);
		}
		else
		{
			GuidToData_RequiresLock.Add_CheckNew(Asset->Guid, FSurfaceTypeData(Type, OutIndex));
		}

		// Once loaded, the asset is tracked via GuidToData; drop the unloaded fallback entry
		GuidToUnloadedPath_RequiresLock.Remove(Asset->Guid);

		bOutRegister = true;
	}

	void GetIndex(
		UVoxelSurfaceTypeInterface* SurfaceTypeInterface,
		FVoxelSurfaceType::EClass& OutType,
		uint16& OutIndex,
		bool& bOutRegister)
	{
		VOXEL_FUNCTION_COUNTER();
		checkUObjectAccess();

		if (!SurfaceTypeInterface)
		{
			return;
		}

		if (!SurfaceTypeInterface->Guid.IsValid())
		{
			SurfaceTypeInterface->Guid = FGuid::NewGuid();
			SurfaceTypeInterface->MarkPackageDirty();
			if (!GIsEditor)
			{
				VOXEL_MESSAGE(
					Error,
					"Surface Type {0} does not have a valid GUID.",
					SurfaceTypeInterface->GetName());
			}
		}

		if (UVoxelSurfaceTypeAsset* SurfaceTypeAsset = Cast<UVoxelSurfaceTypeAsset>(SurfaceTypeInterface))
		{
			OutType = FVoxelSurfaceType::EClass::SurfaceTypeAsset;
			RegisterObject(SurfaceTypeAsset, OutType, SurfaceTypeAssets_RequiresLock, OutIndex, bOutRegister);
			return;
		}

		if (UVoxelSmartSurfaceType* SmartSurfaceType = Cast<UVoxelSmartSurfaceType>(SurfaceTypeInterface))
		{
			OutType = FVoxelSurfaceType::EClass::SmartSurfaceType;
			RegisterObject(SmartSurfaceType, OutType, SmartSurfaceTypes_RequiresLock, OutIndex, bOutRegister);
			return;
		}
	}

	void RegisterFromAssetData(const FAssetData& AssetData)
	{
		VOXEL_FUNCTION_COUNTER();
		checkUObjectAccess();

		const UClass* AssetClass = AssetData.GetClass();
		if (!AssetClass ||
			!AssetClass->IsChildOf(UVoxelSurfaceTypeInterface::StaticClass()))
		{
			return;
		}

		FString GuidString;
		if (!AssetData.GetTagValue(FVoxelSurfaceType::GuidTagName, GuidString))
		{
			VOXEL_MESSAGE(
				Warning,
				"Surface Type {0} has no GUID asset registry tag. Re-save it to fix.",
				AssetData.AssetName);
			return;
		}

		FGuid Guid;
		if (!ensureMsgf(FGuid::Parse(GuidString, Guid), TEXT("Failed to parse Surface Type GUID '%s' for %s"), *GuidString, *AssetData.GetSoftObjectPath().ToString()) ||
			!ensureMsgf(Guid.IsValid(), TEXT("Invalid Surface Type GUID for %s"), *AssetData.GetSoftObjectPath().ToString()))
		{
			return;
		}

		VOXEL_SCOPE_WRITE_LOCK(CriticalSection);

		// If already loaded, nothing to do
		if (GuidToData_RequiresLock.Contains(Guid))
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
		Filter.ClassPaths.Add(UVoxelSurfaceTypeInterface::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		for (const FAssetData& AssetData : Assets)
		{
			RegisterFromAssetData(AssetData);
		}
	}

	void OnSurfaceTypeRegistered(const FVoxelSurfaceType SurfaceType) const
	{
		ensureVoxelSlow(!CriticalSection.IsLocked_Write());

		Voxel::GameTask([SurfaceType]
		{
#if !UE_BUILD_SHIPPING
			if (!GVoxelDebugSurfaceTypes.IsValidIndex(SurfaceType.RawValue))
			{
				GVoxelDebugSurfaceTypes.SetNum(SurfaceType.RawValue + 1);
			}

			GVoxelDebugSurfaceTypes[SurfaceType.RawValue] = SurfaceType.GetSurfaceTypeInterface();
#endif

			FVoxelInvalidationScope Scope("AddSurface " + SurfaceType.GetName());

			FVoxelSurfaceTypeTable::Refresh();
		});
	}

public:
	//~ Begin FVoxelSingleton Interface
	virtual void Initialize() override
	{
		VOXEL_FUNCTION_COUNTER();

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
		VOXEL_SCOPE_WRITE_LOCK(CriticalSection);

		const auto Collect = [&]<typename T>(TStorage<T>& Storage)
		{
			for (auto It = Storage.ObjectToIndex.CreateIterator(); It; ++It)
			{
				TObjectPtr<T> Type = It.Key();
				Collector.AddReferencedObject(Type);

				if (Type)
				{
					continue;
				}

				checkVoxelSlow(Storage.Valid[It.Value()]);
				Storage.Valid[It.Value()] = false;

				It.RemoveCurrent();
			}
		};

		Collect(SurfaceTypeAssets_RequiresLock);
		Collect(SmartSurfaceTypes_RequiresLock);
	}
	//~ End FVoxelSingleton Interface
};
FVoxelSurfaceTypeManager* GVoxelSurfaceTypeManager = new FVoxelSurfaceTypeManager();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSurfaceType::FVoxelSurfaceType(UVoxelSurfaceTypeInterface* SurfaceTypeInterface)
{
	EClass Type = EClass::SurfaceTypeAsset;
	uint16 Index = 0;
	bool bRegister = false;
	GVoxelSurfaceTypeManager->GetIndex(
		SurfaceTypeInterface, 
		Type, 
		Index,
		bRegister);

	InternalType = uint16(Type);
	InternalIndex = Index;

	if (bRegister)
	{
		GVoxelSurfaceTypeManager->OnSurfaceTypeRegistered(*this);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TVoxelSurfaceTypeImpl<UVoxelSurfaceTypeAsset>& FVoxelSurfaceType::GetSurfaceTypeAssetImpl() const
{
	VOXEL_SCOPE_READ_LOCK(GVoxelSurfaceTypeManager->CriticalSection);
	return GVoxelSurfaceTypeManager->SurfaceTypeAssets_RequiresLock.Slots[InternalIndex];
}

TVoxelSurfaceTypeImpl<UVoxelSmartSurfaceType>& FVoxelSurfaceType::GetSmartSurfaceTypeImpl() const
{
	VOXEL_SCOPE_READ_LOCK(GVoxelSurfaceTypeManager->CriticalSection);
	return GVoxelSurfaceTypeManager->SmartSurfaceTypes_RequiresLock.Slots[InternalIndex];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelObjectPtr<UVoxelSurfaceTypeAsset> FVoxelSurfaceType::GetSurfaceTypeAsset() const
{
	if (IsNull() ||
		!ensureVoxelSlow(GetClass() == EClass::SurfaceTypeAsset))
	{
		return {};
	}

	return GetSurfaceTypeAssetImpl().WeakSurfaceType;
}

TVoxelObjectPtr<UVoxelSmartSurfaceType> FVoxelSurfaceType::GetSmartSurfaceType() const
{
	if (IsNull() ||
		!ensureVoxelSlow(GetClass() == EClass::SmartSurfaceType))
	{
		return {};
	}

	return GetSmartSurfaceTypeImpl().WeakSurfaceType;
}

TVoxelObjectPtr<UVoxelSurfaceTypeInterface> FVoxelSurfaceType::GetSurfaceTypeInterface() const
{
	if (GetClass() == EClass::SurfaceTypeAsset)
	{
		return GetSurfaceTypeAsset();
	}
	else
	{
		checkVoxelSlow(GetClass() == EClass::SmartSurfaceType);
		return GetSmartSurfaceType();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FName FVoxelSurfaceType::GetFName() const
{
	if (IsNull())
	{
		return {};
	}

	switch (GetClass())
	{
	case EClass::SurfaceTypeAsset: return GetSurfaceTypeAssetImpl().Name;
	case EClass::SmartSurfaceType: return GetSmartSurfaceTypeImpl().Name;
	}

	return {};
}

FString FVoxelSurfaceType::GetName() const
{
	if (IsNull())
	{
		return {};
	}

	switch (GetClass())
	{
	case EClass::SurfaceTypeAsset: return GetSurfaceTypeAssetImpl().Name.ToString();
	case EClass::SmartSurfaceType: return GetSmartSurfaceTypeImpl().Name.ToString();
	}

	return {};
}

FLinearColor FVoxelSurfaceType::GetDebugColor() const
{
	return FLinearColor::IntToDistinctColor(RawValue, 1.f, 0.75f, 90.f);
}

FGuid FVoxelSurfaceType::GetGuid() const
{
	if (IsNull())
	{
		return {};
	}

	switch (GetClass())
	{
	case EClass::SurfaceTypeAsset: return GetSurfaceTypeAssetImpl().Guid;
	case EClass::SmartSurfaceType: return GetSmartSurfaceTypeImpl().Guid;
	}

	return {};
}

void operator<<(FArchive& Ar, FVoxelSurfaceType& SurfaceType)
{
	if (Ar.GetArchiveName().StartsWith("FVoxel"))
	{
		FGuid Guid;

		if (Ar.IsSaving())
		{
			Guid = SurfaceType.GetGuid();
		}

		Ar << Guid;

		if (Ar.IsLoading())
		{
			if (!FVoxelSurfaceType::FindFromGuid(
				Guid,
				SurfaceType))
			{
				Ar.SetError();
			}
		}

		return;
	}

	UVoxelSurfaceTypeInterface* SurfaceTypeObject = nullptr;

	if (Ar.IsSaving())
	{
		SurfaceTypeObject = SurfaceType.GetSurfaceTypeInterface().Resolve();
		ensureVoxelSlow(SurfaceTypeObject);
	}

	Ar << SurfaceTypeObject;

	if (Ar.IsLoading())
	{
		SurfaceType = FVoxelSurfaceType(SurfaceTypeObject);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSurfaceType::UpdateFromSourceObject() const
{
	if (!ensureVoxelSlow(!IsNull()))
	{
		return;
	}

	switch (GetClass())
	{
	case EClass::SurfaceTypeAsset: GetSurfaceTypeAssetImpl().Update(); break;
	case EClass::SmartSurfaceType: GetSmartSurfaceTypeImpl().Update(); break;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSurfaceType::ForeachSurfaceType(const TFunctionRef<void(FVoxelSurfaceType)> Lambda)
{
	VOXEL_FUNCTION_COUNTER();

	for (const int32 Index : GVoxelSurfaceTypeManager->SurfaceTypeAssets_RequiresLock.Valid.IterateSetBits())
	{
		FVoxelSurfaceType SurfaceType;
		SurfaceType.InternalType = uint16(EClass::SurfaceTypeAsset);
		SurfaceType.InternalIndex = uint16(Index);
		Lambda(SurfaceType);
	}

	for (const int32 Index : GVoxelSurfaceTypeManager->SmartSurfaceTypes_RequiresLock.Valid.IterateSetBits())
	{
		FVoxelSurfaceType SurfaceType;
		SurfaceType.InternalType = uint16(EClass::SmartSurfaceType);
		SurfaceType.InternalIndex = uint16(Index);
		Lambda(SurfaceType);
	}
}

bool FVoxelSurfaceType::FindFromGuid(
	const FGuid Guid,
	FVoxelSurfaceType& OutSurfaceType)
{
	if (!ensure(Guid.IsValid()))
	{
		return true;
	}

	VOXEL_SCOPE_READ_LOCK(GVoxelSurfaceTypeManager->CriticalSection);

	if (const FVoxelSurfaceTypeManager::FSurfaceTypeData* Data = GVoxelSurfaceTypeManager->GuidToData_RequiresLock.Find(Guid))
	{
		OutSurfaceType.InternalType = uint16(Data->Type);
		OutSurfaceType.InternalIndex = Data->Index;
		return true;
	}

	if (const FSoftObjectPath* UnloadedPath = GVoxelSurfaceTypeManager->GuidToUnloadedPath_RequiresLock.Find(Guid))
	{
		VOXEL_MESSAGE(
			Error,
			"Surface Type {0} is referenced by a save but is not loaded. Hard-reference it from your GameInstance (or any always-loaded object) so it loads before the save is applied.",
			UnloadedPath->ToString());
	}
	else
	{
		VOXEL_MESSAGE(
			Error,
			"Failed to find surface type with GUID {0}. The asset may have been deleted, or it has not been re-saved since the GUID asset registry tag was added.",
			Guid.ToString());
	}
	return false;
}