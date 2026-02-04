#include "MOIconGenerator.h"
#include "MOFramework.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/TextureFactory.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "UObject/SavePackage.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#endif

bool UMOIconGenerator::GenerateIconForMesh(UStaticMesh* Mesh, const FString& OutputPath, int32 Resolution)
{
#if WITH_EDITOR
	if (!IsValid(Mesh))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Invalid mesh provided"));
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Generating icon for %s -> %s"), *Mesh->GetName(), *OutputPath);

	// Render thumbnail
	UTexture2D* IconTexture = RenderThumbnail(Mesh, Resolution);
	if (!IsValid(IconTexture))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to render thumbnail for %s"), *Mesh->GetName());
		return false;
	}

	// Save to disk
	if (!SaveTextureAsset(IconTexture, OutputPath))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to save icon to %s"), *OutputPath);
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Successfully generated icon: %s"), *OutputPath);
	return true;
#else
	return false;
#endif
}

bool UMOIconGenerator::GenerateIconForSelectedAsset(int32 Resolution)
{
#if WITH_EDITOR
	// Get selected assets from Content Browser
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] No assets selected in Content Browser"));
		return false;
	}

	// Use first selected asset
	const FAssetData& AssetData = SelectedAssets[0];
	UObject* Asset = AssetData.GetAsset();

	if (!IsValid(Asset))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Could not load selected asset"));
		return false;
	}

	// Try to determine item ID from path
	FName ItemId = GetItemIdFromAssetPath(AssetData.PackageName.ToString());
	FString OutputPath;

	if (!ItemId.IsNone())
	{
		// Asset is in an item folder - use that path
		OutputPath = GetIconPathForItem(ItemId);
	}
	else
	{
		// Use asset's own folder
		FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
		OutputPath = PackagePath / Asset->GetName() + TEXT("Icon");
	}

	// Handle different asset types
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		return GenerateIconForMesh(Mesh, OutputPath, Resolution);
	}
	else
	{
		// For non-mesh assets, render generic thumbnail
		UTexture2D* IconTexture = RenderThumbnail(Asset, Resolution);
		if (IsValid(IconTexture))
		{
			return SaveTextureAsset(IconTexture, OutputPath);
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Unsupported asset type: %s"), *Asset->GetClass()->GetName());
	return false;
#else
	return false;
#endif
}

int32 UMOIconGenerator::GenerateIconsForSelectedAssets(int32 Resolution)
{
#if WITH_EDITOR
	UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] GenerateIconsForSelectedAssets called"));

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Found %d selected assets"), SelectedAssets.Num());

	int32 GeneratedCount = 0;

	for (const FAssetData& AssetData : SelectedAssets)
	{
		UObject* Asset = AssetData.GetAsset();
		if (!IsValid(Asset))
		{
			continue;
		}

		FName ItemId = GetItemIdFromAssetPath(AssetData.PackageName.ToString());
		FString OutputPath;

		if (!ItemId.IsNone())
		{
			OutputPath = GetIconPathForItem(ItemId);
		}
		else
		{
			FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
			OutputPath = PackagePath / Asset->GetName() + TEXT("Icon");
		}

		UTexture2D* IconTexture = RenderThumbnail(Asset, Resolution);
		if (IsValid(IconTexture) && SaveTextureAsset(IconTexture, OutputPath))
		{
			GeneratedCount++;
			UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Generated: %s"), *OutputPath);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Generated %d icons"), GeneratedCount);
	return GeneratedCount;
#else
	return 0;
#endif
}

FString UMOIconGenerator::GetIconPathForItem(const FName& ItemId)
{
	// Path: /MOFramework/Items/{ItemId}/{ItemId}Icon
	return FString::Printf(TEXT("/MOFramework/Items/%s/%sIcon"), *ItemId.ToString(), *ItemId.ToString());
}

bool UMOIconGenerator::DoesIconExistForItem(const FName& ItemId)
{
#if WITH_EDITOR
	FString IconPath = GetIconPathForItem(ItemId);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(IconPath));
	return AssetData.IsValid();
#else
	return false;
#endif
}

UTexture2D* UMOIconGenerator::RenderThumbnail(UObject* Asset, int32 Resolution)
{
#if WITH_EDITOR
	if (!IsValid(Asset))
	{
		return nullptr;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Rendering thumbnail for %s"), *Asset->GetName());

	// Force load materials for static meshes before rendering
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		Mesh->ConditionalPostLoad();
		for (const FStaticMaterial& StaticMaterial : Mesh->GetStaticMaterials())
		{
			if (StaticMaterial.MaterialInterface)
			{
				StaticMaterial.MaterialInterface->ConditionalPostLoad();
				if (UMaterial* BaseMaterial = StaticMaterial.MaterialInterface->GetMaterial())
				{
					BaseMaterial->ConditionalPostLoad();
				}
			}
		}
		UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Loaded %d materials for mesh"), Mesh->GetStaticMaterials().Num());
	}

	// Use FObjectThumbnail which is what the engine uses internally
	FObjectThumbnail ObjectThumbnail;
	ThumbnailTools::RenderThumbnail(
		Asset,
		Resolution,
		Resolution,
		ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush,
		nullptr,  // No render target - use FObjectThumbnail instead
		&ObjectThumbnail
	);

	// Get the uncompressed image data
	const TArray<uint8>& ImageData = ObjectThumbnail.GetUncompressedImageData();
	int32 ThumbWidth = ObjectThumbnail.GetImageWidth();
	int32 ThumbHeight = ObjectThumbnail.GetImageHeight();

	UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Thumbnail size: %dx%d, data size: %d bytes"),
		ThumbWidth, ThumbHeight, ImageData.Num());

	if (ImageData.Num() == 0 || ThumbWidth == 0 || ThumbHeight == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Empty thumbnail data"));
		return nullptr;
	}

	// Create a transient texture with the thumbnail data
	UTexture2D* NewTexture = UTexture2D::CreateTransient(ThumbWidth, ThumbHeight, PF_B8G8R8A8);
	if (!NewTexture)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to create transient texture"));
		return nullptr;
	}

	// Copy image data to texture (FObjectThumbnail stores BGRA8 data)
	void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, ImageData.GetData(), ImageData.Num());
	NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	NewTexture->UpdateResource();

	return NewTexture;
#else
	return nullptr;
#endif
}

bool UMOIconGenerator::SaveTextureAsset(UTexture2D* SourceTexture, const FString& PackagePath)
{
#if WITH_EDITOR
	if (!IsValid(SourceTexture))
	{
		return false;
	}

	// Convert content path to package path
	FString FullPackagePath = PackagePath;
	if (!FullPackagePath.StartsWith(TEXT("/Game")))
	{
		// Assume MOFramework plugin path
		FullPackagePath = TEXT("/Game") + PackagePath;
		if (FullPackagePath.Contains(TEXT("/MOFramework/")))
		{
			FullPackagePath = FullPackagePath.Replace(TEXT("/Game/MOFramework/"), TEXT("/MOFramework/"));
		}
	}

	// Create the package
	FString PackageName = FullPackagePath;
	FString AssetName = FPackageName::GetShortName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to create package: %s"), *PackageName);
		return false;
	}
	Package->FullyLoad();

	// Get source texture dimensions and data
	int32 Width = SourceTexture->GetSizeX();
	int32 Height = SourceTexture->GetSizeY();

	// Read pixel data from source texture
	TArray<FColor> Pixels;
	Pixels.SetNum(Width * Height);

	// Lock and copy the source data
	const FColor* SourceData = reinterpret_cast<const FColor*>(SourceTexture->GetPlatformData()->Mips[0].BulkData.LockReadOnly());
	if (SourceData)
	{
		FMemory::Memcpy(Pixels.GetData(), SourceData, Width * Height * sizeof(FColor));
		SourceTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to lock source texture data"));
		return false;
	}

	// Create new texture directly in target package
	UTexture2D* NewTexture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!NewTexture)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to create texture in package"));
		return false;
	}

	// Set texture properties for UI use
	NewTexture->MipGenSettings = TMGS_NoMipmaps;
	NewTexture->CompressionSettings = TC_EditorIcon;
	NewTexture->LODGroup = TEXTUREGROUP_UI;
	NewTexture->SRGB = true;
	NewTexture->NeverStream = true;

	// Initialize platform data
	FTexturePlatformData* PlatformData = new FTexturePlatformData();
	PlatformData->SizeX = Width;
	PlatformData->SizeY = Height;
	PlatformData->PixelFormat = PF_B8G8R8A8;

	// Create and populate mip level
	FTexture2DMipMap* Mip = new FTexture2DMipMap();
	Mip->SizeX = Width;
	Mip->SizeY = Height;
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	void* TextureData = Mip->BulkData.Realloc(Width * Height * 4);
	FMemory::Memcpy(TextureData, Pixels.GetData(), Width * Height * 4);
	Mip->BulkData.Unlock();

	PlatformData->Mips.Add(Mip);
	NewTexture->SetPlatformData(PlatformData);

	// Initialize source data for editor (required for saving)
	NewTexture->Source.Init(Width, Height, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));

	// Update resource
	NewTexture->UpdateResource();
	NewTexture->PostEditChange();

	// Mark package dirty
	Package->MarkPackageDirty();

	// Get the file path
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	// Ensure directory exists
	FString Directory = FPaths::GetPath(PackageFileName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// Save the package
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;

	bool bSaved = UPackage::SavePackage(Package, NewTexture, *PackageFileName, SaveArgs);

	if (bSaved)
	{
		// Notify asset registry
		FAssetRegistryModule::AssetCreated(NewTexture);
		UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Saved texture %dx%d to %s"), Width, Height, *PackageName);
	}

	return bSaved;
#else
	return false;
#endif
}

FName UMOIconGenerator::GetItemIdFromAssetPath(const FString& AssetPath)
{
	// Look for pattern: /MOFramework/Items/{ItemId}/
	// or: /Game/MOFramework/Items/{ItemId}/

	FString SearchPath = AssetPath;

	int32 ItemsIdx = SearchPath.Find(TEXT("/Items/"), ESearchCase::IgnoreCase);
	if (ItemsIdx == INDEX_NONE)
	{
		return NAME_None;
	}

	// Get everything after "/Items/"
	FString AfterItems = SearchPath.Mid(ItemsIdx + 7); // 7 = length of "/Items/"

	// Get the first path component (the item folder name)
	int32 NextSlash = AfterItems.Find(TEXT("/"));
	if (NextSlash != INDEX_NONE)
	{
		FString ItemFolder = AfterItems.Left(NextSlash);
		return FName(*ItemFolder);
	}

	return NAME_None;
}
