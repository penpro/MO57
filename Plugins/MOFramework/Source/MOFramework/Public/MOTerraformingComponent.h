/**
 * =============================================================================
 * MOTerraformingComponent.h - Voxel Terrain Modification
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Component that enables voxel terrain modification (dig, raise, flatten,
 * smooth). Interfaces with Voxel Plugin Pro 2.0 sculpt actors to modify
 * the destructible terrain.
 *
 * TERRAFORMING MODES:
 * - Dig: Lower terrain at brush location
 * - Raise: Elevate terrain at brush location
 * - Flatten: Level terrain to a target height
 * - Smooth: Reduce terrain roughness
 *
 * VOXEL INTEGRATION:
 * Uses AVoxelHeightSculptActor and AVoxelVolumeSculptActor from Voxel Plugin.
 * The component manages sculpt actor lifecycle and applies modifications
 * based on player input.
 *
 * BRUSH CONFIGURATION:
 * - Radius: Size of affected area (Unreal Units)
 * - Strength: Intensity of modification (0-1)
 * - Falloff: Edge softness for flatten (0-1)
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] VOXEL PLUGIN DEPENDENCY: Requires Voxel Plugin Pro 2.0. If plugin
 *   not loaded, sculpt actors will be null. Check before use.
 *
 * [2024-02] SCULPT PERSISTENCE: Voxel modifications are saved via the Voxel
 *   Plugin's own save system, NOT MOPersistenceSubsystem. Ensure both save.
 *
 * [2024-02] PERFORMANCE: Large radius operations can be expensive. Consider
 *   throttling or chunking for very large modifications.
 *
 * [2024-02] UNDO SUPPORT: Voxel Plugin may have undo support. Check if
 *   modifications should integrate with it.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOPlayerController.h - Owns this component
 * - Voxel Plugin sculpt actors - External dependency
 * - MOPersistenceSubsystem.h - World save (separate from voxel save)
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOTerraformingComponent.generated.h"

class AVoxelHeightSculptActor;
class AVoxelVolumeSculptActor;

/**
 * Terraforming operation mode.
 * See file header for mode descriptions.
 */
UENUM(BlueprintType)
enum class EMOTerraformMode : uint8
{
	Dig UMETA(DisplayName="Dig"),
	Raise UMETA(DisplayName="Raise"),
	Flatten UMETA(DisplayName="Flatten"),
	Smooth UMETA(DisplayName="Smooth")
};

/**
 * Terraforming tool configuration.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTerraformConfig
{
	GENERATED_BODY()

	/** Radius of the terraforming brush (in Unreal units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="10.0"))
	float Radius = 500.0f;

	/** Strength of the operation (0-1). Higher = more dramatic effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Strength = 1.0f;

	/** Falloff for flatten operations (0-1). Higher = softer edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Falloff = 0.5f;

	/** For flatten: target height (if not using hit point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	float FlattenTargetHeight = 0.0f;

	/** For flatten: use hit point Z as target height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	bool bUseFlattenHitHeight = true;
};

/**
 * Component that enables terrain modification using the Voxel plugin.
 * Supports both height-based (2D) and volume-based (3D) terraforming.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOTerraformingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOTerraformingComponent();

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Current terraforming mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	EMOTerraformMode CurrentMode = EMOTerraformMode::Dig;

	/** Terraforming tool configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	FMOTerraformConfig Config;

	/** Maximum distance to terraform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	float MaxDistance = 1000.0f;

	/** Trace channel for terrain detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** If true, use volume sculpting (3D). If false, use height sculpting (2D). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming")
	bool bUseVolumeSculpting = false;

	// ============================================================================
	// SCULPT ACTOR REFERENCES
	// ============================================================================

	/** Height sculpt actor for 2D terrain modification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming|Actors")
	TWeakObjectPtr<AVoxelHeightSculptActor> HeightSculptActor;

	/** Volume sculpt actor for 3D terrain modification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Terraforming|Actors")
	TWeakObjectPtr<AVoxelVolumeSculptActor> VolumeSculptActor;

	// ============================================================================
	// TERRAFORMING API
	// ============================================================================

	/**
	 * Perform terraforming at the current aim location.
	 * Uses the current mode and config.
	 * @return True if terraforming was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool TryTerraform();

	/**
	 * Perform terraforming at a specific world location.
	 * @param WorldLocation The world position to terraform at
	 * @param Mode The terraforming operation to perform
	 * @return True if terraforming was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool TerraformAtLocation(const FVector& WorldLocation, EMOTerraformMode Mode);

	/**
	 * Perform a dig operation (remove terrain).
	 * @param WorldLocation The world position to dig at
	 * @return True if dig was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Dig(const FVector& WorldLocation);

	/**
	 * Perform a raise operation (add terrain).
	 * @param WorldLocation The world position to raise at
	 * @return True if raise was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Raise(const FVector& WorldLocation);

	/**
	 * Perform a flatten operation.
	 * @param WorldLocation The world position to flatten at
	 * @param TargetHeight The height to flatten to (ignored if bUseFlattenHitHeight is true)
	 * @return True if flatten was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Flatten(const FVector& WorldLocation, float TargetHeight = 0.0f);

	/**
	 * Perform a smooth operation.
	 * @param WorldLocation The world position to smooth at
	 * @return True if smooth was initiated
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool Smooth(const FVector& WorldLocation);

	// ============================================================================
	// MODE MANAGEMENT
	// ============================================================================

	/** Cycle to the next terraforming mode. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void CycleMode();

	/** Set the terraforming mode. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void SetMode(EMOTerraformMode NewMode);

	/** Get the current mode as display text. */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	FText GetModeDisplayName() const;

	// ============================================================================
	// EVENTS
	// ============================================================================

	/** Called when terraforming mode changes. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOTerraformModeChangedSignature, EMOTerraformMode, OldMode, EMOTerraformMode, NewMode);
	UPROPERTY(BlueprintAssignable, Category="MO|Terraforming")
	FMOTerraformModeChangedSignature OnModeChanged;

	// ============================================================================
	// UTILITY
	// ============================================================================

	/**
	 * Find terraforming target location via trace.
	 * @param OutLocation The hit location (if found)
	 * @param OutNormal The hit normal (if found)
	 * @return True if a valid target was found
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool FindTerraformTarget(FVector& OutLocation, FVector& OutNormal) const;

	/**
	 * Auto-find sculpt actors in the world.
	 * Call this if sculpt actors aren't set manually.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void AutoFindSculptActors();

	/** Check if we have valid sculpt actors for the current mode. */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	bool HasValidSculptActor() const;

protected:
	virtual void BeginPlay() override;

private:
	bool ResolveViewpoint(FVector& OutLocation, FRotator& OutRotation) const;

	// Height sculpting operations
	bool HeightDig(const FVector2D& Location);
	bool HeightRaise(const FVector2D& Location);
	bool HeightFlatten(const FVector2D& Location, float TargetHeight);
	bool HeightSmooth(const FVector2D& Location);

	// Volume sculpting operations
	bool VolumeDig(const FVector& Location);
	bool VolumeRaise(const FVector& Location);
	bool VolumeFlatten(const FVector& Location, const FVector& Normal, float TargetHeight);
	bool VolumeSmooth(const FVector& Location);
};
