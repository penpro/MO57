#include "MOCreatureAnimInstance.h"
#include "MOCreature.h"
#include "MOFramework.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "BonePose.h"

const FName UMOCreatureAnimInstance::CueSlot(TEXT("DefaultSlot"));

FMOCreatureLocomotionBlend FMOCreatureLocomotionBlend::ForSpeed(float SpeedCm, float WalkCycleSpeedCm, float RunCycleSpeedCm, float MaxRunPlayRate)
{
	FMOCreatureLocomotionBlend Out;
	const float Walk = FMath::Max(WalkCycleSpeedCm, 1.0f);
	const float Run = FMath::Max(RunCycleSpeedCm, Walk + 1.0f);
	const float Speed = FMath::IsFinite(SpeedCm) ? FMath::Max(SpeedCm, 0.0f) : 0.0f;
	if (Speed < Walk)
	{
		Out.Walk = Speed / Walk;   // standing -> walking: the walk fades in at its own pace
		Out.Idle = 1.0f - Out.Walk;
	}
	else if (Speed < Run)
	{
		Out.Run = (Speed - Walk) / (Run - Walk);
		Out.Walk = 1.0f - Out.Run;
		Out.Idle = 0.0f;
	}
	else
	{
		Out.Idle = 0.0f;
		Out.Run = 1.0f;
		Out.RunPlayRate = FMath::Clamp(Speed / Run, 1.0f, FMath::Max(MaxRunPlayRate, 1.0f));
	}
	return Out;
}

void FMOCreatureAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const AMOCreature* Creature = Cast<AMOCreature>(InAnimInstance->GetOwningActor());
	if (!Creature)
	{
		IdleCycle = WalkCycle = RunCycle = nullptr;
		SpeedCm = 0.0f;
		return;
	}
	// A cycle authored for another skeleton would pose the bones wrongly: leave it out (and say so once).
	auto Usable = [this, Creature](const UAnimSequence* Sequence) -> const UAnimSequence*
	{
		if (!Sequence)
		{
			return nullptr;
		}
		if (!IsSkeletonCompatible(Sequence->GetSkeleton()))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCreatureAnim] %s: %s is for another skeleton; not played"), *Creature->GetName(), *Sequence->GetName());
			return nullptr;
		}
		return Sequence;
	};
	if (Sources[0] != Creature->IdleAnimation || Sources[1] != Creature->WalkAnimation || Sources[2] != Creature->RunAnimation)
	{
		Sources[0] = Creature->IdleAnimation;
		Sources[1] = Creature->WalkAnimation;
		Sources[2] = Creature->RunAnimation;
		IdleCycle = Usable(Creature->IdleAnimation);
		WalkCycle = Usable(Creature->WalkAnimation);
		RunCycle = Usable(Creature->RunAnimation);
	}
	WalkCycleSpeedCm = Creature->WalkAnimationSpeedCm;
	RunCycleSpeedCm = Creature->RunAnimationSpeedCm;
	MaxRunPlayRate = Creature->MaxRunPlayRate;
	SpeedCm = Creature->GetVelocity().Size2D();   // replicated movement on a client, so a joiner sees the same gait
}

void FMOCreatureAnimInstanceProxy::Update(float DeltaSeconds)
{
	// The cue slot, as FAnimNode_Slot keeps it: registered once per initialization, its weights read every update.
	if (!SlotRegistration.IsSynchronized_Counter(GetSlotNodeInitializationCounter()))
	{
		SlotRegistration.SynchronizeWith(GetSlotNodeInitializationCounter());
		RegisterSlotNodeWithAnimInstance(UMOCreatureAnimInstance::CueSlot);
	}
	GetSlotWeight(UMOCreatureAnimInstance::CueSlot, SlotNodeWeight, SlotSourceWeight, SlotTotalNodeWeight);
	UpdateSlotNodeWeight(UMOCreatureAnimInstance::CueSlot, SlotNodeWeight, 1.0f);

	Blend = FMOCreatureLocomotionBlend::ForSpeed(SpeedCm, WalkCycleSpeedCm, RunCycleSpeedCm, MaxRunPlayRate);
	if (IdleCycle)
	{
		IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, FMath::Max(static_cast<double>(IdleCycle->GetPlayLength()), UE_KINDA_SMALL_NUMBER));
	}
	// One stride clock for the walk and the run: its length is the blend of the two cycles' lengths (a blend space's sync group).
	const float WalkLength = WalkCycle ? WalkCycle->GetPlayLength() : 0.0f;
	const float RunLength = RunCycle ? RunCycle->GetPlayLength() / Blend.RunPlayRate : 0.0f;
	const float MovingWeight = Blend.Walk + Blend.Run;
	float StrideSeconds = 0.0f;
	if (MovingWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		StrideSeconds = (WalkLength * Blend.Walk + RunLength * Blend.Run) / MovingWeight;
	}
	if (StrideSeconds > UE_KINDA_SMALL_NUMBER)
	{
		StridePhase = FMath::Fmod(StridePhase + DeltaSeconds / StrideSeconds, 1.0f);
	}
}

namespace MOCreatureAnim
{
	static void SamplePose(const UAnimSequence* Sequence, double Time, FPoseContext& Output)
	{
		FAnimationPoseData PoseData(Output);
		Sequence->GetAnimationPose(PoseData, FAnimExtractContext(Time, false, FDeltaTimeRecord(), true));
	}
}

void FMOCreatureAnimInstanceProxy::EvaluateLocomotion(FPoseContext& Output) const
{
	// Up to two cycles show at once (standing <-> walking, walking <-> running); a missing one falls back to the nearest that exists.
	struct FPart { const UAnimSequence* Sequence; double Time; float Weight; };
	const FPart Parts[] = {
		{ IdleCycle ? IdleCycle : WalkCycle, IdleCycle ? IdleTime : 0.0, Blend.Idle },
		{ WalkCycle ? WalkCycle : RunCycle, (WalkCycle ? WalkCycle : RunCycle) ? StridePhase * (WalkCycle ? WalkCycle : RunCycle)->GetPlayLength() : 0.0, Blend.Walk },
		{ RunCycle ? RunCycle : WalkCycle, (RunCycle ? RunCycle : WalkCycle) ? StridePhase * (RunCycle ? RunCycle : WalkCycle)->GetPlayLength() : 0.0, Blend.Run },
	};
	const FPart* First = nullptr;
	const FPart* Second = nullptr;
	for (const FPart& Part : Parts)
	{
		if (!Part.Sequence || Part.Weight <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}
		(First ? Second : First) = &Part;
	}
	if (!First)
	{
		Output.ResetToRefPose();
		return;
	}
	if (!Second)
	{
		MOCreatureAnim::SamplePose(First->Sequence, First->Time, Output);
		return;
	}
	FPoseContext PoseA(Output);
	FPoseContext PoseB(Output);
	MOCreatureAnim::SamplePose(First->Sequence, First->Time, PoseA);
	MOCreatureAnim::SamplePose(Second->Sequence, Second->Time, PoseB);
	const FAnimationPoseData DataA(PoseA);
	const FAnimationPoseData DataB(PoseB);
	FAnimationPoseData OutData(Output);
	FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, First->Weight / (First->Weight + Second->Weight), OutData);
}

bool FMOCreatureAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		EvaluateLocomotion(Output);
		return true;
	}
	// A cue is playing: the locomotion is its source pose (what shows through where the cue blends in or out).
	FPoseContext Source(Output);
	if (SlotSourceWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		EvaluateLocomotion(Source);
	}
	else
	{
		Source.ResetToRefPose();
	}
	const FAnimationPoseData SourceData(Source);
	FAnimationPoseData OutData(Output);
	SlotEvaluatePose(UMOCreatureAnimInstance::CueSlot, SourceData, SlotSourceWeight, OutData, SlotNodeWeight, SlotTotalNodeWeight);
	return true;
}
