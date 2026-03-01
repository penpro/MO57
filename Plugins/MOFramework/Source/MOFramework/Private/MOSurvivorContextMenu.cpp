#include "MOSurvivorContextMenu.h"
#include "MOFramework.h"
#include "MOSurvivorController.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOIdentityComponent.h"
#include "MORecruitmentComponent.h"
#include "MOCharacter.h"
#include "AIController.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UMOSurvivorContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click handlers
	BindButtonClick(FollowMeButton, this, &UMOSurvivorContextMenu::HandleFollowMeClicked);
	BindButtonClick(StayHereButton, this, &UMOSurvivorContextMenu::HandleStayHereClicked);
	BindButtonClick(GoHomeButton, this, &UMOSurvivorContextMenu::HandleGoHomeClicked);
	BindButtonClick(SetHomeButton, this, &UMOSurvivorContextMenu::HandleSetHomeClicked);
	BindButtonClick(OpenTasksButton, this, &UMOSurvivorContextMenu::HandleOpenTasksClicked);
	BindButtonClick(InventoryButton, this, &UMOSurvivorContextMenu::HandleInventoryClicked);
}

void UMOSurvivorContextMenu::NativeDestruct()
{
	// Clear references
	TargetSurvivor.Reset();
	CachedController.Reset();
	CachedJobQueue.Reset();

	Super::NativeDestruct();
}

void UMOSurvivorContextMenu::InitializeForSurvivor(APawn* Survivor, FVector2D ScreenPosition)
{
	if (!IsValid(Survivor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorContextMenu] InitializeForSurvivor called with invalid survivor"));
		return;
	}

	TargetSurvivor = Survivor;

	// Check if survivor is dead
	bSurvivorIsDead = false;
	if (AMOCharacter* MOChar = Cast<AMOCharacter>(Survivor))
	{
		bSurvivorIsDead = MOChar->IsDead();
	}

	// Cache controller - check if it's the right type
	AController* ExistingController = Survivor->GetController();
	AMOSurvivorController* SurvivorController = Cast<AMOSurvivorController>(ExistingController);

	// Log what controller exists
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Survivor %s has controller: %s (IsSurvivorController: %s, IsDead: %s)"),
		*Survivor->GetName(),
		ExistingController ? *ExistingController->GetClass()->GetName() : TEXT("None"),
		SurvivorController ? TEXT("Yes") : TEXT("No"),
		bSurvivorIsDead ? TEXT("Yes") : TEXT("No"));

	// If not dead and no survivor controller exists, spawn one on-demand for recruited survivors
	if (!bSurvivorIsDead && !SurvivorController && Survivor->HasAuthority())
	{
		UMORecruitmentComponent* RecruitComp = Survivor->FindComponentByClass<UMORecruitmentComponent>();
		if (RecruitComp && RecruitComp->IsPossessable())
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Spawning survivor controller on-demand for %s"), *Survivor->GetName());

			// Unpossess from any existing controller
			if (ExistingController)
			{
				ExistingController->UnPossess();
			}

			// Get the AI controller class from the pawn - must be a survivor controller or subclass
			TSubclassOf<AController> AIControllerClass = Survivor->AIControllerClass;

			// Log what class is set on the pawn
			UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Pawn AIControllerClass: %s"),
				AIControllerClass ? *AIControllerClass->GetName() : TEXT("None"));

			// Ensure we use a survivor controller class - fallback if not set or wrong type
			if (!AIControllerClass || !AIControllerClass->IsChildOf(AMOSurvivorController::StaticClass()))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Using default AMOSurvivorController (pawn class was not a survivor controller)"));
				AIControllerClass = AMOSurvivorController::StaticClass();
			}

			// Spawn the new AI controller
			UWorld* World = Survivor->GetWorld();
			if (World)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				AAIController* NewController = World->SpawnActor<AAIController>(AIControllerClass, Survivor->GetActorLocation(), Survivor->GetActorRotation(), SpawnParams);
				if (NewController)
				{
					NewController->Possess(Survivor);
					SurvivorController = Cast<AMOSurvivorController>(NewController);
					UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Successfully spawned controller: %s"), *NewController->GetName());
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorContextMenu] Failed to spawn AI controller"));
				}
			}
		}
	}

	CachedController = SurvivorController;
	CachedJobQueue = Survivor->FindComponentByClass<UMOSurvivorJobQueueComponent>();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Initialized for survivor %s (Controller: %s, JobQueue: %s)"),
		*Survivor->GetName(),
		CachedController.IsValid() ? TEXT("Yes") : TEXT("No"),
		CachedJobQueue.IsValid() ? TEXT("Yes") : TEXT("No"));

	// Update UI
	UpdateDisplayTexts();
	UpdateButtonStates();

	// Position the menu
	SetPopupPosition(ScreenPosition);
}

void UMOSurvivorContextMenu::HandleFollowMeClicked()
{
	AMOSurvivorController* Controller = GetSurvivorController();
	if (!Controller)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorContextMenu] No survivor controller for Follow Me"));
		RequestClose();
		return;
	}

	// Get the player's pawn as the follow target
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		RequestClose();
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn)
	{
		RequestClose();
		return;
	}

	Controller->SetFollowTarget(PlayerPawn);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Commanded survivor to follow player"));
	RequestClose();
}

void UMOSurvivorContextMenu::HandleStayHereClicked()
{
	AMOSurvivorController* Controller = GetSurvivorController();
	APawn* Survivor = TargetSurvivor.Get();
	if (!Controller || !Survivor)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorContextMenu] No controller or survivor for Stay Here"));
		RequestClose();
		return;
	}

	// Stay at current location
	FVector CurrentLocation = Survivor->GetActorLocation();
	Controller->SetStayAtLocation(CurrentLocation);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Commanded survivor to stay at %s"), *CurrentLocation.ToString());
	RequestClose();
}

void UMOSurvivorContextMenu::HandleGoHomeClicked()
{
	AMOSurvivorController* Controller = GetSurvivorController();
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!Controller || !JobQueue || !JobQueue->HasHome())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorContextMenu] Cannot go home - no controller, job queue, or home assigned"));
		RequestClose();
		return;
	}

	Controller->GoToHome();

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Commanded survivor to go home"));
	RequestClose();
}

void UMOSurvivorContextMenu::HandleSetHomeClicked()
{
	APawn* Survivor = TargetSurvivor.Get();
	UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
	if (!Survivor || !JobQueue)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorContextMenu] Cannot set home - no survivor or job queue"));
		RequestClose();
		return;
	}

	// Set current location as home
	FVector CurrentLocation = Survivor->GetActorLocation();
	JobQueue->SetHome(CurrentLocation);

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Set survivor home to %s"), *CurrentLocation.ToString());

	// Update button states since we now have a home
	UpdateButtonStates();
	RequestClose();
}

void UMOSurvivorContextMenu::HandleOpenTasksClicked()
{
	APawn* Survivor = TargetSurvivor.Get();
	if (!Survivor)
	{
		RequestClose();
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Opening task menu for survivor"));

	// Broadcast to UI controller to open task menu
	OnOpenTasksRequested.Broadcast(Survivor);

	// Close this menu (task menu will open)
	RequestClose();
}

void UMOSurvivorContextMenu::HandleInventoryClicked()
{
	APawn* Survivor = TargetSurvivor.Get();
	if (!Survivor)
	{
		RequestClose();
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Opening inventory for survivor"));

	// Broadcast to UI controller to open inventory exchange
	OnInventoryRequested.Broadcast(Survivor);

	// Close this menu (inventory will open)
	RequestClose();
}

void UMOSurvivorContextMenu::UpdateButtonStates()
{
	// If survivor is dead, disable everything except Inventory
	if (bSurvivorIsDead)
	{
		if (FollowMeButton)
		{
			FollowMeButton->SetIsEnabled(false);
		}
		if (StayHereButton)
		{
			StayHereButton->SetIsEnabled(false);
		}
		if (GoHomeButton)
		{
			GoHomeButton->SetIsEnabled(false);
		}
		if (SetHomeButton)
		{
			SetHomeButton->SetIsEnabled(false);
		}
		if (OpenTasksButton)
		{
			OpenTasksButton->SetIsEnabled(false);
		}
		if (InventoryButton)
		{
			InventoryButton->SetIsEnabled(true);
		}
		return;
	}

	// Normal state - enable command buttons
	if (FollowMeButton)
	{
		FollowMeButton->SetIsEnabled(true);
	}
	if (StayHereButton)
	{
		StayHereButton->SetIsEnabled(true);
	}
	if (SetHomeButton)
	{
		SetHomeButton->SetIsEnabled(true);
	}
	if (OpenTasksButton)
	{
		OpenTasksButton->SetIsEnabled(true);
	}
	if (InventoryButton)
	{
		InventoryButton->SetIsEnabled(true);
	}

	// Go Home button is only enabled if survivor has a home
	if (GoHomeButton)
	{
		UMOSurvivorJobQueueComponent* JobQueue = GetJobQueue();
		const bool bHasHome = JobQueue && JobQueue->HasHome();
		GoHomeButton->SetIsEnabled(bHasHome);
	}

	// Update visual state based on current command
	AMOSurvivorController* Controller = GetSurvivorController();
	if (Controller)
	{
		// Could add visual indicators for active commands here
		// e.g., highlight the active command button
	}
}

void UMOSurvivorContextMenu::UpdateDisplayTexts()
{
	APawn* Survivor = TargetSurvivor.Get();
	if (!Survivor)
	{
		return;
	}

	// Update survivor name
	if (SurvivorNameText)
	{
		FText SurvivorName = FText::FromString(TEXT("Survivor"));

		// Try to get display name from identity component
		if (UMOIdentityComponent* IdentityComp = Survivor->FindComponentByClass<UMOIdentityComponent>())
		{
			if (!IdentityComp->DisplayName.IsEmpty())
			{
				SurvivorName = IdentityComp->DisplayName;
			}
		}

		// Append "(Dead)" if dead
		if (bSurvivorIsDead)
		{
			SurvivorName = FText::Format(NSLOCTEXT("MOSurvivorContextMenu", "DeadSurvivorFormat", "{0} (Dead)"), SurvivorName);
		}

		SurvivorNameText->SetText(SurvivorName);
	}

	// Update current task text
	if (CurrentTaskText)
	{
		FText TaskText;

		if (bSurvivorIsDead)
		{
			TaskText = FText::FromString(TEXT("Dead"));
		}
		else
		{
			TaskText = FText::FromString(TEXT("Idle"));

			AMOSurvivorController* Controller = GetSurvivorController();
			if (Controller)
			{
				FName CommandName = Controller->GetActiveCommandName();
				if (!CommandName.IsNone())
				{
					TaskText = FText::FromName(CommandName);
				}
			}
		}

		CurrentTaskText->SetText(TaskText);
	}
}

AMOSurvivorController* UMOSurvivorContextMenu::GetSurvivorController() const
{
	if (CachedController.IsValid())
	{
		return CachedController.Get();
	}

	APawn* Survivor = TargetSurvivor.Get();
	if (Survivor)
	{
		return Cast<AMOSurvivorController>(Survivor->GetController());
	}

	return nullptr;
}

UMOSurvivorJobQueueComponent* UMOSurvivorContextMenu::GetJobQueue() const
{
	if (CachedJobQueue.IsValid())
	{
		return CachedJobQueue.Get();
	}

	APawn* Survivor = TargetSurvivor.Get();
	if (Survivor)
	{
		return Survivor->FindComponentByClass<UMOSurvivorJobQueueComponent>();
	}

	return nullptr;
}
