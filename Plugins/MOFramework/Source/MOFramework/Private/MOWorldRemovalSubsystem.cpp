#include "MOWorldRemovalSubsystem.h"
#include "MOFramework.h"
#include "MOGameState.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

// ============================================================================
// LEDGER (value type)
// ============================================================================

FMORemovedInstance FMOWorldRemovalLedger::MakeEntry(const FVector& WorldLocation, FName MeshName)
{
	FMORemovedInstance E;
	E.CellX = FMath::RoundToInt32(WorldLocation.X / CellCm);
	E.CellY = FMath::RoundToInt32(WorldLocation.Y / CellCm);
	E.CellZ = FMath::RoundToInt32(WorldLocation.Z / CellZCm);
	E.Mesh = MeshName;
	return E;
}

bool FMOWorldRemovalLedger::Add(const FMORemovedInstance& Entry)
{
	TArray<int32, TInlineAllocator<2>>& Zs = Keys.FindOrAdd({ Entry.CellX, Entry.CellY, Entry.Mesh });
	if (Zs.Contains(Entry.CellZ))
	{
		return false;
	}
	Zs.Add(Entry.CellZ);
	++NumEntries;
	return true;
}

bool FMOWorldRemovalLedger::IsRemoved(const FVector& WorldLocation, FName MeshName) const
{
	if (Keys.Num() == 0)
	{
		return false;
	}
	const int32 X = FMath::RoundToInt32(WorldLocation.X / CellCm);
	const int32 Y = FMath::RoundToInt32(WorldLocation.Y / CellCm);
	const int32 Z = FMath::RoundToInt32(WorldLocation.Z / CellZCm);
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			if (const TArray<int32, TInlineAllocator<2>>* Zs = Keys.Find({ X + DX, Y + DY, MeshName }))
			{
				for (const int32 CellZ : *Zs)
				{
					if (CellZ == FMORemovedInstance::AnyZ || FMath::Abs(CellZ - Z) <= 1)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void FMORemovedInstance::PostReplicatedAdd(const FMORemovedInstanceArray& InArraySerializer)
{
	if (AMOGameState* GameState = InArraySerializer.Owner.Get())
	{
		if (UMOWorldRemovalSubsystem* Removal = UMOWorldRemovalSubsystem::Get(GameState))
		{
			Removal->HandleReplicatedRemoval(*this);
		}
	}
}

// ============================================================================
// SUBSYSTEM
// ============================================================================

UMOWorldRemovalSubsystem* UMOWorldRemovalSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOWorldRemovalSubsystem>() : nullptr;
}

bool UMOWorldRemovalSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UMOWorldRemovalSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}
}

void UMOWorldRemovalSubsystem::Deinitialize()
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
	}
	Ledger.Reset();
	Entries.Reset();
	Super::Deinitialize();
}

void UMOWorldRemovalSubsystem::RecordRemoval(const FVector& WorldLocation, const UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	const FMORemovedInstance Entry = FMOWorldRemovalLedger::MakeEntry(WorldLocation, Mesh->GetFName());
	if (!Ledger.Add(Entry))
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		AddAuthoritative(Entry);
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldRemoval] recorded %s at (%.0f, %.0f) -- %d removed"), *Mesh->GetName(), WorldLocation.X, WorldLocation.Y, Ledger.Num());
}

void UMOWorldRemovalSubsystem::RecordInstanceRemoval(const UInstancedStaticMeshComponent* ISM, int32 InstanceIndex)
{
	FTransform Xf;
	if (ISM && ISM->GetStaticMesh() && ISM->GetInstanceTransform(InstanceIndex, Xf, /*bWorldSpace=*/true))
	{
		RecordRemoval(Xf.GetLocation(), ISM->GetStaticMesh());
	}
}

bool UMOWorldRemovalSubsystem::IsRemoved(const FVector& WorldLocation, const UStaticMesh* Mesh) const
{
	return Mesh && Ledger.IsRemoved(WorldLocation, Mesh->GetFName());
}

void UMOWorldRemovalSubsystem::AddAuthoritative(const FMORemovedInstance& Entry)
{
	Entries.Add(Entry);
	if (AMOGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMOGameState>() : nullptr)
	{
		GameState->AddRemovedInstance(Entry);
	}
}

void UMOWorldRemovalSubsystem::HandleReplicatedRemoval(const FMORemovedInstance& Entry)
{
	// (A client that removed the instance itself already has the entry: Add is false and its copy is gone.)
	if (Ledger.Add(Entry))
	{
		RemoveSpawnedMatches(MakeArrayView(&Entry, 1));
	}
}

int32 UMOWorldRemovalSubsystem::RemoveSpawnedMatches(TConstArrayView<FMORemovedInstance> InEntries)
{
	UWorld* World = GetWorld();
	if (!World || InEntries.Num() == 0)
	{
		return 0;
	}
	TMap<FName, TArray<FVector2D>> PlacesByMesh;
	for (const FMORemovedInstance& E : InEntries)
	{
		PlacesByMesh.FindOrAdd(E.Mesh).Add(FVector2D(E.CellX * FMOWorldRemovalLedger::CellCm, E.CellY * FMOWorldRemovalLedger::CellCm));
	}
	const double Margin = 2.0 * FMOWorldRemovalLedger::CellCm;
	int32 Removed = 0;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UInstancedStaticMeshComponent*> Comps;
		ActorIt->GetComponents<UInstancedStaticMeshComponent>(Comps);
		for (UInstancedStaticMeshComponent* ISM : Comps)
		{
			const UStaticMesh* Mesh = ISM ? ISM->GetStaticMesh() : nullptr;
			const TArray<FVector2D>* Places = Mesh ? PlacesByMesh.Find(Mesh->GetFName()) : nullptr;
			if (!Places || ISM->GetInstanceCount() == 0)
			{
				continue;
			}
			// Only components whose footprint can hold one of the places (a single replicated entry touches one or two components, not every forest cell).
			const FBox Box = ISM->Bounds.GetBox();
			bool bAnyInside = false;
			for (const FVector2D& P : *Places)
			{
				if (P.X >= Box.Min.X - Margin && P.X <= Box.Max.X + Margin && P.Y >= Box.Min.Y - Margin && P.Y <= Box.Max.Y + Margin)
				{
					bAnyInside = true;
					break;
				}
			}
			if (!bAnyInside)
			{
				continue;
			}
			TArray<int32> Matches;
			FTransform Xf;
			for (int32 I = 0; I < ISM->GetInstanceCount(); ++I)
			{
				if (ISM->GetInstanceTransform(I, Xf, /*bWorldSpace=*/true) && Ledger.IsRemoved(Xf.GetLocation(), Mesh->GetFName()))
				{
					Matches.Add(I);
				}
			}
			Matches.Sort([](int32 A, int32 B) { return A > B; });   // highest first: the indices below stay valid
			for (const int32 I : Matches)
			{
				Removed += ISM->RemoveInstance(I) ? 1 : 0;   // directly, not through FMOHISMHarvestHelper: these are already recorded
			}
		}
	}
	if (Removed > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldRemoval] removed %d spawned instance(s) for %d recorded removal(s)"), Removed, InEntries.Num());
	}
	return Removed;
}

void UMOWorldRemovalSubsystem::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	Save.WorldRemovalData.Removed = Entries;
}

void UMOWorldRemovalSubsystem::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	Ledger.Reset();
	Entries.Reset();
	for (const FMORemovedInstance& E : Save.WorldRemovalData.Removed)
	{
		if (Ledger.Add(E))
		{
			Entries.Add(E);
		}
	}
	AMOGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMOGameState>() : nullptr;
	if (GameState && GameState->HasAuthority())
	{
		GameState->ResetRemovedInstances(Entries);
	}
	const int32 Swept = RemoveSpawnedMatches(Entries);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldRemoval] loaded %d removed instance(s); %d already-spawned copies removed"), Entries.Num(), Swept);
}
