#pragma once

#include "CoreMinimal.h"
#include "MOCharacter.h"
#include "MOCharacterAppearance.h"
#include "MOCustomizableCharacter.generated.h"

class UGroomComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Character with morph target-based customization.
 * Designed for Daz Genesis 8 style characters (single mesh with many morph targets)
 * but works with any skeletal mesh that has morph targets.
 *
 * Key features:
 * - Single skeletal mesh with morph targets for face/body customization
 * - Dynamic material instances for skin tone, hair color, eye color
 * - Groom components for strand-based hair
 * - Full save/load support via FMOCharacterAppearance
 *
 * Usage:
 * 1. Create a Blueprint child of this class
 * 2. Set the skeletal mesh on the inherited Mesh component
 * 3. Assign groom assets for hair
 * 4. Configure materials with "SkinTone", "HairColor", "EyeColor" parameters
 */
UCLASS()
class MOFRAMEWORK_API AMOCustomizableCharacter : public AMOCharacter
{
	GENERATED_BODY()

public:
	AMOCustomizableCharacter();

	// ============================================================================
	// APPEARANCE
	// ============================================================================

	/** Get the current character appearance. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance")
	const FMOCharacterAppearance& GetAppearance() const { return CurrentAppearance; }

	/** Set and apply a new appearance. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetAppearance(const FMOCharacterAppearance& NewAppearance);

	/** Apply the current stored appearance (call after spawning or loading). */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void ApplyCurrentAppearance();

	// ============================================================================
	// MORPH TARGETS
	// ============================================================================

	/** Set a single morph target value. Updates both mesh and stored appearance. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance|Morphs")
	void SetMorphTargetValue(FName MorphName, float Value);

	/** Get a morph target value from stored appearance. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance|Morphs")
	float GetMorphTargetValue(FName MorphName) const;

	/** Apply all stored morph targets to the mesh. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance|Morphs")
	void ApplyAllMorphTargets();

	/** Get list of available morph targets on the mesh. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance|Morphs")
	TArray<FName> GetAvailableMorphTargets() const;

	// ============================================================================
	// COLORS / MATERIALS
	// ============================================================================

	/** Set skin tone and update materials. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance|Colors")
	void SetSkinTone(const FLinearColor& NewSkinTone);

	/** Set hair color and update materials. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance|Colors")
	void SetHairColor(const FLinearColor& NewHairColor);

	/** Set eye color and update materials. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance|Colors")
	void SetEyeColor(const FLinearColor& NewEyeColor);

	// ============================================================================
	// HAIR (Groom)
	// ============================================================================

	/** Get the Hair groom component. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance|Hair")
	UGroomComponent* GetHairGroom() const { return HairGroom; }

	/** Get the Eyebrows groom component. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance|Hair")
	UGroomComponent* GetEyebrowsGroom() const { return EyebrowsGroom; }

	/** Get the Eyelashes groom component. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance|Hair")
	UGroomComponent* GetEyelashesGroom() const { return EyelashesGroom; }

	/** Get the Beard groom component. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance|Hair")
	UGroomComponent* GetBeardGroom() const { return BeardGroom; }

protected:
	virtual void BeginPlay() override;

	/** Create dynamic material instances from mesh materials. */
	void CreateDynamicMaterials();

	/** Apply color parameters to all dynamic materials. */
	void ApplyColorParameters();

	// ============================================================================
	// GROOM COMPONENTS
	// ============================================================================

	/** Hair groom component (attached to head bone). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Appearance|Hair")
	TObjectPtr<UGroomComponent> HairGroom;

	/** Eyebrows groom component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Appearance|Hair")
	TObjectPtr<UGroomComponent> EyebrowsGroom;

	/** Eyelashes groom component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Appearance|Hair")
	TObjectPtr<UGroomComponent> EyelashesGroom;

	/** Beard groom component (optional). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Appearance|Hair")
	TObjectPtr<UGroomComponent> BeardGroom;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Whether to automatically apply appearance in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance")
	bool bAutoApplyAppearance = true;

	/** Material parameter name for skin tone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Materials")
	FName SkinToneParameterName = TEXT("SkinTone");

	/** Material parameter name for hair color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Materials")
	FName HairColorParameterName = TEXT("HairColor");

	/** Material parameter name for eye color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Materials")
	FName EyeColorParameterName = TEXT("EyeColor");

	/** Material slot indices that should receive skin tone parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Materials")
	TArray<int32> SkinMaterialSlots;

	/** Material slot indices that should receive eye color parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Materials")
	TArray<int32> EyeMaterialSlots;

	/** Current character appearance data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance", SaveGame)
	FMOCharacterAppearance CurrentAppearance;

private:
	/** Dynamic material instances created at runtime. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
};
