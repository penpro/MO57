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

namespace
{
	TAutoConsoleVariable<int32> CVarInteractDebug(TEXT("mo.Interact.Debug"), 0,
		TEXT("1 = log every pass of the interaction targeting (line, sweep, near the aim point: candidates, reach, line of sight). It runs every frame for the focus hint: switch it on around one press."));

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

	// Each interactable actor once, by its overlapping component nearest the aim point (0 inside the component's bounds).
	struct FCandidate { AActor* Actor; UPrimitiveComponent* Component; float DistSq; FVector Point; };
	TArray<FCandidate> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!IsValid(Actor) || !Component || !Actor->FindComponentByClass<UMOInteractableComponent>())
		{
			continue;
		}
		const FBox Box = Component->Bounds.GetBox();
		const FVector Point = Box.GetClosestPointTo(AimPoint);
		const float DistSq = FVector::DistSquared(Point, AimPoint);
		FCandidate* Existing = Candidates.FindByPredicate([Actor](const FCandidate& C) { return C.Actor == Actor; });
		if (!Existing)
		{
			Candidates.Add({Actor, Component, DistSq, Point});
		}
		else if (DistSq < Existing->DistSq)
		{
			*Existing = {Actor, Component, DistSq, Point};
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
		Candidate.Actor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
		const bool bInReach = UMOInteractionSubsystem::IsWithinInteractReach(ViewLocation, FVector::ZeroVector, BoundsOrigin, BoundsExtent, MaxReach, 0.0f);
		const bool bInSight = bInReach && UMOViewpointUtils::HasLineOfSight(World, ViewLocation, Candidate.Actor, Options);
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
		OutTarget.Reset();
		OutTarget.TargetActor = Candidate.Actor;
		OutTarget.HitResult = FHitResult(Candidate.Actor, Candidate.Component, Candidate.Point, (ViewLocation - Candidate.Point).GetSafeNormal());
		OutTarget.HitResult.bBlockingHit = true;
		OutTarget.HitResult.TraceStart = ViewLocation;
		OutTarget.HitResult.TraceEnd = Candidate.Point;
		OutTarget.HitResult.Distance = FVector::Dist(ViewLocation, Candidate.Point);
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
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOInteractor] ISM '%s' (%s) is neither a harvest source nor world cover"),
			*ISMComp->GetName(), *GetNameSafe(ISMComp->GetStaticMesh()));
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

		AActor* OwnerOfISM = Target.TargetActor.Get();

		if (Target.bIsHISM)
		{
			UHierarchicalInstancedStaticMeshComponent* HISMComp = Target.HISMComponent.Get();
			ServerRequestHISMInteract(OwnerOfISM, HISMComp, Target.InstanceIndex);
		}
		else
		{
			UInstancedStaticMeshComponent* ISMComp = Target.ISMComponent.Get();
			ServerRequestISMInteract(OwnerOfISM, ISMComp, Target.InstanceIndex);
		}
		return true;
	}

	// Regular actor interaction
	AActor* TargetActor = Target.TargetActor.Get();
	ServerRequestInteract(TargetActor);
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

	if (InstanceIndex < 0 || InstanceIndex >= Component->GetInstanceCount())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOInteractor] %s rejected: instance index %d out of range (count %d)"),
			RPCName, InstanceIndex, Component->GetInstanceCount());
		return false;
	}

	// Reach: same policy as the actor path (UMOInteractionSubsystem's
	// MaximumInteractDistance), doubled because instance origins can sit
	// meters from the traced surface (tree trunk vs pivot). Still kills the
	// harvest-anything-anywhere exploit by orders of magnitude.
	float MaxDistance = 500.0f;
	if (const UMOInteractionSubsystem* Interaction = World->GetSubsystem<UMOInteractionSubsystem>())
	{
		MaxDistance = Interaction->MaximumInteractDistance;
	}
	MaxDistance *= 2.0f;

	FTransform InstanceTransform;
	if (!Component->GetInstanceTransform(InstanceIndex, InstanceTransform, /*bWorldSpace=*/true))
	{
		return false;
	}
	const float DistSq = FVector::DistSquared(OwnerPawn->GetActorLocation(), InstanceTransform.GetLocation());
	if (DistSq > FMath::Square(MaxDistance))
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOInteractor] %s rejected: instance %.0fcm away (max %.0fcm)"),
			RPCName, FMath::Sqrt(DistSq), MaxDistance);
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

void UMOInteractorComponent::ServerRequestHISMInteract_Implementation(AActor* OwnerActor, UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex)
{
	if (!ValidateInstanceHarvestRequest(HISMComponent, InstanceIndex, TEXT("ServerRequestHISMInteract")))
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	// First try: Use explicit HISM interactable component if present
	if (IsValid(OwnerActor))
	{
		UMOHISMInteractableComponent* HISMInteractable = OwnerActor->FindComponentByClass<UMOHISMInteractableComponent>();
		if (IsValid(HISMInteractable) && HISMInteractable->HandlesHISMComponent(HISMComponent))
		{
			HISMInteractable->HarvestInstance(HISMComponent, InstanceIndex, OwnerPawn);
			return;
		}
	}

	// Second try: Use PCG interaction subsystem for automatic mesh-to-item mapping
	UWorld* World = GetWorld();
	if (World)
	{
		UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
		if (PCGSubsystem)
		{
			FName OutItemId;
			PCGSubsystem->HarvestHISMInstance(HISMComponent, InstanceIndex, OwnerPawn, OutItemId);
		}
	}
}

void UMOInteractorComponent::ServerRequestISMInteract_Implementation(AActor* OwnerActor, UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex)
{
	if (!ValidateInstanceHarvestRequest(ISMComponent, InstanceIndex, TEXT("ServerRequestISMInteract")))
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	// Use PCG interaction subsystem for automatic mesh-to-item mapping
	UWorld* World = GetWorld();
	if (World)
	{
		UMOPCGInteractionSubsystem* PCGSubsystem = World->GetSubsystem<UMOPCGInteractionSubsystem>();
		if (PCGSubsystem)
		{
			FName OutItemId;
			PCGSubsystem->HarvestISMInstance(ISMComponent, InstanceIndex, OwnerPawn, OutItemId);
		}
	}
}
