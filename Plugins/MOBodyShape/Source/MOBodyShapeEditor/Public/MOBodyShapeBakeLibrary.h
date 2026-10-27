// Bakes body-shape morph targets onto a MetaHuman's body mesh from the MetaHuman Creator's body model (Wes, Oct 26: "a single character go
// from skinny to fat and vice versa, starvation etc"; "can we adjust any body parameters independently? breasts, butt, waist, etc?").
//
// The body is a MetaHuman Character asset's own (the Creator's parametric body state; Wes, Oct 26: "we can recreate the existing character
// with the new methods, she's just a placeholder anyway" -- a legacy MetaHuman's DNA reads in another frame and does not import cleanly).
// Its measurements (waist, hips, chest, ... in cm, and the Fat / Muscularity / Masculine-Feminine sliders) are the Creator's body
// constraints: hold every one at its current value, release the free ones, move the named ones, and the model solves the rest of the body.
// The two solved bodies -- held and moved -- are evaluated to vertices (UE's frame) and their difference becomes a morph target on the
// built body mesh, every LOD, through the mesh's DNA-to-vertex map: the way the Creator writes a body state into its own body mesh. The
// shape is written into each LOD's mesh description and the engine's build makes the morph target, so saves and rebuilds keep it.
//
// Not moved: the joints (a morph target moves vertices, not bones); fine for weight, which barely moves a skeleton. A rebuild of the
// character from the Creator replaces the mesh: bake again after it.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOBodyShapeBakeLibrary.generated.h"

class UMetaHumanCharacter;
class USkeletalMesh;

/** One shape to bake: the named measurements moved, the free ones left to follow, every other one held. */
USTRUCT(BlueprintType)
struct MOBODYSHAPEEDITOR_API FMOBodyShapeBakeSpec
{
	GENERATED_BODY()

	/** The morph target to write (replaces one of the same name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	FName MorphName;

	/** Measurement (as GetBodyMeasurements names it) -> change: cm, or slider units for Fat / Muscularity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	TMap<FString, float> MeasurementDeltas;

	/** Measurements left to follow the moved ones (the circumferences, for a whole-body change like weight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	TArray<FString> FreeMeasurements;
};

UCLASS()
class MOBODYSHAPEEDITOR_API UMOBodyShapeBakeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The Creator's body measurements of Character: "Name = current (min..max)", one per constraint (cm, or slider units for Fat etc.). */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	static TArray<FString> GetBodyMeasurements(UMetaHumanCharacter* Character);

	/**
	 * Bake each of Shapes onto BodyMesh (Character's built body mesh) as a morph target on every LOD, with one build of the mesh at the end.
	 * Returns one report per shape, then a "built:" line with each morph's deltas per LOD as the build made them (or "NONE"). A shape's
	 * report gives:
	 * - how far holding every measurement moves the character's own body (the shape's base);
	 * - every measurement that changed (and each one asked for: "asked");
	 * - where it acts: the vertices moved over 2 mm and the heights 90 % of them lie between;
	 * - how straight it is: the body solved half way against the morph at 0.5 (a blend's error);
	 * - per LOD the vertices written and the mesh against the held body (near zero when the mesh was built from this character; a
	 *   different body shows its own difference, a broken vertex mapping tens of cm).
	 * A shape that cannot be made reads "FAILED: <morph>: ...". The mesh is marked dirty, not saved.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	static TArray<FString> BakeBodyMorphs(UMetaHumanCharacter* Character, USkeletalMesh* BodyMesh, const TArray<FMOBodyShapeBakeSpec>& Shapes);

	/**
	 * Carry the body's shapes onto a mesh joined to it or worn on it: the face (whose neck meets the body's at the seam), clothing. Each of
	 * Target's vertices takes the move of the nearest body vertex (both in their reference pose -- one space for a MetaHuman's parts), in full
	 * within NearCm of the body and fading to none at FarCm: the face's seam moves with the body and its neck eases back to its own shape
	 * above it; a garment a few cm off the skin follows it. Every LOD of Target with its own mesh description, one build; the body's shapes are
	 * read from its LOD 0 mesh description (BakeBodyMorphs writes them there). Returns a line per morph -- the vertices moved, how many are
	 * within NearCm (none: the two meshes are not in one space) -- then "built:" as BakeBodyMorphs. The mesh is marked dirty, not saved.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	static TArray<FString> TransferBodyMorphs(USkeletalMesh* BodyMesh, USkeletalMesh* Target, const TArray<FName>& MorphNames, float NearCm = 1.0f,
		float FarCm = 4.0f);
};
