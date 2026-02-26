// Copyright MO57. All Rights Reserved.

#include "MOHISMCullingSubsystem.h"
#include "PCGComponent.h"
#include "PCGCommon.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMOHISMCulling, Log, All);
DEFINE_LOG_CATEGORY(LogMOHISMCulling);

void UMOHISMCullingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TimeSinceLastRefresh = 0.0f;
	bHasRefreshedOnce = false;

	UE_LOG(LogMOHISMCulling, Log, TEXT("MOHISMCullingSubsystem initialized (Enabled: %s)"),
		bEnabled ? TEXT("Yes") : TEXT("No"));
}

void UMOHISMCullingSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

TStatId UMOHISMCullingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOHISMCullingSubsystem, STATGROUP_Tickables);
}

void UMOHISMCullingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_EDITOR
	TimeSinceLastRefresh += DeltaTime;

	// Initial refresh after 5 seconds
	const float FirstRefreshDelay = 5.0f;

	if (!bHasRefreshedOnce && TimeSinceLastRefresh >= FirstRefreshDelay)
	{
		UE_LOG(LogMOHISMCulling, Warning, TEXT("MOHISMCullingSubsystem: Performing initial refresh"));
		RefreshAllHISMComponents();
		bHasRefreshedOnce = true;
		TimeSinceLastRefresh = 0.0f;
	}
	else if (bHasRefreshedOnce && TimeSinceLastRefresh >= RefreshInterval)
	{
		UE_LOG(LogMOHISMCulling, Log, TEXT("MOHISMCullingSubsystem: Performing periodic refresh"));
		RefreshAllHISMComponents();
		TimeSinceLastRefresh = 0.0f;
	}
#endif
}

bool UMOHISMCullingSubsystem::ShouldRefreshPCGComponent(UPCGComponent* PCGComp) const
{
	if (!PCGComp)
	{
		return false;
	}

	AActor* Owner = PCGComp->GetOwner();
	if (!Owner)
	{
		return false;
	}

	// Check if the owner has the required tag
	return Owner->ActorHasTag(PCGActorTag);
}

void UMOHISMCullingSubsystem::RefreshAllHISMComponents()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 TotalPCGComponents = 0;
	int32 TaggedCount = 0;
	int32 RefreshedCount = 0;

	// Find all PCG components with the required tag and trigger refresh
	for (TObjectIterator<UPCGComponent> It; It; ++It)
	{
		UPCGComponent* PCGComp = *It;
		if (!PCGComp || PCGComp->GetWorld() != World)
		{
			continue;
		}

		TotalPCGComponents++;
		AActor* Owner = PCGComp->GetOwner();
		const FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");
		const bool bHasTag = Owner && Owner->ActorHasTag(PCGActorTag);

		if (bHasTag)
		{
			TaggedCount++;
			UE_LOG(LogMOHISMCulling, Warning, TEXT("  FOUND tagged actor: %s (Generated=%s)"),
				*OwnerName, PCGComp->bGenerated ? TEXT("Yes") : TEXT("No"));
		}

		if (!PCGComp->bGenerated)
		{
			continue;
		}

		if (!bHasTag)
		{
			continue;
		}

		// Use the same refresh call the editor Refresh button uses
		PCGComp->Refresh(EPCGChangeType::Structural | EPCGChangeType::GenerationGrid, /*bCancelExistingRefresh=*/true);
		RefreshedCount++;

		UE_LOG(LogMOHISMCulling, Log, TEXT("  -> REFRESHING PCG: %s"), *OwnerName);
	}

	UE_LOG(LogMOHISMCulling, Log, TEXT("Scan complete: %d total PCG components, %d with tag '%s', %d refreshed"),
		TotalPCGComponents, TaggedCount, *PCGActorTag.ToString(), RefreshedCount);
#else
	UE_LOG(LogMOHISMCulling, Warning, TEXT("HISM culling refresh not available in non-editor builds"));
#endif
}
