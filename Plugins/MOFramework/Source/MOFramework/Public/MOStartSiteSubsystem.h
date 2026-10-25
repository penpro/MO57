/**
 * =============================================================================
 * MOStartSiteSubsystem.h - places the start site (the clone egg and its hatch) beside a new world's first survivor, and keeps it
 * =============================================================================
 *
 * See MOStartSiteTypes.h for the why and the site frame. The flow:
 *   - AMOGameMode::OnPawnLandedSafely, for a NEW world's first survivor only (SpawnInitialPawn sets the flag; a load never does), calls
 *     PlaceBesideWhenReady: the spot the survivor landed on and the way they face are kept, and the props go down once the world is READY
 *     (UMOWorldReadinessSubsystem: the vegetation round the spawn has generated, so a tree or rock in the way has collision to find).
 *   - Each candidate site (MOStartSite::Candidates) must have voxel ground under every prop's footprint, no more than MaxFootprintReliefCm
 *     of slope across it, no open water, and nothing (a tree, rock, building) inside the prop's volume. None fits: the first candidate
 *     with ground anyway (logged) -- a survivor with no egg is the worse outcome.
 *   - The props are replicated, always-relevant static mesh actors spawned by the server; the grass is kept out of the site with a
 *     worked-ground zone (UMOTerrainModificationSubsystem, the building placement's sweep: every machine, saved with the terrain edits).
 *   - IMOSaveDomain "StartSite": the placed props are saved; a load spawns them again where they were (SpawnProps is the one routine for
 *     both). A world saved before the site existed has none and is not given one.
 * The props and offsets are UMOStartSiteSettings (Project Settings > MOFramework > Start Site, DefaultGame.ini); the defaults are the
 * clone egg and its hatch.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MOStartSiteTypes.h"
#include "MOStartSiteSubsystem.generated.h"

class APawn;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Start Site"))
class MOFRAMEWORK_API UMOStartSiteSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMOStartSiteSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Start Site"); }

	/** The props, in the site frame (+Y toward the survivor). Default: the clone egg, its hatch thrown off toward the survivor. */
	UPROPERTY(EditAnywhere, Config, Category="Start Site")
	TArray<FMOStartSitePropSpec> Props;

	/** How far the site lies from the survivor's feet (cm). */
	UPROPERTY(EditAnywhere, Config, Category="Start Site", meta=(ClampMin="150"))
	float DistanceCm = 330.0f;

	/** Its preferred bearing from the way the survivor faces (degrees, clockwise seen from above): 140 = behind them, to the right. */
	UPROPERTY(EditAnywhere, Config, Category="Start Site")
	float BearingDeg = 140.0f;

	/** Most the ground may rise across one prop's footprint (cm) before the spot counts as too steep. */
	UPROPERTY(EditAnywhere, Config, Category="Start Site", meta=(ClampMin="0"))
	float MaxFootprintReliefCm = 60.0f;

	/** The grass is kept out to this far beyond each prop's footprint (cm): a worked-ground zone per prop. */
	UPROPERTY(EditAnywhere, Config, Category="Start Site", meta=(ClampMin="0"))
	float ClearMarginCm = 40.0f;
};

/**
 * One placed start-site prop: a static mesh, server-spawned, replicated, always relevant (the egg is a landmark). Its own class rather than
 * AStaticMeshActor because that one's component is Static by default, and a registered Static component refuses a new mesh -- a client could
 * keep an empty prop. The mesh travels as one replicated property and the component is Movable on every machine.
 */
UCLASS(NotBlueprintable)
class MOFRAMEWORK_API AMOStartSiteProp : public AActor
{
	GENERATED_BODY()

public:
	AMOStartSiteProp();

	/** Server: show this mesh here and on every client. */
	void SetPropMesh(UStaticMesh* Mesh);

	UStaticMesh* GetPropMesh() const { return PropMesh; }
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category="Start Site")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(ReplicatedUsing=OnRep_PropMesh)
	TObjectPtr<UStaticMesh> PropMesh;

	UFUNCTION()
	void OnRep_PropMesh();
};

UCLASS()
class MOFRAMEWORK_API UMOStartSiteSubsystem : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	static UMOStartSiteSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Authority. A new world's first survivor has landed: place the site beside the spot they landed on, now if the world is ready, else
	 * once it is. Does nothing if the site was already placed (or is already waiting).
	 */
	void PlaceBesideWhenReady(const APawn* Survivor);

	/** Authority: place the site now beside a survivor whose feet are at Feet, facing FacingYawDeg. False if no prop could be placed. */
	bool PlaceBeside(const FVector& Feet, float FacingYawDeg, const AActor* IgnoreActor);

	bool IsPlaced() const { return SaveData.bPlaced; }
	const FMOStartSiteSaveData& GetSaveData() const { return SaveData; }

	//~ Begin IMOSaveDomain
	virtual FName GetSaveDomainName() const override { return TEXT("StartSite"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 90; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	FMOStartSiteSaveData SaveData;

	/** The spawned props (server; clients get them by replication). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AMOStartSiteProp>> PropActors;

	/** A placement waiting for the world to be ready: where the survivor landed and the way they faced. */
	bool bWaitingForReady = false;
	FVector PendingFeet = FVector::ZeroVector;
	float PendingFacingYawDeg = 0.0f;
	TWeakObjectPtr<const APawn> PendingSurvivor;
	FDelegateHandle ReadyHandle;

	void HandleWorldReady();

	/** The one "the site exists" routine, for a placement and a load alike: (re)spawn every recorded prop. */
	void SpawnProps();
	void DestroyProps();
};
