#include "MOAnimNotify_MeleeHit.h"
#include "MOFramework.h"
#include "MOCombatComponent.h"

UMOAnimNotify_MeleeHit::UMOAnimNotify_MeleeHit()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Red;
#endif
}

void UMOAnimNotify_MeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	UMOCombatComponent* CombatComp = Owner->FindComponentByClass<UMOCombatComponent>();
	if (!CombatComp)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOAnimNotify_MeleeHit] No CombatComponent found on %s"), *Owner->GetName());
		return;
	}

	// Only execute if we're in the attacking state (wind-up complete)
	// This handles cases where animation continues but combat state has changed
	if (CombatComp->IsAttacking())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOAnimNotify_MeleeHit] Triggering hit detection for %s"), *Owner->GetName());
		// Note: ExecuteAttack is called by the combat component's state machine timing
		// This notify can be used for additional hit detection windows if needed
		// For now, the state machine handles ExecuteAttack timing
	}
}

FString UMOAnimNotify_MeleeHit::GetNotifyName_Implementation() const
{
	return TEXT("MO Melee Hit");
}
