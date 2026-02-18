// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Sculpt/Height/VoxelHeightSculptActor.h"
#include "Sculpt/Height/VoxelHeightSculptData.h"
#include "Sculpt/Height/VoxelHeightSculptInnerData.h"
#include "Sculpt/VoxelSculptSave.h"
#include "Sculpt/VoxelSculptSaveAsset.h"
#include "VoxelVersion.h"

#include "TransactionCommon.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "Editor/TransBuffer.h"
#endif

class FVoxelHeightSculptActorSingleton : public FVoxelSingleton
{
public:
	TVoxelMap<FGuid, TSharedPtr<FVoxelHeightSculptData>> GuidToBulkData;

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
FVoxelHeightSculptActorSingleton* GVoxelHeightSculptActorSingleton = new FVoxelHeightSculptActorSingleton();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelHeightSculptStampRef UVoxelHeightSculptComponent::GetStamp() const
{
	if (!ensure(PrivateStamp))
	{
		ConstCast(PrivateStamp) = FVoxelHeightSculptStampRef::New();
	}

	if (ExternalSaveAsset &&
		ExternalSaveAsset->GetSculptData() != PrivateStamp->GetData())
	{
		OnExternalSaveAssetPropertyChanged = MakeSharedVoid();

		ExternalSaveAsset->OnPropertyChanged.Add(MakeWeakPtrDelegate(OnExternalSaveAssetPropertyChanged, MakeWeakObjectPtrLambda(this, [this]
		{
			PrivateStamp->ScaleXY = ExternalSaveAsset->ScaleXY;
			PrivateStamp->bRelativeHeight = ExternalSaveAsset->bRelativeHeight;
			PrivateStamp->StackOverride = ExternalSaveAsset->StackOverride;
			PrivateStamp->SetData(ExternalSaveAsset->GetSculptData());

			ConstCast(this)->UpdateStamp();
		})));

		PrivateStamp->SetData(ExternalSaveAsset->GetSculptData());
	}

	return PrivateStamp;
}

void UVoxelHeightSculptComponent::Serialize(FArchive& Ar)
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
			GVoxelHeightSculptActorSingleton->GuidToBulkData.Add_EnsureNew(Guid, GetStamp()->GetData());
		}

		Ar << Guid;

		if (Ar.IsLoading())
		{
			const TSharedPtr<FVoxelHeightSculptData> Data = GVoxelHeightSculptActorSingleton->GuidToBulkData.FindRef(Guid);
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
		const TSharedRef<FVoxelHeightSculptInnerData> NewInnerData = MakeShared<FVoxelHeightSculptInnerData>();
		NewInnerData->Serialize(Ar);
		GetStamp()->SetData(MakeShared<FVoxelHeightSculptData>(nullptr, NewInnerData));
	}
	else if (Ar.IsCountingMemory())
	{
		ConstCast(GetStamp()->GetData()->GetInnerData())->Serialize(Ar);
	}
}

FVoxelStampRef UVoxelHeightSculptComponent::GetStamp_Internal() const
{
	return GetStamp();
}

#if WITH_EDITOR
bool UVoxelHeightSculptComponent::CanEditChange(const FEditPropertyChain& PropertyChain) const
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

AVoxelHeightSculptActor::AVoxelHeightSculptActor()
{
	bReplicates = true;

	Component = CreateDefaultSubobject<UVoxelHeightSculptComponent>("RootComponent");
	RootComponent = Component;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelHeightSculptStampRef AVoxelHeightSculptActor::GetStamp() const
{
	check(Component);
	return Component->GetStamp();
}

UVoxelHeightSculptSaveAsset* AVoxelHeightSculptActor::GetExternalSaveAsset() const
{
	return Component->ExternalSaveAsset;
}

void AVoxelHeightSculptActor::SetExternalSaveAsset(UVoxelHeightSculptSaveAsset* NewExternalSaveAsset)
{
	VOXEL_FUNCTION_COUNTER();

	if (!NewExternalSaveAsset)
	{
		Component->PrivateStamp = FVoxelHeightSculptStampRef::New();
		Component->UpdateStamp();
		return;
	}

	Component->ExternalSaveAsset = NewExternalSaveAsset;
	Component->UpdateStamp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelFuture AVoxelHeightSculptActor::ApplyModifier(const TSharedRef<FVoxelHeightModifier>& Modifier)
{
	return GetStamp()->ApplyModifier(Modifier);
}

FVoxelFuture AVoxelHeightSculptActor::ClearSculptData()
{
	return GetStamp()->ClearSculptData();
}

void AVoxelHeightSculptActor::ClearSculptCache()
{
	GetStamp()->ClearCache();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelFuture<FVoxelHeightSculptSave> AVoxelHeightSculptActor::GetSave(const bool bCompress) const
{
	const TSharedRef<const FVoxelHeightSculptInnerData> InnerData = GetStamp()->GetData()->GetInnerData();

	return Voxel::AsyncTask([bCompress, InnerData]
	{
		VOXEL_FUNCTION_COUNTER();

		FVoxelWriter Writer;
		ConstCast(InnerData)->Serialize(Writer.Ar());

		TVoxelArray64<uint8> Data = Writer.Move();

		if (bCompress)
		{
			Data = FVoxelUtilities::Compress(Data);
		}

		FVoxelHeightSculptSave Result;
		Result.Data = MakeShared<FVoxelHeightSculptSave::FData>();
		Result.Data->bIsCompressed = bCompress;
		Result.Data->Data = MoveTemp(Data);
		return Result;
	});
}

FVoxelFuture AVoxelHeightSculptActor::LoadFromSave(const FVoxelHeightSculptSave& Save)
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

		const TSharedRef<FVoxelHeightSculptInnerData> InnerData = MakeShared<FVoxelHeightSculptInnerData>();
		InnerData->Serialize(Reader.Ar());

		if (!ensure(Reader.IsAtEndWithoutError()))
		{
			VOXEL_MESSAGE(Error, "Failed to load save: corrupted");
			return {};
		}

		return Voxel::GameTask([=]() -> FVoxelFuture
		{
			AVoxelHeightSculptActor* This = WeakThis.Resolve();
			if (!ensureVoxelSlow(This))
			{
				return {};
			}

			// TODO Replicate
			return This->GetStamp()->SetInnerData(InnerData);
		});
	});
}