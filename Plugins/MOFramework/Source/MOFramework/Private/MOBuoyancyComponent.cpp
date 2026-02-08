#include "MOBuoyancyComponent.h"
#include "MOFramework.h"
#include "MOWaterActorBase.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

UMOBuoyancyComponent::UMOBuoyancyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	BuoyancyForce = 2000.0f;
	BuoyancyDamping = 0.5f;
	WaterLinearDrag = 3.0f;
	WaterAngularDrag = 1.0f;
	MaxBuoyancyDepth = 100.0f;
	bUseMultiplePoints = false;
	bAutoFindWater = true;
}

void UMOBuoyancyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindWater && !WaterActor.IsValid())
	{
		FindWaterActor();
	}

	// Initialize default buoyancy points if using multi-point and none are set
	if (bUseMultiplePoints && BuoyancyPoints.Num() == 0)
	{
		// Create 4 corner points for basic boat buoyancy
		FMOBuoyancyPoint FrontLeft;
		FrontLeft.LocalOffset = FVector(100.0f, -50.0f, 0.0f);
		BuoyancyPoints.Add(FrontLeft);

		FMOBuoyancyPoint FrontRight;
		FrontRight.LocalOffset = FVector(100.0f, 50.0f, 0.0f);
		BuoyancyPoints.Add(FrontRight);

		FMOBuoyancyPoint BackLeft;
		BackLeft.LocalOffset = FVector(-100.0f, -50.0f, 0.0f);
		BuoyancyPoints.Add(BackLeft);

		FMOBuoyancyPoint BackRight;
		BackRight.LocalOffset = FVector(-100.0f, 50.0f, 0.0f);
		BuoyancyPoints.Add(BackRight);
	}
}

void UMOBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!WaterActor.IsValid())
	{
		if (bAutoFindWater)
		{
			FindWaterActor();
		}
		return;
	}

	UpdateState();
	ApplyBuoyancyForces(DeltaTime);
}

void UMOBuoyancyComponent::FindWaterActor()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// First try to find by tag if specified
	if (!WaterActorTag.IsNone())
	{
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(World, WaterActorTag, TaggedActors);

		for (AActor* Actor : TaggedActors)
		{
			if (AMOWaterActorBase* Water = Cast<AMOWaterActorBase>(Actor))
			{
				WaterActor = Water;
				UE_LOG(LogMOFramework, Log, TEXT("[MOBuoyancy] Found water actor by tag: %s"), *Water->GetName());
				return;
			}
		}
	}

	// Otherwise find any water actor
	for (TActorIterator<AMOWaterActorBase> It(World); It; ++It)
	{
		WaterActor = *It;
		UE_LOG(LogMOFramework, Log, TEXT("[MOBuoyancy] Found water actor: %s"), *It->GetName());
		return;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOBuoyancy] No water actor found in world"));
}

UPrimitiveComponent* UMOBuoyancyComponent::GetPhysicsComponent()
{
	// Use cached if valid
	if (CachedPhysicsComponent.IsValid())
	{
		return CachedPhysicsComponent.Get();
	}

	// Use specified component if valid
	if (PhysicsComponent.IsValid())
	{
		CachedPhysicsComponent = PhysicsComponent;
		return CachedPhysicsComponent.Get();
	}

	// Find first primitive with physics enabled
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive->IsSimulatingPhysics())
		{
			CachedPhysicsComponent = Primitive;
			return CachedPhysicsComponent.Get();
		}
	}

	// Fallback to root component if it's a primitive
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		CachedPhysicsComponent = RootPrimitive;
		return CachedPhysicsComponent.Get();
	}

	return nullptr;
}

void UMOBuoyancyComponent::UpdateState()
{
	AActor* Owner = GetOwner();
	if (!Owner || !WaterActor.IsValid())
	{
		return;
	}

	FVector ActorLocation = Owner->GetActorLocation();
	CurrentWaterHeight = WaterActor->GetWaterHeightAtLocation(ActorLocation);
	CurrentDepth = CurrentWaterHeight - ActorLocation.Z;

	bool bWasInWater = bIsInWater;

	if (bUseMultiplePoints && BuoyancyPoints.Num() > 0)
	{
		// Calculate submersion based on points
		int32 UnderwaterCount = 0;
		FTransform ActorTransform = Owner->GetActorTransform();

		for (FMOBuoyancyPoint& Point : BuoyancyPoints)
		{
			FVector WorldPos = ActorTransform.TransformPosition(Point.LocalOffset);
			float PointWaterHeight = WaterActor->GetWaterHeightAtLocation(WorldPos);
			Point.CurrentDepth = PointWaterHeight - WorldPos.Z;
			Point.bIsUnderwater = Point.CurrentDepth > 0.0f;

			if (Point.bIsUnderwater)
			{
				++UnderwaterCount;
			}
		}

		SubmersionPercent = static_cast<float>(UnderwaterCount) / BuoyancyPoints.Num();
		bIsInWater = UnderwaterCount > 0;
	}
	else
	{
		// Simple single-point check
		bIsInWater = CurrentDepth > 0.0f;
		SubmersionPercent = bIsInWater ? FMath::Clamp(CurrentDepth / MaxBuoyancyDepth, 0.0f, 1.0f) : 0.0f;
	}

	// Fire events
	if (bIsInWater && !bWasInWater)
	{
		OnEnteredWater.Broadcast();
	}
	else if (!bIsInWater && bWasInWater)
	{
		OnExitedWater.Broadcast();
	}

	// Fire submersion change event if significant change
	if (FMath::Abs(SubmersionPercent - LastSubmersionPercent) > 0.05f)
	{
		LastSubmersionPercent = SubmersionPercent;
		OnSubmersionChanged.Broadcast(SubmersionPercent);
	}
}

void UMOBuoyancyComponent::ApplyBuoyancyForces(float DeltaTime)
{
	if (!bIsInWater)
	{
		return;
	}

	UPrimitiveComponent* Physics = GetPhysicsComponent();
	if (!Physics || !Physics->IsSimulatingPhysics())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bUseMultiplePoints && BuoyancyPoints.Num() > 0)
	{
		// Multi-point buoyancy
		FTransform ActorTransform = Owner->GetActorTransform();

		for (const FMOBuoyancyPoint& Point : BuoyancyPoints)
		{
			if (Point.bIsUnderwater)
			{
				FVector WorldPos = ActorTransform.TransformPosition(Point.LocalOffset);
				ApplyPointBuoyancy(WorldPos, Point.ForceMultiplier, Point.DragMultiplier, DeltaTime);
			}
		}
	}
	else
	{
		// Single-point buoyancy at actor center
		ApplyPointBuoyancy(Owner->GetActorLocation(), 1.0f, 1.0f, DeltaTime);
	}

	// Apply water drag
	FVector LinearVelocity = Physics->GetPhysicsLinearVelocity();
	FVector AngularVelocity = Physics->GetPhysicsAngularVelocityInDegrees();

	// Scale drag by submersion
	float DragScale = SubmersionPercent;

	FVector LinearDragForce = -LinearVelocity * WaterLinearDrag * DragScale;
	FVector AngularDragTorque = -AngularVelocity * WaterAngularDrag * DragScale;

	Physics->AddForce(LinearDragForce, NAME_None, true);
	Physics->AddTorqueInDegrees(AngularDragTorque, NAME_None, true);
}

void UMOBuoyancyComponent::ApplyPointBuoyancy(const FVector& WorldPosition, float ForceMultiplier, float DragMultiplier, float DeltaTime)
{
	if (!WaterActor.IsValid())
	{
		return;
	}

	UPrimitiveComponent* Physics = GetPhysicsComponent();
	if (!Physics)
	{
		return;
	}

	float WaterHeight = WaterActor->GetWaterHeightAtLocation(WorldPosition);
	float Depth = WaterHeight - WorldPosition.Z;

	if (Depth <= 0.0f)
	{
		return; // Point is above water
	}

	// Calculate buoyancy force
	// Force increases with depth up to MaxBuoyancyDepth
	float DepthFactor = FMath::Clamp(Depth / MaxBuoyancyDepth, 0.0f, 1.0f);

	// Add damping based on vertical velocity to prevent oscillation
	FVector PointVelocity = Physics->GetPhysicsLinearVelocityAtPoint(WorldPosition);
	float VerticalVelocity = PointVelocity.Z;
	float DampingForce = -VerticalVelocity * BuoyancyDamping;

	// Calculate total upward force
	float TotalForce = (BuoyancyForce * DepthFactor + DampingForce) * ForceMultiplier;

	// Apply force at the point location
	FVector Force = FVector::UpVector * TotalForce;
	Physics->AddForceAtLocation(Force, WorldPosition);
}

FVector UMOBuoyancyComponent::GetWaterSurfaceNormal() const
{
	AActor* Owner = GetOwner();
	if (!Owner || !WaterActor.IsValid())
	{
		return FVector::UpVector;
	}

	FMOWaterSurfaceInfo SurfaceInfo = WaterActor->GetWaterSurfaceInfo(Owner->GetActorLocation());
	return SurfaceInfo.SurfaceNormal;
}

FVector UMOBuoyancyComponent::GetWaterVelocity() const
{
	// TODO: Could calculate velocity from wave displacement over time
	// For now, return zero
	return FVector::ZeroVector;
}
