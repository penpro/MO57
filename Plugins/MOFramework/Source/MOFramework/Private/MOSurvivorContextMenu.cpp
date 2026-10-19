#include "MOSurvivorContextMenu.h"
#include "MOSurvivorCommands.h"
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
	// DEBUG (MOSurvivorContextMenu, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Survivor %s has controller: %s (IsSurvivorController: %s, IsDead: %s)"),
	// 	*Survivor->GetName(),
	// 	ExistingController ? *ExistingController->GetClass()->GetName() : TEXT("None"),
	// 	SurvivorController ? TEXT("Yes") : TEXT("No"),
	// 	bSurvivorIsDead ? TEXT("Yes") : TEXT("No"));

	// A recruited survivor without its AI yet gets one (the shared EnsureSurvivorAIController: never from a player who drives the pawn).
	if (!bSurvivorIsDead && !SurvivorController && Survivor->HasAuthority())
	{
		if (UMORecruitmentComponent* RecruitComp = Survivor->FindComponentByClass<UMORecruitmentComponent>(); RecruitComp && RecruitComp->IsPossessable())
		{
			SurvivorController = RecruitComp->EnsureSurvivorAIController();
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
	// Orders go to the server (FMOSurvivorCommands): the survivor's AI exists only there, so a joiner's direct call did nothing.
	FMOSurvivorCommands::Submit(GetOwningPlayer(), TargetSurvivor.Get(), FMOSurvivorCommand::Make(EMOSurvivorCommandKind::FollowMe));
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Commanded survivor to follow player"));
	RequestClose();
}

void UMOSurvivorContextMenu::HandleStayHereClicked()
{
	FMOSurvivorCommands::Submit(GetOwningPlayer(), TargetSurvivor.Get(), FMOSurvivorCommand::Make(EMOSurvivorCommandKind::StayHere));
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Commanded survivor to stay where it stands"));
	RequestClose();
}

void UMOSurvivorContextMenu::HandleGoHomeClicked()
{
	FMOSurvivorCommands::Submit(GetOwningPlayer(), TargetSurvivor.Get(), FMOSurvivorCommand::Make(EMOSurvivorCommandKind::GoHome));
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Commanded survivor to go home"));
	RequestClose();
}

void UMOSurvivorContextMenu::HandleSetHomeClicked()
{
	FMOSurvivorCommands::Submit(GetOwningPlayer(), TargetSurvivor.Get(), FMOSurvivorCommand::Make(EMOSurvivorCommandKind::SetHome));
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorContextMenu] Set survivor home where it stands"));
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
