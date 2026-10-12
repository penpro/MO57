#include "MOEditorContentUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"

bool UMOEditorContentUtils::MountContentPath(const FString& RootPath, const FString& DiskContentDir)
{
	if (!IFileManager::Get().DirectoryExists(*DiskContentDir))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOEditorContentUtils] '%s' does not exist"), *DiskContentDir);
		return false;
	}
	FString Root = RootPath;
	if (!Root.EndsWith(TEXT("/")))
	{
		Root += TEXT("/");
	}
	if (!FPackageName::MountPointExists(Root))
	{
		FPackageName::RegisterMountPoint(Root, DiskContentDir);
	}
	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	Registry.Get().ScanPathsSynchronous({ Root }, /*bForceRescan=*/true);
	UE_LOG(LogTemp, Log, TEXT("[MOEditorContentUtils] mounted %s -> %s"), *Root, *DiskContentDir);
	return true;
}
