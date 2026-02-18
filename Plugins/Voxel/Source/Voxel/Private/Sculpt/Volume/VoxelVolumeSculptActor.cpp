// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Sculpt/Volume/VoxelVolumeSculptActor.h"
#include "Sculpt/Volume/VoxelVolumeSculptData.h"
#include "Sculpt/Volume/VoxelVolumeSculptInnerData.h"
#include "Sculpt/VoxelSculptSave.h"
#include "Sculpt/VoxelSculptSaveAsset.h"
#include "VoxelVersion.h"

#include "TransactionCommon.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "Editor/TransBuffer.h"
#endif

class FVoxelVolumeSculptActorSingleton : public FVoxelSingleton
{
public:
	TVoxelMap<FGuid, TSharedPtr<FVoxelVolumeSculptData>> GuidToBulkData;

	//~ Begin FVoxelSingleton Interface
	virtual void Tick() override
	{
#if WITH_EDITOR
		if (!GIsTransacting &&
			GEditor &&
			GEditor->Trans &&
			CastChecked<UTransBuffer>(GEditor->Trans)->UndoBuffer.Num() == 0)
		{
			GuidToBulkData.Reset();
		}
#endif
	}
	//~ End FVoxelSingleton Interface
};
FVoxelVolumeSculptActorSingleton* GVoxelVolumeSculptActorSingleton = new FVoxelVolumeSculptActorSingleton();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelVolumeSculptStampRef UVoxelVolumeSculptComponent::GetStamp() const
{
	if (!ensure(PrivateStamp))
	{
		ConstCast(PrivateStamp) = FVoxelVolumeSculptStampRef::New();
	}

	if (ExternalSaveAsset &&
		ExternalSaveAsset->GetSculptData() != PrivateStamp->GetData())
	{
		OnExternalSaveAssetPropertyChanged = MakeSharedVoid();

		ExternalSaveAsset->OnPropertyChanged.Add(MakeWeakPtrDelegate(OnExternalSaveAssetPropertyChanged, MakeWeakObjectPtrLambda(this, [this]
		{
			PrivateStamp->Scale = ExternalSaveAsset->Scale;
			PrivateStamp->bUseFastDistances = ExternalSaveAsset->bUseFastDistances;
			PrivateStamp->bEnableDiffing = ExternalSaveAsset->bEnableDiffing;
			PrivateStamp->StackOverride = ExternalSaveAsset->StackOverride;
			PrivateStamp->SetData(ExternalSaveAsset->GetSculptData());

			ConstCast(this)->UpdateStamp();
		})));

		PrivateStamp->SetData(ExternalSaveAsset->GetSculptData());
	}

	return PrivateStamp;
}

void UVoxelVolumeSculptComponent::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	Super::Serialize(Ar);

	SerializeVoxelVersion(Ar);

	if (Ar.CustomVer(GVoxelCustomVersionGUID) < FVoxelVersion::RemoveSculptStamps)
	{
		return;
	}

	FVoxelSerializationGuard Guard(Ar);

	if (Ar.CustomVer(GVoxelCustomVersionGUID) >= FVoxelVersion::AddExternalSculptSaves)
	{
		bool bHasExternalSaveAsset = ExternalSaveAsset != nullptr;
		Ar << bHasExternalSaveAsset;

		if (bHasExternalSaveAsset)
		{
			return;
		}
	}

	if (Ar.IsTransacting() ||
		UE::Transaction::DiffUtil::IsGeneratingDiffableObject(Ar))
	{
		// Don't serialize bulk data, that would be too big/take too long
		// Instead, store a shared snapshot of the data globally and serialize a GUID to it
		// This data is cleared when the undo transaction buffer is cleared

		FGuid Guid;

		if (Ar.IsSaving())
		{
			Guid = FGuid::NewGuid();
			GVoxelVolumeSculptActorSingleton->GuidToBulkData.Add_EnsureNew(Guid, GetStamp()->GetData());
		}

		Ar << Guid;

		if (Ar.IsLoading())
		{
			const TSharedPtr<FVoxelVolumeSculptData> Data = GVoxelVolumeSculptActorSingleton->GuidToBulkData.FindRef(Guid);
			if (ensureVoxelSlow(Data))
			{
				GetStamp()->SetData(Data.ToSharedRef());
			}
		}
	}
	else if (Ar.IsSaving())
	{
		ConstCast(GetStamp()->GetData()->GetInnerData())->Serialize(Ar);
	}
	else if (Ar.IsLoading())
	{
		const TSharedRef<FVoxelVolumeSculptInnerData> NewInnerData = MakeShared<FVoxelVolumeSculptInnerData>(GetStamp()->bUseFastDistances);
		NewInnerData->Serialize(Ar);
		GetStamp()->SetData(MakeShared<FVoxelVolumeSculptData>(nullptr, NewInnerData));
	}
	else if (Ar.IsCountingMemory())
	{
		ConstCast(GetStamp()->GetData()->GetInnerData())->Serialize(Ar);
	}
}

FVoxelStampRef UVoxelVolumeSculptComponent::GetStamp_Internal() const
{
	return GetStamp();
}

#if WITH_EDITOR
bool UVoxelVolumeSculptComponent::CanEditChange(const FEditPropertyChain& PropertyChain) const
{
	if (TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* Node = PropertyChain.GetActiveMemberNode())
	{
		if (const FProperty* Property = Node->GetValue())
		{
			if (Property->GetFName() == GET_OWN_MEMBER_NAME(PrivateStamp))
			{
				if (ExternalSaveAsset)
				{
					return false;
				}
			}
		}
	}

	return UObject::CanEditChange(PropertyChain);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AVoxelVolumeSculptActor::AVoxelVolumeSculptActor()
{
	bReplicates = true;

	Component = CreateDefaultSubobject<UVoxelVolumeSculptComponent>("RootComponent");
	RootComponent = Component;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelVolumeSculptStampRef AVoxelVolumeSculptActor::GetStamp() const
{
	check(Component);
	return Component->GetStamp();
}

UVoxelVolumeSculptSaveAsset* AVoxelVolumeSculptActor::GetExternalSaveAsset() const
{
	return Component->ExternalSaveAsset;
}

void AVoxelVolumeSculptActor::SetExternalSaveAsset(UVoxelVolumeSculptSaveAsset* NewExternalSaveAsset)
{
	VOXEL_FUNCTION_COUNTER();

	if (!NewExternalSaveAsset)
	{
		Component->PrivateStamp = FVoxelVolumeSculptStampRef::New();
		Component->UpdateStamp();
		return;
	}

	Component->ExternalSaveAsset = NewExternalSaveAsset;
	Component->UpdateStamp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture AVoxelVolumeSculptActor::ApplyModifier(const TSharedRef<FVoxelVolumeModifier>& Modifier)
{
	return GetStamp()->ApplyModifier(Modifier);
}

FVoxelFuture AVoxelVolumeSculptActor::ClearSculptData()
{
	return GetStamp()->ClearSculptData();
}

void AVoxelVolumeSculptActor::ClearSculptCache()
{
	GetStamp()->ClearCache();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFuture<FVoxelVolumeSculptSave> AVoxelVolumeSculptActor::GetSave(const bool bCompress) const
{
	const TSharedRef<const FVoxelVolumeSculptInnerData> InnerData = GetStamp()->GetData()->GetInnerData();

	return Voxel::AsyncTask([bCompress, InnerData]
	{
		VOXEL_FUNCTION_COUNTER();

		FVoxelWriter Writer;
		Writer << InnerData->bUseFastDistances;

		ConstCast(InnerData)->Serialize(Writer.Ar());

		TVoxelArray64<uint8> Data = Writer.Move();

		if (bCompress)
		{
			Data = FVoxelUtilities::Compress(Data);
		}

		FVoxelVolumeSculptSave Result;
		Result.Data = MakeShared<FVoxelVolumeSculptSave::FData>();
		Result.Data->bIsCompressed = bCompress;
		Result.Data->Data = MoveTemp(Data);
		return Result;
	});
}

FVoxelFuture AVoxelVolumeSculptActor::LoadFromSave(const FVoxelVolumeSculptSave& Save)
{
	VOXEL_FUNCTION_COUNTER();

	if (!Save.IsValid())
	{
		VOXEL_MESSAGE(Error, "Save is invalid");
		return {};
	}

	return Voxel::AsyncTask([Save, WeakThis = MakeVoxelObjectPtr(this)]() -> FVoxelFuture
	{
		VOXEL_FUNCTION_COUNTER();

		TConstVoxelArrayView64<uint8> Data = Save.Data->Data;

		TVoxelArray64<uint8> DataStorage;
		if (Save.Data->bIsCompressed)
		{
			FVoxelUtilities::Decompress(Data, DataStorage);
			Data = DataStorage;
		}

		FVoxelReader Reader(Data);

		bool bUseFastDistances = false;
		Reader << bUseFastDistances;

		const TSharedRef<FVoxelVolumeSculptInnerData> InnerData = MakeShared<FVoxelVolumeSculptInnerData>(bUseFastDistances);
		InnerData->Serialize(Reader.Ar());

		if (!ensure(Reader.IsAtEndWithoutError()))
		{
			VOXEL_MESSAGE(Error, "Failed to load save: corrupted");
			return {};
		}

		return Voxel::GameTask([=]() -> FVoxelFuture
		{
			AVoxelVolumeSculptActor* This = WeakThis.Resolve();
			if (!ensureVoxelSlow(This))
			{
				return {};
			}

			// TODO Replicate
			return This->GetStamp()->SetInnerData(InnerData);
		});
	});
}