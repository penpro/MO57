#include "MOInspectionActions.h"
#include "MOWorldCreationTypes.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOWorldItem.h"
#include "MOHarvestSubsystem.h"
#include "MOInteractionSubsystem.h"
#include "MOTimedActionComponent.h"
#include "MOPlayerController.h"
#include "MOFramework.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "MOInspectionActions"

const FName FMOInspectionActions::TimedActionId(TEXT("Inspect"));

namespace MOInspectionActions
{
	// 1 s: what every build has used. The study used to be timed by WBP_InspectionProgress, whose DebugInspectionDuration (1.0) overrode the character UI
	// controller's 15 s; the server keeps the length players have had. Whether studying should take longer (and depend on the item) is Wes's call
	// (Docs/DECISIONS.md F26).
	TAutoConsoleVariable<float> CVarInspectSeconds(
		TEXT("MO.Inspect.Seconds"), 1.0f,
		TEXT("Game seconds an item inspection takes (the server times it on the inspecting pawn)."),
		ECVF_Default);

	/** The interact reach, doubled (an instance's origin can sit metres from where the player looks: the rule the instance RPCs use). */
	double Reach(const UWorld* World)
	{
		const UMOInteractionSubsystem* Interaction = World ? World->GetSubsystem<UMOInteractionSubsystem>() : nullptr;
		return 2.0 * (Interaction ? Interaction->MaximumInteractDistance : 500.0);
	}

	/** Whether ItemId is at hand for Inspector (see FMOInspectionActions::Begin): which of the three, for the log, or null. */
	const TCHAR* AtHand(APawn* Inspector, FName ItemId, const FGuid& ItemGuid, const FMOInstanceRef& Instance)
	{
		UWorld* World = Inspector->GetWorld();
		if (ItemGuid.IsValid())
		{
			FMOInventoryEntry Entry;
			const UMOInventoryComponent* Inventory = Inspector->FindComponentByClass<UMOInventoryComponent>();
			if (Inventory && Inventory->TryGetEntryByGuid(ItemGuid, Entry) && Entry.ItemDefinitionId == ItemId)
			{
				return TEXT("carried");
			}
		}
		const double ReachSq = FMath::Square(Reach(World));
		const FVector At = Inspector->GetActorLocation();
		if (Instance.IsSet())
		{
			UInstancedStaticMeshComponent* ISM = nullptr;
			int32 Index = INDEX_NONE;
			FTransform Xf;
			if (Instance.Resolve(World, ISM, Index) && ISM->GetInstanceTransform(Index, Xf, /*bWorldSpace=*/true) && FVector::DistSquared(Xf.GetLocation(), At) <= ReachSq)
			{
				if (ISM->ComponentHasTag(FName(*(FString(TEXT("MOItem_")) + ItemId.ToString()))))
				{
					return TEXT("on the ground");
				}
				const UMOHarvestSubsystem* Harvest = World->GetSubsystem<UMOHarvestSubsystem>();
				if (Harvest && Harvest->GetStudyableItemIds(ISM).Contains(ItemId))   // the menu's own rule: a tree lets you study its bark
				{
					return TEXT("its source");
				}
			}
		}
		for (TActorIterator<AMOWorldItem> It(World); It; ++It)
		{
			const UMOItemComponent* Item = It->GetItemComponent();
			if (Item && Item->ItemDefinitionId == ItemId && !It->IsHidden() && FVector::DistSquared(It->GetActorLocation(), At) <= ReachSq)
			{
				return TEXT("a world item");
			}
		}
		return nullptr;
	}
}

float FMOInspectionActions::GetSeconds()
{
	return FMath::Max(0.1f, MOInspectionActions::CVarInspectSeconds.GetValueOnGameThread());
}

bool FMOInspectionActions::Begin(APawn* Inspector, FName ItemId, const FGuid& ItemGuid, const FMOInstanceRef& Instance, FText& OutReason)
{
	UMOTimedActionComponent* Timed = Inspector ? Inspector->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	const UMOKnowledgeComponent* Knowledge = Inspector ? Inspector->FindComponentByClass<UMOKnowledgeComponent>() : nullptr;
	FMOItemDefinitionRow Def;
	OutReason = LOCTEXT("CannotInspect", "You can't study that here.");
	if (!Inspector || !Inspector->HasAuthority() || !Timed || !Knowledge || !UMOItemDatabaseSettings::GetItemDefinition(ItemId, Def))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInspection] %s: inspecting '%s' refused: needs the server, a timed-action and a knowledge component, and a known item"),
			*GetNameSafe(Inspector), *ItemId.ToString());
		return false;
	}
	const TCHAR* Where = MOInspectionActions::AtHand(Inspector, ItemId, ItemGuid, Instance);
	if (!Where)
	{
		OutReason = LOCTEXT("NotAtHand", "It is not within reach.");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOInspection] %s: inspecting '%s' refused: not carried (%s), not on the ground or at its source within reach (%s)"),
			*Inspector->GetName(), *ItemId.ToString(), *ItemGuid.ToString(), *Instance.ToString());
		return false;
	}
	const FText Label = FText::Format(LOCTEXT("Inspecting", "Inspecting: {0}"), UMOItemDatabaseSettings::GetItemDisplayName(ItemId));
	TWeakObjectPtr<APawn> WeakPawn(Inspector);
	const float Seconds = FMOWorldRules::ScaleSeconds(Inspector, EMOWorldPace::Inspecting, GetSeconds());   // this world's study pace (MOWorldCreationTypes.h)
	const bool bBegun = Timed->Begin(TimedActionId, Label, Seconds, [WeakPawn, ItemId]()
		{
			APawn* Pawn = WeakPawn.Get();
			UMOKnowledgeComponent* K = Pawn ? Pawn->FindComponentByClass<UMOKnowledgeComponent>() : nullptr;
			if (!K)
			{
				return;
			}
			const FMOInspectionResult Result = K->InspectItem(ItemId, Pawn->FindComponentByClass<UMOSkillsComponent>());
			UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] %s studied '%s' on the server: success=%d, %d grant(s)"), *Pawn->GetName(), *ItemId.ToString(),
				Result.bSuccess ? 1 : 0, Result.XPGrants.Num());
			if (AMOPlayerController* PC = Cast<AMOPlayerController>(Pawn->GetController()))
			{
				PC->ClientInspectionResult(ItemId, Result);
			}
		});
	if (bBegun)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOInspection] %s: studying '%s' (%s, %.1f s, server-timed)"), *Inspector->GetName(), *ItemId.ToString(), Where, Seconds);
	}
	return bBegun;
}

#undef LOCTEXT_NAMESPACE
