// Play-in-editor camera verbs.
//
//   MO.Player.ToCamera [stay]   While ejected (F8) and flying around: put the player's pawn where the fly camera is (eyes at the camera, facing the way the
//                               camera faces) and resume play, so the game continues from the spot you flew to. `stay` only moves the pawn; you then press F8
//                               (Possess) yourself.
//   MO.PIE.Toggle               The F8 eject / possess toggle as a console verb (what the Possess/Eject button does), for scripted runs.
//
// Why a verb: the stock Possess returns to the pawn WHERE IT WAS when you ejected. The engine only places a player at the camera when the session started as Simulate.

#include "CoreMinimal.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "LevelEditorViewport.h"
#include "MOFramework.h"

namespace
{
	/** The level viewport that is currently the simulate (ejected) camera, or null. */
	FLevelEditorViewportClient* FindEjectedViewportClient()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
		{
			if (Client && Client->IsSimulateInEditorViewport())
			{
				return Client;
			}
		}
		return nullptr;
	}

	void MovePawnToCamera(const TArray<FString>& Args)
	{
		if (!GEditor || !GEditor->PlayWorld)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPIE] ToCamera: no play session. Start PIE, press F8 to eject, fly, then run this."));
			return;
		}
		if (!GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPIE] ToCamera: you are not ejected. Press F8 first, fly to where you want to play from, then run this."));
			return;
		}
		FLevelEditorViewportClient* Client = FindEjectedViewportClient();
		if (!Client)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPIE] ToCamera: no ejected viewport found."));
			return;
		}

		const FVector CameraLocation = Client->GetViewLocation();
		FRotator CameraRotation = Client->GetViewRotation();
		CameraRotation.Roll = 0.0f;

		APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPIE] ToCamera: the player controller has no pawn to move (%s)."), PC ? TEXT("not possessing") : TEXT("no controller"));
			return;
		}

		// Eyes at the camera: BaseEyeHeight is measured up from the actor location.
		const FVector Destination = CameraLocation - FVector(0.0f, 0.0f, Pawn->BaseEyeHeight);
		const FVector Before = Pawn->GetActorLocation();
		// TeleportTo (not SetActorLocation) so a camera inside terrain or a wall is nudged to the nearest free spot instead of embedding the capsule.
		bool bPlaced = Pawn->TeleportTo(Destination, FRotator(0.0f, CameraRotation.Yaw, 0.0f), /*bIsATest=*/false, /*bNoCheck=*/false);
		if (!bPlaced)
		{
			bPlaced = Pawn->SetActorLocationAndRotation(Destination + FVector(0.0f, 0.0f, 100.0f), FRotator(0.0f, CameraRotation.Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		PC->SetControlRotation(CameraRotation);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPIE] ToCamera: %s moved %s -> %s (camera %s, eye height %.0f)%s"),
			*Pawn->GetName(), *Before.ToCompactString(), *Pawn->GetActorLocation().ToCompactString(), *CameraLocation.ToCompactString(), Pawn->BaseEyeHeight,
			bPlaced ? TEXT("") : TEXT(" [placement refused, forced]"));

		const bool bStay = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("stay"), ESearchCase::IgnoreCase); });
		if (!bStay)
		{
			GEditor->RequestToggleBetweenPIEandSIE();   // the F8 Possess: back into the pawn
		}
	}

	FAutoConsoleCommand GToCameraCmd(
		TEXT("MO.Player.ToCamera"),
		TEXT("While ejected (F8) in PIE: move the player's pawn to the fly camera (eyes at the camera, same facing) and resume play. Add `stay` to only move the pawn."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&MovePawnToCamera));

	FAutoConsoleCommand GToggleCmd(
		TEXT("MO.PIE.Toggle"),
		TEXT("The F8 eject / possess toggle for the running play-in-editor session."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (GEditor && GEditor->PlayWorld)
			{
				GEditor->RequestToggleBetweenPIEandSIE();
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPIE] Toggle: no play session."));
			}
		}));
}
