// Test verbs for the building pieces (Nov 2 2026; Tools/check_buildings.py drives them). Development builds only.
//
//   MO.Test.PlacePiece <recipe> <x> <y> <z> [@world|@pawn|@last] [presses]   aim at a point and place through the player's own path: the
//                         placement preview, UMOBuildingComponent::ApplyGhostAtHit (the snapping, an attaching piece's host, a floor's posts, the
//                         validity), TryPlaceGhost -> ServerPlaceBuilding. @world: a world point; @pawn: an offset in world axes from the pawn;
//                         @last: a point in the last placed piece's frame. presses: Q/E presses first (each steps the edge a wall takes on a floor).
//   MO.Test.PieceProgress <fraction|done> [nameContains]   the nearest placed building (or the nearest whose name holds the text) paused at that
//                         share of its work, its materials in (server): how many of its parts show; done finishes it.
//   MO.Test.PieceInfo [radiusCm=1500]    every building near the pawn: state, parts shown, posts, door, rain / wind passing.
//   MO.Test.PieceDoor [open|close|toggle]  the nearest door (server).
//   MO.Test.ShelterHere [windYawDeg=0]    the shelter rays where the pawn stands (cover, wind shelter, walls, indoors).

#include "MOBuildPieceActor.h"
#include "MOBuildProgressComponent.h"
#include "MOBuildingComponent.h"
#include "MOWeatherBlueprintLibrary.h"
#include "MOFramework.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

namespace MOBuildPieceTest
{
	static TWeakObjectPtr<AMOBuildableActor> LastPlaced;

	static APawn* GetPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	static AMOBuildableActor* Nearest(UWorld* World, const FString& NameContains = FString())
	{
		APawn* Pawn = GetPawn(World);
		AMOBuildableActor* Best = nullptr;
		double BestD = TNumericLimits<double>::Max();
		for (TActorIterator<AMOBuildableActor> It(World); Pawn && It; ++It)
		{
			if (It->IsGhostMode() || (!NameContains.IsEmpty() && !It->GetName().Contains(NameContains)))
			{
				continue;
			}
			const double D = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
			if (D < BestD)
			{
				BestD = D;
				Best = *It;
			}
		}
		return Best;
	}

	/** Parts shown (visible static mesh components with a mesh, the hidden colliders and snap box left out) of all with a mesh. */
	static FString Shown(const AMOBuildableActor* B)
	{
		TArray<UStaticMeshComponent*> Meshes;
		B->GetComponents<UStaticMeshComponent>(Meshes);
		int32 Total = 0;
		int32 Visible = 0;
		for (const UStaticMeshComponent* M : Meshes)
		{
			if (!M->GetStaticMesh() || M->GetName().StartsWith(TEXT("Collider_")) || M->GetName() == TEXT("SnapBox") || M->GetName() == TEXT("Cube"))
			{
				continue;
			}
			++Total;
			Visible += M->IsVisible() ? 1 : 0;
		}
		return FString::Printf(TEXT("%d/%d"), Visible, Total);
	}

	static FString Describe(const AMOBuildableActor* B)
	{
		FString Supports;
		FString Door;
		if (const AMOBuildPieceActor* Piece = Cast<AMOBuildPieceActor>(B))
		{
			for (float L : Piece->GetSupportLengths())
			{
				Supports += FString::Printf(TEXT("%s%.0f"), Supports.IsEmpty() ? TEXT("") : TEXT(","), L);
			}
			Door = Piece->HasDoor() ? (Piece->IsDoorOpen() ? TEXT("open") : TEXT("closed")) : TEXT("-");
		}
		const FVector L = B->GetActorLocation();
		return FString::Printf(TEXT("%s recipe=%s state=%d shown=%s loc=(%.1f,%.1f,%.1f) yaw=%.1f posts=[%s] door=%s rainPass=%.2f windPass=%.2f"),
			*B->GetName(), *B->GetRecipeId().ToString(), static_cast<int32>(B->GetBuildState()), *Shown(B), L.X, L.Y, L.Z, B->GetActorRotation().Yaw,
			*Supports, *Door, B->RainPass, B->WindPass);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMOTestPlacePiece(
	TEXT("MO.Test.PlacePiece"),
	TEXT("Aim at a point and place a building piece through the player's placement path. Usage: MO.Test.PlacePiece <recipe> <x> <y> <z> [@world|@pawn|@last] [presses]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace MOBuildPieceTest;
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = GetPawn(World);
		UMOBuildingComponent* Building = PC ? PC->FindComponentByClass<UMOBuildingComponent>() : nullptr;
		if (Args.Num() < 4 || !Pawn || !Building)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL PlacePiece: usage, or no pawn / building component"));
			return;
		}
		const FName Recipe(*Args[0]);
		const FVector Offset(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
		const FString Anchor = Args.Num() > 4 ? Args[4] : FString(TEXT("@pawn"));
		const int32 Presses = Args.Num() > 5 ? FCString::Atoi(*Args[5]) : 0;
		const FVector Aim = Anchor == TEXT("@world") ? Offset
			: (Anchor == TEXT("@last") && LastPlaced.IsValid()) ? LastPlaced->GetActorTransform().TransformPosition(Offset)
			: Pawn->GetActorLocation() + Offset;

		if (!Building->EnterPlacementMode(Recipe))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL PlacePiece %s: placement mode refused"), *Recipe.ToString());
			return;
		}
		for (int32 i = 0; i < FMath::Abs(Presses); ++i)
		{
			Building->RotateGhostZ(Presses > 0 ? 15.0f : -15.0f);   // Q / E: each press steps the snap edge
		}
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.ImpactPoint = Aim;
		Hit.Location = Aim;
		Hit.ImpactNormal = FVector::UpVector;
		Hit.Normal = FVector::UpVector;
		Building->ApplyGhostAtHit(Hit);
		Building->ApplyGhostAtHit(Hit);   // twice: a floor's preview posts measured, the validity settled
		AMOBuildableActor* Ghost = Building->GetCurrentGhost();
		const bool bValid = Building->CanPlaceAtCurrentLocation();
		const FTransform At = Ghost ? Ghost->GetActorTransform() : FTransform::Identity;
		TSet<AMOBuildableActor*> Before;
		for (TActorIterator<AMOBuildableActor> It(World); It; ++It) { Before.Add(*It); }
		const bool bPlaced = bValid && Building->TryPlaceGhost();
		Building->ExitPlacementMode(true);
		AMOBuildableActor* Made = nullptr;
		for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
		{
			if (!Before.Contains(*It) && !It->IsGhostMode()) { Made = *It; }
		}
		if (Made)
		{
			LastPlaced = Made;
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PlacePiece %s aim=(%.1f,%.1f,%.1f) valid=%d at=(%.1f,%.1f,%.1f) yaw=%.1f placed=%d %s"),
			*Recipe.ToString(), Aim.X, Aim.Y, Aim.Z, bValid ? 1 : 0, At.GetLocation().X, At.GetLocation().Y, At.GetLocation().Z, At.Rotator().Yaw,
			bPlaced ? 1 : 0, Made ? *Describe(Made) : TEXT("(nothing new)"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOTestPieceProgress(
	TEXT("MO.Test.PieceProgress"),
	TEXT("The nearest placed building paused at a share of its work (server). Usage: MO.Test.PieceProgress <fraction|done> [nameContains]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace MOBuildPieceTest;
		AMOBuildableActor* B = Nearest(World, Args.Num() > 1 ? Args[1] : FString());
		UMOBuildProgressComponent* Progress = B ? B->BuildProgressComponent.Get() : nullptr;
		if (!Progress || !B->HasAuthority())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL PieceProgress: no placed building here (or not the server)"));
			return;
		}
		if (Args.Num() > 0 && Args[0] == TEXT("done"))
		{
			Progress->CompleteInstantly(GetPawn(World));
			B->RefreshBuildStateVisual();
		}
		else
		{
			const float Fraction = Args.Num() > 0 ? FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 1.0f) : 0.5f;
			FMOBuildProgress P;
			Progress->BuildSaveData(P);
			TArray<FName> Ids;
			Progress->GetRequiredMaterials(Ids);
			for (const FName& Id : Ids)
			{
				P.DepositedMaterials.Add(Id, Progress->GetRequiredCount(Id));
			}
			P.State = EMOBuildState::Paused;
			P.ElapsedTime = Fraction * P.TotalBuildTime;
			Progress->ApplySaveData(P);
			B->RefreshBuildStateVisual();
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PieceProgress %s"), *Describe(B));
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOTestPieceInfo(
	TEXT("MO.Test.PieceInfo"),
	TEXT("Every building near the pawn. Usage: MO.Test.PieceInfo [radiusCm=1500]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace MOBuildPieceTest;
		APawn* Pawn = GetPawn(World);
		const float Radius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1500.0f;
		int32 N = 0;
		for (TActorIterator<AMOBuildableActor> It(World); Pawn && It; ++It)
		{
			if (!It->IsGhostMode() && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) <= Radius)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PieceInfo %s"), *Describe(*It));
				++N;
			}
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PieceInfo: %d building(s) within %.0f cm"), N, Radius);
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOTestPieceDoor(
	TEXT("MO.Test.PieceDoor"),
	TEXT("Open, close or toggle the nearest door (server). Usage: MO.Test.PieceDoor [open|close|toggle]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace MOBuildPieceTest;
		AMOBuildPieceActor* Door = nullptr;
		APawn* Pawn = GetPawn(World);
		double BestD = TNumericLimits<double>::Max();
		for (TActorIterator<AMOBuildPieceActor> It(World); Pawn && It; ++It)
		{
			const double D = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
			if (It->HasDoor() && !It->IsGhostMode() && D < BestD)
			{
				BestD = D;
				Door = *It;
			}
		}
		if (!Door)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL PieceDoor: no door here"));
			return;
		}
		const FString Op = Args.Num() > 0 ? Args[0] : FString(TEXT("toggle"));
		Door->SetDoorOpen(Op == TEXT("open") ? true : Op == TEXT("close") ? false : !Door->IsDoorOpen());
		FString Leaf;
		TArray<UStaticMeshComponent*> Meshes;
		Door->GetComponents<UStaticMeshComponent>(Meshes);
		for (const UStaticMeshComponent* M : Meshes)
		{
			if (M->GetName() == TEXT("Collider_03"))
			{
				Leaf = M->GetComponentRotation().ToString();
			}
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PieceDoor %s leafCollider=%s"), *Describe(Door), *Leaf);
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOTestShelterHere(
	TEXT("MO.Test.ShelterHere"),
	TEXT("The shelter rays where the pawn stands. Usage: MO.Test.ShelterHere [windYawDeg=0]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace MOBuildPieceTest;
		APawn* Pawn = GetPawn(World);
		if (!Pawn)
		{
			return;
		}
		const float WindYaw = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
		const FVector Wind = FRotator(0.0f, WindYaw, 0.0f).Vector();   // the direction the wind blows TOWARD
		const FMOExposureShelter S = UMOWeatherBlueprintLibrary::TestExposureShelter(World, Pawn->GetActorLocation(), Wind, Pawn);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ShelterHere cover=%.2f wind=%.2f walls=%d/%d enclosure=%.2f indoors=%d (wind toward yaw %.0f)"),
			S.OverheadCover, S.WindShelter, S.WallHits, S.WallRayCount, S.Enclosure, S.bIsIndoors ? 1 : 0, WindYaw);
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOTestShelterRay(
	TEXT("MO.Test.ShelterRay"),
	TEXT("One shelter ray from the pawn: what the solids along it stop of the wind or the rain. Usage: MO.Test.ShelterRay <yawDeg> <lengthCm> [wind|rain] [pitchDeg=0]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace MOBuildPieceTest;
		APawn* Pawn = GetPawn(World);
		if (!Pawn || Args.Num() < 2)
		{
			return;
		}
		const float Yaw = FCString::Atof(*Args[0]);
		const float Length = FCString::Atof(*Args[1]);
		const bool bWind = Args.Num() < 3 || Args[2] != TEXT("rain");
		const float Pitch = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.0f;
		const FVector Start = Pawn->GetActorLocation();
		const FVector End = Start + FRotator(Pitch, Yaw, 0.0f).Vector() * Length;
		const float Block = UMOWeatherBlueprintLibrary::TraceShelterBlock(World, Start, End, bWind, Pawn);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ShelterRay %s yaw=%.0f pitch=%.0f length=%.0f block=%.3f"), bWind ? TEXT("wind") : TEXT("rain"), Yaw, Pitch, Length, Block);
	}));

#endif
