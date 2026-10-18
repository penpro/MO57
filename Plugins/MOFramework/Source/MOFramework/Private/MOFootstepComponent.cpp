#include "MOFootstepComponent.h"
#include "MOAudioSubsystem.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "MOFootstepSurfaceInterface.h"
#include "MOFramework.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

// ============================================================================
// Pure decisions
// ============================================================================

EMOFootstepGait FMOFootstepMath::GaitFor(bool bCrouched, float Speed2D, float RunThresholdCm)
{
	if (bCrouched)
	{
		return EMOFootstepGait::Sneak;
	}
	return Speed2D > RunThresholdCm ? EMOFootstepGait::Run : EMOFootstepGait::Walk;
}

float FMOFootstepMath::VolumeFor(EMOFootstepGait Gait)
{
	switch (Gait)
	{
	case EMOFootstepGait::Sneak: return 0.35f;
	case EMOFootstepGait::Run:   return 1.0f;
	default:                     return 0.7f;
	}
}

float FMOFootstepMath::StepLengthCm(EMOFootstepGait Gait)
{
	switch (Gait)
	{
	case EMOFootstepGait::Sneak: return 100.0f;
	case EMOFootstepGait::Run:   return 230.0f;
	default:                     return 170.0f;
	}
}

FName FMOFootstepMath::SoundIdFor(FName Set, int32 Variant)
{
	return FName(*FString::Printf(TEXT("SFX.Footsteps.%s%d"), *Set.ToString(), (Variant & 1) + 1));
}

FMOFootstepMath::EGround FMOFootstepMath::ClassifyGround(float FloorZ, float GeneratedSurfaceZ)
{
	const float Dz = FloorZ - GeneratedSurfaceZ;
	if (Dz > StructureAboveCm)
	{
		return EGround::Structure;
	}
	if (Dz < -UndergroundBelowCm)
	{
		return EGround::Underground;
	}
	return EGround::Terrain;
}

bool FMOFootPlantDetector::Update(float FootSpeedCm, float BodySpeedCm, float DeltaSeconds)
{
	const float Reference = FMath::Max(BodySpeedCm, ReferenceFloorCmPerSec);
	if (!bSwinging)
	{
		if (FootSpeedCm > SwingFraction * Reference)
		{
			bSwinging = true;
			SwingSeconds = 0.0f;
		}
		return false;
	}
	SwingSeconds += DeltaSeconds;
	if (FootSpeedCm < PlantFraction * Reference && SwingSeconds >= MinSwingSeconds)
	{
		bSwinging = false;
		return true;
	}
	if (SwingSeconds > MaxSwingSeconds)
	{
		bSwinging = false;   // not a swing (the animation froze, the pawn was moved): drop it quietly
	}
	return false;
}

bool FMOStrideFallback::Advance(float DistanceCm, float StepLengthCm)
{
	Accumulated += DistanceCm;
	if (Accumulated >= StepLengthCm * FMOFootstepMath::FallbackFactor)
	{
		Accumulated = 0.0f;
		return true;
	}
	return false;
}

// ============================================================================
// Component
// ============================================================================

UMOFootstepComponent::UMOFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;   // after the animation of this frame is evaluated: the bones are where the player sees the feet
	SetIsReplicatedByDefault(false);
}

bool UMOFootstepComponent::EnsureBones(const USkeletalMeshComponent* Mesh)
{
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		return false;
	}
	if (BonesResolvedFor.Get() != Asset)
	{
		BonesResolvedFor = Asset;
		BoneIndices[0] = Mesh->GetBoneIndex(LeftFootBone);
		BoneIndices[1] = Mesh->GetBoneIndex(RightFootBone);
		Detectors[0].Reset();
		Detectors[1].Reset();
		bBonesMissingLogged = false;
	}
	if (BoneIndices[0] == INDEX_NONE || BoneIndices[1] == INDEX_NONE)
	{
		if (!bBonesMissingLogged)
		{
			bBonesMissingLogged = true;
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOFootstep] %s: skeleton '%s' has no %s / %s bones; no footsteps"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Asset), *LeftFootBone.ToString(), *RightFootBone.ToString());
		}
		return false;
	}
	return true;
}

void UMOFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || World->GetNetMode() == NM_DedicatedServer || DeltaTime <= 0.0f)
	{
		return;
	}
	if (const AMOCharacter* MOCharacter = Cast<AMOCharacter>(Character))
	{
		if (MOCharacter->IsDead())
		{
			return;
		}
	}
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Mesh || !Move || !EnsureBones(Mesh))
	{
		return;
	}

	// Cosmetic: nothing for pawns the listener cannot hear.
	const APlayerController* PC = World->GetFirstPlayerController();
	const FVector Listener = (PC && PC->PlayerCameraManager) ? PC->PlayerCameraManager->GetCameraLocation() : Character->GetActorLocation();
	if (FVector::DistSquared(Listener, Character->GetActorLocation()) > FMath::Square(MaxAudibleDistanceCm))
	{
		return;
	}

	// Wading and swimming have their own water sounds (AMOCharacter::UpdateWaterSounds); steps in water would double them.
	if (const UMOCharacterMovementComponent* MOMove = Cast<UMOCharacterMovementComponent>(Move))
	{
		if (MOMove->GetImmersion() > 0.08f)
		{
			bWasGrounded = false;
			LastVerticalSpeed = 0.0f;   // the fall that ended in the water is the splash's, not a landing for the bank
			return;
		}
	}

	const bool bGrounded = Move->IsMovingOnGround();
	const FVector Velocity = Character->GetVelocity();
	const float Speed2D = Velocity.Size2D();
	const double Now = World->GetTimeSeconds();

	if (bGrounded && !bWasGrounded)
	{
		if (bDebugLog)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP touchdown vertical speed %.0f cm/s (a landing sounds from %.0f)"), LastVerticalSpeed, -LandingSpeedCm);
		}
		if (LastVerticalSpeed < -LandingSpeedCm)
		{
			// A landing: one heavy step, harder the faster the fall (up to ~12 m/s).
			EmitStep(NextVariant & 1, EStepSource::Landing, 0.8f + 0.6f * FMath::Clamp(-LastVerticalSpeed / 1200.0f, 0.0f, 1.0f));
		}
	}
	bWasGrounded = bGrounded;
	LastVerticalSpeed = Velocity.Z;
	if (!bGrounded)
	{
		return;
	}

	const float FloorZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	bool bPlanted[2] = { false, false };
	float Heights[2] = { 0.0f, 0.0f };
	FVector FootLocation[2];
	FVector FootVelocity[2] = { FVector::ZeroVector, FVector::ZeroVector };
	for (int32 I = 0; I < 2; ++I)
	{
		FootLocation[I] = Mesh->GetBoneTransform(BoneIndices[I]).GetLocation();
		if (bHavePrevFoot)
		{
			FootVelocity[I] = (FootLocation[I] - PrevFootLocation[I]) / DeltaTime;
		}
		PrevFootLocation[I] = FootLocation[I];
		Heights[I] = FootLocation[I].Z - FloorZ;
		bPlanted[I] = Detectors[I].Update(FootVelocity[I].Size2D(), Speed2D, DeltaTime);
	}
	bHavePrevFoot = true;
	if (Now < HeightsLogUntil && (bHeightsEveryFrame || Now >= NextHeightsLog))
	{
		NextHeightsLog = Now + 0.1;
		// t, frame dt, pawn speed, then per foot: height above the floor, world horizontal speed, world vertical speed, detector state (^ = swinging)
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP height t=%.3f dt=%.4f speed=%.0f L=%.1f,%.0f,%.0f%s R=%.1f,%.0f,%.0f%s"), Now, DeltaTime, Speed2D,
			Heights[0], FootVelocity[0].Size2D(), FootVelocity[0].Z, Detectors[0].IsSwinging() ? TEXT("^") : TEXT("_"),
			Heights[1], FootVelocity[1].Size2D(), FootVelocity[1].Z, Detectors[1].IsSwinging() ? TEXT("^") : TEXT("_"));
	}

	if (Speed2D < MinStepSpeedCm)
	{
		Fallback.NotePlant();   // standing shuffles make no sound
		return;
	}

	bool bAnyPlant = false;
	for (int32 I = 0; I < 2; ++I)
	{
		if (bPlanted[I])
		{
			bAnyPlant = true;
			if (Now - LastStepTime[I] > 0.15)   // per foot only: the two feet of a crouched or sprinting stride can plant within a few hundredths of a second of each other
			{
				EmitStep(I, EStepSource::Plant, 1.0f);
			}
		}
	}
	if (bAnyPlant)
	{
		Fallback.NotePlant();
	}
	else if (Fallback.Advance(Speed2D * DeltaTime, FMOFootstepMath::StepLengthCm(FMOFootstepMath::GaitFor(Move->IsCrouching(), Speed2D, RunSpeedThresholdCm))))
	{
		EmitStep(NextVariant & 1, EStepSource::Stride, 1.0f);   // the animation reported no plant over ~1.6 steps of ground: do not leave the walk silent
	}
}

FName UMOFootstepComponent::ResolveSet(const FHitResult& FloorHit, const FVector& FootLocation, FString* OutDescription)
{
	// 1. The thing stood on names its own surface.
	if (const AActor* Ground = FloorHit.GetActor())
	{
		if (Ground->GetClass()->ImplementsInterface(UMOFootstepSurfaceInterface::StaticClass()))
		{
			const FName Set = IMOFootstepSurfaceInterface::Execute_GetFootstepSet(Ground);
			if (!Set.IsNone())
			{
				if (OutDescription) { *OutDescription = FString::Printf(TEXT("actor %s"), *Ground->GetName()); }
				return Set;
			}
		}
	}

	// 2. The world generator: where is the floor relative to the generated surface, and which biome is it.
	const UWorld* World = GetWorld();
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
	if (!Seeds || !Seeds->HasActiveSeed())
	{
		if (OutDescription) { *OutDescription = TEXT("no world seed: fallback"); }
		return FallbackTerrainSet;
	}
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const int32 Seed = Seeds->GetActiveTerrainSeed();
	const FVector Ground = FloorHit.bBlockingHit ? FVector(FloorHit.ImpactPoint) : FootLocation;

	const float SurfaceZ = FMOWorldGen::SampleHeightCm(*Params, Ground.X, Ground.Y, Seed, Params->Tuning.VoxelSizeCm);
	const FMOFootstepMath::EGround Class = FMOFootstepMath::ClassifyGround((float)Ground.Z, SurfaceZ);
	if (Class == FMOFootstepMath::EGround::Structure)
	{
		if (OutDescription) { *OutDescription = FString::Printf(TEXT("structure (floor %.0f cm above the generated surface)"), Ground.Z - SurfaceZ); }
		return StructureSet;
	}
	if (Class == FMOFootstepMath::EGround::Underground)
	{
		if (OutDescription) { *OutDescription = FString::Printf(TEXT("underground (floor %.0f cm below the generated surface)"), SurfaceZ - Ground.Z); }
		return UndergroundSet;
	}

	const FIntPoint Cell(FMath::FloorToInt(Ground.X / 150.0), FMath::FloorToInt(Ground.Y / 150.0));
	if (Seed != CachedSeed || Cell != CachedCell || CachedBiomeSet.IsNone())
	{
		const float SlopeDeg = FloorHit.bBlockingHit ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)FloorHit.ImpactNormal.Z, -1.0f, 1.0f))) : 0.0f;
		// The biome the ground under the foot SHOWS (the spawner's soft pick), not the hard climate line: across a wide ecotone the step must sound like the
		// surface it lands on (playtest 2026-10-14 #4).
		CachedBiomeId = UMOBiomeDatabaseSettings::PickSurfaceBiomeAtWorld(Ground, SlopeDeg, Seed);
		const int32 Index = Params->FindBiomeIndex(CachedBiomeId);
		CachedBiomeSet = Params->Biomes.IsValidIndex(Index) ? Params->Biomes[Index].FootstepSet : FallbackTerrainSet;
		CachedSeed = Seed;
		CachedCell = Cell;
	}
	if (OutDescription) { *OutDescription = FString::Printf(TEXT("biome %s"), *CachedBiomeId.ToString()); }
	return CachedBiomeSet;
}

void UMOFootstepComponent::LogHeightsFor(float Seconds, bool bEveryFrame)
{
	if (const UWorld* World = GetWorld())
	{
		HeightsLogUntil = World->GetTimeSeconds() + Seconds;
		NextHeightsLog = 0.0;
		bHeightsEveryFrame = bEveryFrame;
	}
}

void UMOFootstepComponent::EmitStep(int32 FootIndex, EStepSource Source, float VolumeScale)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this);
	if (!Character || !World || !Audio)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	const FVector Foot = (Mesh && BoneIndices[FootIndex] != INDEX_NONE) ? Mesh->GetBoneTransform(BoneIndices[FootIndex]).GetLocation() : Character->GetActorLocation();
	const float FloorZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	const FVector Sole(Foot.X, Foot.Y, FloorZ);

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	const FHitResult FloorHit = Move ? FHitResult(Move->CurrentFloor.HitResult) : FHitResult();
	FString Why;
	const FName Set = ResolveSet(FloorHit, Foot, bDebugLog ? &Why : nullptr);

	const float Speed2D = Character->GetVelocity().Size2D();
	const EMOFootstepGait Gait = FMOFootstepMath::GaitFor(Move && Move->IsCrouching(), Speed2D, RunSpeedThresholdCm);
	const float Volume = FMOFootstepMath::VolumeFor(Gait) * VolumeScale * FMath::FRandRange(0.9f, 1.1f);
	const float Pitch = FMath::FRandRange(0.94f, 1.06f);

	FName Played = Set;
	FName Id = FMOFootstepMath::SoundIdFor(Set, NextVariant);
	if (!Audio->HasAudioId(Id))
	{
		if (!MissingSets.Contains(Set))
		{
			MissingSets.Add(Set);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOFootstep] footstep set '%s' has no bank rows (%s); '%s' plays instead"), *Set.ToString(), *Id.ToString(), *FallbackTerrainSet.ToString());
		}
		Played = FallbackTerrainSet;
		Id = FMOFootstepMath::SoundIdFor(Played, NextVariant);
	}
	const bool bOk = Audio->PlayOneShotAtLocationAttenuated(this, Id, Sole, Volume, Pitch, Audio->GetFootstepAttenuation());

	++NextVariant;
	++StepCount;
	const double Now = World->GetTimeSeconds();
	LastStepTime[FootIndex] = Now;

	if (bDebugLog)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP step %s foot=%s set=%s%s gait=%d speed=%.0f vol=%.2f (%s) %s"),
			Source == EStepSource::Plant ? TEXT("plant") : Source == EStepSource::Stride ? TEXT("stride") : TEXT("landing"),
			FootIndex == 0 ? TEXT("L") : TEXT("R"), *Played.ToString(), Played == Set ? TEXT("") : TEXT(" (fallback)"), (int32)Gait, Speed2D, Volume, *Why, bOk ? TEXT("") : TEXT("NOT PLAYED"));
	}
}
