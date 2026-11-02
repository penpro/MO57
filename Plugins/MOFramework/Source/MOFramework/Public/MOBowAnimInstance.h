// Copyright MO57. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "MOBowAnimInstance.generated.h"

class UAnimSequence;

/**
 * A held bow's pose (UMOBowMeshComponent): its draw clip at the time the component picked -- the limbs bent for the draw --, then the string's
 * nocking point moved toward NockTargetCS by NockAlpha (the drawing fingers, which do not keep to the bow's plane: Sparrow's draw brings the
 * hand in 7-10 cm off it). The string is skinned straight between its end bones and the nocking point, so it stays two straight runs.
 * The inputs are copied on the game thread (PreUpdate); the pose is built on a worker.
 */
USTRUCT()
struct MOFRAMEWORK_API FMOBowAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FMOBowAnimInstanceProxy() = default;
	explicit FMOBowAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	const UAnimSequence* Draw = nullptr;   // held by the instance
	double DrawTime = 0.0;
	FName NockBone;
	float NockAlpha = 0.0f;
	FVector NockTargetCS = FVector::ZeroVector;
};

UCLASS(Transient, NotBlueprintable)
class MOFRAMEWORK_API UMOBowAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The bow's draw clip and the time in it to show. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Draw;

	double DrawTime = 0.0;
	FName NockBone = TEXT("string_nock");

	/** 0: the nocking point where the clip puts it; 1: at NockTargetCS (component space). */
	float NockAlpha = 0.0f;
	FVector NockTargetCS = FVector::ZeroVector;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FMOBowAnimInstanceProxy(this); }
};
