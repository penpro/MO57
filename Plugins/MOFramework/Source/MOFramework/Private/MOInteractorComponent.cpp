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

UMOInteractorComponent::UMOInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOInteractorComponent::BeginPlay()
{
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

	FHitResult HitResult;
	if (!TraceForHit(TraceStart, TraceEnd, HitResult))
	{
		return false;
	}

	OutTarget.HitResult = HitResult;

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] FindInteractionTarget: Hit but no valid actor"));
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

		// Check if this ISM/HISM is interactable via harvestable indicators:
		// 1. KeepOnHarvest tag (trees/bushes that persist after harvest)
		// 2. Harvestable tag (generic harvestable resources)
		// 3. MOResource_* tag (typed resources like Rock, Ore, Tree)
		// 4. PCG subsystem tag/mesh mapping
		const bool bHasKeepOnHarvestTag = ISMComp->ComponentHasTag(TEXT("KeepOnHarvest")) ||
			(HitActor && HitActor->ActorHasTag(TEXT("KeepOnHarvest")));
		const bool bHasHarvestableTag = ISMComp->ComponentHasTag(TEXT("Harvestable")) ||
			(HitActor && HitActor->ActorHasTag(TEXT("Harvestable")));

		// Check for MOResource_* tags (Rock, Ore, Tree, Bush, Plant)
		bool bHasResourceTypeTag = false;
		for (const FName& Tag : ISMComp->ComponentTags)
		{
			if (Tag.ToString().StartsWith(TEXT("MOResource_")))
			{
				bHasResourceTypeTag = true;
				break;
			}
		}

		UWorld* World = GetWorld();
		UMOPCGInteractionSubsystem* PCGSubsystem = World ? World->GetSubsystem<UMOPCGInteractionSubsystem>() : nullptr;
		const bool bHasTagMapping = PCGSubsystem && !PCGSubsystem->GetItemIdForComponentTags(ISMComp).IsNone();
		const bool bHasMeshMapping = PCGSubsystem && PCGSubsystem->IsMeshHarvestable(ISMComp->GetStaticMesh());

		const bool bIsHarvestable = bHasKeepOnHarvestTag || bHasHarvestableTag || bHasResourceTypeTag || bHasTagMapping || bHasMeshMapping;

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOInteractor] ISM Target Check - KeepOnHarvest:%d Harvestable:%d ResourceType:%d TagMap:%d MeshMap:%d = %s"),
			bHasKeepOnHarvestTag, bHasHarvestableTag, bHasResourceTypeTag, bHasTagMapping, bHasMeshMapping,
			bIsHarvestable ? TEXT("HARVESTABLE") : TEXT("not harvestable"));

		if (bIsHarvestable)
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
		else
		{
			// Log component tags for debugging
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ISM not harvestable. Tags on component:"));
			for (const FName& Tag : ISMComp->ComponentTags)
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor]   - %s"), *Tag.ToString());
			}
		}
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

	if (!bFoundTarget || !Target.IsValid())
	{
		return false;
	}

	// Handle instanced mesh targets (ISM/HISM)
	if (Target.bIsInstancedMeshTarget)
	{
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

	if (!bFoundTarget || !Target.IsValid())
	{
		return false;
	}

	// Handle ISM/HISM targets separately - they don't use actor-based interaction
	if (Target.bIsInstancedMeshTarget)
	{
		UInstancedStaticMeshComponent* ISMComp = Target.ISMComponent.Get();
		if (!IsValid(ISMComp))
		{
			return false;
		}

		// Check if this is a harvestable resource (any of these conditions):
		// 1. Has KeepOnHarvest tag (trees, bushes that persist after partial harvest)
		// 2. Has Harvestable tag (generic harvestable resources)
		// 3. Has MOResource_* tag (typed resources like Rock, Ore, Tree)
		// 4. Has PCG subsystem tag/mesh mapping
		AActor* ISMOwner = ISMComp->GetOwner();
		const bool bHasKeepOnHarvestTag = ISMComp->ComponentHasTag(TEXT("KeepOnHarvest")) ||
			(ISMOwner && ISMOwner->ActorHasTag(TEXT("KeepOnHarvest")));
		const bool bHasHarvestableTag = ISMComp->ComponentHasTag(TEXT("Harvestable")) ||
			(ISMOwner && ISMOwner->ActorHasTag(TEXT("Harvestable")));

		// Check for MOResource_* tags (Rock, Ore, Tree, Bush, Plant)
		bool bHasResourceTypeTag = false;
		for (const FName& Tag : ISMComp->ComponentTags)
		{
			if (Tag.ToString().StartsWith(TEXT("MOResource_")))
			{
				bHasResourceTypeTag = true;
				break;
			}
		}

		// Also check PCG subsystem mappings
		UWorld* World = GetWorld();
		UMOPCGInteractionSubsystem* PCGSubsystem = World ? World->GetSubsystem<UMOPCGInteractionSubsystem>() : nullptr;
		const bool bHasTagMapping = PCGSubsystem && !PCGSubsystem->GetItemIdForComponentTags(ISMComp).IsNone();
		const bool bHasMeshMapping = PCGSubsystem && PCGSubsystem->IsMeshHarvestable(ISMComp->GetStaticMesh());

		const bool bIsHarvestable = bHasKeepOnHarvestTag || bHasHarvestableTag ||
			bHasResourceTypeTag || bHasTagMapping || bHasMeshMapping;

		if (!bIsHarvestable)
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
