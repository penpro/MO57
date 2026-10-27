#include "MOStartSiteSubsystem.h"

#include "MOFramework.h"
#include "MOPersistenceSubsystem.h"
#include "MOSpawnClearance.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOWorldReadinessSubsystem.h"
#include "MOWorldSaveGame.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

// ============================================================================
// PLACEMENT MATHS (MOStartSiteTypes.h)
// ============================================================================

float MOStartSite::FrameYawDeg(const FVector2D& Site, const FVector2D& Survivor)
{
	const FVector2D ToSurvivor = (Survivor - Site).GetSafeNormal();
	// A yaw T turns local +Y into (-sin T, cos T): that is ToSurvivor when T = atan2(-x, y).
	return FMath::RadiansToDegrees(FMath::Atan2(-ToSurvivor.X, ToSurvivor.Y));
}

FVector2D MOStartSite::ToWorld(const FVector2D& Site, float FrameYawDeg, const FVector2D& OffsetCm)
{
	float Sin, Cos;
	FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(FrameYawDeg));
	return Site + FVector2D(OffsetCm.X * Cos - OffsetCm.Y * Sin, OffsetCm.X * Sin + OffsetCm.Y * Cos);
}

void MOStartSite::Candidates(const FVector2D& Survivor, float FacingYawDeg, float BearingDeg, float DistanceCm, TArray<FVector2D>& Out)
{
	static const float Steps[] = { 0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 120.0f, -120.0f, 150.0f, -150.0f, 180.0f };
	Out.Reset();
	for (const float Reach : { 1.0f, 1.5f })
	{
		for (const float Step : Steps)
		{
			float Sin, Cos;
			FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(FacingYawDeg + BearingDeg + Step));
			Out.Add(Survivor + FVector2D(Cos, Sin) * DistanceCm * Reach);
		}
	}
}

// ============================================================================
// SETTINGS
// ============================================================================

UMOStartSiteSettings::UMOStartSiteSettings()
{
	// The clone egg (Content/ImageTo3D, TRELLIS.2 from the Oct 18 references): its opening (mesh +Y) faces the survivor, pressed in a little.
	FMOStartSitePropSpec Egg;
	Egg.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ImageTo3D/CloneEggPod01/CloneEggPod01/StaticMeshes/CloneEggPod01.CloneEggPod01")));
	Egg.SinkCm = 10.0f;
	Props.Add(Egg);

	// Its hatch, thrown off toward the survivor and lying where it fell, inner face up.
	FMOStartSitePropSpec Hatch;
	Hatch.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ImageTo3D/CloneEggPodHatch01/CloneEggPodHatch01/StaticMeshes/CloneEggPodHatch01.CloneEggPodHatch01")));
	Hatch.OffsetCm = FVector2D(-30.0f, 175.0f);
	Hatch.YawDeg = 25.0f;
	Hatch.SinkCm = 2.0f;
	Props.Add(Hatch);
}

// ============================================================================
// THE PROP ACTOR
// ============================================================================

AMOStartSiteProp::AMOStartSiteProp()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetMobility(EComponentMobility::Movable);   // on every machine, so a client can take the replicated mesh
	MeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RootComponent = MeshComponent;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);   // it never moves: the spawn carries its transform
}

void AMOStartSiteProp::SetPropMesh(UStaticMesh* Mesh)
{
	PropMesh = Mesh;
	OnRep_PropMesh();
}

void AMOStartSiteProp::OnRep_PropMesh()
{
	MeshComponent->SetStaticMesh(PropMesh);
}

void AMOStartSiteProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOStartSiteProp, PropMesh);
}

// ============================================================================
// SUBSYSTEM
// ============================================================================

namespace
{
	/**
	 * Fit one prop at its place round a candidate site: voxel ground under its footprint (the bounds' four corners and middle, turned with it),
	 * at most MaxReliefCm of rise across it, no open water, and nothing inside its volume (vertical capsules along its length: trees, rocks and
	 * buildings count, the terrain and pawns do not). Out: its transform, bottom on the LOWEST ground under it, less its sink.
	 */
	bool FitProp(const UWorld* World, const FMOStartSitePropSpec& Spec, const UStaticMesh* Mesh, const FVector2D& Site, float FrameYawDeg,
		float ReferenceZ, float MaxReliefCm, const AActor* Ignore, FTransform& OutTransform, FString& OutWhy)
	{
		const FVector2D Place = MOStartSite::ToWorld(Site, FrameYawDeg, Spec.OffsetCm);
		const FRotator Turn(0.0f, FrameYawDeg + Spec.YawDeg, 0.0f);
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Middle(Box.GetCenter().X, Box.GetCenter().Y, 0.0f);
		const FVector Footprint[] = { FVector(Box.Min.X, Box.Min.Y, 0.0f), FVector(Box.Min.X, Box.Max.Y, 0.0f), FVector(Box.Max.X, Box.Min.Y, 0.0f),
			FVector(Box.Max.X, Box.Max.Y, 0.0f), Middle };

		float LowZ = TNumericLimits<float>::Max();
		float HighZ = TNumericLimits<float>::Lowest();
		FVector MiddleGround = FVector::ZeroVector;
		for (const FVector& Corner : Footprint)
		{
			const FVector Point = FVector(Place, 0.0f) + Turn.RotateVector(Corner);
			FHitResult Hit;
			if (!MOSpawnClearance::TraceVoxelGround(World, Point.X, Point.Y, ReferenceZ + 3000.0f, ReferenceZ - 3000.0f, Ignore, Hit))
			{
				OutWhy = FString::Printf(TEXT("%s: no ground at (%.0f, %.0f)"), *Mesh->GetName(), Point.X, Point.Y);
				return false;
			}
			LowZ = FMath::Min(LowZ, Hit.Location.Z);
			HighZ = FMath::Max(HighZ, Hit.Location.Z);
			MiddleGround = Hit.Location;   // the last point is the middle
		}
		if (HighZ - LowZ > MaxReliefCm)
		{
			OutWhy = FString::Printf(TEXT("%s: the ground rises %.0f cm across it"), *Mesh->GetName(), HighZ - LowZ);
			return false;
		}
		if (MOSpawnClearance::IsUnderOpenWater(World, MiddleGround))
		{
			OutWhy = FString::Printf(TEXT("%s: under water"), *Mesh->GetName());
			return false;
		}

		const FVector Extent = Box.GetExtent();
		MOSpawnClearance::FCapsule Capsule;
		Capsule.Radius = FMath::Max(10.0f, 0.8f * FMath::Min(Extent.X, Extent.Y));
		Capsule.HalfHeight = FMath::Max(Extent.Z, Capsule.Radius);
		const bool bLongAlongX = Extent.X >= Extent.Y;
		const FVector LongAxis = bLongAlongX ? FVector::XAxisVector : FVector::YAxisVector;
		const float Reach = FMath::Max(0.0f, (bLongAlongX ? Extent.X : Extent.Y) - Capsule.Radius);
		for (const float Along : { -1.0f, 0.0f, 1.0f })
		{
			const FVector Center = FVector(Place, LowZ + Capsule.HalfHeight + 5.0f) + Turn.RotateVector(Middle + LongAxis * Reach * Along);
			FString Blocker;
			if (MOSpawnClearance::IsCapsuleBlocked(World, Center, Capsule, Ignore, /*bIgnorePawns=*/true, &Blocker))
			{
				OutWhy = FString::Printf(TEXT("%s: %s is in the way"), *Mesh->GetName(), *Blocker);
				return false;
			}
		}
		OutTransform = FTransform(Turn, FVector(Place.X, Place.Y, LowZ - Spec.SinkCm));
		return true;
	}
}

UMOStartSiteSubsystem* UMOStartSiteSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOStartSiteSubsystem>() : nullptr;
}

bool UMOStartSiteSubsystem::GetSiteCenter(FVector& OutCenter) const
{
	if (!SaveData.bPlaced || SaveData.Props.IsEmpty())
	{
		return false;
	}
	FVector Sum = FVector::ZeroVector;
	for (const FMOStartSitePropRecord& Prop : SaveData.Props)
	{
		Sum += Prop.Transform.GetLocation();
	}
	OutCenter = Sum / SaveData.Props.Num();
	return true;
}

static TAutoConsoleVariable<float> CVarPredatorFreeAreaM(
	TEXT("MO.AI.PredatorFreeAreaM"), -1.0f,
	TEXT("The start area no predator spawns, wanders or hunts in, in metres round the start site: -1 = UMOStartSiteSettings::PredatorFreeRadiusCm "
		"(200 m), 0 = none (a test of fire alone, near the start)."));

bool UMOStartSiteSubsystem::IsInPredatorFreeArea(const UObject* WorldContextObject, const FVector& Location)
{
	const float Override = CVarPredatorFreeAreaM.GetValueOnGameThread();
	const float Radius = Override >= 0.0f ? Override * 100.0f : GetDefault<UMOStartSiteSettings>()->PredatorFreeRadiusCm;
	const UMOStartSiteSubsystem* Site = Get(WorldContextObject);
	FVector Center;
	return Radius > 0.0f && Site && Site->GetSiteCenter(Center) && FVector::DistSquared2D(Center, Location) < FMath::Square(Radius);
}

bool UMOStartSiteSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMOStartSiteSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}
}

void UMOStartSiteSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
		if (UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(World))
		{
			Readiness->OnWorldReady.Remove(ReadyHandle);
		}
	}
	ReadyHandle.Reset();
	bWaitingForReady = false;
	PropActors.Reset();
	SaveData = FMOStartSiteSaveData();
	Super::Deinitialize();
}

void UMOStartSiteSubsystem::PlaceBesideWhenReady(const APawn* Survivor)
{
	if (!Survivor || !Survivor->HasAuthority() || SaveData.bPlaced || bWaitingForReady)
	{
		return;
	}
	PendingFeet = Survivor->GetActorLocation() - FVector(0.0f, 0.0f, MOSpawnClearance::CapsuleOf(Survivor).HalfHeight);
	PendingFacingYawDeg = Survivor->GetActorRotation().Yaw;
	PendingSurvivor = Survivor;

	UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(this);
	if (Readiness && Readiness->IsTracking())
	{
		bWaitingForReady = true;
		ReadyHandle = Readiness->OnWorldReady.AddUObject(this, &UMOStartSiteSubsystem::HandleWorldReady);
		UE_LOG(LogMOFramework, Log, TEXT("[MOStartSite] The first survivor landed at (%.0f, %.0f): the start site goes down when the world is ready"),
			PendingFeet.X, PendingFeet.Y);
		return;
	}
	PlaceBeside(PendingFeet, PendingFacingYawDeg, Survivor);
}

void UMOStartSiteSubsystem::HandleWorldReady()
{
	if (UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(this))
	{
		Readiness->OnWorldReady.Remove(ReadyHandle);
	}
	ReadyHandle.Reset();
	if (!bWaitingForReady)
	{
		return;
	}
	bWaitingForReady = false;
	PlaceBeside(PendingFeet, PendingFacingYawDeg, PendingSurvivor.Get());
}

bool UMOStartSiteSubsystem::PlaceBeside(const FVector& Feet, float FacingYawDeg, const AActor* IgnoreActor)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || SaveData.bPlaced)
	{
		return false;
	}
	const UMOStartSiteSettings* Settings = GetDefault<UMOStartSiteSettings>();

	struct FProp
	{
		const FMOStartSitePropSpec* Spec;
		UStaticMesh* Mesh;
	};
	TArray<FProp> Props;
	for (const FMOStartSitePropSpec& Spec : Settings->Props)
	{
		if (UStaticMesh* Mesh = Spec.Mesh.LoadSynchronous())
		{
			Props.Add({ &Spec, Mesh });
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOStartSite] Prop mesh %s did not load: left out"), *Spec.Mesh.ToString());
		}
	}
	if (Props.IsEmpty())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOStartSite] No start-site prop could be loaded: no start site"));
		return false;
	}

	const FVector2D Survivor(Feet);
	TArray<FVector2D> Candidates;
	MOStartSite::Candidates(Survivor, FacingYawDeg, Settings->BearingDeg, Settings->DistanceCm, Candidates);

	TArray<FTransform> Chosen;
	int32 ChosenIndex = INDEX_NONE;
	TArray<FString> Rejections;
	for (int32 Index = 0; Index < Candidates.Num() && ChosenIndex == INDEX_NONE; ++Index)
	{
		const float FrameYaw = MOStartSite::FrameYawDeg(Candidates[Index], Survivor);
		TArray<FTransform> Fits;
		FString Why;
		for (const FProp& Prop : Props)
		{
			FTransform Transform;
			if (!FitProp(World, *Prop.Spec, Prop.Mesh, Candidates[Index], FrameYaw, Feet.Z, Settings->MaxFootprintReliefCm, IgnoreActor, Transform, Why))
			{
				break;
			}
			Fits.Add(Transform);
		}
		if (Fits.Num() == Props.Num())
		{
			Chosen = MoveTemp(Fits);
			ChosenIndex = Index;
		}
		else if (Rejections.Num() < 6)
		{
			Rejections.Add(FString::Printf(TEXT("#%d %s"), Index, *Why));
		}
	}

	if (ChosenIndex == INDEX_NONE)
	{
		// Nothing fits (a dense wood, a cliff edge): the preferred spot on whatever ground it has -- a survivor with no egg is the worse outcome.
		const FVector2D Site = Candidates[0];
		const float FrameYaw = MOStartSite::FrameYawDeg(Site, Survivor);
		for (const FProp& Prop : Props)
		{
			const FVector2D Place = MOStartSite::ToWorld(Site, FrameYaw, Prop.Spec->OffsetCm);
			FHitResult Hit;
			const float GroundZ = MOSpawnClearance::TraceVoxelGround(World, Place.X, Place.Y, Feet.Z + 3000.0f, Feet.Z - 3000.0f, IgnoreActor, Hit)
				? Hit.Location.Z : Feet.Z;
			Chosen.Add(FTransform(FRotator(0.0f, FrameYaw + Prop.Spec->YawDeg, 0.0f), FVector(Place.X, Place.Y, GroundZ - Prop.Spec->SinkCm)));
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOStartSite] No candidate site fits (%s): the preferred spot anyway"), *FString::Join(Rejections, TEXT("; ")));
	}

	SaveData.Props.Reset();
	for (int32 Index = 0; Index < Props.Num(); ++Index)
	{
		FMOStartSitePropRecord& Record = SaveData.Props.AddDefaulted_GetRef();
		Record.Mesh = FSoftObjectPath(Props[Index].Mesh);
		Record.Transform = Chosen[Index];
	}
	SaveData.bPlaced = true;
	SpawnProps();

	// Keep the grass out of the site on every machine: the building placement's worked-ground zone, saved with the terrain edits.
	if (UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(World))
	{
		for (int32 Index = 0; Index < Props.Num(); ++Index)
		{
			const FVector Extent = Props[Index].Mesh->GetBoundingBox().GetExtent();
			TerrainMod->RegisterModifiedZone(Chosen[Index].GetLocation(), FMath::Max(Extent.X, Extent.Y) + Settings->ClearMarginCm);
		}
	}

	const FVector First = Chosen[0].GetLocation();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOStartSite] Start site placed: %d prop(s), %s at (%.0f, %.0f, %.0f) yaw %.0f, %.0f cm from the survivor (candidate %d of %d)%s"),
		Props.Num(), *Props[0].Mesh->GetName(), First.X, First.Y, First.Z, Chosen[0].Rotator().Yaw, FVector2D::Distance(FVector2D(First), Survivor),
		ChosenIndex == INDEX_NONE ? 0 : ChosenIndex + 1, Candidates.Num(), ChosenIndex == INDEX_NONE ? TEXT(" (none fitted)") : TEXT(""));
	return true;
}

void UMOStartSiteSubsystem::SpawnProps()
{
	DestroyProps();
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	for (const FMOStartSitePropRecord& Record : SaveData.Props)
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Record.Mesh.TryLoad());
		if (!Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOStartSite] Saved prop mesh %s did not load: not spawned"), *Record.Mesh.ToString());
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMOStartSiteProp* Prop = World->SpawnActor<AMOStartSiteProp>(AMOStartSiteProp::StaticClass(), Record.Transform, Params);
		if (Prop)
		{
			Prop->SetPropMesh(Mesh);
			PropActors.Add(Prop);
		}
	}
}

void UMOStartSiteSubsystem::DestroyProps()
{
	for (AMOStartSiteProp* Prop : PropActors)
	{
		if (IsValid(Prop))
		{
			Prop->Destroy();
		}
	}
	PropActors.Reset();
}

void UMOStartSiteSubsystem::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	Save.StartSiteData = SaveData;
}

void UMOStartSiteSubsystem::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	SaveData = Save.StartSiteData;
	bWaitingForReady = false;
	if (SaveData.bPlaced)
	{
		SpawnProps();
		UE_LOG(LogMOFramework, Log, TEXT("[MOStartSite] Loaded the start site: %d prop(s)"), PropActors.Num());
	}
	else
	{
		DestroyProps();
	}
}
