/**
 * =============================================================================
 * MOWaterMaterialGenerator.h - Water Material Generation Utilities
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Utility class for generating water materials with Gerstner wave displacement.
 * Includes editor-only functions for creating material assets and runtime
 * functions for creating dynamic material instances.
 *
 * EDITOR FUNCTIONS (WITH_EDITOR only):
 * - CreateWaterMaterial: Full water material with waves
 * - CreateSimpleWaterMaterial: Basic water for testing
 *
 * RUNTIME FUNCTIONS:
 * - CreateWaterMaterialInstance: Dynamic instance from base material
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] EDITOR ONLY: CreateWaterMaterial/CreateSimpleWaterMaterial are
 *   WITH_EDITOR only. Don't call from runtime code.
 *
 * [2024-02] PACKAGE PATH: PackagePath must be valid content browser path.
 *   Format: "/MOFramework/Materials/M_Water"
 *
 * [2024-02] OUTER PARAMETER: CreateWaterMaterialInstance requires valid Outer.
 *   Pass the water actor or component as Outer.
 *
 * =============================================================================
 * RELATED FILES: MOWaterActorBase.h, MOWaterTypes.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

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
