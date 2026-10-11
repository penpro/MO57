#include "MOCharacterMovementComponent.h"
#include "MOFramework.h"
#include "MOWaterSubsystem.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PhysicsVolume.h"

UMOCharacterMovementComponent::UMOCharacterMovementComponent()
{
	// Stock swimming defaults are for a game character; these are a person's (a steady crawl is about 1 m/s; fast front crawl 1.5-2 m/s).
	MaxSwimSpeed = 110.0f;
	BrakingDecelerationSwimming = 300.0f;
	Buoyancy = UnloadedBuoyancy;
}

float UMOCharacterMovementComponent::ComputeImmersion(float SurfaceZ, float FeetZ, float CapsuleHeight)
{
	if (CapsuleHeight <= KINDA_SMALL_NUMBER)
	{
		return SurfaceZ > FeetZ ? 1.0f : 0.0f;
	}
	return FMath::Clamp((SurfaceZ - FeetZ) / CapsuleHeight, 0.0f, 1.0f);
}

bool UMOCharacterMovementComponent::ShouldSwim(bool bWasSwimming, float Immersion, float EnterFraction, float ExitFraction, bool bStandable)
{
	return bWasSwimming ? (Immersion >= ExitFraction || !bStandable) : Immersion >= EnterFraction;
}

void UMOCharacterMovementComponent::RefreshWater()
{
	bWaterKnown = false;
	Immersion = 0.0f;
	if (!UpdatedComponent || !CharacterOwner)
	{
		return;
	}
	const UMOWaterSubsystem* Water = UMOWaterSubsystem::Get(this);
	if (!Water)
	{
		return;
	}
	const FVector Location = UpdatedComponent->GetComponentLocation();
	float Surface = 0.0f;
	if (!Water->GetSurfaceZ(Location.X, Location.Y, Surface))
	{
		return;
	}
	const float HalfHeight = CharacterOwner->GetSimpleCollisionHalfHeight();
	WaterSurfaceZ = Surface;
	bWaterKnown = true;
	Immersion = ComputeImmersion(Surface, Location.Z - HalfHeight, 2.0f * HalfHeight);
}

void UMOCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	RefreshWater();

	// Mode changes only from the ground / air / water modes: never override flying, a custom mode or the ragdoll's disabled movement.
	if (CharacterOwner && CanEverSwim())
	{
		const bool bSwimming = MovementMode == MOVE_Swimming;
		const bool bSwimmable = bSwimming || MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
		if (bSwimmable)
		{
			// Only look for ground when it can matter (a swimmer whose water has dropped below the exit line): a floor sweep per frame would be wasted on every deep-water frame.
			bool bStandable = false;
			if (bSwimming && Immersion < SwimExitImmersion)
			{
				FFindFloorResult Floor;
				FindFloor(UpdatedComponent->GetComponentLocation(), Floor, /*bCanUseCachedLocation=*/false);
				bStandable = Floor.IsWalkableFloor() && Floor.FloorDist <= 20.0f;
			}
			const bool bShouldSwim = ShouldSwim(bSwimming, Immersion, SwimEnterImmersion, SwimExitImmersion, bStandable);
			if (bShouldSwim && !bSwimming)
			{
				SetMovementMode(MOVE_Swimming);
			}
			else if (!bShouldSwim && bSwimming)
			{
				SetMovementMode(MOVE_Falling);   // onto the bottom / the bank: lands in Walking
			}
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

float UMOCharacterMovementComponent::GetMaxSpeed() const
{
	const float Base = Super::GetMaxSpeed();
	if (MovementMode == MOVE_Swimming)
	{
		return Base * FMath::Max(SwimSpeedScale, 0.05f);
	}
	if ((MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && Immersion > 0.0f)
	{
		return Base * (1.0f - WadeDragAtFullImmersion * Immersion);
	}
	return Base;
}

APhysicsVolume* UMOCharacterMovementComponent::GetPhysicsVolume() const
{
	if (MovementMode == MOVE_Swimming)
	{
		if (UMOWaterSubsystem* Water = UMOWaterSubsystem::Get(this))
		{
			if (APhysicsVolume* Volume = Water->GetSwimVolume())
			{
				return Volume;
			}
		}
	}
	return Super::GetPhysicsVolume();
}

bool UMOCharacterMovementComponent::IsHeadUnderwater() const
{
	if (!bWaterKnown || !UpdatedComponent || !CharacterOwner)
	{
		return false;
	}
	const float EyeZ = UpdatedComponent->GetComponentLocation().Z + CharacterOwner->BaseEyeHeight;
	return WaterSurfaceZ > EyeZ;
}

float UMOCharacterMovementComponent::GetWaterDepthCm() const
{
	if (!bWaterKnown || !UpdatedComponent || !CharacterOwner)
	{
		return 0.0f;
	}
	const float FeetZ = UpdatedComponent->GetComponentLocation().Z - CharacterOwner->GetSimpleCollisionHalfHeight();
	return FMath::Max(0.0f, WaterSurfaceZ - FeetZ);
}
