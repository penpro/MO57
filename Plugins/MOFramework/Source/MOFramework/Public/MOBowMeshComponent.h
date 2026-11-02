// Copyright MO57. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "MOBowMeshComponent.generated.h"

class UAnimSequence;
class UMOBowAnimInstance;

/**
 * A held bow (Nov 2 2026): its limbs bend and its string follows the hand that draws it.
 *
 * The bow is a skeletal mesh with one clip, its draw, in which the string's nocking point (NockBone) travels LINEARLY from its forward limit
 * (the first frame: the overshoot after a release) to full draw (the last), the bow braced in the reference pose -- Tools/blender_items_blender.py
 * builds them so, the limbs solved against a fixed string length. The clip is never played: every tick picks the frame for where the string
 * should be, and UMOBowAnimInstance builds the pose.
 *  - Nocked: once the drawing fingers (the midpoint of DrawFingerBoneA / B on the body) come within NockReachCm of the string, the string is in
 *    them: the limbs bend for the fingers' distance along the draw and the nocking point goes to the fingers themselves (blended in over
 *    NockBlendSeconds), so whichever clip draws the body draws the bow with it.
 *  - Released: the string snaps back to brace in SnapSeconds and swings past it, settling (VibrationHz, VibrationDecaySeconds), the swing as
 *    big as the draw it left.
 *  - Otherwise braced.
 * Cosmetic and local: each machine runs it from its own copy of the body. Ranged attacks (F53) call Nock / Release; MO.Test.BowShot does today.
 */
UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOBowMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UMOBowMeshComponent();

	/** Holds Mesh, drawn by Draw, with Body's hand drawing it (the right hand when the bow is in the left). A null Mesh hides the bow. */
	void SetBow(USkeletalMesh* Mesh, UAnimSequence* Draw, USkeletalMeshComponent* Body, bool bBowInLeftHand);

	/** The string goes into the drawing fingers as soon as they reach it, and follows them. */
	UFUNCTION(BlueprintCallable, Category="MO|Bow")
	void Nock();

	/** The string leaves the fingers: it snaps back to brace and swings to rest. */
	UFUNCTION(BlueprintCallable, Category="MO|Bow")
	void Release();

	/** The string is in the fingers (not just waiting for them). */
	UFUNCTION(BlueprintPure, Category="MO|Bow")
	bool IsNocked() const { return bNocked; }

	/** The string's nocking point now, measured along the draw from the grip (cm): brace ~16, full draw ~65. */
	UFUNCTION(BlueprintPure, Category="MO|Bow")
	float GetDrawCm() const { return DrawCm; }

	/** Where the drawing fingers are, measured the same way (cm); < 0 when no body draws this bow. */
	UFUNCTION(BlueprintPure, Category="MO|Bow")
	float GetFingerDrawCm() const;

	/** How far the string's nocking point is from the drawing fingers (cm), from the pose last evaluated; < 0 without a body or a draw. */
	UFUNCTION(BlueprintPure, Category="MO|Bow")
	float GetNockToFingersCm() const;

	/** The draw clip's range along the draw (cm): its first frame, brace (the reference pose) and its last frame. */
	UFUNCTION(BlueprintPure, Category="MO|Bow")
	FVector GetDrawRangeCm() const { return FVector(StartCm, BraceCm, EndCm); }

	/** The draw's direction (bow space: from the forward limit toward full draw), and whether the bow has one (a draw clip that moves the nock). */
	FVector GetDrawAxis() const { return DrawAxis; }
	bool HasDraw() const { return DrawClip != nullptr; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	FName NockBone = TEXT("string_nock");

	/** The drawing hand's two finger bones; the string lies between them (right hand; the _l pair is used when the bow is in the right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	FName DrawFingerBoneA = TEXT("index_02_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	FName DrawFingerBoneB = TEXT("middle_02_r");

	/** Nocking: the fingers take the string once within this of it, and it goes over to them in this long. Sparrow's draw brings the fingers
	 *  4.6 cm from the string on Femme, 10.2 on Trey and 16.0 on Harvey (longer forearms; Nov 2): 9 never nocked the men's bows. Until the
	 *  drawing hand is brought to the string (Docs/Held_Grip.md section 7), the string goes over to a bigger body's fingers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	float NockReachCm = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	float NockBlendSeconds = 0.15f;

	/** The release: back to brace in this long, then a swing past it at VibrationHz dying away over VibrationDecaySeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	float SnapSeconds = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	float VibrationHz = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Bow")
	float VibrationDecaySeconds = 0.09f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UMOBowAnimInstance* BowInstance() const;
	/** The nocking point (component space) in the clip at Seconds, by evaluating this component's pose here. */
	FVector EvaluateNock(float Seconds);
	void ShowDrawCm(float Cm);
	bool FingersCS(FVector& Out) const;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> DrawClip;

	TWeakObjectPtr<USkeletalMeshComponent> DrawingBody;
	FName FingerA, FingerB;

	FVector DrawAxis = FVector(0.0, 1.0, 0.0);   // unit, bow space: from the forward limit toward full draw
	FVector BraceNockCS = FVector::ZeroVector;
	float StartCm = 0.0f;
	float BraceCm = 0.0f;
	float EndCm = 1.0f;
	float DrawCm = 0.0f;

	bool bNockPending = false;   // waiting for the fingers to reach the string
	bool bNocked = false;
	float NockAge = 0.0f;
	float ReleasedAtCm = 0.0f;
	float ReleaseAge = -1.0f;   // < 0: not swinging
};
