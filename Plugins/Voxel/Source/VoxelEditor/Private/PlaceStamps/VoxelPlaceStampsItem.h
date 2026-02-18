// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#pragma once

#include "VoxelEditorMinimal.h"
#include "AssetSelection.h"
#include "VoxelActorFactories.h"

struct FVoxelPlaceStampsItem
{
	FString Name;
	FString Description;
	FString Type;
	FString BrushName;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	TSet<FName> UserTags;
	TSet<FName> SystemTags;

	FAssetData Asset;
	TScriptInterface<IAssetFactoryInterface> ActorFactory = nullptr;
	bool bIsFavorite = false;

	TDelegate<TSet<FName>()> GetTags;
	TDelegate<FString()> GetDescription;

	void Setup()
	{
		if (Asset.GetClass()->IsChildOf<UScriptStruct>())
		{
			ActorFactory = UActorFactory_VoxelPlaceStampActor::StaticClass()->GetDefaultObject<UActorFactory>();
		}
		else
		{
			if (UActorFactory* Factory = GEditor->FindActorFactoryByClass(Asset.GetClass()))
			{
				ActorFactory = Factory;
			}
			else
			{
				UObject* ClassObject = Asset.GetClass()->GetDefaultObject();
				ActorFactory = FActorFactoryAssetProxy::GetFactoryForAssetObject(ClassObject);
			}
		}

		if (GetDescription.IsBound())
		{
			Description = GetDescription.Execute();
		}

		if (GetTags.IsBound())
		{
			UserTags = GetTags.Execute();
		}
	}
};