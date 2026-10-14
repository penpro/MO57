/**
 * =============================================================================
 * MOSpeciesGallery.h - Every species of a biome on a test floor, and the character dropped on each
 * =============================================================================
 *
 * Wes, Oct 14 2026: "set up an asset gallery for each biome as well and a 'drop' test for collision to make sure the pawn can step on them (have the
 * asset in the gallery and drop the pawn on it to see if she stands on it properly)".
 *
 *   MO.Gallery [BiomeId|all]   lay the biome's species (DT_Biomes Species + WaterEdgeSpecies) out on a floor high over the ocean corner and stand there.
 *                              Each entry is placed as the biome spawner places it (FMOSpeciesPlacement: collision profile, burial) at its mid scale,
 *                              labelled with its mesh, profile and the contact it should have.
 *   MO.Gallery.Test            the real character, entry by entry: STANDABLE species (rocks, logs) -- dropped from above, she must end up standing ON it
 *                              (not on the floor below, not sunk into it); BLOCKS-WALK species (trees, saplings, bushes) -- walking into its centre she must
 *                              be stopped, walking past it off-centre she must NOT be (only the trunk / core blocks, not the canopy); WALK-THROUGH species
 *                              (grass, flowers, herbs) -- she must walk through. PASS / FAIL on the labels, `[MOGallery]` log lines
 *                              and Saved/Logs/MOGallery_<biome>.tsv.
 *   MO.Gallery.Clear           remove it and go back to where you were.
 *
 * A dev tool (the developer menu lists it): local world only, the host / standalone moves its own character.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOSpeciesPlacement.h"
#include "MOSpeciesGallery.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ACharacter;

UCLASS(NotPlaceable, Transient)
class MOFRAMEWORK_API AMOSpeciesGallery : public AActor
{
	GENERATED_BODY()

public:
	AMOSpeciesGallery();

	/** The gallery in this world, if one was built. */
	static AMOSpeciesGallery* Find(const UWorld* World);

	/** Lay out every species of BiomeId (NAME_None = every biome) around this actor. Returns the number of entries. */
	int32 Build(FName BiomeId);

	/** Where a visitor stands to look at the gallery (the floor's near edge, facing in). */
	FTransform GetViewpoint() const;

	/** Drop / walk Character through every entry, one after the other (Tick drives it). */
	void StartContactTest(ACharacter* Character);
	bool IsTesting() const { return TestIndex != INDEX_NONE; }

	FName GetBiomeId() const { return BiomeId; }

	/** Where the visitor came from (MO.Gallery.Clear goes back). */
	FTransform ReturnTransform;

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FEntry
	{
		FString Name;                       // "<Biome>/<mesh>"
		FString MeshPath;
		UInstancedStaticMeshComponent* Component = nullptr;
		UTextRenderComponent* Text = nullptr;
		FVector Center = FVector::ZeroVector;   // on the floor
		float TopZ = 0.0f;
		float RadiusCm = 0.0f;
		FName Profile;
		EMOSpeciesContact Expected = EMOSpeciesContact::WalkThrough;
		FString Result;
		bool bPass = false;
	};

	void BeginEntry(int32 Index);
	void BeginProbe();
	void FinishEntry(bool bPass, const FString& Result);
	void FinishTest();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY()
	TArray<TObjectPtr<UActorComponent>> BuiltComponents;

	TArray<FEntry> Entries;
	FName BiomeId;
	float FloorTopZ = 0.0f;
	FBox2D FloorArea = FBox2D(ForceInit);

	TWeakObjectPtr<ACharacter> TestCharacter;
	int32 TestIndex = INDEX_NONE;
	double PhaseStartSeconds = 0.0;
	bool bWalking = false;
	int32 Probe = 0;               // blocks-walk species: 0 = into the centre (must stop), 1 = past it off-centre (must pass: only the trunk / core blocks)
	FString CentreResult;
	FTransform TestStartTransform;
};
