#include "MOCharacterMovementComponent.h"
#include "MOFramework.h"
#include "MOWaterSubsystem.h"
#include "MOCharacter.h"
#include "MONetDiagnostics.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"

// ---- gait in the saved moves (see SetWantedGait) ----

class FSavedMove_MO : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToJog : 1;
	uint8 bSavedWantsToSprint : 1;

	FSavedMove_MO() : bSavedWantsToJog(0), bSavedWantsToSprint(0) {}

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToJog = 0;
		bSavedWantsToSprint = 0;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Result = Super::GetCompressedFlags();
		if (bSavedWantsToJog)
		{
			Result |= FLAG_Custom_0;
		}
		if (bSavedWantsToSprint)
		{
			Result |= FLAG_Custom_1;
		}
		return Result;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FSavedMove_MO* Other = static_cast<const FSavedMove_MO*>(NewMove.Get());
		if (bSavedWantsToJog != Other->bSavedWantsToJog || bSavedWantsToSprint != Other->bSavedWantsToSprint)
		{
			return false;   // a gait change is a move boundary: the server must see where it happened
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
		if (const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToJog = Move->bWantsToJog;
			bSavedWantsToSprint = Move->bWantsToSprint;
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);
		if (UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			Move->bWantsToJog = bSavedWantsToJog;   // a replayed move runs at the gait it was made with
			Move->bWantsToSprint = bSavedWantsToSprint;
		}
	}
};

class FNetworkPredictionData_Client_MO : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_MO(const UCharacterMovementComponent& ClientMovement) : FNetworkPredictionData_Client_Character(ClientMovement) {}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_MO());
	}
};

FNetworkPredictionData_Client* UMOCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UMOCharacterMovementComponent* MutableThis = const_cast<UMOCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_MO(*this);
	}
	return ClientPredictionData;
}

void UMOCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	const bool bJog = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bool bSprint = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	// The server judges what the body can do: a sprint the server's stamina refuses runs at the jog / walk speed (the joiner is corrected to it).
	AMOCharacter* MOOwner = Cast<AMOCharacter>(CharacterOwner);
	if (bSprint && MOOwner && !MOOwner->CanSprint_Implementation())
	{
		bSprint = false;
	}
	const bool bChanged = bJog != (bool)bWantsToJog || bSprint != (bool)bWantsToSprint;
	bWantsToJog = bJog;
	bWantsToSprint = bSprint;
	if (bChanged && MOOwner && !MOOwner->IsLocallyControlled())
	{
		MOOwner->ApplyGaitFromMovement(bJog, bSprint);   // the server's copy of the character learns the gait (exertion, metabolism, what other players see)
	}
}

void UMOCharacterMovementComponent::SetGaitSpeeds(float Walk, float Jog, float Sprint)
{
	MaxWalkSpeed = Walk;
	GaitJogSpeed = Jog;
	GaitSprintSpeed = Sprint;
}

void UMOCharacterMovementComponent::SetWantedGait(bool bJog, bool bSprint)
{
	bWantsToJog = bJog;
	bWantsToSprint = bSprint;
}

UMOCharacterMovementComponent::UMOCharacterMovementComponent()
{
	bWantsToJog = 0;
	bWantsToSprint = 0;
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

bool UMOCharacterMovementComponent::ShouldSwim(bool bWasSwimming, float WaterDepthCm, float EnterDepthCm, float ExitDepthCm)
{
	return bWasSwimming ? WaterDepthCm >= ExitDepthCm : WaterDepthCm >= EnterDepthCm;
}

void UMOCharacterMovementComponent::RefreshWater()
{
	bWaterKnown = false;
	bBuried = false;
	Immersion = 0.0f;
	WaterColumnDepthCm = 0.0f;
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
	const float FeetZ = Location.Z - HalfHeight;
	// Only below the surface can "buried" matter, and only then is the (cheap but not free) ground query worth making.
	if (FeetZ < Surface)
	{
		float Ground = 0.0f;
		if (Water->GetGroundZ(Location.X, Location.Y, Ground) && IsBuriedInTerrain(FeetZ, Ground, BuriedMarginCm))
		{
			bBuried = true;   // inside solid rock: no water here
			return;
		}
	}
	WaterSurfaceZ = Surface;
	bWaterKnown = true;
	Immersion = ComputeImmersion(Surface, FeetZ, 2.0f * HalfHeight);

	// The depth of the water here (what the swim decision goes by): from the surface straight down to the first walkable thing this character could stand on. The trace uses the
	// character's own collision channel and responses, so the water it swims through is not "ground", while the real floor (dug pits included), a submerged rock or a dock is.
	// Probed only as deep as the decision needs: no bottom within reach counts as deep water. Deterministic in the position, like the rest of this refresh (client replays agree).
	if (FeetZ < Surface)
	{
		WaterColumnDepthCm = TNumericLimits<float>::Max();
		const float ProbeCm = FMath::Max(SwimEnterDepthCm, SwimExitDepthCm) + 50.0f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MOWaterColumnDepth), /*bTraceComplex=*/false, CharacterOwner);
		FCollisionResponseParams Response;
		InitCollisionParams(Params, Response);
		FHitResult Hit;
		const FVector Start(Location.X, Location.Y, Surface);
		const FVector End(Location.X, Location.Y, Surface - ProbeCm);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, UpdatedComponent->GetCollisionObjectType(), Params, Response) && IsWalkable(Hit))
		{
			WaterColumnDepthCm = FMath::Max(0.0f, Surface - Hit.ImpactPoint.Z);
		}
	}
}

void UMOCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	RefreshWater();
	bSwimmableWater = !bBuried && ShouldSwim(MovementMode == MOVE_Swimming, WaterColumnDepthCm, SwimEnterDepthCm, SwimExitDepthCm);

	// Mode changes only from the ground / air / water modes: never override flying, a custom mode or the ragdoll's disabled movement.
	if (CharacterOwner && CanEverSwim())
	{
		const bool bSwimming = MovementMode == MOVE_Swimming;
		const bool bSwimmable = bSwimming || MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
		if (bSwimmable)
		{
			// Buried in the terrain: never "swim", and leave the swimming mode (deep water's keep-swimming rule is for water, not rock): the fall-through rescue takes over.
			const bool bShouldSwim = !bBuried && ShouldSwim(bSwimming, WaterColumnDepthCm, SwimEnterDepthCm, SwimExitDepthCm);
			bSwimmableWater = bShouldSwim;   // what the engine's landing / teleport code asks through IsInWater()
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
	float Base = Super::GetMaxSpeed();
	if (MovementMode == MOVE_Swimming)
	{
		return Base * FMath::Max(SwimSpeedScale, 0.05f);
	}
	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		if (!IsCrouching())   // crouched walking keeps its own speed (Super's MaxWalkSpeedCrouched)
		{
			Base = GaitSpeed(bWantsToJog, bWantsToSprint, Base, GaitJogSpeed, GaitSprintSpeed);
		}
		if (Immersion > 0.0f)
		{
			return Base * (1.0f - WadeDragAtFullImmersion * Immersion);
		}
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

// ---- co-op smoothness counters (MONetDiagnostics.h) ----

namespace
{
	/** What a character stands on, for the co-op correction lines: the floor's actor class and mesh (a PCG rock, a building floor, the voxel
	 *  landscape), the floor's height and the gap. The Oct 18 Steam session's corrections were floor-height disagreements (host higher by 3-38 cm,
	 *  a constant 4.7 cm while standing still, "walking" on the host but "falling" on the joiner): the two machines' lines name the two floors. */
	FString DescribeFloor(const FFindFloorResult& Floor)
	{
		if (!Floor.bBlockingHit)
		{
			return TEXT("none");
		}
		const AActor* Owner = Floor.HitResult.GetActor();
		const UPrimitiveComponent* Component = Floor.HitResult.GetComponent();
		FString What = Owner ? Owner->GetClass()->GetName() : FString(TEXT("?"));
		if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component))
		{
			What += TEXT("/") + (Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetName() : FString(TEXT("?")));
		}
		else if (Component)
		{
			What += TEXT("/") + Component->GetClass()->GetName();
		}
		return FString::Printf(TEXT("%s z=%.1f gap=%.1f%s"), *What, Floor.HitResult.ImpactPoint.Z, Floor.FloorDist, Floor.bWalkableFloor ? TEXT("") : TEXT(" not-walkable"));
	}

	/** The movement BASE (not the same object as the floor hit: on the voxel terrain the floor is a per-chunk collision component, the base the
	 *  landscape actor's root) and whether the engine sends positions RELATIVE to it. A relative base the other machine cannot resolve -- a
	 *  per-machine PCG or voxel component -- would turn a relative position into a wrong world position there. */
	FString DescribeBase(const ACharacter* Character)
	{
		const UObject* Base = Character ? Character->GetMovementBaseObject() : nullptr;
		if (!Base)
		{
			return TEXT("none");
		}
		const UActorComponent* Component = Cast<UActorComponent>(Base);
		const AActor* Owner = Component ? Component->GetOwner() : Cast<AActor>(Base);
		const bool bRelative = MovementBaseUtility::UseRelativeLocation(Character->GetMovementBaseInterfaceData());
		return FString::Printf(TEXT("%s.%s %s"), Owner ? *Owner->GetName() : TEXT("?"), *Base->GetName(), bRelative ? TEXT("RELATIVE") : TEXT("absolute"));
	}
}

bool UMOCharacterMovementComponent::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation,
	const FVector& RelativeClientLocation, FMovementBaseInterfaceData* ClientMovementBaseInterfaceData, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	const bool bNeedsCorrection = Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation,
		ClientMovementBaseInterfaceData, ClientBaseBoneName, ClientMovementMode);
	const FVector ServerMinusClient = UpdatedComponent ? UpdatedComponent->GetComponentLocation() - ClientWorldLocation : FVector::ZeroVector;
	FMONetCounters::NoteServerMoveChecked(CharacterOwner, bNeedsCorrection, ServerMinusClient);
	if (bNeedsCorrection && ServerMinusClient.Size() >= FMONetCounters::EventCorrectionCm)
	{
		FMONetCounters::NoteEvent(CharacterOwner, FString::Printf(TEXT("host corrected joiner pawn %s by %.0f cm (dx %.0f dy %.0f dz %.0f) at %s, host floor %s, base %s"),
			*GetNameSafe(CharacterOwner), ServerMinusClient.Size(), ServerMinusClient.X, ServerMinusClient.Y, ServerMinusClient.Z,
			UpdatedComponent ? *UpdatedComponent->GetComponentLocation().ToCompactString() : TEXT("?"), *DescribeFloor(CurrentFloor), *DescribeBase(CharacterOwner)));
	}
	if (bNeedsCorrection && FMONetCounters::TakeDetailLine())
	{
		TEnumAsByte<EMovementMode> ClientMode, ClientGroundMode;
		uint8 ClientCustomMode = 0;
		UnpackNetworkMovementMode(ClientMovementMode, ClientMode, ClientCustomMode, ClientGroundMode);
		UE_LOG(LogMOFramework, Warning, TEXT("[MONetCorr] server: %s off by %.1f cm (dx %.1f dy %.1f dz %.1f) | server mode %d speed %.0f max %.0f gait %s | client mode %d | dt %.3f accel %.0f | at %s floor %s base %s"),
			*GetNameSafe(CharacterOwner), ServerMinusClient.Size(), ServerMinusClient.X, ServerMinusClient.Y, ServerMinusClient.Z,
			static_cast<int32>(MovementMode), Velocity.Size2D(), GetMaxSpeed(), bWantsToSprint ? TEXT("sprint") : (bWantsToJog ? TEXT("jog") : TEXT("walk")),
			static_cast<int32>(ClientMode.GetValue()), DeltaTime, Accel.Size2D(),
			UpdatedComponent ? *UpdatedComponent->GetComponentLocation().ToCompactString() : TEXT("?"), *DescribeFloor(CurrentFloor), *DescribeBase(CharacterOwner));
	}
	return bNeedsCorrection;
}

void UMOCharacterMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	const bool bCorrection = MoveResponse.IsCorrection();
	FMONetCounters::NoteClientMoveResponse(bCorrection);
	if (bCorrection && !bCorrectionPending && UpdatedComponent != nullptr)
	{
		PreCorrectionLocation = UpdatedComponent->GetComponentLocation();   // where the pawn is drawn now; the stock code moves it to the server's spot next
		PreCorrectionFloor = DescribeFloor(CurrentFloor) + TEXT(" base ") + DescribeBase(CharacterOwner);
		bCorrectionPending = true;
	}
	Super::ClientHandleMoveResponse(MoveResponse);
}

bool UMOCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	const bool bReplayed = Super::ClientUpdatePositionAfterServerUpdate();
	const FNetworkPredictionData_Client_Character* ClientData = HasPredictionData_Client() ? GetPredictionData_Client_Character() : nullptr;
	if (bCorrectionPending && ClientData && !ClientData->bUpdatePosition && UpdatedComponent != nullptr)
	{
		// the server's position plus every move made since: the visible jump the correction caused (0 when client and server agreed in the end)
		bCorrectionPending = false;
		const FVector Snap = UpdatedComponent->GetComponentLocation() - PreCorrectionLocation;
		const float SnapCm = Snap.Size();
		FMONetCounters::NoteClientCorrectionSnap(CharacterOwner, SnapCm);
		const FString After = DescribeFloor(CurrentFloor) + TEXT(" base ") + DescribeBase(CharacterOwner);
		if (SnapCm >= FMONetCounters::EventCorrectionCm)
		{
			FMONetCounters::NoteEvent(CharacterOwner, FString::Printf(TEXT("joiner rubber-banded %.0f cm (dx %.0f dy %.0f dz %.0f) at %s; joiner floor before %s, after %s"),
				SnapCm, Snap.X, Snap.Y, Snap.Z, *UpdatedComponent->GetComponentLocation().ToCompactString(), *PreCorrectionFloor, *After));
		}
		if (FMONetCounters::TakeDetailLine())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MONetCorr] client: %s snapped %.1f cm (dx %.1f dy %.1f dz %.1f) after a correction (mode %d speed %.0f gait %s) | at %s floor before %s, after %s"),
				*GetNameSafe(CharacterOwner), SnapCm, Snap.X, Snap.Y, Snap.Z, static_cast<int32>(MovementMode), Velocity.Size2D(),
				bWantsToSprint ? TEXT("sprint") : (bWantsToJog ? TEXT("jog") : TEXT("walk")),
				*UpdatedComponent->GetComponentLocation().ToCompactString(), *PreCorrectionFloor, *After);
		}
	}
	return bReplayed;
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
