// A field plot (Docs/Farming_And_Husbandry.md §2): built like any building (the ground broken with a digging stick or a hoe), then
// right-click -> Sow <crop> (with its seed), the crop grows on the game clock through its stages (FMOCropGrowth), Harvest when ripe
// (a sickle; a knife slower). The server keeps what was sown and when (saved with the building); every machine shows the stage the
// server replicates. One plot = one crop; harvesting it leaves it fallow for the next sowing.

#pragma once

#include "CoreMinimal.h"
#include "MOBuildableActor.h"
#include "MOWorldActionProvider.h"
#include "MOCropTypes.h"
#include "MOCropPlotActor.generated.h"

class UInstancedStaticMeshComponent;
class UDataTable;
struct FStreamableHandle;

UCLASS()
class MOFRAMEWORK_API AMOCropPlotActor : public AMOBuildableActor, public IMOWorldActionProvider
{
	GENERATED_BODY()

public:
	AMOCropPlotActor();

	/** The crops (DT_Crops: FMOCropDefinitionRow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Crop")
	TSoftObjectPtr<UDataTable> CropTable;

	/** The worked ground's side (cm, square): the plants spread over it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Crop", meta=(ClampMin="50"))
	float PlotSizeCm = 400.0f;

	/** How often the server re-reads the clock for the stage (real seconds). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Crop", meta=(ClampMin="1"))
	float StageCheckSeconds = 10.0f;

	// ---- state ----

	/** The crop in the ground (DT_Crops row), None = fallow. */
	FName GetCropId() const { return CropId; }
	EMOCropStage GetStage() const { return Stage; }
	/** 0 at sowing -> 1 at ripe. */
	float GetGrowth() const { return GrowthPercent / 100.0f; }
	/** Game days the crop has grown, scaled by the world's Growing pace (server). */
	double GetDaysGrown() const;
	const FMOCropDefinitionRow* FindCrop(FName Id) const;

	// ---- the work (server; the timed actions end here) ----

	/** Put Crop in the ground: takes its seed from Sower's pack. False if it cannot (not built, already sown, no seed). */
	bool Sow(FName Crop, APawn* Sower);
	/** Take the crop (what is left of it, if overripe) into Harvester's pack (the ground beside if full); the plot is fallow again. */
	bool Harvest(APawn* Harvester);
	/** Pull out whatever is growing (nothing is gained). */
	void ClearCrop();
	/** Server, for tests and dev: as if Days had passed since sowing. */
	void DevAgeCrop(double Days);

	// ---- IMOWorldActionProvider ----
	virtual FText GetWorldActionTitle() const override;
	virtual void GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const override;
	virtual bool PerformWorldAction(APawn* Pawn, FName ActionId) override;

	virtual void BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const override;
	virtual void ApplySaveData(const FMOPersistedBuildingRecord& InRecord) override;

	/** Right-click: the plot's menu (sow / harvest / clear) once the ground is broken. */
	virtual bool HandleSecondaryInteract(AController* Controller) override;
	/** Finished: the worked ground is laid on the land (FitToGround). */
	virtual void SetCompletedVisual() override;

	static const FName HarvestAction;
	static const FName ClearAction;
	/** A sow option's id: "Sow:<CropId>". */
	static FName SowActionFor(FName Crop);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnCompleteInteracted_Implementation(AController* Controller) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Crop")
	TObjectPtr<UInstancedStaticMeshComponent> Plants;

	UFUNCTION()
	void OnRep_CropState();

private:
	/** Server: the stage and size from the clock; replicates when they change. Every machine: lays the worked ground on the land. */
	void Tick10s();
	void UpdateStage();
	/** Every machine, once the voxel ground is there: the worked ground (MeshComponent, a rigid 4 x 4 m slab) laid on the plane through
	 *  the ground under its four corners, so it follows a slope instead of floating over the downhill side (Oct 26). False = not yet. */
	bool FitToGround();
	/** The top of the worked ground at (X, Y), or the voxel ground if that is higher: where a plant stands. */
	float PlantZ(float X, float Y, float GroundZ) const;
	/** Every machine: the plants for the replicated stage and size. */
	void RefreshPlants();
	bool CanWork(const APawn* Pawn, FText& OutReason) const;
	/** Harvest speed from the pawn's tools: a sickle 1 x its quality, a knife half; 0 = cannot. */
	float HarvestToolSpeed(const APawn* Pawn, const FMOCropDefinitionRow& Crop) const;
	double NowGameSeconds() const;

	UPROPERTY(ReplicatedUsing=OnRep_CropState)
	FName CropId;

	UPROPERTY(ReplicatedUsing=OnRep_CropState)
	EMOCropStage Stage = EMOCropStage::Fallow;

	UPROPERTY(ReplicatedUsing=OnRep_CropState)
	uint8 GrowthPercent = 0;

	/** Server: when it was sown (game-clock seconds, UMOGameClockSubsystem); < 0 = nothing sown. */
	double SownAtGameSeconds = -1.0;
	/** Server: days already grown at SownAtGameSeconds (a load restores the saved days here; MO.Test.AgeCrops adds to it). */
	double HeadStartDays = 0.0;

	/** A pawn at work on this plot (one at a time). */
	TWeakObjectPtr<APawn> Worker;

	FTimerHandle StageTimer;
	bool bFittedToGround = false;
	FVector GroundCentre = FVector::ZeroVector;
	FVector GroundNormal = FVector::UpVector;
	TSharedPtr<FStreamableHandle> MeshLoad;
	TWeakObjectPtr<UStaticMesh> ShownMesh;
	float ShownScale = -1.0f;
};
