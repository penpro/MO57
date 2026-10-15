/**
 * MODesignationSubsystem.cpp - Terraform excavation designations (unit 3, stage 2)
 * See MODesignationSubsystem.h for architecture & rationale.
 */

#include "MODesignationSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "MOFramework.h"
#include "MOSpawnClearance.h"
#include "Engine/World.h"

// ============================================================================
// LIFECYCLE + REGISTRATION
// ============================================================================

void UMODesignationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Register as a save domain here (NOT in Initialize — cross-subsystem binds in
	// Initialize can promote a cook-commandlet ensure to fatal; see CLAUDE.md).
	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}
}

void UMODesignationSubsystem::Deinitialize()
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
	Zones.Empty();
	Super::Deinitialize();
}

bool UMODesignationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Game worlds only — skip editor preview / PIE-template worlds.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

UMODesignationSubsystem* UMODesignationSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UMODesignationSubsystem>();
}

bool UMODesignationSubsystem::HasAuthorityWorld() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

int32 UMODesignationSubsystem::IndexOfZone(const FGuid& ZoneId) const
{
	for (int32 i = 0; i < Zones.Num(); ++i)
	{
		if (Zones[i].ZoneId == ZoneId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

// ============================================================================
// DESIGNATION CRUD
// ============================================================================

FGuid UMODesignationSubsystem::DesignateZone(EMODesignationKind Kind, const FVector& Center, float Radius,
	float TargetHeightZ, float VolumeM3)
{
	if (!HasAuthorityWorld())
	{
		return FGuid();
	}
	if (Kind == EMODesignationKind::MAX || Radius <= 0.0f)
	{
		return FGuid();
	}

	FMODesignationZone Zone;
	Zone.ZoneId = FGuid::NewGuid();
	Zone.Kind = Kind;
	Zone.Center = Center;
	Zone.Radius = FMath::Max(1.0f, Radius);
	Zone.TargetHeightZ = TargetHeightZ;

	if (VolumeM3 > 0.0f)
	{
		Zone.RemainingVolumeM3 = VolumeM3;
	}
	else
	{
		// Auto-estimate: earth in a sphere-ish dig ~ footprint (πr²) × radius-as-depth.
		const float RadiusMeters = Zone.Radius * 0.01f;
		Zone.RemainingVolumeM3 = PI * RadiusMeters * RadiusMeters * RadiusMeters;
	}

	Zones.Add(Zone);
	UE_LOG(LogMOFramework, Log,
		TEXT("[MODesignation] Designated %s zone %s at %s r=%.0f vol=%.2fm3 (total=%d)"),
		*UEnum::GetValueAsString(Kind), *Zone.ZoneId.ToString(EGuidFormats::Short),
		*Center.ToString(), Zone.Radius, Zone.RemainingVolumeM3, Zones.Num());
	return Zone.ZoneId;
}

bool UMODesignationSubsystem::ClearZone(const FGuid& ZoneId)
{
	if (!HasAuthorityWorld())
	{
		return false;
	}
	const int32 Idx = IndexOfZone(ZoneId);
	if (Idx == INDEX_NONE)
	{
		return false;
	}
	Zones.RemoveAt(Idx);
	return true;
}

int32 UMODesignationSubsystem::ClearAllZones()
{
	if (!HasAuthorityWorld())
	{
		return 0;
	}
	const int32 N = Zones.Num();
	Zones.Reset();
	return N;
}

float UMODesignationSubsystem::ConsumeZoneVolume(const FGuid& ZoneId, float DeltaM3)
{
	if (!HasAuthorityWorld())
	{
		return 0.0f;
	}
	const int32 Idx = IndexOfZone(ZoneId);
	if (Idx == INDEX_NONE)
	{
		return 0.0f;
	}
	Zones[Idx].RemainingVolumeM3 -= FMath::Max(0.0f, DeltaM3);
	if (Zones[Idx].RemainingVolumeM3 <= KINDA_SMALL_NUMBER)
	{
		// Budget exhausted — the zone is finished, drop it.
		Zones.RemoveAt(Idx);
		return 0.0f;
	}
	return Zones[Idx].RemainingVolumeM3;
}

// ============================================================================
// QUERIES
// ============================================================================

bool UMODesignationSubsystem::FindBitePoint(const FMODesignationZone& Zone, bool bDig, FVector& OutPoint, int32& OutGroundSamples, float ToleranceCm) const
{
	const float Tolerance = ToleranceCm >= 0.0f ? ToleranceCm : LevelToleranceCm;
	OutGroundSamples = 0;
	const UWorld* World = GetWorld();
	if (!World || !Zone.IsValidZone() || (bDig && Zone.Kind == EMODesignationKind::Dump) || (!bDig && Zone.Kind == EMODesignationKind::Dig))
	{
		return false;   // nothing is dug in a fill zone, nothing is filled in a dig zone
	}
	const bool bTargeted = Zone.Kind != EMODesignationKind::Dig;   // fill and flatten zones work toward TargetHeightZ; a dig zone just loses earth
	const float Step = FMath::Max(Zone.Radius / 4.0f, 60.0f);
	const int32 N = FMath::CeilToInt(Zone.Radius / Step);
	bool bFound = false;
	for (int32 Iy = -N; Iy <= N; ++Iy)
	{
		for (int32 Ix = -N; Ix <= N; ++Ix)
		{
			const float Dx = Ix * Step, Dy = Iy * Step;
			if (Dx * Dx + Dy * Dy > Zone.Radius * Zone.Radius)
			{
				continue;
			}
			FHitResult Hit;
			const float X = Zone.Center.X + Dx, Y = Zone.Center.Y + Dy;
			if (!MOSpawnClearance::TraceVoxelGround(World, X, Y, Zone.Center.Z + 5000.0f, Zone.Center.Z - 5000.0f, nullptr, Hit))
			{
				continue;
			}
			++OutGroundSamples;
			const float Z = Hit.ImpactPoint.Z;
			if (bTargeted && (bDig ? Z <= Zone.TargetHeightZ + Tolerance : Z >= Zone.TargetHeightZ - Tolerance))
			{
				continue;   // already at the target on this side
			}
			if (!bFound || (bDig ? Z > OutPoint.Z : Z < OutPoint.Z))
			{
				bFound = true;
				OutPoint = FVector(X, Y, Z);
			}
		}
	}
	return bFound;
}

bool UMODesignationSubsystem::FindFlattenWork(const FMODesignationZone& Zone, FVector& OutDig, FVector& OutFill) const
{
	if (Zone.Kind != EMODesignationKind::Flatten)
	{
		return false;
	}
	FVector Unused;
	int32 Samples = 0;
	const bool bOffTarget = FindBitePoint(Zone, /*bDig=*/true, Unused, Samples) || FindBitePoint(Zone, /*bDig=*/false, Unused, Samples);
	return bOffTarget && FindBitePoint(Zone, /*bDig=*/true, OutDig, Samples, 0.0f) && FindBitePoint(Zone, /*bDig=*/false, OutFill, Samples, 0.0f);
}

bool UMODesignationSubsystem::FindZoneBitePoint(const FGuid& ZoneId, bool bDig, FVector& OutPoint, int32& OutGroundSamples) const
{
	OutGroundSamples = 0;
	const int32 Idx = IndexOfZone(ZoneId);
	return Idx != INDEX_NONE && FindBitePoint(Zones[Idx], bDig, OutPoint, OutGroundSamples);
}

bool UMODesignationSubsystem::FindZone(const FGuid& ZoneId, FMODesignationZone& OutZone) const
{
	const int32 Idx = IndexOfZone(ZoneId);
	if (Idx == INDEX_NONE)
	{
		return false;
	}
	OutZone = Zones[Idx];
	return true;
}

void UMODesignationSubsystem::GetZonesOfKind(EMODesignationKind Kind, TArray<FMODesignationZone>& OutZones) const
{
	OutZones.Reset();
	for (const FMODesignationZone& Z : Zones)
	{
		if (Z.Kind == Kind)
		{
			OutZones.Add(Z);
		}
	}
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

void UMODesignationSubsystem::BuildSaveData(FMODesignationSaveData& OutData) const
{
	OutData.Zones = Zones;
	OutData.Version = 1;
}

bool UMODesignationSubsystem::ApplySaveDataAuthority(const FMODesignationSaveData& InData)
{
	Zones.Reset();
	Zones.Reserve(InData.Zones.Num());

	// Future: if InData.Version != 1, migrate here.
	for (const FMODesignationZone& Z : InData.Zones)
	{
		// Refuse zero-radius / id-less zones a buggy producer might have saved.
		if (Z.Radius <= 0.0f || !Z.ZoneId.IsValid())
		{
			continue;
		}
		Zones.Add(Z);
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MODesignation] Loaded %d designation zone(s) from save (v%d)"),
		Zones.Num(), InData.Version);
	return true;
}

void UMODesignationSubsystem::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	BuildSaveData(Save.DesignationData);
}

void UMODesignationSubsystem::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	ApplySaveDataAuthority(Save.DesignationData);
}
