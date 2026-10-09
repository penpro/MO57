/**
 * MOWorldSeedSubsystem.cpp - turns a world seed into voxel terrain, identically on host and client.
 * See the header for the flow. The apply bodies below were moved verbatim from AMOGameMode (re-parameterised
 * so they need neither a GameMode nor `this`), which is what lets a client run them.
 */

#include "MOWorldSeedSubsystem.h"
#include "MOFramework.h"
#include "MOGameState.h"
#include "MOVoxelReadinessSubsystem.h"
#include "MOHarvestDebugSubsystem.h"
#include "VoxelWorld.h"
#include "VoxelStampComponent.h"
#include "VoxelExposedSeed.h"
#include "Graphs/VoxelHeightGraph.h"
#include "VoxelGraph.h"
#include "VoxelParameter.h"
#include "VoxelParameterOverridesOwner.h"
#include "VoxelPinValue.h"
#include "Graphs/VoxelHeightGraphStamp.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

namespace
{
	/** A client that has not heard the host's seed by now is told so loudly instead of silently generating nothing. */
	constexpr float SeedArrivalWarningSeconds = 15.0f;

	/**
	 * TEST ONLY: makes the host skip the replication step, reproducing the original co-op bug (client terrain from its own
	 * default seed). `ue.py nettest game --withhold-seed` sets it to prove the harness's terrain check actually catches
	 * that bug instead of passing vacuously.
	 */
	TAutoConsoleVariable<bool> CVarWithholdWorldSeed(
		TEXT("MO.WorldSeed.Withhold"), false,
		TEXT("TEST ONLY: the host does not publish its world seed to clients (reproduces the client-terrain mismatch)."),
		ECVF_Cheat);

	/**
	 * TEST ONLY: the host publishes (its seed + this) instead, so the client generates DIFFERENT terrain from a real,
	 * valid seed. `ue.py nettest game --skew-seed` uses it to prove the terrain-height comparison fails when the
	 * terrain differs (withholding the seed leaves the client with no terrain, so nothing is compared at all).
	 */
	TAutoConsoleVariable<int32> CVarSkewWorldSeed(
		TEXT("MO.WorldSeed.Skew"), 0,
		TEXT("TEST ONLY: the host publishes (world seed + this value) to clients, so their terrain differs from the host's."),
		ECVF_Cheat);
}

UMOWorldSeedSubsystem* UMOWorldSeedSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWorldSeedSubsystem>() : nullptr;
}

void UMOWorldSeedSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SeedTimeoutHandle);
	}
	Super::Deinitialize();
}

// ============================================================================
// SHARED MECHANICS (moved from AMOGameMode)
// ============================================================================

FString UMOWorldSeedSubsystem::IntSeedToVoxelSeedString(int32 Seed)
{
	// Replicate the algorithm from FVoxelExposedSeed::Randomize()
	// Generates an 8-character uppercase string (A-Z) from the seed
	const FRandomStream Stream(Seed);

	FString Result;
	Result.Reserve(8);
	for (int32 Index = 0; Index < 8; Index++)
	{
		Result += TCHAR(Stream.RandRange(TEXT('A'), TEXT('Z')));
	}

	return Result;
}

int32 UMOWorldSeedSubsystem::ApplySeedToStamps(UWorld* World, int32 WorldSeed, FName VoxelSeedParameterName, const UObject* LogContext)
{
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSeed] ApplySeedToStamps: No world available"));
		return 0;
	}

	// Convert integer seed to voxel seed string format
	const FString VoxelSeedString = IntSeedToVoxelSeedString(WorldSeed);

	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Applying voxel seed: %d (%s)"), WorldSeed, *VoxelSeedString);

	int32 StampsUpdated = 0;
	int32 TotalStampsFound = 0;

	// Find all VoxelStampComponent instances in the world
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor)
		{
			continue;
		}

		// Get all stamp components on this actor
		TArray<UVoxelStampComponent*> StampComponents;
		Actor->GetComponents<UVoxelStampComponent>(StampComponents);

		for (UVoxelStampComponent* StampComp : StampComponents)
		{
			if (!StampComp)
			{
				continue;
			}

			TotalStampsFound++;

			// Get the current stamp, modify its seed
			FVoxelStampRef StampRef = StampComp->GetStamp();
			if (StampRef.IsValid())
			{
				// Log the old seed before changing
				const FString OldSeed = StampRef->StampSeed.Seed;

				// 1) Set the StampSeed (placement-RNG field).
				StampRef->StampSeed.Seed = VoxelSeedString;
				StampsUpdated++;

				UE_LOG(LogMOFramework, Verbose, TEXT("[MOWorldSeed] Stamp in '%s': StampSeed '%s' -> '%s'"),
					*Actor->GetName(), *OldSeed, *VoxelSeedString);

				// 2) CRITICAL FIX (2026-05): If this stamp is a HeightGraphStamp
				// (the level's world-gen stamp using VHG_Flat), it has its OWN
				// "Seed" parameter override that drives terrain generation —
				// SEPARATE from the StampSeed above. The graph asset's own
				// Seed override is IGNORED in favor of the stamp's override.
				// Per the user's editor screenshot, the world-gen stamp has
				// a "Seed" parameter set inside the stamp itself (not on the
				// underlying graph asset). Need to call SetParameter on the
				// stamp via its IVoxelParameterOverridesOwner interface.
				if (StampRef.IsA<FVoxelHeightGraphStamp>())
				{
					FVoxelHeightGraphStamp* HGStamp = StampRef.As<FVoxelHeightGraphStamp>();
					if (HGStamp)
					{
						IVoxelParameterOverridesOwner* StampOwner = static_cast<IVoxelParameterOverridesOwner*>(HGStamp);
						if (StampOwner->HasParameter(VoxelSeedParameterName))
						{
							FVoxelExposedSeed StampSeedValue;
							StampSeedValue.Seed = VoxelSeedString;
							FString StampError;
							if (StampOwner->SetParameter(VoxelSeedParameterName, FVoxelPinValue::Make(StampSeedValue), &StampError))
							{
								MOHARVEST_LOG(LogContext, "Seed",
									"  HGStamp '%s': Set 'Seed' param='%s' (stamp's overrides now=%d) — THIS is the world-gen seed",
									*Actor->GetName(), *VoxelSeedString,
									StampOwner->GetParameterOverrides().GuidToValueOverride.Num());
							}
							else
							{
								MOHARVEST_LOG(LogContext, "Seed",
									"  HGStamp '%s': FAILED to set Seed: %s",
									*Actor->GetName(), *StampError);
							}
						}
						else
						{
							MOHARVEST_LOG(LogContext, "Seed",
								"  HGStamp '%s': no 'Seed' parameter on this stamp",
								*Actor->GetName());
						}

						// Tell the runtime the stamp changed so it re-runs.
						StampRef.Update();
					}
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOWorldSeed] Stamp in '%s': no stamp data"),
					*Actor->GetName());
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Voxel seed applied to %d/%d stamps"), StampsUpdated, TotalStampsFound);

	return StampsUpdated;
}

bool UMOWorldSeedSubsystem::ApplySeedToHeightGraphs(int32 WorldSeed, FName VoxelSeedParameterName, const UObject* LogContext)
{
	// Create the seed value in Voxel's expected format
	FVoxelExposedSeed SeedValue;
	SeedValue.Seed = IntSeedToVoxelSeedString(WorldSeed);

	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Attempting to set seed parameter '%s' = '%s' on height graphs"),
		*VoxelSeedParameterName.ToString(), *SeedValue.Seed);

	// PACKAGED-BUILD FIX (2026-05): In packaged builds, UVoxelHeightGraph
	// assets are loaded LAZILY — the height graph referenced by the level's
	// VoxelWorld actor may not be in memory yet when this runs (the level
	// has spawned the actor but the actor's CreateRuntime() hasn't pulled
	// in the graph reference yet). TObjectIterator below only sees
	// in-memory objects, so without an explicit pre-load it returns 0
	// graphs in packaged → seed silently doesn't apply → terrain
	// regenerates with the default seed baked into the cooked graph →
	// saved voxel sculpt data lands at world positions that no longer match
	// the heightmap (looks like a pit).
	//
	// Force-load every cooked UVoxelHeightGraph asset via Asset Registry
	// before iterating. The user's project has only one or two graphs, so
	// the cost is negligible and the iteration below is now guaranteed to
	// see them.
	int32 PreloadCount = 0;
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> GraphAssets;
		AssetRegistry.GetAssetsByClass(UVoxelHeightGraph::StaticClass()->GetClassPathName(), GraphAssets, /*bSearchSubClasses=*/true);
		MOHARVEST_LOG(LogContext, "Seed", "ApplySeedToHeightGraphParameter: AssetRegistry returned %d UVoxelHeightGraph assets", GraphAssets.Num());
		for (const FAssetData& AD : GraphAssets)
		{
			// .GetAsset() forces synchronous load if not already in memory
			UObject* Loaded = AD.GetAsset();
			UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Pre-loaded height graph for seed application: %s (%s)"),
				*AD.AssetName.ToString(), Loaded ? TEXT("ok") : TEXT("FAILED"));
			MOHARVEST_LOG(LogContext, "Seed", "Pre-load attempt: %s -> %s", *AD.AssetName.ToString(), Loaded ? TEXT("OK") : TEXT("NULL"));
			if (Loaded) ++PreloadCount;
		}
	}

	int32 GraphsUpdated = 0;
	int32 GraphsChecked = 0;

	// Iterate through all loaded UVoxelHeightGraph assets and set the seed parameter
	// UVoxelGraph (parent of UVoxelHeightGraph) implements IVoxelParameterOverridesObjectOwner
	// which provides the SetParameter method
	for (TObjectIterator<UVoxelHeightGraph> It; It; ++It)
	{
		UVoxelHeightGraph* Graph = *It;
		if (!Graph)
		{
			continue;
		}

		// Skip transient/template objects
		if (Graph->HasAnyFlags(RF_Transient | RF_ClassDefaultObject))
		{
			continue;
		}

		GraphsChecked++;

		// Check if this graph has a parameter with the expected name
		if (!Graph->HasParameter(VoxelSeedParameterName))
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOWorldSeed] Graph '%s' has no parameter named '%s'"),
				*Graph->GetName(), *VoxelSeedParameterName.ToString());
			continue;
		}

		// Set the parameter value
		// UVoxelGraph implements IVoxelParameterOverridesObjectOwner which provides SetParameter
		FString Error;
		const FVoxelPinValue PinValue = FVoxelPinValue::Make(SeedValue);

		if (Graph->SetParameter(VoxelSeedParameterName, PinValue, &Error))
		{
			GraphsUpdated++;
			UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Set seed='%s' on HeightGraph '%s'"),
				*SeedValue.Seed, *Graph->GetName());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSeed] Failed to set seed on '%s': %s"),
				*Graph->GetName(), *Error);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Seed parameter set on %d/%d height graphs"),
		GraphsUpdated, GraphsChecked);
	MOHARVEST_LOG(LogContext, "Seed",
		"ApplySeedToHeightGraphParameter result: seedString='%s' preloaded=%d graphsChecked=%d graphsUpdated=%d",
		*SeedValue.Seed, PreloadCount, GraphsChecked, GraphsUpdated);

	// DIAGNOSTIC: dump the FULL parameter list AND the current override map
	// on every UVoxelGraph. Same seed string applied in both new-game and
	// load produced different terrain. Now logging the OVERRIDE MAP (which
	// is what the runtime actually reads) — if it differs between sessions
	// despite identical SetParameter calls, that proves the divergence is
	// in another override owner (a stamp, a scatter actor, etc) that we're
	// not seeing.
	for (TObjectIterator<UVoxelGraph> GraphIt; GraphIt; ++GraphIt)
	{
		UVoxelGraph* G = *GraphIt;
		if (!G) continue;
		if (G->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)) continue;
		const int32 ParamCount = G->NumParameters();
		const FVoxelParameterOverrides& Overrides = G->GetParameterOverrides();
		MOHARVEST_LOG(LogContext, "Seed", "ParamDump graph '%s' (class=%s, params=%d, overrides=%d):",
			*G->GetName(), *G->GetClass()->GetName(), ParamCount, Overrides.GuidToValueOverride.Num());
		G->ForeachParameter([LogContext, G](const FGuid& Guid, const FVoxelParameter& Param)
		{
			MOHARVEST_LOG(LogContext, "Seed", "  '%s' name='%s' type='%s'",
				*G->GetName(), *Param.Name.ToString(),
				*Param.Type.ToString());
		});
		for (const auto& OPair : Overrides.GuidToValueOverride)
		{
			MOHARVEST_LOG(LogContext, "Seed",
				"  override guid=%s enable=%d valueType='%s'",
				*OPair.Key.ToString(), OPair.Value.bEnable ? 1 : 0,
				*OPair.Value.Value.GetType().ToString());
		}
	}

	// Also enumerate all UObjects implementing IVoxelParameterOverridesObjectOwner
	// — these are stamp components, scatter actors, etc that have their OWN
	// override maps that take precedence over the graph asset's defaults.
	// If the terrain bug is from one of these, we'll see it here.
	int32 OwnerCount = 0;
	for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
	{
		UObject* Obj = *ObjIt;
		if (!Obj) continue;
		if (Obj->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)) continue;
		if (!Obj->Implements<UVoxelParameterOverridesObjectOwner>()) continue;

		IVoxelParameterOverridesObjectOwner* OwnerObj = Cast<IVoxelParameterOverridesObjectOwner>(Obj);
		if (!OwnerObj) continue;
		IVoxelParameterOverridesOwner* ParamOwner = static_cast<IVoxelParameterOverridesOwner*>(OwnerObj);
		++OwnerCount;
		const UVoxelGraph* OwnerGraph = ParamOwner->GetGraph();
		const FVoxelParameterOverrides& OwnerOverrides = ParamOwner->GetParameterOverrides();
		MOHARVEST_LOG(LogContext, "Seed",
			"ParamOwner #%d: obj='%s' class='%s' graph='%s' overrides=%d",
			OwnerCount, *Obj->GetName(), *Obj->GetClass()->GetName(),
			OwnerGraph ? *OwnerGraph->GetName() : TEXT("<null>"),
			OwnerOverrides.GuidToValueOverride.Num());

		// If this owner has a "Seed" parameter, apply our seed to it too —
		// the runtime may use this owner's override chain instead of the
		// graph asset's own defaults.
		if (ParamOwner->HasParameter(VoxelSeedParameterName))
		{
			FString OwnerError;
			if (ParamOwner->SetParameter(VoxelSeedParameterName, FVoxelPinValue::Make(SeedValue), &OwnerError))
			{
				MOHARVEST_LOG(LogContext, "Seed",
					"  -> applied Seed='%s' to owner '%s' (its overrides now=%d)",
					*SeedValue.Seed, *Obj->GetName(),
					ParamOwner->GetParameterOverrides().GuidToValueOverride.Num());
			}
			else
			{
				MOHARVEST_LOG(LogContext, "Seed",
					"  -> FAILED to apply Seed on owner '%s': %s",
					*Obj->GetName(), *OwnerError);
			}
		}
	}
	MOHARVEST_LOG(LogContext, "Seed", "Total IVoxelParameterOverridesObjectOwner instances found: %d", OwnerCount);

	return GraphsUpdated > 0;
}

bool UMOWorldSeedSubsystem::RegenerateVoxelWorld(UWorld* World, int32 WorldSeed, FName VoxelSeedParameterName, const UObject* LogContext)
{
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSeed] RegenerateVoxelWorld: No world available"));
		return false;
	}

	// Apply seed to all stamp components (for runtime stamps)
	const int32 StampsUpdated = ApplySeedToStamps(World, WorldSeed, VoxelSeedParameterName, LogContext);

	// Apply seed to height graph parameters (for base terrain generation)
	const bool bGraphParameterSet = ApplySeedToHeightGraphs(WorldSeed, VoxelSeedParameterName, LogContext);

	// Find and initialize the voxel world
	AVoxelWorld* VoxelWorld = nullptr;
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		VoxelWorld = *It;
		break;
	}

	if (!VoxelWorld)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSeed] RegenerateVoxelWorld: No VoxelWorld found in level"));
		return false;
	}

	// Check if runtime is already created (mid-game load scenario, or a client whose level began play first)
	if (VoxelWorld->IsRuntimeCreated())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] VoxelWorld runtime already exists - destroying and recreating with seed %d"), WorldSeed);
		VoxelWorld->DestroyRuntime();

		// Re-apply seed parameters after destroying runtime (they may have been cleared)
		ApplySeedToStamps(World, WorldSeed, VoxelSeedParameterName, LogContext);
		ApplySeedToHeightGraphs(WorldSeed, VoxelSeedParameterName, LogContext);
	}

	// Create the runtime to start generation with the new seed
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldSeed] Creating VoxelWorld runtime with seed %d (stamps=%d, graphParam=%s)"),
		WorldSeed, StampsUpdated, bGraphParameterSet ? TEXT("SET") : TEXT("NOT SET"));
	MOHARVEST_LOG(LogContext, "Seed",
		"CreateRuntime: seed=%d stamps=%d graphParamSet=%d",
		WorldSeed, StampsUpdated, bGraphParameterSet ? 1 : 0);
	VoxelWorld->CreateRuntime();
	return true;
}

// ============================================================================
// HOST: PUBLISH
// ============================================================================

void UMOWorldSeedSubsystem::PublishWorldSeed(int32 WorldSeed, FName VoxelSeedParameterName)
{
	UWorld* World = GetWorld();
	AMOGameState* State = World ? World->GetGameState<AMOGameState>() : nullptr;
	if (!State || !State->HasAuthority())
	{
		// Loud on purpose: without this the clients silently generate default-seed terrain (the original bug).
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOWorldSeed] Cannot publish world seed %d: GameState is %s. Clients will NOT receive the seed and their terrain will not match "
				"the host's. AMOGameMode must use AMOGameState (a Blueprint GameStateClass override breaks this)."),
			WorldSeed, (World && World->GetGameState()) ? *World->GetGameState()->GetClass()->GetName() : TEXT("missing"));
		return;
	}

	bHasActiveSeed = true;
	ActiveSeed = WorldSeed;
	ActiveParameterName = VoxelSeedParameterName;

	if (CVarWithholdWorldSeed.GetValueOnGameThread())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSeed] TEST: MO.WorldSeed.Withhold is set -- world seed %d NOT published"), WorldSeed);
		return;
	}

	const int32 Skew = CVarSkewWorldSeed.GetValueOnGameThread();
	if (Skew != 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldSeed] TEST: MO.WorldSeed.Skew=%d -- publishing seed %d instead of %d"),
			Skew, WorldSeed + Skew, WorldSeed);
	}

	FMOWorldSeedInfo Info;
	Info.bPublished = true;
	Info.Seed = WorldSeed + Skew;
	Info.ParameterName = VoxelSeedParameterName;
	State->SetWorldSeedInfo(Info);

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWorldSeed] host published world seed %d (parameter '%s'); clients generate terrain from it"),
		Info.Seed, *VoxelSeedParameterName.ToString());
}

// ============================================================================
// CLIENT: RECEIVE
// ============================================================================

void UMOWorldSeedSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Only a CLIENT defers: the host/standalone path is AMOGameMode (it owns the seed decision and creates the runtime).
	// This hook runs before any actor's BeginPlay, so the client never generates default-seed terrain just to discard it.
	if (InWorld.GetNetMode() != NM_Client)
	{
		return;
	}

	int32 Found = 0;
	int32 Deferred = 0;
	for (TActorIterator<AVoxelWorld> It(&InWorld); It; ++It)
	{
		++Found;
		if (It->bCreateRuntimeOnBeginPlay)
		{
			It->bCreateRuntimeOnBeginPlay = false;
			++Deferred;
		}
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWorldSeed] client: %d AVoxelWorld(s) in the level; voxel runtime creation deferred on %d "
			"(the others already have bCreateRuntimeOnBeginPlay off) until the host's world seed arrives"), Found, Deferred);

	InWorld.GetTimerManager().SetTimer(SeedTimeoutHandle, this, &UMOWorldSeedSubsystem::WarnSeedNeverArrived,
		SeedArrivalWarningSeconds, /*bLoop=*/false);
}

void UMOWorldSeedSubsystem::HandleReplicatedWorldSeed(const FMOWorldSeedInfo& Info)
{
	if (!Info.bPublished)
	{
		return;
	}
	if (bHasActiveSeed && ActiveSeed == Info.Seed && ActiveParameterName == Info.ParameterName)
	{
		return; // already generating from exactly this
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(SeedTimeoutHandle);

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWorldSeed] client received the host's world seed %d (parameter '%s'); generating terrain from it"),
		Info.Seed, *Info.ParameterName.ToString());

	const bool bStarted = RegenerateVoxelWorld(World, Info.Seed, Info.ParameterName, this);

	bHasActiveSeed = true;
	ActiveSeed = Info.Seed;
	ActiveParameterName = Info.ParameterName;

	// Give the client the same readiness signal the host has (OnVoxelReady / IsReady).
	if (UMOVoxelReadinessSubsystem* Readiness = UMOVoxelReadinessSubsystem::Get(World))
	{
		Readiness->BeginPolling();
	}

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOWorldSeed] client applied world seed %d: voxel runtime %s"),
		Info.Seed, bStarted ? TEXT("created") : TEXT("NOT created (no AVoxelWorld in this level)"));
}

void UMOWorldSeedSubsystem::WarnSeedNeverArrived()
{
	if (!bHasActiveSeed)
	{
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOWorldSeed] client: no world seed from the host after %.0f s -- voxel terrain was NOT generated. "
				"Is the host running a build that publishes AMOGameState's world seed?"),
			SeedArrivalWarningSeconds);
	}
}
