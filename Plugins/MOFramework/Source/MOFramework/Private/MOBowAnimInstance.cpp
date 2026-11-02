// Copyright MO57. All Rights Reserved.

#include "MOBowAnimInstance.h"

#include "Animation/AnimSequence.h"

void FMOBowAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UMOBowAnimInstance* Bow = CastChecked<UMOBowAnimInstance>(InAnimInstance);
	Draw = Bow->Draw;
	DrawTime = Bow->DrawTime;
	NockBone = Bow->NockBone;
	NockAlpha = Bow->NockAlpha;
	NockTargetCS = Bow->NockTargetCS;
}

bool FMOBowAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!Draw)
	{
		Output.ResetToRefPose();
		return true;
	}
	FAnimationPoseData PoseData(Output);
	Draw->GetAnimationPose(PoseData, FAnimExtractContext(DrawTime, false, FDeltaTimeRecord(), false));
	if (NockAlpha <= 0.0f)
	{
		return true;
	}
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(NockBone);
	if (MeshIndex == INDEX_NONE)
	{
		return true;
	}
	const FCompactPoseBoneIndex Nock = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	if (!Nock.IsValid())
	{
		return true;
	}
	FTransform ParentCS = FTransform::Identity;   // the nock's parent chain to the root, in component space
	for (FCompactPoseBoneIndex B = Bones.GetParentBoneIndex(Nock); B.IsValid(); B = Bones.GetParentBoneIndex(B))
	{
		ParentCS = ParentCS * Output.Pose[B];
	}
	const FVector Animated = ParentCS.TransformPosition(Output.Pose[Nock].GetTranslation());
	const FVector Wanted = FMath::Lerp(Animated, NockTargetCS, FMath::Clamp(NockAlpha, 0.0f, 1.0f));
	Output.Pose[Nock].SetTranslation(ParentCS.InverseTransformPosition(Wanted));
	return true;
}
