#include "MOEditorContentUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
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

UFont* UMOEditorContentUtils::MakeCompositeFont(const FString& AssetPath, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces)
{
	if (TypefaceNames.Num() == 0 || TypefaceNames.Num() != Faces.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOEditorContentUtils] MakeCompositeFont(%s): needs one face per typeface name"), *AssetPath);
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	UFont* Font = FindObject<UFont>(Package, *AssetName);
	const bool bNew = Font == nullptr;
	if (bNew)
	{
		Font = NewObject<UFont>(Package, UFont::StaticClass(), FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	}
	Font->FontCacheType = EFontCacheType::Runtime;
	FTypeface& Default = Font->CompositeFont.DefaultTypeface;
	Default.Fonts.Reset();
	for (int32 i = 0; i < TypefaceNames.Num(); ++i)
	{
		if (!Faces[i])
		{
			UE_LOG(LogTemp, Warning, TEXT("[MOEditorContentUtils] MakeCompositeFont(%s): face %d (%s) is null"), *AssetPath, i, *TypefaceNames[i].ToString());
			continue;
		}
		FTypefaceEntry Entry(TypefaceNames[i]);
		Entry.Font = FFontData(Faces[i]);   // hinting and loading policy come from the face asset
		Default.Fonts.Add(Entry);
	}
	Font->MarkPackageDirty();
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Font);
	}
	return Font;
}
