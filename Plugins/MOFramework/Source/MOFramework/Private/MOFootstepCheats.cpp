// Dev verbs for the footstep system. Output lines start with [MOQUERY] FOOTSTEP.
//
//   MO.Footstep.Probe            what the ground under the local pawn sounds like: the set, why, and whether the sound bank has its rows
//   MO.Footstep.Log <0|1>        log every step (set, gait, source: animation plant / stride fallback / landing)
//   MO.Footstep.Heights [s=4] [full]   log both feet's height above the floor and world velocity 10 times a second, or every frame with `full` (tune the plant detector against an animation)
//   MO.Footstep.Count            steps played so far by the local pawn
//   MO.Footstep.Walk [s=5] [walk|sprint|sneak]   hold forward movement (the camera's heading) for a few seconds: the held key, for tests

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MOAudioSubsystem.h"
#include "MOCharacter.h"
#include "MOFootstepComponent.h"
#include "MOControllableInterface.h"
#include "MOFramework.h"

namespace
{
	UMOFootstepComponent* LocalFootsteps(UWorld* World, AMOCharacter** OutPawn = nullptr)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AMOCharacter* Pawn = PC ? Cast<AMOCharacter>(PC->GetPawn()) : nullptr;
		if (OutPawn)
		{
			*OutPawn = Pawn;
		}
		return Pawn ? Pawn->FootstepComponent.Get() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GProbeCmd(
		TEXT("MO.Footstep.Probe"),
		TEXT("Dev: what the ground under the local pawn sounds like (set, reason, bank rows present)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = nullptr;
			UMOFootstepComponent* Steps = LocalFootsteps(World, &Pawn);
			if (!Steps)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Probe FAILED: no local MO pawn with a footstep component"));
				return;
			}
			const UCharacterMovementComponent* Move = Pawn->GetCharacterMovement();
			const FHitResult Floor = Move ? FHitResult(Move->CurrentFloor.HitResult) : FHitResult();
			const FVector Foot = Pawn->GetActorLocation() - FVector(0.0f, 0.0f, Pawn->GetSimpleCollisionHalfHeight());
			FString Why;
			const FName Set = Steps->ResolveSet(Floor, Foot, &Why);
			const UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(Pawn);
			const bool bBank1 = Audio && Audio->HasAudioId(FMOFootstepMath::SoundIdFor(Set, 0));
			const bool bBank2 = Audio && Audio->HasAudioId(FMOFootstepMath::SoundIdFor(Set, 1));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Probe pos=(%.0f, %.0f, %.0f) floorActor=%s grounded=%d crouched=%d canCrouch=%d speedMax=%.0f set=%s (%s) bank=%d/%d steps=%d"),
				Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z, *GetNameSafe(Floor.GetActor()),
				Move && Move->IsMovingOnGround() ? 1 : 0, Pawn->bIsCrouched ? 1 : 0, Move && Move->CanEverCrouch() ? 1 : 0, Move ? Move->GetMaxSpeed() : 0.0f,
				*Set.ToString(), *Why, bBank1 ? 1 : 0, bBank2 ? 1 : 0, Steps->GetStepCount());
		}));

	FAutoConsoleCommandWithWorldAndArgs GLogCmd(
		TEXT("MO.Footstep.Log"),
		TEXT("Dev: log every footstep of the local pawn. Usage: MO.Footstep.Log <0|1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOFootstepComponent* Steps = LocalFootsteps(World))
			{
				const bool bOn = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
				Steps->SetDebugLog(bOn);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Log %s"), bOn ? TEXT("on") : TEXT("off"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GLogAllCmd(
		TEXT("MO.Footstep.LogAll"),
		TEXT("Dev: log every footstep of every pawn in this world (creatures too). Usage: MO.Footstep.LogAll <0|1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bOn = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			int32 Count = 0;
			for (TObjectIterator<UMOFootstepComponent> It; It; ++It)
			{
				if (It->GetWorld() == World)
				{
					It->SetDebugLog(bOn);
					++Count;
				}
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP LogAll %s (%d pawns)"), bOn ? TEXT("on") : TEXT("off"), Count);
		}));

	FAutoConsoleCommandWithWorldAndArgs GHeightsCmd(
		TEXT("MO.Footstep.Heights"),
		TEXT("Dev: log the feet's height above the floor and world velocity (10x/s, or every frame with `full`). Usage: MO.Footstep.Heights [seconds=4] [full]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOFootstepComponent* Steps = LocalFootsteps(World))
			{
				const float Seconds = Args.Num() > 0 && Args[0].IsNumeric() ? FCString::Atof(*Args[0]) : 4.0f;
				const bool bEveryFrame = Args.Num() > 1 && Args[1].Equals(TEXT("full"), ESearchCase::IgnoreCase);
				Steps->LogHeightsFor(Seconds, bEveryFrame);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Heights for %.1f s"), Seconds);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GCountCmd(
		TEXT("MO.Footstep.Count"),
		TEXT("Dev: the number of footsteps the local pawn has played."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOFootstepComponent* Steps = LocalFootsteps(World))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Count %d"), Steps->GetStepCount());
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GWalkCmd(
		TEXT("MO.Footstep.Walk"),
		TEXT("Dev: hold forward movement along the camera heading. Usage: MO.Footstep.Walk [seconds=5] [walk|sprint|sneak]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalFootsteps(World) ? Cast<AMOCharacter>(World->GetFirstPlayerController()->GetPawn()) : nullptr;
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Walk FAILED: no local MO pawn"));
				return;
			}
			double Seconds = 5.0;
			bool bSprint = false;
			bool bSneak = false;
			for (const FString& A : Args)
			{
				if (A.IsNumeric())
				{
					Seconds = FMath::Clamp((double)FCString::Atof(*A), 0.5, 120.0);
				}
				else if (A.Equals(TEXT("sprint"), ESearchCase::IgnoreCase))
				{
					bSprint = true;
				}
				else if (A.Equals(TEXT("sneak"), ESearchCase::IgnoreCase))
				{
					bSneak = true;
				}
			}
			if (bSprint)
			{
				IMOControllableInterface::Execute_RequestSprintStart(Pawn);
			}
			if (bSneak)
			{
				IMOControllableInterface::Execute_RequestCrouchToggle(Pawn);
			}
			const double EndTime = FPlatformTime::Seconds() + Seconds;
			TWeakObjectPtr<AMOCharacter> Weak(Pawn);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, EndTime, bSprint, bSneak](float)
			{
				AMOCharacter* P = Weak.Get();
				if (!P)
				{
					return false;
				}
				if (FPlatformTime::Seconds() >= EndTime)
				{
					if (bSprint)
					{
						IMOControllableInterface::Execute_RequestSprintEnd(P);
					}
					if (bSneak)
					{
						IMOControllableInterface::Execute_RequestCrouchToggle(P);
					}
					return false;
				}
				if (P->GetController())
				{
					const FRotator Heading(0.0f, P->GetController()->GetControlRotation().Yaw, 0.0f);
					P->AddMovementInput(Heading.Vector(), 1.0f);
				}
				return true;
			}), 0.0f);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FOOTSTEP Walk for %.1f s (%s)"), Seconds, bSprint ? TEXT("sprint") : bSneak ? TEXT("sneak") : TEXT("walk"));
		}));

}

#endif // !UE_BUILD_SHIPPING
