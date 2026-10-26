#include "MOCropPlotActor.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOWorldCreationTypes.h"
#include "MOWorldSaveGame.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOPlayerController.h"
#include "MOTimedActionComponent.h"
#include "MOUIContractInterface.h"
#include "MOUIUtils.h"
#include "MOSpawnClearance.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

const FName AMOCropPlotActor::HarvestAction(TEXT("Harvest"));
const FName AMOCropPlotActor::ClearAction(TEXT("ClearPlot"));

namespace MOCropPlot
{
	static const FString SowPrefix(TEXT("Sow:"));
	static constexpr float ClearSeconds = 120.0f;
}

AMOCropPlotActor::AMOCropPlotActor()
{
	Plants = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Plants"));
	Plants->SetupAttachment(GetRootComponent());
	Plants->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plants->SetCanEverAffectNavigation(false);
	Plants->SetCastShadow(true);
	CropTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/MOFramework/Data/DT_Crops.DT_Crops")));
}

FName AMOCropPlotActor::SowActionFor(FName Crop)
{
	return FName(*(MOCropPlot::SowPrefix + Crop.ToString()));
}

void AMOCropPlotActor::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(StageTimer, this, &AMOCropPlotActor::Tick10s, FMath::Max(StageCheckSeconds, 1.0f), true);
	if (IsComplete())
	{
		FitToGround();
	}
	UpdateStage();
	RefreshPlants();
}

void AMOCropPlotActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StageTimer);
	if (MeshLoad.IsValid())
	{
		MeshLoad->CancelHandle();
		MeshLoad.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AMOCropPlotActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOCropPlotActor, CropId);
	DOREPLIFETIME(AMOCropPlotActor, Stage);
	DOREPLIFETIME(AMOCropPlotActor, GrowthPercent);
}

const FMOCropDefinitionRow* AMOCropPlotActor::FindCrop(FName Id) const
{
	const UDataTable* Table = CropTable.LoadSynchronous();   // a small table, kept by the class's users (loaded at the first plot)
	return Table && !Id.IsNone() ? Table->FindRow<FMOCropDefinitionRow>(Id, TEXT("AMOCropPlotActor"), false) : nullptr;
}

double AMOCropPlotActor::NowGameSeconds() const
{
	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	return Clock ? Clock->GetGameTimeSeconds() : 0.0;
}

double AMOCropPlotActor::GetDaysGrown() const
{
	if (CropId.IsNone() || SownAtGameSeconds < 0.0)
	{
		return -1.0;
	}
	// The world's Growing pace stretches the season like every other time (2 = twice as long; FMOWorldRules, DECISIONS F42).
	const double Pace = FMath::Max(static_cast<double>(FMOWorldRules::GetPace(this, EMOWorldPace::Growing)), 0.01);
	return FMath::Max(NowGameSeconds() - SownAtGameSeconds, 0.0) / FMOCropGrowth::SecondsPerDay / Pace + HeadStartDays;
}

void AMOCropPlotActor::Tick10s()
{
	if (!bFittedToGround && IsComplete() && FitToGround())
	{
		ShownScale = -1.0f;   // the plants stand on the worked ground: place them again
		RefreshPlants();
	}
	UpdateStage();
}

void AMOCropPlotActor::SetCompletedVisual()
{
	Super::SetCompletedVisual();
	bFittedToGround = false;
	FitToGround();
}

bool AMOCropPlotActor::FitToGround()
{
	if (!MeshComponent || !GetWorld())
	{
		return false;
	}
	const float Half = PlotSizeCm * 0.5f;
	const FTransform Here = GetActorTransform();
	const FVector Local[4] = { FVector(-Half, -Half, 0.0f), FVector(Half, -Half, 0.0f), FVector(Half, Half, 0.0f), FVector(-Half, Half, 0.0f) };
	FVector Ground[4];
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Corner = Here.TransformPosition(Local[i]);
		FHitResult Hit;
		if (!MOSpawnClearance::TraceVoxelGround(GetWorld(), Corner.X, Corner.Y, Corner.Z + 400.0f, Corner.Z - 800.0f, this, Hit))
		{
			return false;   // the land is not built here yet (a joiner far off): the next check tries again
		}
		Ground[i] = Hit.ImpactPoint;
	}
	FVector Normal = FVector::CrossProduct(Ground[2] - Ground[0], Ground[3] - Ground[1]).GetSafeNormal();
	if (Normal.Z < 0.0f)
	{
		Normal = -Normal;
	}
	if (Normal.Z < 0.5f)
	{
		Normal = FVector::UpVector;   // a cliff under it is no field: stay level rather than stand on edge
	}
	GroundCentre = (Ground[0] + Ground[1] + Ground[2] + Ground[3]) * 0.25f;
	GroundNormal = Normal;
	MeshComponent->SetWorldLocationAndRotation(GroundCentre, FRotationMatrix::MakeFromZX(Normal, Here.GetUnitAxis(EAxis::X)).Rotator());
	bFittedToGround = true;
	return true;
}

float AMOCropPlotActor::PlantZ(float X, float Y, float GroundZ) const
{
	if (!bFittedToGround || GroundNormal.Z <= KINDA_SMALL_NUMBER)
	{
		return GroundZ;
	}
	// The plane through the corners' ground at (X, Y), plus the slab's top (it stands 6 cm proud of its pivot).
	const float PlaneZ = GroundCentre.Z - (GroundNormal.X * (X - GroundCentre.X) + GroundNormal.Y * (Y - GroundCentre.Y)) / GroundNormal.Z;
	return FMath::Max(GroundZ, PlaneZ + 6.0f);
}

void AMOCropPlotActor::UpdateStage()
{
	if (!HasAuthority())
	{
		return;
	}
	const FMOCropDefinitionRow* Crop = FindCrop(CropId);
	const double Days = Crop ? GetDaysGrown() : -1.0;
	const EMOCropStage NewStage = Crop ? FMOCropGrowth::StageAt(*Crop, Days) : EMOCropStage::Fallow;
	const uint8 NewGrowth = Crop ? static_cast<uint8>(FMath::RoundToInt(FMOCropGrowth::GrowthFraction(*Crop, Days) * 100.0f)) : 0;
	if (NewStage == Stage && NewGrowth == GrowthPercent)
	{
		return;
	}
	if (NewStage != Stage)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCrop] %s: %s now %s (%.1f days grown)"), *GetName(), *CropId.ToString(),
			*StaticEnum<EMOCropStage>()->GetNameStringByValue(static_cast<int64>(NewStage)), Days);
	}
	Stage = NewStage;
	GrowthPercent = NewGrowth;
	RefreshPlants();   // the listen server's own view (OnRep does not run here)
}

void AMOCropPlotActor::OnRep_CropState()
{
	RefreshPlants();
}

void AMOCropPlotActor::RefreshPlants()
{
	const FMOCropDefinitionRow* Crop = FindCrop(CropId);
	if (!Crop || Stage == EMOCropStage::Fallow || Stage == EMOCropStage::Sown || Crop->StageMeshes.Num() == 0 || !IsComplete())
	{
		Plants->ClearInstances();
		ShownMesh.Reset();
		ShownScale = -1.0f;
		return;
	}
	// Sprouted -> mesh 0, Growing -> mesh 1, Ripe / Overripe -> mesh 2 (missing = the last given).
	const int32 Want = Stage == EMOCropStage::Sprouted ? 0 : Stage == EMOCropStage::Growing ? 1 : 2;
	const TSoftObjectPtr<UStaticMesh>& MeshRef = Crop->StageMeshes[FMath::Min(Want, Crop->StageMeshes.Num() - 1)];
	UStaticMesh* Mesh = MeshRef.Get();
	if (!Mesh)
	{
		if (!MeshRef.IsNull() && !MeshLoad.IsValid())
		{
			TWeakObjectPtr<AMOCropPlotActor> WeakThis(this);
			MeshLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(MeshRef.ToSoftObjectPath(), [WeakThis]()
			{
				if (AMOCropPlotActor* Plot = WeakThis.Get())
				{
					Plot->MeshLoad.Reset();
					Plot->RefreshPlants();
				}
			});
		}
		return;
	}
	// The plant's size: a seedling a fifth of its full height, growing to full at ripe.
	const float MeshHeight = FMath::Max(static_cast<float>(Mesh->GetBounds().BoxExtent.Z) * 2.0f, 1.0f);
	const float Scale = Crop->RipeHeightCm / MeshHeight * FMath::Lerp(0.2f, 1.0f, GetGrowth());
	if (ShownMesh.Get() == Mesh && FMath::IsNearlyEqual(ShownScale, Scale, 0.01f))
	{
		return;
	}
	Plants->SetStaticMesh(Mesh);
	Plants->ClearInstances();
	// Rows across the worked ground, each plant a little off the grid and turned (the same on every machine: seeded by the plot's place).
	const int32 PerSide = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(static_cast<float>(Crop->PlantsPerPlot))));
	const float Step = PlotSizeCm / PerSide;
	const FVector Here = GetActorLocation();
	FRandomStream Jitter(static_cast<int32>(GetTypeHash(FIntVector(FMath::FloorToInt(Here.X / 10.0), FMath::FloorToInt(Here.Y / 10.0), FMath::FloorToInt(Here.Z / 10.0)))));
	TArray<FTransform> Instances;
	for (int32 X = 0; X < PerSide; ++X)
	{
		for (int32 Y = 0; Y < PerSide; ++Y)
		{
			const FVector Local((X + 0.5f) * Step - PlotSizeCm * 0.5f + Jitter.FRandRange(-0.3f, 0.3f) * Step,
				(Y + 0.5f) * Step - PlotSizeCm * 0.5f + Jitter.FRandRange(-0.3f, 0.3f) * Step, 0.0f);
			FVector World = GetActorTransform().TransformPosition(Local);
			// On the ground under it (the plot lies on uneven ground; the plants stand on it, not on the plot's plane).
			FHitResult Hit;
			if (MOSpawnClearance::TraceVoxelGround(GetWorld(), World.X, World.Y, World.Z + 400.0f, World.Z - 800.0f, this, Hit))
			{
				World.Z = PlantZ(World.X, World.Y, Hit.ImpactPoint.Z);
			}
			const float S = Scale * Jitter.FRandRange(0.85f, 1.15f);
			Instances.Add(FTransform(FRotator(0.0f, Jitter.FRandRange(0.0f, 360.0f), 0.0f), GetActorTransform().InverseTransformPosition(World),
				FVector(S)));
		}
	}
	Plants->AddInstances(Instances, false);
	ShownMesh = Mesh;
	ShownScale = Scale;
}

bool AMOCropPlotActor::CanWork(const APawn* Pawn, FText& OutReason) const
{
	if (!IsComplete())
	{
		OutReason = NSLOCTEXT("MOCrop", "NotWorked", "The ground is not broken yet.");
		return false;
	}
	if (Worker.IsValid() && Worker.Get() != Pawn)
	{
		OutReason = NSLOCTEXT("MOCrop", "Busy", "Someone is working this plot.");
		return false;
	}
	return Pawn != nullptr;
}

float AMOCropPlotActor::HarvestToolSpeed(const APawn* Pawn, const FMOCropDefinitionRow& Crop) const
{
	const UMOInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	const UEnum* ToolEnum = StaticEnum<EMOToolType>();
	const int64 Tool = Crop.HarvestToolType.IsNone() ? INDEX_NONE : ToolEnum->GetValueByNameString(Crop.HarvestToolType.ToString());
	if (Tool == INDEX_NONE || Tool == static_cast<int64>(EMOToolType::None))
	{
		return 1.0f;   // by hand (pulling flax)
	}
	const float Best = Inventory ? Inventory->GetBestToolEffectiveness(static_cast<EMOToolType>(Tool)) : 0.0f;
	if (Best > 0.0f)
	{
		return Best;
	}
	// Grain was cut with flint blades long before the sickle: a knife does it, at half the pace.
	const float Knife = Tool == static_cast<int64>(EMOToolType::Sickle) && Inventory ? Inventory->GetBestToolEffectiveness(EMOToolType::Knife) : 0.0f;
	return Knife * 0.5f;
}

FText AMOCropPlotActor::GetWorldActionTitle() const
{
	const FMOCropDefinitionRow* Crop = FindCrop(CropId);
	if (!Crop)
	{
		return NSLOCTEXT("MOCrop", "FallowTitle", "Field plot (fallow)");
	}
	return FText::Format(NSLOCTEXT("MOCrop", "CropTitle", "Field plot: {0} ({1})"), Crop->DisplayName,
		StaticEnum<EMOCropStage>()->GetDisplayNameTextByValue(static_cast<int64>(Stage)));
}

void AMOCropPlotActor::GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const
{
	FText Reason;
	const bool bCanWork = CanWork(Pawn, Reason);
	const UMOInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	if (CropId.IsNone())
	{
		// One option per crop whose seed the pawn carries (none at all: say what is needed).
		const UDataTable* Table = CropTable.LoadSynchronous();
		bool bAny = false;
		if (Table)
		{
			for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
			{
				const FMOCropDefinitionRow& Crop = *reinterpret_cast<const FMOCropDefinitionRow*>(Row.Value);
				const int32 Have = Inventory ? Inventory->CountIngredient(Crop.SeedItemId) : 0;
				if (Have <= 0)
				{
					continue;
				}
				bAny = true;
				FMOItemActionOption Option;
				Option.Id = SowActionFor(Row.Key);
				Option.Label = FText::Format(NSLOCTEXT("MOCrop", "Sow", "Sow {0} ({1})"), Crop.DisplayName,
					UMOUIUtils::FormatDurationAsText(FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, Crop.SowSeconds)));
				Option.bEnabled = bCanWork && Have >= Crop.SeedPerPlot;
				Option.DisabledReason = !bCanWork ? Reason
					: FText::Format(NSLOCTEXT("MOCrop", "SeedShort", "Needs {0} x{1}."), UMOItemDatabaseSettings::GetItemDisplayName(Crop.SeedItemId), FText::AsNumber(Crop.SeedPerPlot));
				Out.Add(MoveTemp(Option));
			}
		}
		if (!bAny)
		{
			FMOItemActionOption None;
			None.Id = NAME_None;
			None.Label = NSLOCTEXT("MOCrop", "NoSeed", "Sow");
			None.bEnabled = false;
			None.DisabledReason = bCanWork ? NSLOCTEXT("MOCrop", "NoSeedReason", "You carry no seed to sow.") : Reason;
			Out.Add(MoveTemp(None));
		}
		return;
	}
	const FMOCropDefinitionRow* Crop = FindCrop(CropId);
	if (!Crop)
	{
		return;
	}
	if (Stage == EMOCropStage::Ripe || Stage == EMOCropStage::Overripe)
	{
		const float Speed = HarvestToolSpeed(Pawn, *Crop);
		FMOItemActionOption Option;
		Option.Id = HarvestAction;
		Option.Label = FText::Format(NSLOCTEXT("MOCrop", "Harvest", "Harvest {0} ({1})"), Crop->DisplayName,
			UMOUIUtils::FormatDurationAsText(FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, Crop->HarvestSeconds) / FMath::Max(Speed, 0.05f)));
		Option.bEnabled = bCanWork && Speed > 0.0f;
		Option.DisabledReason = !bCanWork ? Reason : UMOItemDatabaseSettings::DescribeToolNeed(EMOToolType::Sickle);
		Out.Add(MoveTemp(Option));
	}
	FMOItemActionOption Clear;
	Clear.Id = ClearAction;
	Clear.Label = FText::Format(NSLOCTEXT("MOCrop", "Clear", "Pull up the {0} ({1})"), Crop->DisplayName,
		UMOUIUtils::FormatDurationAsText(FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, MOCropPlot::ClearSeconds)));
	Clear.bEnabled = bCanWork;
	Clear.DisabledReason = Reason;
	Out.Add(MoveTemp(Clear));
}

bool AMOCropPlotActor::PerformWorldAction(APawn* Pawn, FName ActionId)
{
	FText Reason;
	if (!HasAuthority() || !IsValid(Pawn) || !CanWork(Pawn, Reason))
	{
		return false;
	}
	UMOTimedActionComponent* Timed = Pawn->FindComponentByClass<UMOTimedActionComponent>();
	if (!Timed)
	{
		return false;
	}
	TWeakObjectPtr<AMOCropPlotActor> WeakThis(this);
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	auto Release = [WeakThis, WeakPawn]()
	{
		if (AMOCropPlotActor* Plot = WeakThis.Get(); Plot && Plot->Worker == WeakPawn)
		{
			Plot->Worker.Reset();
		}
	};
	const FString Action = ActionId.ToString();
	if (Action.StartsWith(MOCropPlot::SowPrefix))
	{
		const FName Crop(*Action.RightChop(MOCropPlot::SowPrefix.Len()));
		const FMOCropDefinitionRow* Row = FindCrop(Crop);
		const UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
		if (!Row || !CropId.IsNone() || !Inventory || Inventory->CountIngredient(Row->SeedItemId) < Row->SeedPerPlot)
		{
			return false;
		}
		Worker = Pawn;
		return Timed->BeginKneeling(FName(TEXT("Sow")), FText::Format(NSLOCTEXT("MOCrop", "Sowing", "Sowing {0}"), Row->DisplayName),
			FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, Row->SowSeconds),
			[WeakThis, WeakPawn, Crop, Release]()
			{
				Release();
				if (AMOCropPlotActor* Plot = WeakThis.Get(); Plot && WeakPawn.IsValid())
				{
					Plot->Sow(Crop, WeakPawn.Get());
				}
			}, Release);
	}
	const FMOCropDefinitionRow* Row = FindCrop(CropId);
	if (!Row)
	{
		return false;
	}
	if (ActionId == HarvestAction)
	{
		const float Speed = HarvestToolSpeed(Pawn, *Row);
		if ((Stage != EMOCropStage::Ripe && Stage != EMOCropStage::Overripe) || Speed <= 0.0f)
		{
			return false;
		}
		Worker = Pawn;
		return Timed->BeginKneeling(FName(TEXT("Harvest")), FText::Format(NSLOCTEXT("MOCrop", "Harvesting", "Harvesting {0}"), Row->DisplayName),
			FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, Row->HarvestSeconds) / Speed,
			[WeakThis, WeakPawn, Release]()
			{
				Release();
				if (AMOCropPlotActor* Plot = WeakThis.Get(); Plot && WeakPawn.IsValid())
				{
					Plot->Harvest(WeakPawn.Get());
				}
			}, Release);
	}
	if (ActionId == ClearAction)
	{
		Worker = Pawn;
		return Timed->BeginKneeling(FName(TEXT("ClearPlot")), NSLOCTEXT("MOCrop", "Clearing", "Clearing the plot"),
			FMOWorldRules::ScaleSeconds(this, EMOWorldPace::Gathering, MOCropPlot::ClearSeconds),
			[WeakThis, Release]()
			{
				Release();
				if (AMOCropPlotActor* Plot = WeakThis.Get())
				{
					Plot->ClearCrop();
				}
			}, Release);
	}
	return false;
}

bool AMOCropPlotActor::Sow(FName Crop, APawn* Sower)
{
	const FMOCropDefinitionRow* Row = FindCrop(Crop);
	UMOInventoryComponent* Inventory = Sower ? Sower->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	if (!HasAuthority() || !Row || !CropId.IsNone() || !IsComplete() || !Inventory || !Inventory->ConsumeIngredient(Row->SeedItemId, Row->SeedPerPlot))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCrop] %s: could not sow %s (%s)"), *GetName(), *Crop.ToString(),
			!Row ? TEXT("no such crop") : !CropId.IsNone() ? TEXT("already sown") : !IsComplete() ? TEXT("not built") : TEXT("no seed"));
		return false;
	}
	CropId = Crop;
	SownAtGameSeconds = NowGameSeconds();
	HeadStartDays = 0.0;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOCrop] %s sowed %s on %s (%d x %s) at game second %.0f"), *GetNameSafe(Sower), *Crop.ToString(), *GetName(),
		Row->SeedPerPlot, *Row->SeedItemId.ToString(), SownAtGameSeconds);
	UpdateStage();
	return true;
}

bool AMOCropPlotActor::Harvest(APawn* Harvester)
{
	const FMOCropDefinitionRow* Row = FindCrop(CropId);
	UMOInventoryComponent* Inventory = Harvester ? Harvester->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	const float Left = Row ? FMOCropGrowth::HarvestableFraction(*Row, GetDaysGrown()) : 0.0f;
	if (!HasAuthority() || !Row || !Inventory || Left <= 0.0f)
	{
		return false;
	}
	auto Give = [this, Inventory, Harvester](FName Item, int32 Quantity)
	{
		if (Item.IsNone() || Quantity <= 0)
		{
			return;
		}
		const FGuid Guid = FGuid::NewGuid();
		if (!Inventory->AddItemByGuid(Guid, Item, Quantity))
		{
			const FVector Beside = GetActorLocation() + (Harvester->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 60.0f + FVector(0.0f, 0.0f, 30.0f);
			Inventory->SpawnWorldItem(Item, Quantity, Guid, Beside, FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f));
		}
		if (AMOPlayerController* PC = Cast<AMOPlayerController>(Harvester->GetController()))
		{
			PC->ClientShowNotification(FText::Format(NSLOCTEXT("MOCrop", "Got", "{0} x{1}"), UMOItemDatabaseSettings::GetItemDisplayName(Item), FText::AsNumber(Quantity)),
				2.5f, EMONotificationType::ItemPickup);
		}
	};
	const int32 Crop = FMath::Max(1, FMath::RoundToInt(Row->HarvestQuantity * Left));
	const int32 Seed = FMath::RoundToInt(Row->SeedReturn * Left);
	Give(Row->HarvestItemId, Crop);
	Give(Row->SeedItemId, Seed);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOCrop] %s harvested %s from %s: %d x %s + %d seed (%.0f%% left standing)"), *Harvester->GetName(), *CropId.ToString(),
		*GetName(), Crop, *Row->HarvestItemId.ToString(), Seed, Left * 100.0f);
	ClearCrop();
	return true;
}

void AMOCropPlotActor::ClearCrop()
{
	if (!HasAuthority())
	{
		return;
	}
	CropId = NAME_None;
	SownAtGameSeconds = -1.0;
	HeadStartDays = 0.0;
	UpdateStage();
}

void AMOCropPlotActor::DevAgeCrop(double Days)
{
	if (HasAuthority() && !CropId.IsNone())
	{
		HeadStartDays += Days;
		UpdateStage();
	}
}

void AMOCropPlotActor::OnCompleteInteracted_Implementation(AController* Controller)
{
	HandleSecondaryInteract(Controller);
}

bool AMOCropPlotActor::HandleSecondaryInteract(AController* Controller)
{
	if (IsComplete() && Controller && Controller->Implements<UMOUIContractInterface>())
	{
		IMOUIContractInterface::Execute_RequestShowWorldActionMenu(Controller, this, GetActorLocation());
		return true;
	}
	return Super::HandleSecondaryInteract(Controller);
}

void AMOCropPlotActor::BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const
{
	Super::BuildSaveData(OutRecord);
	OutRecord.CropId = CropId;
	// Saved as the days grown, not the clock second: the clock's own save brings it back, and this survives a clock reset.
	OutRecord.CropDaysGrown = CropId.IsNone() ? -1.0 : GetDaysGrown();
}

void AMOCropPlotActor::ApplySaveData(const FMOPersistedBuildingRecord& InRecord)
{
	Super::ApplySaveData(InRecord);
	if (!HasAuthority())
	{
		return;
	}
	CropId = InRecord.CropDaysGrown >= 0.0 ? InRecord.CropId : NAME_None;
	SownAtGameSeconds = CropId.IsNone() ? -1.0 : NowGameSeconds();
	HeadStartDays = CropId.IsNone() ? 0.0 : InRecord.CropDaysGrown;
	UpdateStage();
}
