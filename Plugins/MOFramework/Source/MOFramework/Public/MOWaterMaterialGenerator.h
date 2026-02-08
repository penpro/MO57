#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOWaterMaterialGenerator.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;

/**
 * Utility class for generating water materials with Gerstner wave displacement.
 * Editor-only: Creates material assets programmatically.
 */
UCLASS()
class MOFRAMEWORK_API UMOWaterMaterialGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Create a complete water material with Gerstner waves at the specified path.
	 * @param PackagePath The content browser path (e.g., "/MOFramework/Materials/M_Water")
	 * @return The created material, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water|Material", meta=(DevelopmentOnly))
	static UMaterial* CreateWaterMaterial(const FString& PackagePath);

	/**
	 * Create a simple water material for testing (no fancy effects).
	 * @param PackagePath The content browser path
	 * @return The created material
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water|Material", meta=(DevelopmentOnly))
	static UMaterial* CreateSimpleWaterMaterial(const FString& PackagePath);
#endif

	/**
	 * Create a dynamic material instance from a base water material.
	 * Works at runtime.
	 * @param BaseMaterial The base material to instance
	 * @param Outer The outer object for the instance
	 * @return Dynamic material instance
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Water|Material")
	static UMaterialInstanceDynamic* CreateWaterMaterialInstance(UMaterialInterface* BaseMaterial, UObject* Outer);
};
