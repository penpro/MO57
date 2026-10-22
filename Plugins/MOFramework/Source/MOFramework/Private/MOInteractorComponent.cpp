#include "MOInteractorComponent.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"
#include "MOItemComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MOInteractionSubsystem.h"
#include "MOInteractableComponent.h"
#include "MOHISMInteractableComponent.h"
#include "MOPCGInteractionSubsystem.h"
#include "MOUIManagerComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"

namespace
{
	TAutoConsoleVariable<int32> CVarInteractDebug(TEXT("mo.Interact.Debug"), 0,
		TEXT("1 = log every pass of the interaction targeting (line, sweep, near the aim point: candidates, reach, line of sight). It runs every frame for the focus hint: switch it on around one press."));

	TAutoConsoleVariable<int32> CVarServerInstanceSight(TEXT("mo.Interact.ServerInstanceSight"), 1,
		TEXT("1 = the server refuses a player's pick-up / harvest of an instance out of the player's sight (C8, UMOInteractorComponent::ServerCheckInstanceRequest); 0 = reach and rate only."));

	FString DescribeHit(bool bHit, const FHitResult& Hit)
	{
		return bHit ? FString::Printf(TEXT("%s/%s at %.0f cm (%s)"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance, *Hit.ImpactPoint.ToCompactString())
			: FString(TEXT("nothing"));
	}
}

UMOInteractorComponent::UMOInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOInteractorComponent::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UMOInteractorComponent_BeginPlay);   // spawn-cost breakdown in traces (-statnamedevents)
	Super::BeginPlay();
}

bool UMOInteractorComponent::ResolveViewpoint(FVector& OutViewLocation, FRotator& OutViewRotation) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn))
	{
		return false;
	}

	// Try controller first (handles both player and AI controllers)
	AController* OwnerController = OwnerPawn->GetController();
	if (UMOViewpointUtils::ResolveViewpointForController(OwnerController, OutViewLocation, OutViewRotation))
	{
		return true;
	}

	// Fall back to pawn eyes if no controller
	return UMOViewpointUtils::ResolveViewpointForPawn(OwnerPawn, OutViewLocation, OutViewRotation);
}

void UMOInteractorComponent::BuildTrace(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutTraceStart, FVector& OutTraceEnd) const
{
	const FVector ViewForwardVector = ViewRotation.Vector();
	OutTraceStart = ViewLocation + (ViewForwardVector * TraceConfig.ViewStartForwardOffset);
	OutTraceEnd = OutTraceStart + (ViewForwardVector * TraceConfig.TraceDistance);
}

bool UMOInteractorComponent::TraceForHit(const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutHitResult) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOInteractorTrace), TraceConfig.bTraceComplex);
	QueryParams.bReturnPhysicalMaterial = false;

	if (IsValid(OwnerActor))
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	const ECollisionChannel TraceChannel = TraceConfig.TraceChannel.GetValue();

	if (TraceConfig.TraceRadius > 0.0f)
	{
		const FCollisionShape SphereShape = FCollisionShape::MakeSphere(TraceConfig.TraceRadius);
		return World->SweepSingleByChannel(OutHitResult, TraceStart, TraceEnd, FQuat::Identity, TraceChannel, SphereShape, QueryParams);
	}

	return World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, QueryParams);
}

bool UMOInteractorComponent::FindInteractTarget(AActor*& OutTargetActor, FHitResult& OutHitResult) const
{
	OutTargetActor = nullptr;
	OutHitResult = FHitResult();

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveViewpoint(ViewLocation, ViewRotation))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] FindInteractTarget: Failed to resolve viewpoint"));
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	BuildTrace(ViewLocation, ViewRotation, TraceStart, TraceEnd);

	if (!TraceForHit(TraceStart, TraceEnd, OutHitResult))
	{
		return false;
	}

	AActor* HitActor = OutHitResult.GetActor();
	if (!IsValid(HitActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] FindInteractTarget: Hit but no valid actor"));
		return false;
	}

	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (!IsValid(InteractableComponent))
	{
		return false;
	}

	OutTargetActor = HitActor;
	return true;
}

bool UMOInteractorComponent::IsHarvestableInstanceComponent(const UInstancedStaticMeshComponent* ISMComp, const UWorld* World)
{
	if (!ISMComp)
	{
		return false;
	}
	// KeepOnHarvest (trees / bushes that persist), Harvestable (generic), MOResource_* (typed: Rock, Ore, Tree, Bush, Plant) -- on the component
	// or, for the first two, its owner.
	const AActor* Owner = ISMComp->GetOwner();
	if (ISMComp->ComponentHasTag(TEXT("KeepOnHarvest")) || ISMComp->ComponentHasTag(TEXT("Harvestable"))
		|| (Owner && (Owner->ActorHasTag(TEXT("KeepOnHarvest")) || Owner->ActorHasTag(TEXT("Harvestable")))))
	{
		return true;
	}
	for (const FName& Tag : ISMComp->ComponentTags)
	{
		if (Tag.ToString().StartsWith(TEXT("MOResource_")))
		{
			return true;
		}
	}
	// An item lying in the world as an instance (a stick, a stone): the PCG subsystem's tag / mesh -> item mapping.
	const UMOPCGInteractionSubsystem* PCGSubsystem = World ? World->GetSubsystem<UMOPCGInteractionSubsystem>() : nullptr;
	return PCGSubsystem && (!PCGSubsystem->GetItemIdForComponentTags(ISMComp).IsNone() || PCGSubsystem->IsMeshHarvestable(ISMComp->GetStaticMesh()));
}

bool UMOInteractorComponent::IsWorldCoverComponent(const UInstancedStaticMeshComponent* ISMComp)
{
	if (!ISMComp)
	{
		return false;
	}
	// biome_palettes.py tags every decorative layer MOBiomeCover_<Grass|Flower|Shrub|Debris>; the native grass spawner tags its own "grass".
	for (const FName& Tag : ISMComp->ComponentTags)
	{
		if (Tag == TEXT("grass") || Tag.ToString().StartsWith(TEXT("MOBiomeCover_")))
		{
			return true;
		}
	}
	return false;
}

bool UMOInteractorComponent::IsSeeThroughForInteraction(const UPrimitiveComponent* Component)
{
	const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Component);
	return Ism && IsWorldCoverComponent(Ism) && !IsHarvestableInstanceComponent(Ism, Ism->GetWorld());
}

bool UMOInteractorComponent::IsInstanceInSight(UWorld* World, const FVector& ViewLocation, const UInstancedStaticMeshComponent* ISM, int32 Index, const AActor* IgnoredActor,
	ECollisionChannel Channel, float NearTolerance)
{
	FTransform Xf;
	if (!World || !IsValid(ISM) || !ISM->GetInstanceTransform(Index, Xf, /*bWorldSpace=*/true))
	{
		return false;
	}
	const FVector Pivot = Xf.GetLocation();
	TArray<FVector, TInlineAllocator<4>> Points;
	Points.Add(Pivot + FVector(0.0f, 0.0f, 5.0f));   // a stone lying on the ground
	if (const UStaticMesh* Mesh = ISM->GetStaticMesh())
	{
		const FBox Box = Mesh->GetBoundingBox().TransformBy(Xf);
		Points.Add(FVector(Pivot.X, Pivot.Y, FMath::Clamp(ViewLocation.Z, Box.Min.Z, Box.Max.Z)));   // a trunk at eye height
		Points.Add(Box.GetCenter());
		Points.Add(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z - 0.1 * Box.GetSize().Z));   // a crown, a boulder's top
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MOInstanceSight), false);
	if (IgnoredActor)
	{
		Params.AddIgnoredActor(IgnoredActor);
	}
	for (const FVector& Point : Points)
	{
		FHitResult Block;
		FHitResult Cover;
		const bool bBlocked = UMOViewpointUtils::TraceThroughSeeThrough(World, ViewLocation, Point, Channel, Params, 0.0f,
			&UMOInteractorComponent::IsSeeThroughForInteraction, Block, Cover);
		if (!bBlocked || Block.GetComponent() == ISM || FVector::Dist(Block.ImpactPoint, Point) < NearTolerance)
		{
			return true;
		}
	}
	return false;
}

UMOInteractorComponent::EInstanceRequestCheck UMOInteractorComponent::ServerCheckInstanceRequest(APawn* Pawn, const UInstancedStaticMeshComponent* ISM, int32 Index, FString& OutWhy)
{
	UWorld* World = IsValid(Pawn) ? Pawn->GetWorld() : nullptr;
	FTransform Xf;
	if (!World || !IsValid(ISM) || Index < 0 || Index >= ISM->GetInstanceCount() || !ISM->GetInstanceTransform(Index, Xf, /*bWorldSpace=*/true))
	{
		OutWhy = FString::Printf(TEXT("instance index %d out of range (count %d)"), Index, IsValid(ISM) ? ISM->GetInstanceCount() : 0);
		return EInstanceRequestCheck::BadIndex;
	}

	// Reach: the actor path's (UMOInteractionSubsystem::MaximumInteractDistance), doubled because an instance's pivot can sit metres from the
	// surface the player aimed at (a trunk's). Still kills the harvest-anything-anywhere exploit by orders of magnitude.
	const UMOInteractionSubsystem* Interaction = World->GetSubsystem<UMOInteractionSubsystem>();
	const float Reach = 2.0f * (Interaction ? Interaction->MaximumInteractDistance : 500.0f);
	const float Distance = FVector::Dist(Pawn->GetActorLocation(), Xf.GetLocation());
	if (Distance > Reach)
	{
		OutWhy = FString::Printf(TEXT("instance %.0fcm away (max %.0fcm)"), Distance, Reach);
		return EInstanceRequestCheck::TooFar;
	}

	// Sight: from the player's view (the actor path's viewpoint), on the pawn's own interaction channel, by the client's own rule.
	if (CVarServerInstanceSight.GetValueOnGameThread() > 0)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		if (!UMOViewpointUtils::ResolveViewpointForController(Pawn->GetController(), ViewLocation, ViewRotation))
		{
			Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		}
		const UMOInteractorComponent* Interactor = Pawn->FindComponentByClass<UMOInteractorComponent>();
		const ECollisionChannel Channel = Interactor ? Interactor->TraceConfig.TraceChannel.GetValue() : ECC_Visibility;
		const float Near = Interactor ? Interactor->TraceConfig.AimAssistRadius : 40.0f;
		if (!IsInstanceInSight(World, ViewLocation, ISM, Index, Pawn, Channel, Near))
		{
			OutWhy = FString::Printf(TEXT("out of sight: %s #%d, %.0f cm away, from the view at %s"), *GetNameSafe(ISM->GetStaticMesh()), Index, Distance,
				*ViewLocation.ToCompactString());
			return EInstanceRequestCheck::OutOfSight;
		}
	}
	return EInstanceRequestCheck::Ok;
}

bool UMOInteractorComponent::FindInteractionTarget(FMOInteractionTarget& OutTarget) const
{
	OutTarget.Reset();

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolveViewpoint(ViewLocation, ViewRotation))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] FindInteractionTarget: Failed to resolve viewpoint"));
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	BuildTrace(ViewLocation, ViewRotation, TraceStart, TraceEnd);

	// Every pass looks through decorative cover (IsSeeThroughForInteraction): a stick lying in grass is the target, not the grass. The grass is
	// the last resort -- it forages when nothing interactable lies behind it.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOInteractorTrace), TraceConfig.bTraceComplex);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.AddIgnoredActor(GetOwner());
	const ECollisionChannel Channel = TraceConfig.TraceChannel.GetValue();
	const bool bDebug = CVarInteractDebug.GetValueOnGameThread() > 0;
	auto Found = [&OutTarget, bDebug](const TCHAR* Pass)
	{
		if (bDebug)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] target %s via %s"), *GetNameSafe(OutTarget.TargetActor.Get()), Pass);
		}
		return true;
	};

	// 1. Precise: what the crosshair is exactly on.
	FHitResult LineHit;
	FHitResult LineCover;
	const bool bLineSolid = UMOViewpointUtils::TraceThroughSeeThrough(GetWorld(), TraceStart, TraceEnd, Channel, QueryParams, 0.0f,
		&UMOInteractorComponent::IsSeeThroughForInteraction, LineHit, LineCover);
	if (bLineSolid && EvaluateInteractionHit(LineHit, OutTarget))
	{
		return Found(TEXT("the line"));
	}

	// 2. Forgiving: the sweep -- a thin branch or a post the crosshair slipped past.
	FHitResult SweepHit;
	FHitResult SweepCover;
	const bool bSweepSolid = UMOViewpointUtils::TraceThroughSeeThrough(GetWorld(), TraceStart, TraceEnd, Channel, QueryParams, TraceConfig.TraceRadius,
		&UMOInteractorComponent::IsSeeThroughForInteraction, SweepHit, SweepCover);
	if (bDebug)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] target passes: line -> %s (cover %s) | sweep %.0f -> %s (cover %s)"),
			*DescribeHit(bLineSolid, LineHit), *GetNameSafe(LineCover.GetComponent()), TraceConfig.TraceRadius, *DescribeHit(bSweepSolid, SweepHit),
			*GetNameSafe(SweepCover.GetComponent()));
	}
	if (bSweepSolid && EvaluateInteractionHit(SweepHit, OutTarget))
	{
		return Found(TEXT("the sweep"));
	}

	// 3. Near where the crosshair rests: something small lying on the ground there. The sweep meets the ground in front of it first, and the
	// line can pass a few centimetres over it.
	if (bLineSolid && FindInteractableNearAimPoint(ViewLocation, LineHit.ImpactPoint, OutTarget))
	{
		return Found(TEXT("near the aim point"));
	}

	// 4. Cover with nothing interactable behind it forages.
	const FHitResult& CoverHit = LineCover.GetComponent() ? LineCover : SweepCover;
	if (CoverHit.GetComponent())
	{
		OutTarget.Reset();
		OutTarget.HitResult = CoverHit;
		OutTarget.TargetActor = CoverHit.GetActor();
		OutTarget.bIsForageTarget = true;
		return Found(TEXT("cover (forage)"));
	}
	OutTarget.Reset();
	if (bDebug)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] no target"));
	}
	return false;
}

bool UMOInteractorComponent::FindInteractableNearAimPoint(const FVector& ViewLocation, const FVector& AimPoint, FMOInteractionTarget& OutTarget) const
{
	UWorld* World = GetWorld();
	if (!World || TraceConfig.AimAssistRadius <= 0.0f)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOInteractorAimAssist), false);
	QueryParams.AddIgnoredActor(GetOwner());
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, AimPoint, FQuat::Identity, TraceConfig.TraceChannel.GetValue(),
		FCollisionShape::MakeSphere(TraceConfig.AimAssistRadius), QueryParams);

	// Each interactable actor once, by its overlapping component nearest the aim point (0 inside the component's bounds); each harvestable instance
	// (a stick, a flint, a bush) by its own pivot.
	struct FCandidate { AActor* Actor; UPrimitiveComponent* Component; float DistSq; FVector Point; int32 InstanceIndex = INDEX_NONE; };
	TArray<FCandidate> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		// An instance lying here: a ground item flush with the terrain (a flint half sunk in it) that neither the line nor the sweep meets. Only actors
		// with an interactable component were candidates until Oct 18 2026, so such an item could not be picked up with E at all.
		UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Component);
		if (CVarInteractDebug.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor]   overlap %s/%s item %d%s"), *GetNameSafe(Actor), *GetNameSafe(Component), Overlap.ItemIndex,
				ISM ? (IsHarvestableInstanceComponent(ISM, World) ? TEXT(" (harvestable instance)") : TEXT(" (instance, not harvestable)")) : TEXT(""));
		}
		if (IsValid(Actor) && ISM && IsHarvestableInstanceComponent(ISM, World))
		{
			// The overlap names the instance it met (one result per instance body); its pivot is where the item lies.
			FTransform Xf;
			if (Overlap.ItemIndex != INDEX_NONE && ISM->GetInstanceTransform(Overlap.ItemIndex, Xf, /*bWorldSpace=*/true))
			{
				const FVector Pivot = Xf.GetLocation();
				const float DistSq = FVector::DistSquared(Pivot, AimPoint);
				FCandidate* Same = Candidates.FindByPredicate([ISM, &Overlap](const FCandidate& C) { return C.Component == ISM && C.InstanceIndex == Overlap.ItemIndex; });
				if (!Same)
				{
					Candidates.Add({Actor, ISM, DistSq, Pivot, Overlap.ItemIndex});
				}
			}
			continue;
		}
		if (!IsValid(Actor) || !Component || !Actor->FindComponentByClass<UMOInteractableComponent>())
		{
			continue;
		}
		const FBox Box = Component->Bounds.GetBox();
		const FVector Point = Box.GetClosestPointTo(AimPoint);
		const float DistSq = FVector::DistSquared(Point, AimPoint);
		FCandidate* Existing = Candidates.FindByPredicate([Actor](const FCandidate& C) { return C.Actor == Actor && C.InstanceIndex == INDEX_NONE; });
		if (!Existing)
		{
			Candidates.Add({Actor, Component, DistSq, Point});
		}
		else if (DistSq < Existing->DistSq)
		{
			*Existing = {Actor, Component, DistSq, Point};
		}
	}
	// Harvestable instances by their components' bounds: their bodies do not answer the overlap above (measured Oct 18 2026: a stick 2 cm from the
	// aim point, and only the terrain's collision came back), so a ground item flush with the terrain was never a candidate.
	const float RadiusSq = FMath::Square(TraceConfig.AimAssistRadius);
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* ISM = *It;
		if (!IsValid(ISM) || ISM->GetWorld() != World || ISM->GetInstanceCount() == 0 || ISM->Bounds.ComputeSquaredDistanceFromBoxToPoint(AimPoint) > RadiusSq
			|| !IsHarvestableInstanceComponent(ISM, World))
		{
			continue;
		}
		for (const int32 Index : ISM->GetInstancesOverlappingSphere(AimPoint, TraceConfig.AimAssistRadius, /*bSphereInWorldSpace=*/true))
		{
			FTransform Xf;
			if (!ISM->GetInstanceTransform(Index, Xf, /*bWorldSpace=*/true) || FVector::DistSquared(Xf.GetLocation(), AimPoint) > RadiusSq
				|| Candidates.ContainsByPredicate([ISM, Index](const FCandidate& C) { return C.Component == ISM && C.InstanceIndex == Index; }))
			{
				continue;
			}
			Candidates.Add({ISM->GetOwner(), ISM, (float)FVector::DistSquared(Xf.GetLocation(), AimPoint), Xf.GetLocation(), Index});
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });
	if (CVarInteractDebug.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor]   near the aim point %s (r %.0f): %d overlap(s), %d interactable"),
			*AimPoint.ToCompactString(), TraceConfig.AimAssistRadius, Overlaps.Num(), Candidates.Num());
	}

	// The server checks the same reach and line of sight (UMOInteractionSubsystem::ServerExecuteInteract): a target it would refuse is no
	// target here.
	const UMOInteractionSubsystem* Interaction = World->GetSubsystem<UMOInteractionSubsystem>();
	const float MaxReach = Interaction ? Interaction->MaximumInteractDistance : TraceConfig.TraceDistance;
	UMOViewpointUtils::FLineOfSightOptions Options;
	Options.TraceChannel = TraceConfig.TraceChannel.GetValue();
	Options.bAllowAttachedHits = true;
	Options.IgnoredActor = GetOwner();
	Options.SeeThrough = &UMOInteractorComponent::IsSeeThroughForInteraction;
	for (const FCandidate& Candidate : Candidates)
	{
		FVector BoundsOrigin;
		FVector BoundsExtent;
		bool bInReach = false;
		bool bInSight = false;
		if (Candidate.InstanceIndex != INDEX_NONE)
		{
			// An instance: reach to its pivot, and in sight by the rule the server checks (its PCG cell's actor bounds say nothing).
			bInReach = UMOInteractionSubsystem::IsWithinInteractReach(ViewLocation, FVector::ZeroVector, Candidate.Point, FVector(15.0f), MaxReach, 0.0f);
			bInSight = bInReach && IsInstanceInSight(World, ViewLocation, Cast<UInstancedStaticMeshComponent>(Candidate.Component), Candidate.InstanceIndex, GetOwner(),
				TraceConfig.TraceChannel.GetValue(), TraceConfig.AimAssistRadius);
		}
		else
		{
			Candidate.Actor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
			bInReach = UMOInteractionSubsystem::IsWithinInteractReach(ViewLocation, FVector::ZeroVector, BoundsOrigin, BoundsExtent, MaxReach, 0.0f);
			bInSight = bInReach && UMOViewpointUtils::HasLineOfSight(World, ViewLocation, Candidate.Actor, Options);
		}
		if (CVarInteractDebug.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor]   near the aim point %s: %s/%s %.0f cm from it, reach %s, sight %s"),
				*AimPoint.ToCompactString(), *Candidate.Actor->GetName(), *GetNameSafe(Candidate.Component), FMath::Sqrt(Candidate.DistSq),
				bInReach ? TEXT("ok") : TEXT("NO"), bInReach ? (bInSight ? TEXT("ok") : TEXT("NO")) : TEXT("-"));
		}
		if (!bInSight)
		{
			continue;
		}
		FHitResult Hit(Candidate.Actor, Candidate.Component, Candidate.Point, (ViewLocation - Candidate.Point).GetSafeNormal());
		Hit.bBlockingHit = true;
		Hit.TraceStart = ViewLocation;
		Hit.TraceEnd = Candidate.Point;
		Hit.Distance = FVector::Dist(ViewLocation, Candidate.Point);
		if (Candidate.InstanceIndex != INDEX_NONE)
		{
			Hit.Item = Candidate.InstanceIndex;
			if (EvaluateInteractionHit(Hit, OutTarget))   // the instance target, exactly as if the line had met it
			{
				return true;
			}
			continue;
		}
		OutTarget.Reset();
		OutTarget.TargetActor = Candidate.Actor;
		OutTarget.HitResult = Hit;
		return true;
	}
	return false;
}

bool UMOInteractorComponent::EvaluateInteractionHit(const FHitResult& HitResult, FMOInteractionTarget& OutTarget) const
{
	OutTarget.Reset();
	OutTarget.HitResult = HitResult;

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	OutTarget.TargetActor = HitActor;

	// Check if we hit an instanced static mesh component (HISM or ISM)
	UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(HitResult.GetComponent());
	UHierarchicalInstancedStaticMeshComponent* HISMComp = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent());

	// Debug: Log what we hit (commented out - fires constantly on mouse hover)
	// UPrimitiveComponent* HitComp = HitResult.GetComponent();
	// UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Hit: Actor='%s', Component='%s' (ISM:%d, HISM:%d, Item:%d)"),
	// 	*HitActor->GetName(),
	// 	HitComp ? *HitComp->GetName() : TEXT("null"),
	// 	IsValid(ISMComp) ? 1 : 0,
	// 	IsValid(HISMComp) ? 1 : 0,
	// 	HitResult.Item);

	if (IsValid(ISMComp) && HitResult.Item != INDEX_NONE)
	{
		// For HISM, check for explicit HISM interactable component first
		if (IsValid(HISMComp))
		{
			UMOHISMInteractableComponent* HISMInteractable = HitActor->FindComponentByClass<UMOHISMInteractableComponent>();
			if (IsValid(HISMInteractable) && HISMInteractable->HandlesHISMComponent(HISMComp))
			{
				OutTarget.bIsInstancedMeshTarget = true;
				OutTarget.bIsHISM = true;
				OutTarget.ISMComponent = ISMComp;
				OutTarget.HISMComponent = HISMComp;
				OutTarget.InstanceIndex = HitResult.Item;
				OutTarget.HISMInteractable = HISMInteractable;
				return true;
			}
		}

		if (IsHarvestableInstanceComponent(ISMComp, GetWorld()))
		{
			OutTarget.bIsInstancedMeshTarget = true;
			OutTarget.ISMComponent = ISMComp;
			OutTarget.InstanceIndex = HitResult.Item;

			if (IsValid(HISMComp))
			{
				OutTarget.bIsHISM = true;
				OutTarget.HISMComponent = HISMComp;
			}
			else
			{
				OutTarget.bIsHISM = false;
			}
			return true;
		}

		// World cover with no harvest data of its own (hazel, debris, a grass tuft with collision): it forages. Before, E on it did nothing
		// at all -- Common Hazel, the commonest bush around a woodland spawn, while the tutorial said to inspect a plant (Oct 15 2026).
		if (IsWorldCoverComponent(ISMComp))
		{
			OutTarget.bIsForageTarget = true;
			return true;
		}
		// DEBUG (MOInteractor, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOInteractor] ISM '%s' (%s) is neither a harvest source nor world cover"),
		// 	*ISMComp->GetName(), *GetNameSafe(ISMComp->GetStaticMesh()));
	}

	// Check for regular interactable component
	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (IsValid(InteractableComponent))
	{
		return true;
	}

	// No valid interaction target
	OutTarget.Reset();
	return false;
}

FText UMOInteractorComponent::GetCurrentInteractionPrompt() const
{
	if (!CurrentTarget.IsValid())
	{
		return FText::GetEmpty();
	}

	if (CurrentTarget.bIsForageTarget)
	{
		return NSLOCTEXT("MO", "InteractForage", "Forage");
	}

	if (CurrentTarget.bIsInstancedMeshTarget)
	{
		// Try HISM component first (if using component-based approach)
		if (UMOHISMInteractableComponent* HISMInteractable = CurrentTarget.HISMInteractable.Get())
		{
			return HISMInteractable->GetInteractionPrompt();
		}

		// Fall back to subsystem for both ISM and HISM
		UWorld* World = GetWorld();
		if (World)
		{
			UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
			if (PCGSubsystem && CurrentTarget.ISMComponent.IsValid())
			{
				return PCGSubsystem->GetInteractionPromptForMesh(CurrentTarget.ISMComponent->GetStaticMesh());
			}
		}
	}
	// Regular actor interactables don't have a standard prompt property
	// Return empty - can be extended to support per-actor prompts if needed

	return FText::GetEmpty();
}

bool UMOInteractorComponent::TryInteract()
{
	AActor* OwnerActor = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return false;
	}

	// Use the new unified target finding
	FMOInteractionTarget Target;
	const bool bFoundTarget = FindInteractionTarget(Target);

	// Update cached target
	CurrentTarget = Target;
	LastTracedActor = bFoundTarget ? Target.TargetActor.Get() : nullptr;

	// World cover forages: the player controller opens the forage menu at the crosshair (it owns the ground-forage path).
	if (!bFoundTarget || !Target.IsValid() || Target.bIsForageTarget)
	{
		return false;
	}

	// Handle instanced mesh targets (ISM/HISM)
	if (Target.bIsInstancedMeshTarget)
	{
		// A RESOURCE NODE (a bush, a tree, a rock: KeepOnHarvest / ResourceNode_* / MOResource_* tags) is not an item to pick up: the instant-pickup RPC below
		// found no item for it, logged a warning and did nothing, so pressing interact on a berry bush gave no response at all (playtest 2026-10-14 #21).
		// It opens the same harvest menu as the secondary interact (Gather Berries / Gather Twigs ...); item-like instances keep the instant pickup.
		if (const UInstancedStaticMeshComponent* TargetISM = Target.ISMComponent.Get())
		{
			bool bIsResourceNode = TargetISM->ComponentHasTag(TEXT("KeepOnHarvest"));
			for (const FName& Tag : TargetISM->ComponentTags)
			{
				const FString TagString = Tag.ToString();
				bIsResourceNode |= TagString.StartsWith(TEXT("ResourceNode_")) || TagString.StartsWith(TEXT("MOResource_"));
			}
			if (bIsResourceNode)
			{
				return TrySecondaryInteract();
			}
		}

		return RequestInteractWithInstance(Target.ISMComponent.Get(), Target.InstanceIndex);
	}

	// Regular actor interaction
	AActor* TargetActor = Target.TargetActor.Get();
	ServerRequestInteract(TargetActor);
	return true;
}

bool UMOInteractorComponent::RequestInteractWithInstance(const UInstancedStaticMeshComponent* ISM, int32 InstanceIndex)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return false;
	}
	// By mesh + world location: the host has its own copy of this instance, under a component name and index of its own.
	const FMOInstanceRef Instance = FMOInstanceRef::Make(ISM, InstanceIndex);
	if (!Instance.IsSet())
	{
		return false;
	}
	ServerRequestInstanceInteract(Instance);
	return true;
}

void UMOInteractorComponent::RequestInteractWithActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	// UI/Blueprint may call this on the owning client; route to the authoritative
	// server RPC exactly like TryInteract does, but for an already-resolved target.
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	// Route to the specific-item pickup path (distance-validated, no crosshair re-trace),
	// not ServerRequestInteract -- that anti-cheat re-traces and would reject an off-aim
	// item the UI explicitly selected (the H21 regression #159 caught by MO.Test.DropPickup).
	ServerPickUpWorldItem(TargetActor);
}

void UMOInteractorComponent::ServerPickUpWorldItem_Implementation(AActor* TargetActor)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->HasAuthority() || !IsValid(TargetActor))
	{
		return;
	}
	AController* Controller = OwnerPawn->GetController();
	if (!IsValid(Controller))
	{
		return;
	}

	// Distance validation (anti-cheat): the UI selected a specific item, so validate by
	// reach instead of an aim trace. Generous reach covers the nearby-items radius.
	const float MaxReachSq = FMath::Square(3000.0f);
	if (FVector::DistSquared(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation()) > MaxReachSq)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteract] ServerPickUpWorldItem: %s out of reach"), *TargetActor->GetName());
		return;
	}

	// Canonical authoritative pickup: preserves the world-item GUID into inventory and
	// hides/destroys the actor via replication (UMOItemComponent handles the authority).
	if (UMOItemComponent* ItemComp = TargetActor->FindComponentByClass<UMOItemComponent>())
	{
		const bool bOk = ItemComp->GiveToInteractorInventory(Controller);
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteract] ServerPickUpWorldItem: %s -> %s"),
			*TargetActor->GetName(), bOk ? TEXT("picked up") : TEXT("give failed (full?)"));
	}
}

void UMOInteractorComponent::ServerRequestInteract_Implementation(AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InteractorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!IsValid(InteractorController) || !IsValid(TargetActor))
	{
		return;
	}

	UMOInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UMOInteractionSubsystem>();
	if (!InteractionSubsystem)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInteractor] No InteractionSubsystem"));
		return;
	}

	InteractionSubsystem->ServerExecuteInteract(InteractorController, TargetActor);
}

bool UMOInteractorComponent::TrySecondaryInteract()
{
	AActor* OwnerActor = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return false;
	}

	// Use unified target finding (supports ISM/HISM)
	FMOInteractionTarget Target;
	const bool bFoundTarget = FindInteractionTarget(Target);

	// Update cached target
	CurrentTarget = Target;
	LastTracedActor = bFoundTarget ? Target.TargetActor.Get() : nullptr;

	// World cover forages: the player controller opens the forage menu at the crosshair (it owns the ground-forage path).
	if (!bFoundTarget || !Target.IsValid() || Target.bIsForageTarget)
	{
		return false;
	}

	// Handle ISM/HISM targets separately - they don't use actor-based interaction
	if (Target.bIsInstancedMeshTarget)
	{
		UInstancedStaticMeshComponent* ISMComp = Target.ISMComponent.Get();
		if (!IsValid(ISMComp) || !IsHarvestableInstanceComponent(ISMComp, GetWorld()))
		{
			return false;
		}

		// Get UIManager from player controller
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (!IsValid(PC))
		{
			return false;
		}

		UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>();
		if (!IsValid(UIManager))
		{
			return false;
		}

		// ShowKeepOnHarvestContextMenu works for any harvestable resource
		// (the name is historical - it handles all ISM harvest targets)
		UIManager->ShowKeepOnHarvestContextMenu(Target);
		return true;
	}

	// Regular actor secondary interaction (NOT for ISM targets)
	AActor* TargetActor = Target.TargetActor.Get();
	if (!IsValid(TargetActor))
	{
		return false;
	}

	ServerRequestSecondaryInteract(TargetActor);
	return true;
}

void UMOInteractorComponent::ServerRequestSecondaryInteract_Implementation(AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InteractorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!IsValid(InteractorController) || !IsValid(TargetActor))
	{
		return;
	}

	UMOInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UMOInteractionSubsystem>();
	if (InteractionSubsystem)
	{
		InteractionSubsystem->ServerExecuteSecondaryInteract(InteractorController, TargetActor);
	}
}

bool UMOInteractorComponent::ValidateInstanceHarvestRequest(UInstancedStaticMeshComponent* Component, int32 InstanceIndex, const TCHAR* RPCName)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(OwnerPawn) || !IsValid(Component) || !World)
	{
		return false;
	}

	FString Why;
	if (ServerCheckInstanceRequest(OwnerPawn, Component, InstanceIndex, Why) != EInstanceRequestCheck::Ok)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] %s rejected: %s"), RPCName, *Why);
		return false;
	}

	// Rate limit: real harvests arrive seconds apart (timed actions); spam
	// arrives per-frame. 0.1s spacing never touches legit play.
	const double Now = World->GetTimeSeconds();
	if (LastInstanceHarvestServerTime >= 0.0 && Now - LastInstanceHarvestServerTime < 0.1)
	{
		return false;
	}
	LastInstanceHarvestServerTime = Now;
	return true;
}

void UMOInteractorComponent::ServerRequestInstanceInteract_Implementation(const FMOInstanceRef& Instance)
{
	UWorld* World = GetWorld();
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!World || !IsValid(OwnerPawn))
	{
		return;
	}

	// The host's own copy of the instance the client aimed at. Not there: someone took it first, or the client sees a world the host does not.
	UInstancedStaticMeshComponent* ISM = nullptr;
	int32 InstanceIndex = INDEX_NONE;
	if (!Instance.Resolve(World, ISM, InstanceIndex))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestInstanceInteract: %s asked for %s -- no such instance here (taken already?)"),
			*OwnerPawn->GetName(), *Instance.ToString());
		return;
	}
	if (!ValidateInstanceHarvestRequest(ISM, InstanceIndex, TEXT("ServerRequestInstanceInteract")))
	{
		return;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] instance interact: %s -> %s #%d"), *Instance.ToString(), *ISM->GetName(), InstanceIndex);

	UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
	FName OutItemId;
	if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(ISM))
	{
		// First try: an explicit HISM interactable component on the instance's owner
		AActor* OwnerActor = HISM->GetOwner();
		UMOHISMInteractableComponent* HISMInteractable = OwnerActor ? OwnerActor->FindComponentByClass<UMOHISMInteractableComponent>() : nullptr;
		if (IsValid(HISMInteractable) && HISMInteractable->HandlesHISMComponent(HISM))
		{
			HISMInteractable->HarvestInstance(HISM, InstanceIndex, OwnerPawn);
			return;
		}
		// Second try: the PCG interaction subsystem's mesh-to-item mapping
		if (PCGSubsystem)
		{
			PCGSubsystem->HarvestHISMInstance(HISM, InstanceIndex, OwnerPawn, OutItemId);
		}
		return;
	}
	if (PCGSubsystem)
	{
		PCGSubsystem->HarvestISMInstance(ISM, InstanceIndex, OwnerPawn, OutItemId);
	}
}
