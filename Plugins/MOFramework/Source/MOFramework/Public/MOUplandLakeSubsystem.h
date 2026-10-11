/**
 * =============================================================================
 * MOUplandLakeSubsystem.h - Water surfaces for the upland lakes (lakes above sea level)
 * =============================================================================
 *
 * PURPOSE:
 * FMOWorldGen carves a bowl for every upland lake into the terrain (Docs/Terrain_Generation_Exploration.md, "Water"), but the world's one water plane sits at sea
 * level, so a bowl high in the hills would be dry. This subsystem gives each such lake a water surface of its own: an Oceanology lake actor (AOceanologyLake, built from the
 * BP_OceanologyLake blueprint: its waves, underwater look, ground caustics, audio and bubbles) laid at the lake's surface height and sized to the lake's ellipse. The terrain outside the
 * ellipse is buried above the surface by construction (the generator picks the surface as the lowest ground around the lake minus a freeboard), so the plane
 * shows only where the bowl is.
 *
 * Lakes are created and destroyed around the players: a lake exists while any player is within SpawnRadiusCm of it and goes away beyond DespawnRadiusCm (the gap
 * stops one at the edge from flickering). There is no per-lake data to save or replicate: every machine derives the same lakes from the replicated seed.
 *
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * - THE PLANE IS AXIS-ALIGNED. Oceanology's water mesh is a quad tree over the actor's WORLD-aligned bounds, so a lake actor cannot be rotated to the ellipse: it is sized to the ellipse's own
 *   axis-aligned bounding box instead (see SpawnLake), and MOFramework.WorldGen.UplandLakePlane checks the ground is buried over that box.
 * - THE PLANE'S SIZE IS A WHOLE NUMBER OF TILES (TileSize x floor(ExtentInTiles x actor scale)) CENTRED ON A TILE-GRID POINT: SpawnLake rounds the scale up and snaps the actor to the grid so the
 *   plane always covers the shore (a plane that ends a metre short leaves a dry strip of bowl).
 * - Lake actors REPLICATE by default; SpawnLake turns that off (each machine spawns its own from the seed), or clients would see two lakes.
 * - The lake blueprint is loaded asynchronously on first need (a lake appears a moment later, not as a hitch) and must be cooked: /Oceanology_Plugin/Design/Lake is in DirectoriesToAlwaysCook.
 * - A level needs no ocean for lakes to work (they are spawned from the class), but the Oceanology plugin must be enabled.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/StreamableManager.h"
#include "MOWorldGen.h"
#include "MOUplandLakeSubsystem.generated.h"

class AOceanologyLake;

UCLASS()
class MOFRAMEWORK_API UMOUplandLakeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UMOUplandLakeSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UMOUplandLakeSubsystem* Get(const UObject* WorldContextObject);

	/** Create the lakes near the players and destroy the far ones now (Tick does this once a second; tests and tools call it directly). */
	void RefreshNow();

	/** Lakes with a live water actor right now. */
	int32 GetNumLakeBodies() const { return Bodies.Num(); }

	/** The water actor of the lake owned by this generator cell, if one exists. */
	AOceanologyLake* FindLakeBody(int64 CellX, int64 CellY) const;

	/** Is the lake blueprint loaded (lakes can only be spawned once it is)? */
	bool IsLakeClassLoaded() const { return LakeClass.IsValid(); }

	/** The half-size (cm) of the axis-aligned rectangle the lake's water plane must cover: the ellipse's own bounding box (x then y), margin included. Pure; tests use it. */
	static FVector2D ComputePlaneHalfSize(const FMOWorldGen::FLake& Lake);

	/** The lake blueprint: an AOceanologyLake class. */
	UPROPERTY(EditAnywhere, Category = "MO|Lakes")
	TSoftClassPtr<AActor> LakeClass;

	/** Lakes appear within this distance of a player (cm). */
	UPROPERTY(EditAnywhere, Category = "MO|Lakes", meta = (ClampMin = "10000"))
	float SpawnRadiusCm = 400000.0f;

	/** ... and disappear beyond this one (cm). Larger than SpawnRadiusCm. */
	UPROPERTY(EditAnywhere, Category = "MO|Lakes", meta = (ClampMin = "10000"))
	float DespawnRadiusCm = 500000.0f;

	/** Real seconds between refreshes. */
	UPROPERTY(EditAnywhere, Category = "MO|Lakes", meta = (ClampMin = "0.1"))
	float RefreshIntervalSeconds = 1.0f;

private:
	struct FLakeBody
	{
		TWeakObjectPtr<AOceanologyLake> Body;
		FVector2D Center = FVector2D::ZeroVector;
	};

	AOceanologyLake* SpawnLake(const FMOWorldGen::FLake& Lake, UClass* Class);
	void DestroyAll();

	TMap<FIntPoint, FLakeBody> Bodies;
	TSharedPtr<FStreamableHandle> LakeClassHandle;
	uint32 LastParamsHash = 0;
	int32 LastSeed = 0;
	bool bHaveLastSeed = false;
	bool bWarnedNoClass = false;
	float TimeSinceRefresh = 1.0e6f;
};
