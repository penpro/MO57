/**
 * =============================================================================
 * MOAnimNotify_MeleeHit.h - Melee Attack Hit Detection Notify
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Animation notify that triggers hit detection at the exact animation frame
 * when the attack should land. Place this notify in attack montages at the
 * moment the weapon/fist reaches peak attack position.
 *
 * USAGE:
 * 1. Open attack animation montage in UE Editor
 * 2. Add notify track if not present
 * 3. Right-click and select "Add Notify" -> "MO Melee Hit"
 * 4. Position notify at the frame where the attack should connect
 *
 * WORKFLOW:
 * Animation plays -> Notify fires -> CombatComponent::ExecuteAttack() called
 * -> Sphere trace performed -> Hits processed
 *
 * RELATED FILES:
 * - MOCombatComponent.h - ExecuteAttack() performs hit detection
 * - MOWeaponTypes.h - Attack damage profiles
 *
 * LAST UPDATED: 2026-02-26
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MOAnimNotify_MeleeHit.generated.h"

/**
 * Animation notify that triggers melee hit detection.
 * Add this to attack montages at the moment the attack should connect.
 */
UCLASS(DisplayName = "MO Melee Hit")
class MOFRAMEWORK_API UMOAnimNotify_MeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UMOAnimNotify_MeleeHit();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual FLinearColor GetEditorColor() override { return FLinearColor::Red; }
#endif
};
