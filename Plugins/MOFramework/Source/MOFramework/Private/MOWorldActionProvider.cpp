#include "MOWorldActionProvider.h"
#include "MOFramework.h"
#include "MOPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

bool FMOWorldActions::Submit(APlayerController* PC, AActor* Target, FName ActionId)
{
	if (!IsValid(PC) || !IsValid(Target))
	{
		return false;
	}
	if (PC->HasAuthority())
	{
		return Execute(PC, Target, ActionId);
	}
	if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC))
	{
		MOPC->ServerWorldAction(Target, ActionId);
		return true;   // the result arrives by replication (the pawn's timed action, the target's state)
	}
	return false;
}

bool FMOWorldActions::Execute(APlayerController* PC, AActor* Target, FName ActionId)
{
	APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
	IMOWorldActionProvider* Provider = Cast<IMOWorldActionProvider>(Target);
	if (!Pawn || !PC->HasAuthority() || !Provider)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldAction] %s on %s refused: no pawn, no authority or not a world-action target"), *ActionId.ToString(), *GetNameSafe(Target));
		return false;
	}
	FVector Origin = Target->GetActorLocation();
	FVector Extent = FVector::ZeroVector;
	Target->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
	const float Gap = FMath::Max(0.0f, (float)FVector::Dist(Pawn->GetActorLocation(), Origin) - (float)Extent.Size());
	if (Gap > ReachCm)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldAction] %s on %s refused for %s: %.0f cm out of reach"), *ActionId.ToString(), *Target->GetName(), *PC->GetName(), Gap);
		return false;
	}
	const bool bOk = Provider->PerformWorldAction(Pawn, ActionId);
	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldAction] %s on %s by %s: %s"), *ActionId.ToString(), *Target->GetName(), *PC->GetName(), bOk ? TEXT("begun") : TEXT("refused by the target"));
	return bOk;
}
