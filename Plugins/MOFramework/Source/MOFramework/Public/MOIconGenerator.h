#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOIconGenerator.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 * Editor utility for generating item icons from static meshes.
 *
 * Usage:
 * 1. Select a static mesh in Content Browser
 * 2. Right-click > Scripted Actions > Generate Item Icon
 *
 * Or via Blueprint/C++:
 *   UMOIconGenerator::GenerateIconForMesh(Mesh, "/Game/MOFramework/Items/Sword01/Sword01Icon");
 */
UCLASS()
class MOFRAMEWORK_API UMOIconGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Generate an icon from a static mesh and save it to disk.
	 * Uses the Content Browser thumbnail renderer for consistent results.
	 *
	 * @param Mesh - The static mesh to render
	 * @param OutputPath - Full path for output (e.g., "/Game/MOFramework/Items/Sword01/Sword01Icon")
	 * @param Resolution - Icon resolution (default 256)
	 * @return True if icon was generated successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Icons", meta=(DevelopmentOnly))
	static bool GenerateIconForMesh(UStaticMesh* Mesh, const FString& OutputPath, int32 Resolution = 256);

	/**
	 * Generate an icon for the currently selected asset in Content Browser.
	 * Automatically determines output path based on asset name.
	 *
	 * @param Resolution - Icon resolution (default 256)
	 * @return True if icon was generated successfully
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Icons", meta=(DevelopmentOnly))
	static bool GenerateIconForSelectedAsset(int32 Resolution = 256);

	/**
	 * Generate icons for all selected static meshes in Content Browser.
	 *
	 * @param Resolution - Icon resolution (default 256)
	 * @return Number of icons generated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Icons", meta=(DevelopmentOnly))
	static int32 GenerateIconsForSelectedAssets(int32 Resolution = 256);

	/**
	 * Get the expected icon output path for an item.
	 *
	 * @param ItemId - The item ID (e.g., "BronzeSword01")
	 * @return Content path (e.g., "/Game/MOFramework/Items/BronzeSword01/BronzeSword01Icon")
	 */
	UFUNCTION(BlueprintPure, Category="MO|Icons")
	static FString GetIconPathForItem(const FName& ItemId);

	/**
	 * Check if an icon already exists for an item.
	 *
	 * @param ItemId - The item ID
	 * @return True if icon texture exists
	 */
	UFUNCTION(BlueprintPure, Category="MO|Icons")
	static bool DoesIconExistForItem(const FName& ItemId);

private:
	/** Render a thumbnail for an object at specified resolution. */
	static UTexture2D* RenderThumbnail(UObject* Asset, int32 Resolution);

	/** Save a texture to disk as a .uasset. */
	static bool SaveTextureAsset(UTexture2D* Texture, const FString& PackagePath);

	/** Get the item ID from an asset path (if it's in an item folder). */
	static FName GetItemIdFromAssetPath(const FString& AssetPath);
};
