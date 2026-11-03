/**
 * =============================================================================
 * MOBuildPieceActor.h - a building piece made of parts that go up one after another
 * =============================================================================
 *
 * Wes, Nov 2 2026: "let's flesh out a lot of the building system and remember to have the assets 'pop in' as building goes on so it looks like
 * it's actually finishing" -- wattle walls and their moss or clay daub, frame members that snap to a panel's edges, floors that grow posts down
 * to the ground, cobble foundations and walls, thatch, doors, hide and thatch lean-tos and a hide tent.
 *
 * A piece is DATA on a Blueprint of this class (made by Content/Python/import_blender_buildings.py from the Blender build,
 * Tools/blender_buildings_blender.py): its Parts in build order (each a mesh, once or instanced), its Colliders (hidden boxes, solid once built),
 * its SnapBox (what snapping measures), its door, its supports. The actor makes the components when it is constructed, on every machine alike,
 * and hands the parts to AMOBuildableActor's staging: part i of N appears once the work reaches (i + 1) / N (UnbuiltStagedPartStyle Hidden), so
 * the stakes stand first, the weave climbs, the daub goes on patch by patch. The next piece is one Blueprint (a new file), not new code.
 *
 * Frames: a wall piece is authored in the stick wall's frame (its panel's centre 50 cm in -X of the pivot, 1 m wide along Y, 1 m tall), a roof
 * in the stick roof's (its eave at X 0, Z -50, rising to the ridge at X -100, Z +50), a floor centred (its top at Z +5): a daub or a thatch
 * (EMOPieceAttach::Face) takes its host's transform as it is, so it fits the old stick pieces and the new ones alike.
 *
 * Supports (bGrowsSupports): when a floor is placed, every support point under it measures the drop to whatever solid is below (the ground,
 * another building) on the server; the lengths replicate and are saved, and each becomes a post (SupportMesh, scaled to its length) that is
 * built first, and its material and time join the work (UMOBuildProgressComponent::SetExtraWork). The placement preview shows them as it moves.
 *
 * Door (any part or collider with bDoorLeaf): turns about DoorHinge; E on the finished piece opens and closes it (server; replicated, saved).
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"

#include "MOBuildPieceActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/** One part of a piece: a mesh where it stands in the piece, or the same mesh at several places (a course of stones: instanced). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildPiecePart
{
	GENERATED_BODY()

	/** Its name in the piece (the component is Part_<index>_<Name>). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	TObjectPtr<UStaticMesh> Mesh;

	/** Where it stands in the piece (ignored when Instances is set). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	FTransform Transform;

	/** The same mesh at each of these (one instanced component). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	TArray<FTransform> Instances;

	/** It swings with the door (about DoorHinge). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	bool bDoorLeaf = false;
};

/** A box in the piece's frame: a collider, or the snap box. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOBuildPieceBox
{
	GENERATED_BODY()

	/** Its centre and rotation in the piece. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	FVector HalfSize = FVector::ZeroVector;

	/** It swings with the door (about DoorHinge). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Building|Piece")
	bool bDoorLeaf = false;
};

UCLASS(Blueprintable)
class MOFRAMEWORK_API AMOBuildPieceActor : public AMOBuildableActor
{
	GENERATED_BODY()

public:
	AMOBuildPieceActor();

	// ---- The piece (set on the Blueprint's defaults by import_blender_buildings.py) ----

	/** The parts in build order: each appears when the work reaches it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Piece")
	TArray<FMOBuildPiecePart> Parts;

	/** Hidden boxes that make the finished piece solid (and let a plan be clicked). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Piece")
	TArray<FMOBuildPieceBox> Colliders;

	/** What snapping measures (UMOBuildingComponent: the MO_SnapBounds box); a zero size = the colliders' bounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Piece")
	FMOBuildPieceBox SnapBox;

	/** The door's hinge in the piece: the leaf turns about its Z axis. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Door")
	FTransform DoorHinge;

	/** How far the open door has turned (degrees about the hinge's Z). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Door")
	float DoorOpenYawDeg = 100.0f;

	/** How long the leaf takes to swing (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Door")
	float DoorSwingSeconds = 0.8f;

	// ---- Supports: posts down to the ground under a floor ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports")
	bool bGrowsSupports = false;

	/** A post 100 cm tall standing on its foot at the mesh origin (+Z up), scaled to each support's length. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	TObjectPtr<UStaticMesh> SupportMesh;

	/** Where the posts carry the piece (its frame): the top of each post, under the floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	TArray<FVector> SupportPoints;

	/** A drop shorter than this needs no post (the floor rests on the ground); longer than SupportMaxCm finds no ground: no post. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	float SupportMinCm = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	float SupportMaxCm = 300.0f;

	/** Each post is cut from this (one per SupportCmPerItem of its length, rounded up) and lashed with SupportLashingItem. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	FName SupportItem = TEXT("StraightBranch01");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	float SupportCmPerItem = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	FName SupportLashingItem = TEXT("Cordage01");

	/** The work per post: digging its foot in, trimming, setting and lashing it (seconds of real time; Principle 11). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	float SupportSecondsPerPost = 900.0f;

	/** A support point already carried by a neighbour's post this close (cm) gets none of its own: two floors share the post at their corner. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Building|Supports", meta=(EditCondition="bGrowsSupports"))
	float SupportShareCm = 20.0f;

	/** The placement preview's posts follow it as it moves (UMOBuildingComponent::UpdateGhostPosition). */
	void UpdatePreviewSupports();

	/** The posts' lengths (cm, one per SupportPoints entry; 0 = none) -- for tests and the check scripts. */
	UFUNCTION(BlueprintPure, Category="MO|Building|Supports")
	TArray<float> GetSupportLengths() const { return SupportLengthsCm; }

	/** A support post of this piece stands this close (cm, horizontally) to a world point. */
	bool HasSupportNear(const FVector& WorldPoint, float RadiusCm) const;

	UFUNCTION(BlueprintPure, Category="MO|Building|Door")
	bool HasDoor() const;

	UFUNCTION(BlueprintPure, Category="MO|Building|Door")
	bool IsDoorOpen() const { return bDoorOpen; }

	/** Open or close the door (server; a finished piece only). */
	UFUNCTION(BlueprintCallable, Category="MO|Building|Door")
	void SetDoorOpen(bool bOpen);

	/** Pure: how many items of SupportItem a post this long needs. */
	static int32 ItemsForPost(float LengthCm, float CmPerItem);

	//~ AActor / AMOBuildableActor
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void InitializeBuilding(FName InRecipeId) override;
	virtual void BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const override;
	virtual void ApplySaveData(const FMOPersistedBuildingRecord& InRecord) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnCompleteInteracted_Implementation(AController* Controller) override;

private:
	/** Make the parts, colliders, snap box and door hinge (once). */
	void BuildPieceComponents();

	/** Measure the drop under each support point (the server, or the placement preview); returns the lengths. */
	TArray<float> MeasureSupports() const;

	/** Make the posts for SupportLengthsCm, put them first in the staging and give their work to the build. */
	void ApplySupports();

	UFUNCTION()
	void OnRep_SupportLengths();

	UFUNCTION()
	void OnRep_DoorOpen();

	/** The leaf's yaw this frame toward the open / closed target. */
	void ApplyDoorYaw(float Yaw);

	bool bPieceBuilt = false;

	/** The parts in build order (their component names), before any supports. */
	TArray<FName> PieceStagedNames;

	UPROPERTY(ReplicatedUsing=OnRep_SupportLengths)
	TArray<float> SupportLengthsCm;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SupportComponents;

	UPROPERTY(ReplicatedUsing=OnRep_DoorOpen)
	bool bDoorOpen = false;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> DoorHingeComponent;

	float DoorYaw = 0.0f;

	/** The preview's transform its posts were measured at. */
	FTransform PreviewSupportsAt;
};
