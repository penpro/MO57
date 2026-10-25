// A dropped item that tunnels through the ground comes back resting on it: the BOTTOM of its mesh on the ground, not its pivot 10 cm above
// (N-209, Oct 24 2026: 1 of 4 BronzeArrowhead01 drops fell through the voxel floor despite CCD; the settle hung the 5 cm head 8.5 cm in the air,
// and a big item lying on its side would have sunk the same way). Headless: a cube floor in an empty game world; the fall-through guard in
// AMOWorldItem::Tick is forced by moving the item well below the ground its drop found, exactly what the voxel terrain did to the arrowhead.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "MOWorldItem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MOWorldItemSettleTests
{
	constexpr double FloorTopZ = 10.0;   // a 100 cm engine cube scaled to 20 cm thick, centred on the origin

	struct FWorld
	{
		UWorld* World;
		UStaticMesh* Cube;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOWorldItemSettleWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (Cube)
			{
				AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
				Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Floor->GetStaticMeshComponent()->SetWorldScale3D(FVector(40.0f, 40.0f, 0.2f));
				Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			}
			Settle();
		}

		/** The world never ticks here: push new and moved bodies into the scene's query structure before tracing. */
		void Settle() const
		{
			if (FPhysScene* Scene = World->GetPhysicsScene())
			{
				Scene->Flush();
			}
		}
		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		/** Spawns an item whose mesh is a cube of Side cm with its pivot at the centre, Side / 2 over its bottom. */
		AMOWorldItem* Item(double Side) const
		{
			AMOWorldItem* Result = World->SpawnActor<AMOWorldItem>(FVector(0.0, 0.0, FloorTopZ + 60.0), FRotator::ZeroRotator);
			UStaticMeshComponent* Mesh = Result->FindComponentByClass<UStaticMeshComponent>();
			Mesh->SetStaticMesh(Cube);
			Mesh->SetWorldScale3D(FVector(Side / 100.0));
			Settle();
			return Result;
		}

		/** The ground straight under the item's pivot, found the way the world finds it (the item itself ignored). */
		bool GroundUnder(const AMOWorldItem* Item, FVector& OutHit) const
		{
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Item);
			const FVector Start = Item->GetActorLocation() + FVector(0.0, 0.0, 200.0);
			if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0, 0.0, 1000.0), ECC_WorldStatic, Params))
			{
				return false;
			}
			OutHit = Hit.Location;
			return true;
		}
	};

	/** The gap between the lowest point of the item's mesh and the ground under it: 0 at rest, positive floating, negative sunk. */
	double BottomGap(const AMOWorldItem* Item, const FVector& Ground)
	{
		const UStaticMeshComponent* Mesh = Item->FindComponentByClass<UStaticMeshComponent>();
		return Mesh->CalcBounds(Mesh->GetComponentTransform()).GetBox().Min.Z - Ground.Z;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOItems_SettleRestsMeshOnGround,
	"MOFramework.Items.SettleRestsMeshOnGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOItems_SettleRestsMeshOnGround::RunTest(const FString& Parameters)
{
	using namespace MOWorldItemSettleTests;
	FWorld W;
	if (!TestNotNull(TEXT("the engine's basic cube mesh loads"), W.Cube))
	{
		return false;
	}

	// The guard in Tick must be what brings each item back (two items below).
	AddExpectedMessage(TEXT("fell through the ground"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);

	// A thin item (5 cm, like the arrowhead: the old pivot + 10 cm left it 7.5 cm in the air) and a big one (80 cm: the old rule sank it 30 cm).
	for (const double Side : {5.0, 80.0})
	{
		AMOWorldItem* Item = W.Item(Side);
		Item->EnableDropPhysics();
		W.Settle();

		FVector DropGround;
		if (!TestTrue(FString::Printf(TEXT("%.0f cm item: the drop found the floor"), Side), W.GroundUnder(Item, DropGround)))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("%.0f cm item: the floor is where the test put it"), Side), DropGround.Z, FloorTopZ, 0.5);

		// The voxel terrain's physics body was not there: the item is now far under the ground its drop found.
		const FVector Here = Item->GetActorLocation();
		Item->SetActorLocation(FVector(Here.X, Here.Y, DropGround.Z - 200.0));
		Item->Tick(1.0f / 60.0f);
		W.Settle();

		FVector Ground;
		if (!TestTrue(FString::Printf(TEXT("%.0f cm item: ground found under the settled item"), Side), W.GroundUnder(Item, Ground)))
		{
			return false;
		}
		const double Gap = BottomGap(Item, Ground);
		TestTrue(FString::Printf(TEXT("%.0f cm item: its mesh bottom rests on the ground (gap %.2f cm, want |gap| < 1)"), Side, Gap), FMath::Abs(Gap) < 1.0);
		TestTrue(FString::Printf(TEXT("%.0f cm item: it is no longer simulating"), Side), !Item->FindComponentByClass<UStaticMeshComponent>()->IsSimulatingPhysics());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
