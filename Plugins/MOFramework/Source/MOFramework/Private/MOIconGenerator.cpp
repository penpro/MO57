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
#include "Slate/SceneViewport.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
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
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

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

	// Create a render target
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->InitAutoFormat(Resolution, Resolution);
	RenderTarget->UpdateResourceImmediate(true);

	// Get thumbnail renderer for this asset type
	FThumbnailRenderingInfo* RenderInfo = UThumbnailManager::Get().GetRenderingInfo(Asset);
	if (!RenderInfo || !RenderInfo->Renderer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] No thumbnail renderer for asset type: %s"), *Asset->GetClass()->GetName());
		return nullptr;
	}

	// Create canvas for rendering
	FIntPoint TargetSize(Resolution, Resolution);

	// Allocate buffer for pixel data
	TArray<FColor> OutPixels;
	OutPixels.SetNumZeroed(Resolution * Resolution);

	// Use thumbnail manager to render
	// This is a bit of a workaround - we draw to an off-screen buffer
	FObjectThumbnail NewThumbnail;
	ThumbnailTools::RenderThumbnail(Asset, Resolution, Resolution, ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr, &NewThumbnail);

	if (NewThumbnail.GetImageWidth() == 0 || NewThumbnail.GetImageHeight() == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Thumbnail render returned empty image"));
		return nullptr;
	}

	// Create texture from thumbnail data
	const TArray<uint8>& ThumbnailData = NewThumbnail.AccessImageData();
	int32 Width = NewThumbnail.GetImageWidth();
	int32 Height = NewThumbnail.GetImageHeight();

	UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!NewTexture)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to create transient texture"));
		return nullptr;
	}

	// Copy thumbnail data to texture
	void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, ThumbnailData.GetData(), ThumbnailData.Num());
	NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	NewTexture->UpdateResource();

	return NewTexture;
#else
	return nullptr;
#endif
}

bool UMOIconGenerator::SaveTextureAsset(UTexture2D* Texture, const FString& PackagePath)
{
#if WITH_EDITOR
	if (!IsValid(Texture))
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

	// Duplicate texture into the package
	UTexture2D* SavedTexture = DuplicateObject<UTexture2D>(Texture, Package, *AssetName);
	if (!SavedTexture)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Failed to duplicate texture"));
		return false;
	}

	// Set texture properties for UI use
	SavedTexture->MipGenSettings = TMGS_NoMipmaps;
	SavedTexture->CompressionSettings = TC_EditorIcon;
	SavedTexture->LODGroup = TEXTUREGROUP_UI;
	SavedTexture->SRGB = true;

	// Mark package dirty and save
	SavedTexture->MarkPackageDirty();
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

	bool bSaved = UPackage::SavePackage(Package, SavedTexture, *PackageFileName, SaveArgs);

	if (bSaved)
	{
		// Notify asset registry
		FAssetRegistryModule::AssetCreated(SavedTexture);
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
