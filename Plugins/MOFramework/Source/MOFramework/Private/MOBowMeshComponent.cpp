// Copyright MO57. All Rights Reserved.

#include "MOBowMeshComponent.h"

#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "MOBowAnimInstance.h"
#include "MOFramework.h"

namespace
{
	/** hand_r's finger -> hand_l's (and back): the drawing hand is the one the bow is not in. */
	FName OtherHandBone(FName Bone)
	{
		FString S = Bone.ToString();
		if (S.EndsWith(TEXT("_r")))
		{
			S[S.Len() - 1] = TEXT('l');
		}
		else if (S.EndsWith(TEXT("_l")))
		{
			S[S.Len() - 1] = TEXT('r');
		}
		return FName(*S);
	}
}

UMOBowMeshComponent::UMOBowMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetVisibility(false);
	bEnableUpdateRateOptimizations = false;
	// The frame is picked every tick from where the hand is: pose even when the bow is off screen (a shadow, another view of the archer).
	VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UMOBowAnimInstance* UMOBowMeshComponent::BowInstance() const
{
	return Cast<UMOBowAnimInstance>(GetAnimInstance());
}

void UMOBowMeshComponent::SetBow(USkeletalMesh* Mesh, UAnimSequence* Draw, USkeletalMeshComponent* Body, bool bBowInLeftHand)
{
	bNockPending = bNocked = false;
	ReleaseAge = -1.0f;
	DrawClip = nullptr;
	if (!Mesh)
	{
		SetVisibility(false);
		SetSkeletalMeshAsset(nullptr);
		SetComponentTickEnabled(false);
		DrawingBody = nullptr;
		return;
	}

	SetSkeletalMeshAsset(Mesh);
	DrawingBody = Body;
	FingerA = bBowInLeftHand ? DrawFingerBoneA : OtherHandBone(DrawFingerBoneA);
	FingerB = bBowInLeftHand ? DrawFingerBoneB : OtherHandBone(DrawFingerBoneB);
	SetVisibility(true);
	SetAnimationMode(EAnimationMode::AnimationBlueprint);
	SetAnimInstanceClass(UMOBowAnimInstance::StaticClass());

	const int32 NockIndex = Mesh->GetRefSkeleton().FindBoneIndex(NockBone);
	UMOBowAnimInstance* Instance = BowInstance();
	if (!Draw || NockIndex == INDEX_NONE || !Instance)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBow] %s: %s has %s: held braced, the string will not follow the hand"), *GetNameSafe(GetOwner()),
			*Mesh->GetName(), !Draw ? TEXT("no draw clip") : !Instance ? TEXT("no bow anim instance")
			: *FString::Printf(TEXT("no bone '%s'"), *NockBone.ToString()));
		SetComponentTickEnabled(false);
		return;
	}
	Instance->Draw = Draw;
	Instance->NockBone = NockBone;
	Instance->NockAlpha = 0.0f;
	DrawClip = Draw;

	// The draw's line and range, from the clip itself: its nocking point at the first and last frame, brace in the reference pose.
	const FVector First = EvaluateNock(0.0f);
	const FVector Last = EvaluateNock(Draw->GetPlayLength());
	BraceNockCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), NockIndex).GetLocation();
	DrawAxis = (Last - First).GetSafeNormal();
	if (DrawAxis.IsNearlyZero())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOBow] %s: %s's draw does not move '%s' (first %s, last %s): held braced"), *GetNameSafe(GetOwner()),
			*Draw->GetName(), *NockBone.ToString(), *First.ToCompactString(), *Last.ToCompactString());
		DrawClip = nullptr;
		Instance->Draw = nullptr;
		SetComponentTickEnabled(false);
		return;
	}
	StartCm = FVector::DotProduct(First, DrawAxis);
	EndCm = FVector::DotProduct(Last, DrawAxis);
	BraceCm = FVector::DotProduct(BraceNockCS, DrawAxis);
	SetComponentTickEnabled(true);
	ShowDrawCm(BraceCm);
	UE_LOG(LogMOFramework, Log, TEXT("[MOBow] %s holds %s: the string runs %.1f .. %.1f cm along %s (brace %.1f), drawn by %s / %s"),
		*GetNameSafe(GetOwner()), *Mesh->GetName(), StartCm, EndCm, *DrawAxis.ToCompactString(), BraceCm, *FingerA.ToString(), *FingerB.ToString());
}

FVector UMOBowMeshComponent::EvaluateNock(float Seconds)
{
	if (UMOBowAnimInstance* Instance = BowInstance())
	{
		Instance->DrawTime = Seconds;
	}
	TickAnimation(0.0f, false);
	RefreshBoneTransforms();   // no tick function: evaluated here, on this thread
	return GetBoneLocation(NockBone, EBoneSpaces::ComponentSpace);
}

void UMOBowMeshComponent::Nock()
{
	if (DrawClip)
	{
		bNockPending = true;
		bNocked = false;
		ReleaseAge = -1.0f;
	}
}

void UMOBowMeshComponent::Release()
{
	bNockPending = false;
	if (!bNocked)
	{
		return;
	}
	bNocked = false;
	ReleasedAtCm = DrawCm;
	ReleaseAge = 0.0f;
	if (UMOBowAnimInstance* Instance = BowInstance())
	{
		Instance->NockAlpha = 0.0f;
	}
}

bool UMOBowMeshComponent::FingersCS(FVector& Out) const
{
	const USkeletalMeshComponent* Body = DrawingBody.Get();
	if (!Body || Body->GetBoneIndex(FingerA) == INDEX_NONE || Body->GetBoneIndex(FingerB) == INDEX_NONE)
	{
		return false;
	}
	Out = GetComponentTransform().InverseTransformPosition((Body->GetSocketLocation(FingerA) + Body->GetSocketLocation(FingerB)) * 0.5);
	return true;
}

float UMOBowMeshComponent::GetFingerDrawCm() const
{
	FVector Fingers;
	return FingersCS(Fingers) ? FVector::DotProduct(Fingers, DrawAxis) : -1.0f;
}

float UMOBowMeshComponent::GetNockToFingersCm() const
{
	FVector Fingers;
	if (!DrawClip || !FingersCS(Fingers))
	{
		return -1.0f;
	}
	return FVector::Dist(GetBoneLocation(NockBone, EBoneSpaces::ComponentSpace), Fingers) * GetComponentScale().GetMax();
}

void UMOBowMeshComponent::ShowDrawCm(float Cm)
{
	DrawCm = FMath::Clamp(Cm, StartCm, EndCm);
	if (UMOBowAnimInstance* Instance = BowInstance())
	{
		const float Alpha = (DrawCm - StartCm) / FMath::Max(EndCm - StartCm, 1.0e-3f);
		Instance->DrawTime = Alpha * DrawClip->GetPlayLength();
	}
}

void UMOBowMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Before Super: the pose this tick evaluates is the one for where the string is now. The body ticks first (attached: parent before child),
	// so its fingers are this frame's.
	UMOBowAnimInstance* Instance = BowInstance();
	if (DrawClip && Instance)
	{
		float Cm = BraceCm;
		FVector Fingers;
		const bool bFingers = FingersCS(Fingers);
		if (bNockPending && bFingers && FVector::Dist(Fingers, GetBoneLocation(NockBone, EBoneSpaces::ComponentSpace)) <= NockReachCm)
		{
			bNockPending = false;   // the fingers have the string
			bNocked = true;
			NockAge = 0.0f;
		}
		if (bNocked && bFingers)
		{
			NockAge += DeltaTime;
			Cm = FVector::DotProduct(Fingers, DrawAxis);
			Instance->NockTargetCS = Fingers;
			Instance->NockAlpha = FMath::Clamp(NockAge / FMath::Max(NockBlendSeconds, 1.0e-3f), 0.0f, 1.0f);
		}
		else if (ReleaseAge >= 0.0f)
		{
			ReleaseAge += DeltaTime;
			if (ReleaseAge < SnapSeconds)
			{
				Cm = BraceCm + (ReleasedAtCm - BraceCm) * FMath::Cos(HALF_PI * ReleaseAge / SnapSeconds);
			}
			else
			{
				// Past brace and back, dying away: as far forward as the draw was deep (a full draw swings to the clip's forward limit).
				const float T = ReleaseAge - SnapSeconds;
				const float Depth = FMath::Clamp((ReleasedAtCm - BraceCm) / FMath::Max(EndCm - BraceCm, 1.0f), 0.0f, 1.0f);
				Cm = BraceCm - (BraceCm - StartCm) * Depth * FMath::Exp(-T / VibrationDecaySeconds) * FMath::Sin(2.0f * PI * VibrationHz * T);
				if (T > 6.0f * VibrationDecaySeconds)
				{
					ReleaseAge = -1.0f;
					Cm = BraceCm;
				}
			}
		}
		ShowDrawCm(Cm);
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
