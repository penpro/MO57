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

	// Check if we hit an instanced static mesh component (HISM or ISM)
	UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(HitResult.GetComponent());
	UHierarchicalInstancedStaticMeshComponent* HISMComp = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent());

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
			}
			else
			{
				OutTarget.bIsHISM = false;
			}
			return true;
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

		// Check for KeepOnHarvest tag on component or owner actor
		const bool bHasKeepOnHarvestTag = ISMComp->ComponentHasTag(TEXT("KeepOnHarvest")) ||
			(ISMComp->GetOwner() && ISMComp->GetOwner()->ActorHasTag(TEXT("KeepOnHarvest")));

		if (!bHasKeepOnHarvestTag)
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

void UMOInteractorComponent::ServerRequestHISMInteract_Implementation(AActor* OwnerActor, UHierarchicalInstancedStaticMeshComponent* HISMComponent, int32 InstanceIndex)
{
	if (!IsValid(HISMComponent))
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
	if (!IsValid(ISMComponent))
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
