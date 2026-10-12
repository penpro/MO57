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
#include "Async/Future.h"
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

	/** Create the lakes near the players and destroy the far ones now, SYNCHRONOUSLY (tests and tools call it directly). Tick uses the asynchronous path: see Refresh(). */
	void RefreshNow();

	/**
	 * Is a lake query made at CachedPositions still good for Viewers? Same viewers (count and order), each within ToleranceCm of where it was. Pure; the automation tests use it.
	 * (The lake grid of an 8 km square costs ~30 ms; the players move a few metres a second: re-running it every refresh was a 1 Hz hitch on the game thread.)
	 */
	static bool IsQueryFresh(const TArray<FVector2D>& CachedPositions, const TArray<FVector>& Viewers, float ToleranceCm);

	/** How far (cm) the viewers may move before the lake query is re-run. The query asks for SpawnRadiusCm + this, so a lake that comes within SpawnRadiusCm meanwhile is already in the result. */
	UPROPERTY(EditAnywhere, Category = "MO|Lakes", meta = (ClampMin = "1000"))
	float RequeryDistanceCm = 20000.0f;

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

	/** One lake query: the lakes within SpawnRadius + tolerance of each viewer position, valid for that params hash and seed. A pure function of immutable data: it may run on any thread. */
	struct FLakeQuery
	{
		uint32 ParamsHash = 0;
		int32 Seed = 0;
		int32 Generation = 0;
		TArray<FVector2D> Positions;
		TArray<TArray<FMOWorldGen::FLake>> PerViewer;
	};
	static FLakeQuery RunLakeQuery(FMOWorldGenParamsRef Params, int32 Seed, int32 Generation, TArray<FVector2D> Positions, double RadiusCm);

	/** The shared body of RefreshNow (bAllowAsync = false: the query runs now) and Tick (true: it runs on a worker thread and lakes appear when it lands). */
	void Refresh(bool bAllowAsync);

	AOceanologyLake* SpawnLake(const FMOWorldGen::FLake& Lake, UClass* Class);
	void DestroyAll();

	FLakeQuery LakeCache;
	bool bHaveLakeCache = false;
	TFuture<FLakeQuery> PendingQuery;
	bool bQueryPending = false;
	int32 QueryGeneration = 0;   // bumped by DestroyAll: a query that was started for another world / seed is dropped when it lands

	TMap<FIntPoint, FLakeBody> Bodies;
	TSharedPtr<FStreamableHandle> LakeClassHandle;
	uint32 LastParamsHash = 0;
	int32 LastSeed = 0;
	bool bHaveLastSeed = false;
	bool bWarnedNoClass = false;
	float TimeSinceRefresh = 1.0e6f;
};
