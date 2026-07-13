/**
 * =============================================================================
 * MODesignationTypes.h - Terraform excavation designation records (unit 3)
 * =============================================================================
 *
 * A "designation" is a player-placed WORK AREA that pawns act on autonomously:
 * a Dig zone (excavate earth here), a Dump zone (fill/raise terrain here,
 * consuming dug earth), or a Flatten zone (level toward a target = dig the high
 * spots + fill the low spots within the zone). Zones are SPHERES (center +
 * radius) per Wes's locked design decision — reuses the terraform brush footprint
 * and keeps the geometry trivial.
 *
 * Owned by UMODesignationSubsystem (a world subsystem, persisted via IMOSaveDomain).
 * Consumed by the excavation survivor job (stage 3) and the colony excavation
 * dispatch pass (stage 4). See Docs/Terraform_Excavation_Plan.md.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MODesignationTypes.generated.h"

/** What a designated zone tells pawns to do with the earth inside it. */
UENUM(BlueprintType)
enum class EMODesignationKind : uint8
{
	/** Excavate earth here — lowers terrain, produces carryable spoil. */
	Dig			UMETA(DisplayName="Dig"),
	/** Fill here — raises terrain toward TargetHeightZ, consuming spoil (conservation). */
	Dump		UMETA(DisplayName="Dump"),
	/** Level toward TargetHeightZ — dig the high spots + fill the low spots within the zone. */
	Flatten		UMETA(DisplayName="Flatten"),

	MAX			UMETA(Hidden)
};

/**
 * One designated work area. A sphere with an earth budget the excavation loop
 * decrements as pawns move dirt. Persisted verbatim.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMODesignationZone
{
	GENERATED_BODY()

	/** Stable id — how jobs, save/load, and the dump popup reference this zone. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	FGuid ZoneId;

	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	EMODesignationKind Kind = EMODesignationKind::Dig;

	/** World-space center of the sphere. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	FVector Center = FVector::ZeroVector;

	/** Sphere radius (Unreal units). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	float Radius = 250.0f;

	/** Target ground Z for Dump (fill up to) and Flatten (level to). Ignored by Dig. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	float TargetHeightZ = 0.0f;

	/**
	 * Earth budget still to move, in cubic metres. For Dig: how much is left to
	 * excavate. For Dump: how much fill capacity remains. The excavation loop
	 * decrements this as pawns work; <= 0 means the zone is finished.
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	float RemainingVolumeM3 = 0.0f;

	/**
	 * Optional default destination for a Dig zone's spoil — a Dump-zone GUID or a
	 * container GUID (resolved via the identity registry). Invalid = "ask / auto".
	 */
	UPROPERTY(BlueprintReadOnly, Category="MO|Designation")
	FGuid DefaultDumpTargetGuid;

	bool IsValidZone() const { return ZoneId.IsValid() && Radius > 0.0f; }
};

/**
 * Save-game payload for the designation subsystem. Tiny — the flat zone list plus
 * a version for future migration. A new IMOSaveDomain field on UMOWorldSaveGame.
 */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMODesignationSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Designation")
	TArray<FMODesignationZone> Zones;

	/** Bumped when the save format changes incompatibly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Save|Designation")
	int32 Version = 1;
};
