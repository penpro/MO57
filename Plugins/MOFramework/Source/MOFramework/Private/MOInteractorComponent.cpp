#include "MOInteractorComponent.h"
#include "MOFramework.h"
#include "MOViewpointUtils.h"

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
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] FindInteractTarget: No hit"));
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] FindInteractTarget: Hit actor '%s' has no InteractableComponent"), *HitActor->GetName());
		return false;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] FindInteractTarget: Found target '%s'"), *HitActor->GetName());
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
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOInteractor] FindInteractionTarget: No hit"));
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] FindInteractionTarget: Hit actor '%s', Component '%s', Item=%d"),
		*HitActor->GetName(),
		*GetNameSafe(HitResult.GetComponent()),
		HitResult.Item);

	// Check if we hit an instanced static mesh component (HISM or ISM)
	UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(HitResult.GetComponent());
	UHierarchicalInstancedStaticMeshComponent* HISMComp = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent());

	if (IsValid(ISMComp) && HitResult.Item != INDEX_NONE)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Hit ISM/HISM: IsHISM=%s, Instance=%d, Mesh=%s"),
			HISMComp ? TEXT("yes") : TEXT("no"),
			HitResult.Item,
			*GetNameSafe(ISMComp->GetStaticMesh()));

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

				UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Found HISM target (component): Actor=%s, Instance=%d, Item=%s"),
					*HitActor->GetName(), HitResult.Item, *HISMInteractable->GetItemDefinitionId().ToString());
				return true;
			}
		}

		// Check if this ISM/HISM is interactable via:
		// 1. KeepOnHarvest tag on component or owner actor
		// 2. PCG subsystem tag/mesh mapping
		const bool bHasKeepOnHarvestTag = ISMComp->ComponentHasTag(TEXT("KeepOnHarvest")) ||
			(HitActor && HitActor->ActorHasTag(TEXT("KeepOnHarvest")));

		UWorld* World = GetWorld();
		UMOPCGInteractionSubsystem* PCGSubsystem = World ? World->GetSubsystem<UMOPCGInteractionSubsystem>() : nullptr;
		const bool bHasTagMapping = PCGSubsystem && !PCGSubsystem->GetItemIdForComponentTags(ISMComp).IsNone();
		const bool bHasMeshMapping = PCGSubsystem && PCGSubsystem->IsMeshHarvestable(ISMComp->GetStaticMesh());

		if (bHasKeepOnHarvestTag || bHasTagMapping || bHasMeshMapping)
		{
			OutTarget.bIsInstancedMeshTarget = true;
			OutTarget.ISMComponent = ISMComp;
			OutTarget.InstanceIndex = HitResult.Item;

			if (IsValid(HISMComp))
			{
				OutTarget.bIsHISM = true;
				OutTarget.HISMComponent = HISMComp;
				UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Found HISM target: Actor=%s, Instance=%d, Mesh=%s, KeepOnHarvest=%s"),
					*HitActor->GetName(), HitResult.Item, *GetNameSafe(HISMComp->GetStaticMesh()),
					bHasKeepOnHarvestTag ? TEXT("yes") : TEXT("no"));
			}
			else
			{
				OutTarget.bIsHISM = false;
				UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Found ISM target: Actor=%s, Instance=%d, Mesh=%s, KeepOnHarvest=%s"),
					*HitActor->GetName(), HitResult.Item, *GetNameSafe(ISMComp->GetStaticMesh()),
					bHasKeepOnHarvestTag ? TEXT("yes") : TEXT("no"));
			}
			return true;
		}
		else
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ISM not interactable: %s (no KeepOnHarvest tag or PCG mapping)"),
				*GetNameSafe(ISMComp->GetStaticMesh()));
		}
	}
	else if (IsValid(ISMComp))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Hit ISM but no instance index (Item=%d)"), HitResult.Item);
	}

	// Check for regular interactable component
	UMOInteractableComponent* InteractableComponent = HitActor->FindComponentByClass<UMOInteractableComponent>();
	if (IsValid(InteractableComponent))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] Found actor target: %s"), *HitActor->GetName());
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
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TryInteract called"));

	AActor* OwnerActor = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TryInteract: Owner is not a valid pawn"));
		return false;
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TryInteract: Pawn is not locally controlled"));
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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TryInteract: No valid target found"));
		return false;
	}

	// Handle instanced mesh targets (ISM/HISM)
	if (Target.bIsInstancedMeshTarget)
	{
		AActor* OwnerOfISM = Target.TargetActor.Get();

		if (Target.bIsHISM)
		{
			UHierarchicalInstancedStaticMeshComponent* HISMComp = Target.HISMComponent.Get();
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TryInteract: Sending ServerRequestHISMInteract for instance %d"),
				Target.InstanceIndex);
			ServerRequestHISMInteract(OwnerOfISM, HISMComp, Target.InstanceIndex);
		}
		else
		{
			UInstancedStaticMeshComponent* ISMComp = Target.ISMComponent.Get();
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TryInteract: Sending ServerRequestISMInteract for instance %d"),
				Target.InstanceIndex);
			ServerRequestISMInteract(OwnerOfISM, ISMComp, Target.InstanceIndex);
		}
		return true;
	}

	// Regular actor interaction
	AActor* TargetActor = Target.TargetActor.Get();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TryInteract: Sending ServerRequestInteract for '%s'"), *TargetActor->GetName());
	ServerRequestInteract(TargetActor);
	return true;
}

void UMOInteractorComponent::ServerRequestInteract_Implementation(AActor* TargetActor)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestInteract for '%s'"), *GetNameSafe(TargetActor));

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestInteract: No world"));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* InteractorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!IsValid(InteractorController) || !IsValid(TargetActor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestInteract: Invalid controller or target"));
		return;
	}

	UMOInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UMOInteractionSubsystem>();
	if (!InteractionSubsystem)
	{
		UE_LOG(LogMOFramework, Error, TEXT("[MOInteractor] ServerRequestInteract: No InteractionSubsystem!"));
		return;
	}

	UE_LOG(LogMOFramework, Error, TEXT("[MOInteractor] ServerRequestInteract: About to call ServerExecuteInteract, Subsystem=%p"), InteractionSubsystem);
	const bool bResult = InteractionSubsystem->ServerExecuteInteract(InteractorController, TargetActor);
	UE_LOG(LogMOFramework, Error, TEXT("[MOInteractor] ServerRequestInteract: ServerExecuteInteract returned %s"), bResult ? TEXT("true") : TEXT("false"));
}

bool UMOInteractorComponent::TrySecondaryInteract()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract called"));

	AActor* OwnerActor = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!IsValid(OwnerPawn))
	{
		return false;
	}

	if (!OwnerPawn->IsLocallyControlled())
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
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract: No valid target found"));
		return false;
	}

	// Handle ISM/HISM targets separately - they don't use actor-based interaction
	if (Target.bIsInstancedMeshTarget)
	{
		UInstancedStaticMeshComponent* ISMComp = Target.ISMComponent.Get();
		if (!IsValid(ISMComp))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TrySecondaryInteract: ISM target but component is invalid"));
			return false;
		}

		// Check for KeepOnHarvest tag on component or owner actor
		const bool bHasKeepOnHarvestTag = ISMComp->ComponentHasTag(TEXT("KeepOnHarvest")) ||
			(ISMComp->GetOwner() && ISMComp->GetOwner()->ActorHasTag(TEXT("KeepOnHarvest")));

		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract: ISM target, KeepOnHarvest=%s, Owner=%s"),
			bHasKeepOnHarvestTag ? TEXT("yes") : TEXT("no"),
			*GetNameSafe(ISMComp->GetOwner()));

		if (!bHasKeepOnHarvestTag)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract: ISM has no KeepOnHarvest tag"));
			return false;
		}

		// Get UIManager from player controller
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (!IsValid(PC))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TrySecondaryInteract: No player controller"));
			return false;
		}

		UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>();
		if (!IsValid(UIManager))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] TrySecondaryInteract: No UIManager on controller"));
			return false;
		}

		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract: Showing KeepOnHarvest context menu"));
		UIManager->ShowKeepOnHarvestContextMenu(Target);
		return true;
	}

	// Regular actor secondary interaction (NOT for ISM targets)
	AActor* TargetActor = Target.TargetActor.Get();
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract: No valid actor target"));
		return false;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] TrySecondaryInteract: Sending ServerRequestSecondaryInteract for '%s'"), *TargetActor->GetName());
	ServerRequestSecondaryInteract(TargetActor);
	return true;
}

void UMOInteractorComponent::ServerRequestSecondaryInteract_Implementation(AActor* TargetActor)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ServerRequestSecondaryInteract for '%s'"), *GetNameSafe(TargetActor));

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
		UE_LOG(LogMOFramework, Error, TEXT("[MOInteractor] ServerRequestSecondaryInteract: No InteractionSubsystem!"));
		return;
	}

	InteractionSubsystem->ServerExecuteSecondaryInteract(InteractorController, TargetActor);
}

void UMOInteractorComponent::ServerRequestHISMInteract_Implementation(AActor* OwnerActor, UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ServerRequestHISMInteract: Actor=%s, Instance=%d"),
		*GetNameSafe(OwnerActor), InstanceIndex);

	if (!IsValid(HISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestHISMInteract: Invalid HISM component"));
		return;
	}

	// Get the harvester (the pawn that owns this interactor)
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestHISMInteract: Owner is not a pawn"));
		return;
	}

	// First try: Use explicit HISM interactable component if present
	if (IsValid(OwnerActor))
	{
		UMOHISMInteractableComponent* HISMInteractable = OwnerActor->FindComponentByClass<UMOHISMInteractableComponent>();
		if (IsValid(HISMInteractable) && HISMInteractable->HandlesHISMComponent(HISMComponent))
		{
			const bool bSuccess = HISMInteractable->HarvestInstance(HISMComponent, InstanceIndex, OwnerPawn);
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ServerRequestHISMInteract (component): Harvest %s"),
				bSuccess ? TEXT("succeeded") : TEXT("failed"));
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
			const bool bSuccess = PCGSubsystem->HarvestHISMInstance(HISMComponent, InstanceIndex, OwnerPawn, OutItemId);
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ServerRequestHISMInteract (subsystem): Harvest %s, Item=%s"),
				bSuccess ? TEXT("succeeded") : TEXT("failed"), *OutItemId.ToString());
			return;
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestHISMInteract: No handler found for HISM"));
}

void UMOInteractorComponent::ServerRequestISMInteract_Implementation(AActor* OwnerActor, UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ServerRequestISMInteract: Actor=%s, Instance=%d"),
		*GetNameSafe(OwnerActor), InstanceIndex);

	if (!IsValid(ISMComponent))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestISMInteract: Invalid ISM component"));
		return;
	}

	// Get the harvester (the pawn that owns this interactor)
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestISMInteract: Owner is not a pawn"));
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
			const bool bSuccess = PCGSubsystem->HarvestISMInstance(ISMComponent, InstanceIndex, OwnerPawn, OutItemId);
			UE_LOG(LogMOFramework, Log, TEXT("[MOInteractor] ServerRequestISMInteract (subsystem): Harvest %s, Item=%s"),
				bSuccess ? TEXT("succeeded") : TEXT("failed"), *OutItemId.ToString());
			return;
		}
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOInteractor] ServerRequestISMInteract: No handler found for ISM"));
}
