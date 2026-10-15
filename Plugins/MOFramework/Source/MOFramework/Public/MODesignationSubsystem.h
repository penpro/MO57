/**
 * =============================================================================
 * MODesignationSubsystem.h - Terraform excavation designations (unit 3, stage 2)
 * =============================================================================
 *
 * World-scoped registry of player-placed excavation work areas (Dig / Dump /
 * Flatten spheres). The excavation survivor job (stage 3) reads Dig zones to
 * know where to excavate and Dump zones (or containers) to know where to put
 * the spoil; the colony excavation pass (stage 4) dispatches idle villagers to
 * zones with remaining volume. Persisted via IMOSaveDomain.
 *
 * WHY A SUBSYSTEM (not per-zone actors): designations are lightweight records
 * (center + radius + budget), multiple systems query them, and save/load belongs
 * to the world. Mirrors UMOTerrainModificationSubsystem's shape, minus the sweep/
 * grid (designations are few — a handful of work areas, not thousands of worked
 * spheres — so a linear scan is fine; add a spatial index only if profiling shows
 * a need). See Docs/Terraform_Excavation_Plan.md.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MODesignationTypes.h"
#include "MODesignationSubsystem.generated.h"

UCLASS()
class MOFRAMEWORK_API UMODesignationSubsystem : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	//~ Begin IMOSaveDomain (designation zones persist here, not in the persistence subsystem)
	virtual FName GetSaveDomainName() const override { return TEXT("Designation"); }
	/** After ResourceDepletion(70) — designation zones are self-contained (they carry
	 *  GUIDs, not resolved pointers), so ordering relative to other domains is loose. */
	virtual int32 GetSaveDomainApplyPriority() const override { return 80; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Convenience accessor — same pattern as the other world subsystems. */
	static UMODesignationSubsystem* Get(const UObject* WorldContextObject);

	// ============================================================================
	// DESIGNATION CRUD (authority-only mutations)
	// ============================================================================

	/**
	 * Create a designation zone. Server-side only (returns an invalid GUID on a
	 * client). VolumeM3 is the earth budget to move; pass <= 0 to auto-estimate
	 * from the sphere footprint (πr² × radius-as-depth heuristic).
	 * @return the new zone's stable GUID (invalid on failure / no authority).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	FGuid DesignateZone(EMODesignationKind Kind, const FVector& Center, float Radius,
		float TargetHeightZ, float VolumeM3);

	/** Remove a zone by id. Returns true if it existed. Authority-only. */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	bool ClearZone(const FGuid& ZoneId);

	/** Remove every designation. Returns how many were cleared. Authority-only. */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	int32 ClearAllZones();

	/**
	 * Decrement a zone's remaining earth budget as pawns move dirt; the zone is
	 * removed once its budget reaches 0. Authority-only.
	 * @return the remaining volume after the decrement (0 if the zone was removed
	 *         or not found).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	float ConsumeZoneVolume(const FGuid& ZoneId, float DeltaM3);

	// ============================================================================
	// QUERIES
	// ============================================================================

	UFUNCTION(BlueprintPure, Category="MO|Designation")
	int32 GetZoneCount() const { return Zones.Num(); }

	UFUNCTION(BlueprintPure, Category="MO|Designation")
	const TArray<FMODesignationZone>& GetZones() const { return Zones; }

	/** Copy the zone with the given id into OutZone. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	bool FindZone(const FGuid& ZoneId, FMODesignationZone& OutZone) const;

	/** All zones of a given kind (e.g. every Dig zone with work left). */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	void GetZonesOfKind(EMODesignationKind Kind, TArray<FMODesignationZone>& OutZones) const;

	/**
	 * Where the next bite of earth goes, read from the LIVE ground (voxel-only traces on a grid over the zone's disc). bDig: the highest
	 * ground (in a Flatten zone only ground more than LevelToleranceCm above TargetHeightZ). !bDig: the lowest ground more than
	 * LevelToleranceCm below TargetHeightZ (Dump and Flatten zones; ToleranceCm >= 0 overrides). False when nothing qualifies; OutGroundSamples says why: > 0 = the
	 * zone is done on that side (a fill zone at its target, a level flatten zone), 0 = no terrain collision under it yet (far from every pawn).
	 * (Until stage 5 every bite went to the zone's centre: a pit in a dig zone, a mound in a fill zone that ignored its target height.)
	 */
	bool FindBitePoint(const FMODesignationZone& Zone, bool bDig, FVector& OutPoint, int32& OutGroundSamples, float ToleranceCm = -1.0f) const;

	/**
	 * A Flatten zone's work, from the live ground: bHasWork = some ground is more than LevelToleranceCm off its target (either side);
	 * OutDig / OutFill = the highest ground above the target and the lowest below it (ANY amount: the earth inside the tolerance band
	 * still finishes the other side). False when the zone is level, or when it is off target on one side only (it would need earth from
	 * outside: it waits).
	 */
	bool FindFlattenWork(const FMODesignationZone& Zone, FVector& OutDig, FVector& OutFill) const;

	/** FindBitePoint by zone id (tests / Blueprint). */
	UFUNCTION(BlueprintCallable, Category="MO|Designation")
	bool FindZoneBitePoint(const FGuid& ZoneId, bool bDig, FVector& OutPoint, int32& OutGroundSamples) const;

	/** How close to TargetHeightZ a fill or flatten zone counts as done: one bite depth (15 cm), so the loop cannot ping-pong around the target. */
	UPROPERTY(EditAnywhere, Category="MO|Designation")
	float LevelToleranceCm = 15.0f;

	// ============================================================================
	// SAVE / LOAD (codebase pattern)
	// ============================================================================

	/** Write the current zone list into OutData. Any caller. */
	UFUNCTION(BlueprintCallable, Category="MO|Designation|Save")
	void BuildSaveData(FMODesignationSaveData& OutData) const;

	/** Replace zone state from a save payload. Trusted/authority (persistence caller). */
	UFUNCTION(BlueprintCallable, Category="MO|Designation|Save")
	bool ApplySaveDataAuthority(const FMODesignationSaveData& InData);

private:
	UPROPERTY()
	TArray<FMODesignationZone> Zones;

	int32 IndexOfZone(const FGuid& ZoneId) const;

	/** World subsystems have no actor-style HasAuthority; treat non-client net modes
	 *  as authority (single-player + listen-host are authority). */
	bool HasAuthorityWorld() const;
};
