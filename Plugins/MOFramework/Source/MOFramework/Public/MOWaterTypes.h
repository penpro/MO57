/**
 * =============================================================================
 * MOWaterTypes.h - Water System Data Structures
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Defines data structures for the water system. Includes Gerstner wave
 * configuration and water surface query results. Used by MOWaterActorBase
 * for wave simulation and buoyancy calculations.
 *
 * KEY TYPES:
 * - FMOGerstnerWave: Single wave configuration (direction, amplitude, etc.)
 * - FMOWaterSurfaceInfo: Result of water surface queries
 *
 * GERSTNER WAVES:
 * Gerstner waves provide realistic ocean-like surface displacement. Multiple
 * waves are summed for complex, natural-looking water motion.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] UNITS: All values are in CENTIMETERS (Unreal units).
 *   Amplitude 50 = 50cm peak height. Wavelength 500 = 5 meters.
 *
 * [2024-02] STEEPNESS RANGE: Steepness > 1.0 causes wave self-intersection.
 *   Keep clamped to 0-1 range. Engine enforces via meta=(ClampMax).
 *
 * [2024-02] DIRECTION NORMALIZATION: Direction is normalized internally.
 *   Non-normalized input is acceptable.
 *
 * =============================================================================
 * RELATED FILES: MOWaterActorBase.h, MOWaterMaterialGenerator.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWaterTypes.generated.h"

/**
 * Configuration for a single Gerstner wave.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOGerstnerWave
{
	GENERATED_BODY()

	/** Wave direction (will be normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	FVector2D Direction = FVector2D(1.0f, 0.0f);

	/** Wave amplitude (height from rest to peak) in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="0.0"))
	float Amplitude = 50.0f;

	/** Wavelength in cm (distance between wave peaks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="1.0"))
	float Wavelength = 500.0f;

	/** Steepness/sharpness of wave peaks (0-1). Higher = sharper peaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Steepness = 0.5f;

	/** Wave speed multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave", meta=(ClampMin="0.0"))
	float Speed = 1.0f;

	/** Phase offset for this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wave")
	float PhaseOffset = 0.0f;

	FMOGerstnerWave()
	{
		Direction = FVector2D(1.0f, 0.0f);
		Amplitude = 50.0f;
		Wavelength = 500.0f;
		Steepness = 0.5f;
		Speed = 1.0f;
		PhaseOffset = 0.0f;
	}
};

/**
 * Result of a water surface query.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWaterSurfaceInfo
{
	GENERATED_BODY()

	/** World Z height of the water surface at the queried location. */
	UPROPERTY(BlueprintReadOnly, Category="Water")
	float SurfaceHeight = 0.0f;

	/** Surface normal at the queried location. */
	UPROPERTY(BlueprintReadOnly, Category="Water")
	FVector SurfaceNormal = FVector::UpVector;

	/** Horizontal displacement from waves at the queried location. */
	UPROPERTY(BlueprintReadOnly, Category="Water")
	FVector2D HorizontalDisplacement = FVector2D::ZeroVector;

	/** Whether the query location is within the water body bounds. */
	UPROPERTY(BlueprintReadOnly, Category="Water")
	bool bIsInWaterBounds = false;
};
