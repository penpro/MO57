#include "MOWaterActions.h"
#include "MOWaterSubsystem.h"
#include "MOWaterSourceSettings.h"
#include "MOWaterPhysiology.h"
#include "MOTimedActionComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOPlayerController.h"
#include "MONotificationWidget.h"
#include "MOFramework.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "MOWaterActions"

EMOWaterBodyKind FMOWaterActions::ReachableWaterAt(const APawn* Drinker, const FVector& Point)
{
	const UMOWaterSubsystem* Water = Drinker ? UMOWaterSubsystem::Get(Drinker) : nullptr;
	if (!Water)
	{
		return EMOWaterBodyKind::None;
	}
	// A little slack over the reach for the client's view of where the pawn stood (latency), never more.
	const float Reach = GetDefault<UMOWaterSourceSettings>()->ReachCm + 100.0f;
	if (FVector::Dist(Drinker->GetActorLocation(), Point) > Reach)
	{
		return EMOWaterBodyKind::None;
	}
	return Water->ClassifyWaterAt(Point.X, Point.Y);
}

EMODrinkRefusal FMOWaterActions::BeginDrink(APawn* Drinker, const FVector& Point)
{
	if (!Drinker || !Drinker->HasAuthority())
	{
		return EMODrinkRefusal::NoPawn;
	}
	UMOTimedActionComponent* Action = Drinker->FindComponentByClass<UMOTimedActionComponent>();
	if (!Action)
	{
		return EMODrinkRefusal::NoPawn;
	}
	const UMOWaterSubsystem* Water = UMOWaterSubsystem::Get(Drinker);
	const EMOWaterBodyKind Kind = Water ? Water->ClassifyWaterAt(Point.X, Point.Y) : EMOWaterBodyKind::None;
	if (Kind == EMOWaterBodyKind::None)
	{
		return EMODrinkRefusal::NoWater;
	}
	if (ReachableWaterAt(Drinker, Point) == EMOWaterBodyKind::None)
	{
		return EMODrinkRefusal::TooFar;
	}

	const UMOWaterSourceSettings* Settings = GetDefault<UMOWaterSourceSettings>();
	const FMOWaterSourceProfile Profile = UMOWaterSourceSettings::GetProfile(Kind);
	const float Ml = Settings->DraughtMillilitres;
	const EMOConditionType Illness = Settings->IllnessCondition;
	TWeakObjectPtr<APawn> WeakDrinker(Drinker);
	Action->BeginKneeling(FName(TEXT("Drink")), FText::Format(LOCTEXT("Drinking", "Drinking {0}"), Profile.DisplayName.ToLower()), Settings->DraughtSeconds,
		[WeakDrinker, Ml, Profile, Illness, Kind]()
		{
			APawn* Pawn = WeakDrinker.Get();
			UMOSurvivalStatsComponent* Stats = Pawn ? Pawn->FindComponentByClass<UMOSurvivalStatsComponent>() : nullptr;
			if (!Stats)
			{
				return;
			}
			const float Net = Stats->IngestWater(Ml, Profile.SalinityGPerL);
			const float Chance = FMOWaterPhysiology::IllnessChance(Ml, Profile.IllnessChancePerLitre);
			UMOAnatomyComponent* Anatomy = Pawn->FindComponentByClass<UMOAnatomyComponent>();
			const bool bCaught = Anatomy && Anatomy->ExposeToCondition(Illness, Chance);
			UE_LOG(LogMOFramework, Log, TEXT("[MOWater] %s drank %.0f ml of %s: net body water %+.0f ml, illness chance %.2f %%%s"), *Pawn->GetName(), Ml,
				*Profile.DisplayName.ToString(), Net, Chance * 100.0f, bCaught ? TEXT(" -- CAUGHT (incubating)") : TEXT(""));
			// The body tells the drinker what salt water does; nothing tells them they caught something (that is the point of an incubation).
			if (Net < 0.0f)
			{
				if (AMOPlayerController* PC = Cast<AMOPlayerController>(Pawn->GetController()))
				{
					PC->ClientShowNotification(LOCTEXT("SaltWater", "Salt water. It burns your throat and leaves you thirstier than before."), 5.0f, EMONotificationType::Warning);
				}
			}
		});
	return EMODrinkRefusal::None;
}

#undef LOCTEXT_NAMESPACE
