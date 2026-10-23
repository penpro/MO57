// Test verbs for PCG instances in co-op: what THIS machine has, so a two-process test can compare host and joiner. Output lines start with [MOTEST].
//
//   MO.Test.GroundItems [radiusCm=3000] [n=5]        the PCG ground items (MOItem_<ItemId> tags) nearest the local pawn, nearest first
//   MO.Test.InstanceAt <x> <y> <z> <meshName>        whether an instance of that mesh stands within FMOInstanceRef::ToleranceCm of the point here
//   MO.Test.InstanceInteract <x> <y> <z> <meshName>  E on that instance without aiming: the interactor's real server RPC, next tick (for refusal controls)
//   MO.Test.SightWall <x> <y> <z> [PlayerIndex=1]    (host) a 3 x 3 m slab on this machine only, between that player's view and the point; 'clear' (C8)
//   MO.Test.NearestSweepable [radiusCm=2000]         the decorative instance (AutoSweepTags) nearest the local pawn: where a foliage test can clear something
//   MO.Test.Sweepable <x> <y> <z> [radiusCm=250]     decorative instances (UMOTerrainModificationSubsystem::AutoSweepTags) within the radius here, and the
//                                                    worked-ground zones this machine knows (a client's are the host's)
//   MO.Test.ResourceNodes [radiusCm=3000] [n=5]      resource nodes (ResourceNode_ tags) nearest the local pawn, each with the first action the pawn can do
//                                                    that keeps the node (its yields, seconds)
//   MO.Test.Harvest <x> <y> <z> <meshName> <ActionId> the KeepOnHarvest menu's request for that instance: AMOPlayerController::ServerBeginHarvest, next tick
//   MO.Test.InjectInput <IA_Name> [X Y] [Seconds]  a REAL Enhanced Input action, as if its key were held (the controller's own handlers run)
//   MO.Test.Inspect <ItemId>                         the inventory's Inspect on the first carried item of that kind (UMOUIManagerComponent::StartItemInspection), next tick
//   MO.Test.SurvivorCommand <FollowMe|StayHere|SetHome|GoHome|Job <JobType>>  the survivor menus' order to the nearest recruited survivor
//                                                    (FMOSurvivorCommands::Submit), next tick
//   MO.Test.RemotePawns                              every OTHER player's pawn as THIS machine simulates it: speed, sim gravity off, floor (N-160)
//
// Instance names and indices differ per machine (MOInstanceRef.h), so every verb names an instance by mesh + world location.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MOViewpointUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "MOFramework.h"
#include "MOInstanceRef.h"
#include "MOInteractorComponent.h"
#include "MOTerrainModificationSubsystem.h"
#include "MOHarvestSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOPlayerController.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOUIManagerComponent.h"
#include "MOSurvivorCommands.h"
#include "MORecruitmentComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Containers/Ticker.h"
#include "EngineUtils.h"

namespace MOCoopInstanceTest   // named, not anonymous: unity builds put this file in one translation unit with others
{
	APawn* LocalPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	/** The ItemId of a ground-item component (its MOItem_<ItemId> tag), or None. */
	FName GroundItemIdOf(const UInstancedStaticMeshComponent* ISM)
	{
		for (const FName& Tag : ISM->ComponentTags)
		{
			FString S = Tag.ToString();
			if (S.RemoveFromStart(TEXT("MOItem_")))
			{
				return FName(*S);
			}
		}
		return NAME_None;
	}

	/** Every in-world instanced mesh component of World whose bounds come within RadiusCm of P. */
	template <typename FVisit>
	void ForEachISMNear(UWorld* World, const FVector& P, double RadiusCm, FVisit&& Visit)
	{
		for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
		{
			UInstancedStaticMeshComponent* ISM = *It;
			if (IsValid(ISM) && ISM->GetWorld() == World && ISM->GetInstanceCount() > 0 && ISM->GetStaticMesh()
				&& ISM->Bounds.ComputeSquaredDistanceFromBoxToPoint(P) <= RadiusCm * RadiusCm)
			{
				Visit(ISM);
			}
		}
	}

	/** <x> <y> <z> from Args[0..2]. */
	bool ParsePoint(const TArray<FString>& Args, FVector& Out)
	{
		if (Args.Num() < 3)
		{
			return false;
		}
		Out = FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2]));
		return true;
	}

	/** The instance of the mesh named MeshName nearest P within FMOInstanceRef::ToleranceCm on this machine. */
	bool FindInstanceAt(UWorld* World, const FVector& P, const FString& MeshName, UInstancedStaticMeshComponent*& OutISM, int32& OutIndex, double& OutDist)
	{
		OutISM = nullptr;
		OutIndex = INDEX_NONE;
		OutDist = FMOInstanceRef::ToleranceCm;
		ForEachISMNear(World, P, FMOInstanceRef::ToleranceCm, [&](UInstancedStaticMeshComponent* ISM)
		{
			if (ISM->GetStaticMesh()->GetName() != MeshName)
			{
				return;
			}
			for (int32 I = 0; I < ISM->GetInstanceCount(); ++I)
			{
				FTransform Xf;
				const double D = ISM->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) ? FVector::Dist(Xf.GetLocation(), P) : TNumericLimits<double>::Max();
				if (D <= OutDist)
				{
					OutDist = D;
					OutISM = ISM;
					OutIndex = I;
				}
			}
		});
		return OutISM != nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GGroundItemsCmd(
		TEXT("MO.Test.GroundItems"),
		TEXT("Test: the PCG ground items (MOItem_ tags) nearest the local pawn on THIS machine, nearest first. Logs [MOTEST] GROUND_ITEM lines. Usage: MO.Test.GroundItems [radiusCm=3000] [n=5]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APawn* Pawn = LocalPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL GroundItems: no local pawn"));
				return;
			}
			const double Radius = Args.Num() > 0 ? FMath::Clamp(FCString::Atod(*Args[0]), 100.0, 100000.0) : 3000.0;
			const int32 MaxLines = Args.Num() > 1 ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 100) : 5;
			struct FFound { double Dist; FName Item; FString Mesh; FVector At; FVector Centre; };
			TArray<FFound> Found;
			const FVector P = Pawn->GetActorLocation();
			ForEachISMNear(World, P, Radius, [&](UInstancedStaticMeshComponent* ISM)
			{
				const FName Item = GroundItemIdOf(ISM);
				if (Item.IsNone())
				{
					return;
				}
				for (int32 I = 0; I < ISM->GetInstanceCount(); ++I)
				{
					FTransform Xf;
					if (ISM->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) && FVector::Dist(Xf.GetLocation(), P) <= Radius)
					{
						// The centre of the item's bounds: aim there, not at its origin (a small stone's origin is at the ground, and a line
						// to it grazed the terrain first: N-187)
						const FVector Centre = ISM->GetStaticMesh()->GetBoundingBox().TransformBy(Xf).GetCenter();
						Found.Add({ FVector::Dist(Xf.GetLocation(), P), Item, ISM->GetStaticMesh()->GetName(), Xf.GetLocation(), Centre });
					}
				}
			});
			Found.Sort([](const FFound& A, const FFound& B) { return A.Dist < B.Dist; });
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] GROUND_ITEMS n=%d within %.0f cm of (%.0f, %.0f, %.0f)"), Found.Num(), Radius, P.X, P.Y, P.Z);
			for (int32 K = 0; K < FMath::Min(MaxLines, Found.Num()); ++K)
			{
				const FFound& F = Found[K];
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] GROUND_ITEM i=%d item=%s mesh=%s at=%.1f,%.1f,%.1f dist=%.0f centre=%.1f,%.1f,%.1f"),
					K, *F.Item.ToString(), *F.Mesh, F.At.X, F.At.Y, F.At.Z, F.Dist, F.Centre.X, F.Centre.Y, F.Centre.Z);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GRemotePawnsCmd(
		TEXT("MO.Test.RemotePawns"),
		TEXT("Test: every other player's pawn as THIS machine simulates it (a client's simulated proxies): its speed, whether sim gravity is off, its movement mode. Logs [MOTEST] REMOTE_PAWN lines. Usage: MO.Test.RemotePawns"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			int32 Count = 0;
			for (TActorIterator<ACharacter> It(World); It; ++It)
			{
				ACharacter* Other = *It;
				if (Other->IsLocallyControlled() || !Other->GetPlayerState())
				{
					continue;   // this machine's own player, or a pawn no player drives
				}
				const UCharacterMovementComponent* Move = Other->GetCharacterMovement();
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] REMOTE_PAWN name=%s role=%s speed=%.0f simGravityOff=%d mode=%s at=%s"),
					*Other->GetName(), *UEnum::GetValueAsString(Other->GetLocalRole()), Other->GetVelocity().Size2D(), Other->bSimGravityDisabled ? 1 : 0,
					Move ? *UEnum::GetValueAsString(Move->MovementMode.GetValue()) : TEXT("-"), *Other->GetActorLocation().ToCompactString());
				++Count;
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] REMOTE_PAWNS n=%d"), Count);
		}));

	FAutoConsoleCommandWithWorldAndArgs GInstanceAtCmd(
		TEXT("MO.Test.InstanceAt"),
		TEXT("Test: whether an instance of the named mesh stands within 25 cm of the point on THIS machine. Logs [MOTEST] INSTANCE_AT present=0|1. Usage: MO.Test.InstanceAt <x> <y> <z> <meshName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FVector P;
			if (!ParsePoint(Args, P) || Args.Num() < 4)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL InstanceAt: usage MO.Test.InstanceAt <x> <y> <z> <meshName>"));
				return;
			}
			UInstancedStaticMeshComponent* ISM = nullptr;
			int32 Index = INDEX_NONE;
			double Dist = 0.0;
			const bool bPresent = FindInstanceAt(World, P, Args[3], ISM, Index, Dist);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] INSTANCE_AT present=%d mesh=%s at=%.1f,%.1f,%.1f%s"), bPresent ? 1 : 0, *Args[3], P.X, P.Y, P.Z,
				bPresent ? *FString::Printf(TEXT(" off=%.1fcm item=%s"), Dist, *GroundItemIdOf(ISM).ToString()) : TEXT(""));
		}));

	FAutoConsoleCommandWithWorldAndArgs GInstanceInteractCmd(
		TEXT("MO.Test.InstanceInteract"),
		TEXT("Test: E on the named mesh's instance at the point, without aiming: the interactor's real server RPC, next tick. Usage: MO.Test.InstanceInteract <x> <y> <z> <meshName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FVector P;
			if (!ParsePoint(Args, P) || Args.Num() < 4)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL InstanceInteract: usage MO.Test.InstanceInteract <x> <y> <z> <meshName>"));
				return;
			}
			const FString MeshName = Args[3];
			// Next tick: inside the dev bridge's call a joiner's RPC would run locally (Docs/Coop_And_Networking.md section 6).
			World->GetTimerManager().SetTimerForNextTick([WeakWorld = TWeakObjectPtr<UWorld>(World), P, MeshName]()
			{
				UWorld* W = WeakWorld.Get();
				APawn* Pawn = LocalPawn(W);
				UMOInteractorComponent* Interactor = Pawn ? Pawn->FindComponentByClass<UMOInteractorComponent>() : nullptr;
				UInstancedStaticMeshComponent* ISM = nullptr;
				int32 Index = INDEX_NONE;
				double Dist = 0.0;
				if (!Interactor || !FindInstanceAt(W, P, MeshName, ISM, Index, Dist))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL InstanceInteract: %s"), Interactor ? TEXT("no such instance here") : TEXT("no local pawn with an interactor"));
					return;
				}
				// The name before the request: on the host (or standalone) the pick-up runs at once and the index then names another instance.
				const FString Named = FMOInstanceRef::Make(ISM, Index).ToString();
				const bool bSent = Interactor->RequestInteractWithInstance(ISM, Index);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] InstanceInteract: %s %s"), bSent ? TEXT("sent") : TEXT("NOT sent"), *Named);
			});
		}));

	/** The slab MO.Test.SightWall put up on this machine (one at a time). */
	TWeakObjectPtr<AActor> GSightWall;

	FAutoConsoleCommandWithWorldAndArgs GSightWallCmd(
		TEXT("MO.Test.SightWall"),
		TEXT("Test (host): a 3 x 3 m slab on THIS machine only, 45% of the way from player PlayerIndex's view (the server's own viewpoint for that player) to the point: what the server's line-of-sight check on an instance request must refuse through (C8). 'clear' takes it down. Usage: MO.Test.SightWall <x> <y> <z> [PlayerIndex=1] | MO.Test.SightWall clear"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AActor* Old = GSightWall.Get())
			{
				Old->Destroy();
			}
			GSightWall.Reset();
			if (Args.Num() > 0 && Args[0] == TEXT("clear"))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SIGHT_WALL cleared"));
				return;
			}
			FVector P;
			APlayerController* PC = World && Args.Num() >= 3 ? UGameplayStatics::GetPlayerController(World, Args.Num() > 3 ? FCString::Atoi(*Args[3]) : 1) : nullptr;
			FVector View;
			FRotator ViewRotation;
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (!ParsePoint(Args, P) || !PC || !Cube || !UMOViewpointUtils::ResolveViewpointForController(PC, View, ViewRotation))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL SightWall: usage MO.Test.SightWall <x> <y> <z> [PlayerIndex=1] (needs that player's controller here and the engine cube)"));
				return;
			}
			const FVector At = View + (P - View) * 0.45;
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(At, FRotator(0.0, (P - View).Rotation().Yaw, 0.0), Spawn);
			if (!Wall)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL SightWall: the spawn failed"));
				return;
			}
			UStaticMeshComponent* Mesh = Wall->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(Cube);
			Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->SetActorScale3D(FVector(0.1, 3.0, 3.0));   // the cube is 1 m: a slab 10 cm thick, 3 m wide and tall, across the line
			GSightWall = Wall;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SIGHT_WALL at=%s view=%s point=%s"), *At.ToCompactString(), *View.ToCompactString(), *P.ToCompactString());
		}));

	FAutoConsoleCommandWithWorldAndArgs GResourceNodesCmd(
		TEXT("MO.Test.ResourceNodes"),
		TEXT("Test: resource nodes nearest the local pawn on THIS machine, each with the first action the pawn can do that keeps the node. Logs [MOTEST] RESOURCE_NODE lines. Usage: MO.Test.ResourceNodes [radiusCm=3000] [n=5]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Pawn = LocalPawn(World);
			UMOHarvestSubsystem* Harvest = World ? World->GetSubsystem<UMOHarvestSubsystem>() : nullptr;
			if (!Pawn || !Harvest)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL ResourceNodes: no local pawn or no harvest subsystem"));
				return;
			}
			const double Radius = Args.Num() > 0 ? FMath::Clamp(FCString::Atod(*Args[0]), 100.0, 100000.0) : 3000.0;
			const int32 MaxLines = Args.Num() > 1 ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 100) : 5;
			const UMOInventoryComponent* Inventory = Pawn->FindComponentByClass<UMOInventoryComponent>();
			struct FFound { double Dist; FString Line; };
			TArray<FFound> Found;
			const FVector P = Pawn->GetActorLocation();
			ForEachISMNear(World, P, Radius, [&](UInstancedStaticMeshComponent* ISM)
			{
				const FMOResourceNodeDefinitionRow* Def = Harvest->GetResourceDefinitionForComponent(ISM);
				if (!Def)
				{
					return;
				}
				const FMOResourceHarvestAction* Doable = nullptr;
				for (const FMOResourceHarvestAction& Action : Def->HarvestActions)
				{
					if (!Action.bDestroysResource && Action.YieldsItems.Num() > 0 && UMOHarvestSubsystem::HasToolForAction(Action, Inventory)
						&& UMOHarvestSubsystem::HasKnowledgeForAction(Action, Pawn))
					{
						Doable = &Action;
						break;
					}
				}
				if (!Doable)
				{
					return;
				}
				FString Yields;
				for (const FName& Y : Doable->YieldsItems)
				{
					Yields += (Yields.IsEmpty() ? TEXT("") : TEXT(",")) + Y.ToString();
				}
				for (int32 I = 0; I < ISM->GetInstanceCount(); ++I)
				{
					FTransform Xf;
					if (ISM->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) && FVector::Dist(Xf.GetLocation(), P) <= Radius)
					{
						const FVector L = Xf.GetLocation();
						Found.Add({ FVector::Dist(L, P), FString::Printf(TEXT("mesh=%s at=%.1f,%.1f,%.1f dist=%.0f action=%s yields=%s seconds=%.1f"),
							*ISM->GetStaticMesh()->GetName(), L.X, L.Y, L.Z, FVector::Dist(L, P), *Doable->ActionId.ToString(), *Yields, Doable->BaseActionTime) });
					}
				}
			});
			Found.Sort([](const FFound& A, const FFound& B) { return A.Dist < B.Dist; });
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] RESOURCE_NODES n=%d within %.0f cm"), Found.Num(), Radius);
			for (int32 K = 0; K < FMath::Min(MaxLines, Found.Num()); ++K)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] RESOURCE_NODE i=%d %s"), K, *Found[K].Line);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GHarvestCmd(
		TEXT("MO.Test.Harvest"),
		TEXT("Test: the KeepOnHarvest menu's request for the named mesh's instance at the point (AMOPlayerController::ServerBeginHarvest, next tick). Usage: MO.Test.Harvest <x> <y> <z> <meshName> <ActionId>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FVector P;
			if (!ParsePoint(Args, P) || Args.Num() < 5)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Harvest: usage MO.Test.Harvest <x> <y> <z> <meshName> <ActionId>"));
				return;
			}
			const FString MeshName = Args[3];
			const FName ActionId(*Args[4]);
			// Next tick: inside the dev bridge's call a joiner's RPC would run locally (Docs/Coop_And_Networking.md section 6).
			World->GetTimerManager().SetTimerForNextTick([WeakWorld = TWeakObjectPtr<UWorld>(World), P, MeshName, ActionId]()
			{
				UWorld* W = WeakWorld.Get();
				AMOPlayerController* PC = W ? Cast<AMOPlayerController>(W->GetFirstPlayerController()) : nullptr;
				UInstancedStaticMeshComponent* ISM = nullptr;
				int32 Index = INDEX_NONE;
				double Dist = 0.0;
				if (!PC || !FindInstanceAt(W, P, MeshName, ISM, Index, Dist))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Harvest: %s"), PC ? TEXT("no such instance here") : TEXT("no MO player controller"));
					return;
				}
				const FMOInstanceRef Ref = FMOInstanceRef::Make(ISM, Index);
				PC->ServerBeginHarvest(Ref, ActionId);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] Harvest: asked %s on %s"), *ActionId.ToString(), *Ref.ToString());
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs GInspectCmd(
		TEXT("MO.Test.Inspect"),
		TEXT("Test: the inventory's Inspect on the first carried item of that kind (UMOUIManagerComponent::StartItemInspection: the server's study), next tick. Usage: MO.Test.Inspect <ItemId>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Inspect: usage MO.Test.Inspect <ItemId>"));
				return;
			}
			const FName ItemId(*Args[0]);
			World->GetTimerManager().SetTimerForNextTick([WeakWorld = TWeakObjectPtr<UWorld>(World), ItemId]()
			{
				UWorld* W = WeakWorld.Get();
				APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
				APawn* Pawn = PC ? PC->GetPawn() : nullptr;
				const UMOInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
				UMOUIManagerComponent* UIManager = PC ? PC->FindComponentByClass<UMOUIManagerComponent>() : nullptr;
				FGuid Guid;
				TArray<FMOInventoryEntry> Entries;
				if (Inventory)
				{
					Inventory->GetInventoryEntries(Entries);
				}
				for (const FMOInventoryEntry& Entry : Entries)
				{
					if (Entry.ItemDefinitionId == ItemId)
					{
						Guid = Entry.ItemGuid;
						break;
					}
				}
				if (!UIManager || !Guid.IsValid())
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Inspect: %s"), UIManager ? TEXT("the pawn carries none") : TEXT("no UI manager"));
					return;
				}
				UIManager->StartItemInspection(ItemId, Guid);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] Inspect: asked for %s (%s)"), *ItemId.ToString(), *Guid.ToString());
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs GSurvivorCommandCmd(
		TEXT("MO.Test.SurvivorCommand"),
		TEXT("Test: the survivor menus' order to the nearest recruited survivor within 30 m (FMOSurvivorCommands::Submit), next tick. Usage: MO.Test.SurvivorCommand <FollowMe|StayHere|SetHome|GoHome|Job <JobType>>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL SurvivorCommand: usage MO.Test.SurvivorCommand <FollowMe|StayHere|SetHome|GoHome|Job <JobType>>"));
				return;
			}
			FMOSurvivorCommand Command;
			const FString Kind = Args[0];
			if (Kind.Equals(TEXT("FollowMe"), ESearchCase::IgnoreCase)) { Command.Kind = EMOSurvivorCommandKind::FollowMe; }
			else if (Kind.Equals(TEXT("StayHere"), ESearchCase::IgnoreCase)) { Command.Kind = EMOSurvivorCommandKind::StayHere; }
			else if (Kind.Equals(TEXT("SetHome"), ESearchCase::IgnoreCase)) { Command.Kind = EMOSurvivorCommandKind::SetHome; }
			else if (Kind.Equals(TEXT("GoHome"), ESearchCase::IgnoreCase)) { Command.Kind = EMOSurvivorCommandKind::GoHome; }
			else if (Kind.Equals(TEXT("Job"), ESearchCase::IgnoreCase) && Args.Num() > 1)
			{
				Command.Kind = EMOSurvivorCommandKind::EnqueueJob;
				const int64 Value = StaticEnum<EMOSurvivorJobType>()->GetValueByNameString(Args[1]);
				Command.JobType = Value == INDEX_NONE ? EMOSurvivorJobType::None : static_cast<EMOSurvivorJobType>(Value);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL SurvivorCommand: unknown order '%s'"), *Kind);
				return;
			}
			World->GetTimerManager().SetTimerForNextTick([WeakWorld = TWeakObjectPtr<UWorld>(World), Command]()
			{
				UWorld* W = WeakWorld.Get();
				APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
				APawn* Own = PC ? PC->GetPawn() : nullptr;
				APawn* Best = nullptr;
				double BestSq = FMath::Square(3000.0);
				for (TActorIterator<APawn> It(W); It && Own; ++It)
				{
					const UMORecruitmentComponent* Recruitment = It->FindComponentByClass<UMORecruitmentComponent>();
					const double D = FVector::DistSquared(It->GetActorLocation(), Own->GetActorLocation());
					if (*It != Own && !It->IsPlayerControlled() && Recruitment && Recruitment->IsPossessable() && D < BestSq)
					{
						BestSq = D;
						Best = *It;
					}
				}
				if (!Best)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL SurvivorCommand: no recruited survivor within 30 m"));
					return;
				}
				const bool bSent = FMOSurvivorCommands::Submit(PC, Best, Command);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SurvivorCommand: %s order %d (job %d) to %s"), bSent ? TEXT("sent") : TEXT("NOT sent"),
					static_cast<int32>(Command.Kind), static_cast<int32>(Command.JobType), *Best->GetName());
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs GNearestSweepableCmd(
		TEXT("MO.Test.NearestSweepable"),
		TEXT("Test: the decorative instance (the terrain sweep's AutoSweepTags) nearest the local pawn on THIS machine. Logs [MOTEST] NEAREST_SWEEPABLE. Usage: MO.Test.NearestSweepable [radiusCm=2000]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APawn* Pawn = LocalPawn(World);
			const UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(World);
			if (!Pawn || !TerrainMod)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL NearestSweepable: no local pawn or no terrain-modification subsystem"));
				return;
			}
			const double Radius = Args.Num() > 0 ? FMath::Clamp(FCString::Atod(*Args[0]), 100.0, 20000.0) : 2000.0;
			const FVector P = Pawn->GetActorLocation();
			double BestSq = Radius * Radius;
			FVector Best = FVector::ZeroVector;
			bool bFound = false;
			ForEachISMNear(World, P, Radius, [&](UInstancedStaticMeshComponent* ISM)
			{
				const AActor* Owner = ISM->GetOwner();
				if (!TerrainMod->AutoSweepTags.ContainsByPredicate([ISM, Owner](const FName& Tag) { return !Tag.IsNone() && (ISM->ComponentHasTag(Tag) || (Owner && Owner->ActorHasTag(Tag))); }))
				{
					return;
				}
				for (int32 I = 0; I < ISM->GetInstanceCount(); ++I)
				{
					FTransform Xf;
					if (ISM->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) && FVector::DistSquared(Xf.GetLocation(), P) < BestSq)
					{
						BestSq = FVector::DistSquared(Xf.GetLocation(), P);
						Best = Xf.GetLocation();
						bFound = true;
					}
				}
			});
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] NEAREST_SWEEPABLE found=%d at=%.1f,%.1f,%.1f dist=%.0f"), bFound ? 1 : 0, Best.X, Best.Y, Best.Z, bFound ? FMath::Sqrt(BestSq) : -1.0);
		}));

	FAutoConsoleCommandWithWorldAndArgs GSweepableCmd(
		TEXT("MO.Test.Sweepable"),
		TEXT("Test: decorative instances (the terrain sweep's AutoSweepTags) within the radius of the point on THIS machine, and the worked-ground zones it knows. Usage: MO.Test.Sweepable <x> <y> <z> [radiusCm=250]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FVector P;
			const UMOTerrainModificationSubsystem* TerrainMod = UMOTerrainModificationSubsystem::Get(World);
			if (!ParsePoint(Args, P) || !TerrainMod)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Sweepable: %s"), TerrainMod ? TEXT("usage MO.Test.Sweepable <x> <y> <z> [radiusCm=250]") : TEXT("no terrain-modification subsystem"));
				return;
			}
			const double Radius = Args.Num() > 3 ? FMath::Clamp(FCString::Atod(*Args[3]), 10.0, 5000.0) : 250.0;
			int32 N = 0;
			ForEachISMNear(World, P, Radius, [&](UInstancedStaticMeshComponent* ISM)
			{
				const AActor* Owner = ISM->GetOwner();
				const bool bTagged = TerrainMod->AutoSweepTags.ContainsByPredicate([ISM, Owner](const FName& Tag)
				{
					return !Tag.IsNone() && (ISM->ComponentHasTag(Tag) || (Owner && Owner->ActorHasTag(Tag)));
				});
				if (!bTagged)
				{
					return;
				}
				for (int32 I = 0; I < ISM->GetInstanceCount(); ++I)
				{
					FTransform Xf;
					N += (ISM->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) && FVector::Dist(Xf.GetLocation(), P) <= Radius) ? 1 : 0;
				}
			});
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] SWEEPABLE n=%d radius=%.0f zones=%d modified=%d net=%s"), N, Radius, TerrainMod->GetZoneCount(),
				TerrainMod->IsLocationModified(P) ? 1 : 0, World->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"));
		}));
	// MO.Test.InjectInput <IA_Name> [X=1] [Y=0] [Seconds=0.3]: an Enhanced Input action injected as if its key were held, through the
	// controller's bindings and handlers -- tutorial events, menu gates, the HUD. MO.Test.Input skips all of that (it calls the pawn's
	// interface directly), so a naive-player pass of the tutorial saw no quest move at all (Oct 18 2026). Actions are /MOFramework/Input/<Name>.
	FAutoConsoleCommandWithWorldAndArgs GInjectInputCmd(
		TEXT("MO.Test.InjectInput"),
		TEXT("Test: a real Enhanced Input action as if its key were held -- the controller's own handlers run. Usage: MO.Test.InjectInput <IA_Name> [X=1] [Y=0] [Seconds=0.3]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ULocalPlayer* Local = PC ? PC->GetLocalPlayer() : nullptr;
			UEnhancedInputLocalPlayerSubsystem* Input = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			const FString Name = Args.Num() > 0 ? Args[0] : FString();
			UInputAction* Action = Name.IsEmpty() ? nullptr : LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/MOFramework/Input/%s.%s"), *Name, *Name));
			if (!Input || !Action)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] InjectInput FAILED: %s"), !Input ? TEXT("no local player input subsystem") : TEXT("no such action in /MOFramework/Input"));
				return;
			}
			const double X = Args.Num() > 1 ? FCString::Atod(*Args[1]) : 1.0;
			const double Y = Args.Num() > 2 ? FCString::Atod(*Args[2]) : 0.0;
			const double Seconds = Args.Num() > 3 ? FCString::Atod(*Args[3]) : 0.3;
			Input->StartContinuousInputInjectionForAction(Action, FInputActionValue(Action->ValueType, FVector(X, Y, 0.0)), {}, {});
			TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> WeakInput(Input);
			TWeakObjectPtr<UInputAction> WeakAction(Action);
			const double End = FPlatformTime::Seconds() + Seconds;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakInput, WeakAction, End](float)
			{
				if (FPlatformTime::Seconds() < End)
				{
					return true;
				}
				if (WeakInput.IsValid() && WeakAction.IsValid())
				{
					WeakInput->StopContinuousInputInjectionForAction(WeakAction.Get());
				}
				return false;
			}));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] InjectInput %s (%.2f, %.2f) for %.2f s"), *Name, X, Y, Seconds);
		}));
}

#endif // !UE_BUILD_SHIPPING

