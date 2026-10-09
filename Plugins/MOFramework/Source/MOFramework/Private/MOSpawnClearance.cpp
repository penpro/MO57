#include "MOSpawnClearance.h"
#include "MOFramework.h"
#include "VoxelWorld.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"

namespace MOSpawnClearance
{
	FCapsule CapsuleOf(TSubclassOf<APawn> PawnClass)
	{
		FCapsule Result;
		if (const ACharacter* CDO = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr)
		{
			if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
			{
				Result.Radius = Capsule->GetScaledCapsuleRadius();
				Result.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}
		return Result;
	}

	FCapsule CapsuleOf(const APawn* Pawn)
	{
		FCapsule Result;
		if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				Result.Radius = Capsule->GetScaledCapsuleRadius();
				Result.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}
		return Result;
	}

	bool TraceVoxelGround(const UWorld* World, float X, float Y, float StartZ, float EndZ, const AActor* Ignore, FHitResult& OutHit)
	{
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOSpawnGround), /*bTraceComplex=*/false, Ignore);
		TArray<FHitResult> Hits;
		World->LineTraceMultiByChannel(Hits, FVector(X, Y, StartZ), FVector(X, Y, EndZ), ECC_WorldStatic, Params);
		for (const FHitResult& Hit : Hits)
		{
			const AActor* HitActor = Hit.GetActor();
			if (Hit.bBlockingHit && HitActor && HitActor->IsA<AVoxelWorld>())
			{
				OutHit = Hit;
				return true;
			}
		}
		return false;
	}

	bool IsCapsuleBlocked(const UWorld* World, const FVector& Center, const FCapsule& Capsule, const AActor* Ignore,
		bool bIgnorePawns, FString* OutBlockerName)
	{
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOSpawnClearance), /*bTraceComplex=*/false, Ignore);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(Capsule.Radius, Capsule.HalfHeight), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (!Overlap.bBlockingHit)
			{
				continue; // an Overlap response (pickups, triggers) does not stop a pawn
			}
			const AActor* Actor = Overlap.GetActor();
			if (Actor && Actor->IsA<AVoxelWorld>())
			{
				continue; // the terrain is what we stand ON, never an obstacle
			}
			if (bIgnorePawns && Actor && Actor->IsA<APawn>())
			{
				continue;
			}
			if (OutBlockerName)
			{
				*OutBlockerName = FString::Printf(TEXT("%s/%s"), *GetNameSafe(Actor), *GetNameSafe(Overlap.GetComponent()));
			}
			return true;
		}
		return false;
	}

	bool IsStandingSpotClear(const UWorld* World, const FVector& GroundLocation, const FCapsule& Capsule, const AActor* Ignore)
	{
		// Centre = ground + half height + a small lift so the capsule's base is not coplanar with the surface.
		const FVector Center = GroundLocation + FVector(0.0f, 0.0f, Capsule.HalfHeight + 6.0f);
		return !IsCapsuleBlocked(World, Center, Capsule, Ignore, /*bIgnorePawns=*/false);
	}

	bool FindClearGround(const UWorld* World, const FVector& GroundLocation, const FCapsule& Capsule, const AActor* Ignore,
		FVector& OutGround, float SearchRadius, float TraceHalfHeight, float MinSurfaceNormalZ)
	{
		if (!World)
		{
			return false;
		}
		if (IsStandingSpotClear(World, GroundLocation, Capsule, Ignore))
		{
			OutGround = GroundLocation;
			return true;
		}

		// Rings outward, nearest first. Step by about two capsule widths so each ring is a genuinely different place.
		const float Step = FMath::Max(120.0f, Capsule.Radius * 3.0f);
		for (float Radius = Step; Radius <= SearchRadius; Radius += Step)
		{
			const int32 Samples = FMath::Max(8, FMath::CeilToInt(2.0f * PI * Radius / Step));
			for (int32 Sample = 0; Sample < Samples; ++Sample)
			{
				const float Angle = 2.0f * PI * static_cast<float>(Sample) / static_cast<float>(Samples);
				const float X = GroundLocation.X + FMath::Cos(Angle) * Radius;
				const float Y = GroundLocation.Y + FMath::Sin(Angle) * Radius;

				FHitResult Ground;
				if (!TraceVoxelGround(World, X, Y, GroundLocation.Z + TraceHalfHeight, GroundLocation.Z - TraceHalfHeight, Ignore, Ground)
					|| Ground.ImpactNormal.Z < MinSurfaceNormalZ)
				{
					continue;
				}
				if (IsStandingSpotClear(World, Ground.Location, Capsule, Ignore))
				{
					OutGround = Ground.Location;
					return true;
				}
			}
		}
		return false;
	}
}
