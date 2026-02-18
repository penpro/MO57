// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Sculpt/VoxelSculptSaveAsset.h"
#include "Sculpt/Height/VoxelHeightSculptData.h"
#include "Sculpt/Height/VoxelHeightSculptInnerData.h"
#include "Sculpt/Volume/VoxelVolumeSculptData.h"
#include "Sculpt/Volume/VoxelVolumeSculptInnerData.h"

DEFINE_VOXEL_FACTORY(UVoxelHeightSculptSaveAsset);
DEFINE_VOXEL_FACTORY(UVoxelVolumeSculptSaveAsset);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelHeightSculptData> UVoxelHeightSculptSaveAsset::GetSculptData()
{
	if (!PrivateData)
	{
		PrivateData = MakeShared<FVoxelHeightSculptData>(this);
		PrivateData->OnChanged.Add(MakeWeakObjectPtrDelegate(this, [this]
		{
			MarkPackageDirty();
		}));
	}

	return PrivateData.ToSharedRef();
}

void UVoxelHeightSculptSaveAsset::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	Super::Serialize(Ar);

	SerializeVoxelVersion(Ar);

	FVoxelSerializationGuard Guard(Ar);

	if (Ar.IsSaving())
	{
		ConstCast(GetSculptData()->GetInnerData())->Serialize(Ar);
	}
	else if (Ar.IsLoading())
	{
		const TSharedRef<FVoxelHeightSculptInnerData> NewInnerData = MakeShared<FVoxelHeightSculptInnerData>();
		NewInnerData->Serialize(Ar);

		PrivateData = MakeShared<FVoxelHeightSculptData>(this, NewInnerData);
		PrivateData->OnChanged.Add(MakeWeakObjectPtrDelegate(this, [this]
		{
			MarkPackageDirty();
		}));
	}
	else if (Ar.IsCountingMemory())
	{
		ConstCast(GetSculptData()->GetInnerData())->Serialize(Ar);
	}
}

#if WITH_EDITOR
void UVoxelHeightSculptSaveAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.MemberProperty)
	{
		return;
	}

	OnPropertyChanged.Broadcast();
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<FVoxelVolumeSculptData> UVoxelVolumeSculptSaveAsset::GetSculptData()
{
	if (!PrivateData ||
		PrivateData->bUseFastDistances != bUseFastDistances)
	{
		PrivateData = MakeShared<FVoxelVolumeSculptData>(this, bUseFastDistances);
		PrivateData->OnChanged.Add(MakeWeakObjectPtrDelegate(this, [this]
		{
			MarkPackageDirty();
		}));
	}

	return PrivateData.ToSharedRef();
}

void UVoxelVolumeSculptSaveAsset::Serialize(FArchive& Ar)
{
	VOXEL_FUNCTION_COUNTER();

	Super::Serialize(Ar);

	SerializeVoxelVersion(Ar);

	FVoxelSerializationGuard Guard(Ar);

	if (Ar.IsSaving())
	{
		ConstCast(GetSculptData()->GetInnerData())->Serialize(Ar);
	}
	else if (Ar.IsLoading())
	{
		const TSharedRef<FVoxelVolumeSculptInnerData> NewInnerData = MakeShared<FVoxelVolumeSculptInnerData>(bUseFastDistances);
		NewInnerData->Serialize(Ar);

		PrivateData = MakeShared<FVoxelVolumeSculptData>(this, NewInnerData);
		PrivateData->OnChanged.Add(MakeWeakObjectPtrDelegate(this, [this]
		{
			MarkPackageDirty();
		}));
	}
	else if (Ar.IsCountingMemory())
	{
		ConstCast(GetSculptData()->GetInnerData())->Serialize(Ar);
	}
}

#if WITH_EDITOR
void UVoxelVolumeSculptSaveAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.MemberProperty)
	{
		return;
	}

	OnPropertyChanged.Broadcast();
}
#endif