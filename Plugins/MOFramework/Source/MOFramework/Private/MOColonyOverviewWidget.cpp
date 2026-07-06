#include "MOColonyOverviewWidget.h"
#include "MOFramework.h"
#include "MOColonyManagerSubsystem.h"
#include "MOCommonButton.h"
#include "MOIdentityComponent.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOCraftingStationActor.h"
#include "MOContainerActor.h"
#include "MOSurvivorController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"

void UMOColonyOverviewWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Fully C++-built tree: root -> title + roster box + close button.
	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ColonyRoot"));
	WidgetTree->RootWidget = RootBox;

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ColonyTitle"));
	TitleText->SetText(FText::FromString(TEXT("Colony")));
	RootBox->AddChildToVerticalBox(TitleText);

	RosterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ColonyRoster"));
	RootBox->AddChildToVerticalBox(RosterBox);

	UMOCommonButton* CloseButton = CreateWidget<UMOCommonButton>(this, UMOCommonButton::StaticClass(), TEXT("ColonyClose"));
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &UMOColonyOverviewWidget::HandleClose);
		RootBox->AddChildToVerticalBox(CloseButton);
	}
}

void UMOColonyOverviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildRoster();
}

void UMOColonyOverviewWidget::RebuildRoster()
{
	if (!RosterBox)
	{
		return;
	}
	RosterBox->ClearChildren();
	RosterPawns.Reset();

	UMOColonyManagerSubsystem* Colony = UMOColonyManagerSubsystem::Get(this);
	if (!Colony)
	{
		return;
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("Colony — %s"),
			*Colony->GetSettlement().DisplayName.ToString())));
	}

	int32 Index = 0;
	for (APawn* Villager : Colony->GetColonyRoster())
	{
		const UMOIdentityComponent* Id = Villager->FindComponentByClass<UMOIdentityComponent>();
		const FGuid Guid = Id ? Id->GetGuid() : FGuid();

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			FName(*FString::Printf(TEXT("ColonyRow_%d"), Index)));

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("ColonyLabel_%d"), Index)));
		FString JobDesc = TEXT("Idle");
		if (const UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>())
		{
			const FMOSurvivorJobEntry Job = JobQueue->GetCurrentJob();
			if (Job.IsValid())
			{
				JobDesc = UEnum::GetDisplayValueAsText(Job.JobType).ToString();
			}
		}
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s  mood %.2f  %s  %s"),
			*(Id ? Id->DisplayName.ToString() : Villager->GetName()),
			Colony->GetVillagerMood(Guid),
			Colony->HasResidence(Guid) ? TEXT("housed") : TEXT("no home"),
			*JobDesc)));
		Row->AddChildToHorizontalBox(Label);

		// Stable harness-addressable name: ColonyAssignJob_<i>.
		UMOCommonButton* AssignButton = CreateWidget<UMOCommonButton>(this, UMOCommonButton::StaticClass(),
			FName(*FString::Printf(TEXT("ColonyAssignJob_%d"), Index)));
		if (AssignButton)
		{
			const int32 CapturedIndex = Index;
			AssignButton->OnClicked().RemoveAll(this);
			AssignButton->OnClicked().AddWeakLambda(this, [this, CapturedIndex]()
			{
				HandleAssignJob(CapturedIndex);
			});
			Row->AddChildToHorizontalBox(AssignButton);
		}

		RosterBox->AddChildToVerticalBox(Row);
		RosterPawns.Add(Villager);
		++Index;
	}

	UE_LOG(LogMOFramework, Warning, TEXT("[MOColonyUI] Overview roster rebuilt: %d villagers"), RosterPawns.Num());
}

void UMOColonyOverviewWidget::HandleAssignJob(int32 VillagerIndex)
{
	APawn* Villager = RosterPawns.IsValidIndex(VillagerIndex) ? RosterPawns[VillagerIndex].Get() : nullptr;
	UMOSurvivorJobQueueComponent* JobQueue = Villager ? Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>() : nullptr;
	UWorld* World = GetWorld();
	if (!JobQueue || !World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColonyUI] AssignJob %d: villager/queue missing"), VillagerIndex);
		return;
	}

	// Nearest station + storage to the villager — the V0 flow, UI-driven.
	auto Nearest = [Villager](auto It) -> AActor*
	{
		AActor* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		const FVector From = Villager->GetActorLocation();
		for (; It; ++It)
		{
			const float D = FVector::DistSquared(It->GetActorLocation(), From);
			if (IsValid(*It) && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		return Best;
	};
	AActor* Station = Nearest(TActorIterator<AMOCraftingStationActor>(World));
	AActor* Storage = Nearest(TActorIterator<AMOContainerActor>(World));
	if (!Station || !Storage)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColonyUI] AssignJob: no station/storage in world"));
		return;
	}

	const FGuid JobId = JobQueue->EnqueueCraftJob(DefaultCraftRecipeId, Station, Storage, 1);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColonyUI] AssignJob via UI: %s -> %s at %s (ok=%d)"),
		*Villager->GetName(), *DefaultCraftRecipeId.ToString(), *Station->GetName(), JobId.IsValid() ? 1 : 0);
	RebuildRoster();
}

void UMOColonyOverviewWidget::HandleClose()
{
	DeactivateWidget();
}
