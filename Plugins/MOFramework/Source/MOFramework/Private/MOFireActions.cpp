#include "MOFireActions.h"
#include "HAL/IConsoleManager.h"
#include "MOCarriedLightComponent.h"
#include "MOLightSourceSettings.h"
#include "MOCraftingStationActor.h"
#include "MOInventoryComponent.h"
#include "MOTimedActionComponent.h"
#include "MOSkillsComponent.h"
#include "MOVitalsComponent.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOItemDatabaseSettings.h"
#include "MOCraftingSubsystem.h"
#include "MOPlayerController.h"
#include "MONotificationComponent.h"
#include "MONotificationWidget.h"
#include "MOFramework.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "MOFireActions"

const FName FMOFireActions::AddFuel(TEXT("AddFuel"));
const FName FMOFireActions::Light(TEXT("Light"));
const FName FMOFireActions::PutOut(TEXT("PutOut"));
const FName FMOFireActions::LightCarried(TEXT("LightCarried"));
const FName FMOFireActions::LightFromCarried(TEXT("LightFromCarried"));

namespace
{
	const FName TinderId(TEXT("Tinder01"));
	/** The improvised hand drill: a straight DRY stick twirled between the palms on a second one -- deadfall (Branch01, "already seasoned"); a green branch
	 *  (Stick01) is too wet to make an ember. */
	const FName DrillStickId(TEXT("Branch01"));
	const FName FiremakingSkill(TEXT("Firemaking"));
	/** One attempt: a hand drill takes minutes of hard twirling; a good tool (a bow drill's mechanical advantage) about half that. */
	constexpr float HandDrillSeconds = 3.0f * 60.0f;
	constexpr float BowDrillSeconds = 90.0f;
	constexpr float QuickToolEffectiveness = 0.6f;

	/** How this pawn would light a fire now: its best Firestarter tool, else two dry branches, else nothing. */
	struct FLightingMethod
	{
		FGuid ToolGuid;          // invalid = improvised (or none)
		FName ToolItemId;
		float Effectiveness = 0.0f;
		bool IsAvailable() const { return Effectiveness > 0.0f; }
		bool IsImprovised() const { return !ToolGuid.IsValid(); }
		float Seconds() const { return Effectiveness >= QuickToolEffectiveness ? BowDrillSeconds : HandDrillSeconds; }
	};

	FLightingMethod FindLightingMethod(const APawn* Pawn, UMOInventoryComponent* Inventory)
	{
		FLightingMethod Method;
		UMOCraftingSubsystem* Crafting = Pawn && Pawn->GetWorld() ? Pawn->GetWorld()->GetSubsystem<UMOCraftingSubsystem>() : nullptr;
		FGuid Guid;
		float Effectiveness = 0.0f;
		if (Crafting && Inventory && Crafting->FindBestTool(Inventory, EMOToolType::Firestarter, 0.01f, Guid, Effectiveness)
			&& Effectiveness > FMOFireActions::ImprovisedDrillEffectiveness)
		{
			FMOInventoryEntry Entry;
			Method.ToolGuid = Guid;
			Method.ToolItemId = Inventory->TryGetEntryByGuid(Guid, Entry) ? Entry.ItemDefinitionId : NAME_None;
			Method.Effectiveness = Effectiveness;
		}
		else if (Inventory && Inventory->GetItemCountByDefinitionId(DrillStickId) >= 2)
		{
			Method.Effectiveness = FMOFireActions::ImprovisedDrillEffectiveness;
		}
		return Method;
	}
#if !UE_BUILD_SHIPPING
	// Test builds only: the co-op harness lights a joiner's fire without drilling for 3 minutes (nettest actions, N-183). 0 = the real length.
	// Compiled out of Shipping: the attempt is the simulation (Principle 11).
	TAutoConsoleVariable<float> CVarTestAttemptSeconds(
		TEXT("MO.Test.FireAttemptSeconds"), 0.0f,
		TEXT("Test builds only: when > 0, a fire-lighting attempt takes this many seconds instead of the real 3 min (hand drill) / 90 s (bow drill)."),
		ECVF_Cheat);
#endif

	float AttemptSeconds(const FLightingMethod& Method)
	{
#if !UE_BUILD_SHIPPING
		if (CVarTestAttemptSeconds.GetValueOnGameThread() > 0.0f)
		{
			return CVarTestAttemptSeconds.GetValueOnGameThread();
		}
#endif
		return Method.Seconds();
	}

	/** Laying fuel on: a few seconds a piece. */
	constexpr float SecondsPerFuelItem = 2.0f;
	constexpr float ReachCm = 300.0f;

	UMOInventoryComponent* FireInventoryOf(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	}

	void Notify(APawn* Pawn, const FText& Message, EMONotificationType Type)
	{
		if (AMOPlayerController* PC = Pawn ? Cast<AMOPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientShowNotification(Message, 4.0f, Type);
		}
	}

	/** The drilling player sees what the attempt taught ("Firemaking +25 XP"): practice that never shows does not feel like it counts (F33). */
	void ShowPracticeToast(APawn* Pawn, const UMOSkillsComponent* Skills, float XP, int32 LevelBefore)
	{
		AMOPlayerController* PC = Pawn ? Cast<AMOPlayerController>(Pawn->GetController()) : nullptr;
		UMONotificationComponent* Notes = PC ? PC->GetNotificationComponent() : nullptr;
		FMOSkillProgress After;
		if (Notes && Skills && Skills->GetSkillProgress(FiremakingSkill, After))
		{
			Notes->ShowSkillToast(FiremakingSkill, /*bIsKnowledge=*/false, XP, LevelBefore, After.Level, 3.0f, After.CurrentXP, After.XPToNextLevel);
		}
	}
}

float FMOFireActions::LightFromFlameChance(bool bRaining)
{
	return bRaining ? 0.6f : 0.95f;
}

float FMOFireActions::LightChance(float ToolEffectiveness, int32 FiremakingLevel, bool bRaining, float Wetness01)
{
	if (ToolEffectiveness <= 0.0f)
	{
		return 0.0f;
	}
	float Chance = ToolEffectiveness + 0.06f * FMath::Max(0, FiremakingLevel);
	if (bRaining)
	{
		Chance *= 0.3f;
	}
	if (Wetness01 > 0.5f)
	{
		Chance *= 0.6f;
	}
	return FMath::Clamp(Chance, 0.0f, 0.95f);
}

void FMOFireActions::GetActions(const APawn* Pawn, const AMOCraftingStationActor* Station, TArray<FMOItemActionOption>& Out)
{
	const UMOInventoryComponent* Inventory = FireInventoryOf(Pawn);
	if (!Pawn || !Station || !Inventory || !Station->bRequiresFuel || FVector::Dist(Pawn->GetActorLocation(), Station->GetActorLocation()) > ReachCm + 200.0f)
	{
		return;
	}
	// One "add" per kind of fuel carried, so the player chooses what goes on (dry deadfall is worth keeping for the drill).
	bool bAnyFuel = false;
	const bool bFull = Station->CurrentFuel > Station->MaxFuel - Station->FuelPerItemUnit;
	for (const FName& Id : Station->AcceptedFuelItems)
	{
		const int32 Carried = Inventory->GetItemCountByDefinitionId(Id);
		if (Carried <= 0)
		{
			continue;
		}
		bAnyFuel = true;
		const int32 Fits = AMOCraftingStationActor::ComputeAcceptedFuelItems(Station->CurrentFuel, Station->MaxFuel, Station->FuelPerItemUnit, Carried);
		FMOItemActionOption& Fuel = Out.AddDefaulted_GetRef();
		Fuel.Id = FName(*FString::Printf(TEXT("%s.%s"), *AddFuel.ToString(), *Id.ToString()));
		Fuel.Label = FText::Format(LOCTEXT("AddN", "Add {0} ({1})"), UMOItemDatabaseSettings::GetItemDisplayName(Id), FText::AsNumber(FMath::Max(Fits, 1)));
		Fuel.bEnabled = Fits > 0;
		Fuel.DisabledReason = LOCTEXT("FullFuel", "It holds all the fuel it can.");
	}
	if (!bAnyFuel)
	{
		FMOItemActionOption& Fuel = Out.AddDefaulted_GetRef();
		Fuel.Id = AddFuel;
		Fuel.Label = LOCTEXT("AddFuel", "Add fuel");
		Fuel.bEnabled = false;
		Fuel.DisabledReason = bFull ? LOCTEXT("FullFuel", "It holds all the fuel it can.") : LOCTEXT("NoFuel", "You carry nothing it burns (branches, firewood, charcoal).");
	}

	if (Station->IsStationActive())
	{
		// A torch or lamp in hand is lit from the flames.
		const UMOCarriedLightComponent* Carried = Pawn->FindComponentByClass<UMOCarriedLightComponent>();
		EMOEquipmentSlot HeldSlot = EMOEquipmentSlot::RightHand;
		FMOEquippedItem Held;
		if (Carried && Carried->FindUnlitHeld(HeldSlot, Held))
		{
			FMOItemActionOption& LightIt = Out.AddDefaulted_GetRef();
			LightIt.Id = LightCarried;
			LightIt.Label = FText::Format(LOCTEXT("LightCarried", "Light the {0} in the fire"), UMOItemDatabaseSettings::GetItemDisplayName(Held.ItemDefinitionId));
		}
		FMOItemActionOption& Out1 = Out.AddDefaulted_GetRef();
		Out1.Id = PutOut;
		Out1.Label = LOCTEXT("PutOut", "Put it out");
		return;
	}
	// Fire carried from the last one: a lit torch in hand lights the laid fuel directly.
	const UMOCarriedLightComponent* CarriedFlame = Pawn->FindComponentByClass<UMOCarriedLightComponent>();
	if (CarriedFlame && CarriedFlame->IsLit())
	{
		FMOItemActionOption& FromFlame = Out.AddDefaulted_GetRef();
		FromFlame.Id = LightFromCarried;
		FromFlame.Label = FText::Format(LOCTEXT("LightFromCarried", "Light it from the {0}"), UMOItemDatabaseSettings::GetItemDisplayName(CarriedFlame->GetLitItemId()));
		FromFlame.bEnabled = Station->CurrentFuel > 0.0f;
		FromFlame.DisabledReason = LOCTEXT("LayFuelFlame", "Lay some fuel first.");
	}
	const FLightingMethod Method = FindLightingMethod(Pawn, const_cast<UMOInventoryComponent*>(Inventory));
	const bool bTinder = Inventory->HasItem(TinderId);
	FMOItemActionOption& LightOpt = Out.AddDefaulted_GetRef();
	LightOpt.Id = Light;
	LightOpt.Label = Method.IsAvailable() && !Method.IsImprovised()
		? FText::Format(LOCTEXT("LightWith", "Light ({0})"), UMOItemDatabaseSettings::GetItemDisplayName(Method.ToolItemId))
		: LOCTEXT("LightHand", "Light (hand drill)");
	LightOpt.bEnabled = Station->CurrentFuel > 0.0f && bTinder && Method.IsAvailable();
	LightOpt.DisabledReason = Station->CurrentFuel <= 0.0f ? LOCTEXT("LayFuel", "Lay some fuel first.")
		: !bTinder ? LOCTEXT("NeedTinder", "Needs tinder (dry fibre from dead shrubs or shredded bark).")
		: LOCTEXT("NeedDrill", "Needs two dry branches (deadfall) for a hand drill: green wood is too wet to make an ember.");
}

bool FMOFireActions::Perform(APawn* Pawn, AMOCraftingStationActor* Station, FName ActionId)
{
	UMOInventoryComponent* Inventory = FireInventoryOf(Pawn);
	UMOTimedActionComponent* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr;
	if (!Pawn || !Pawn->HasAuthority() || !Station || !Inventory || !Timed)
	{
		return false;
	}
	TArray<FMOItemActionOption> Options;
	GetActions(Pawn, Station, Options);
	const FMOItemActionOption* Option = Options.FindByPredicate([ActionId](const FMOItemActionOption& O) { return O.Id == ActionId; });
	if (!Option || !Option->bEnabled)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOFire] %s: %s refused (%s)"), *Pawn->GetName(), *ActionId.ToString(), Option ? *Option->DisabledReason.ToString() : TEXT("not on offer"));
		return false;
	}
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	TWeakObjectPtr<AMOCraftingStationActor> WeakStation(Station);

	if (ActionId == LightCarried)
	{
		UMOCarriedLightComponent* Carried = Pawn->FindComponentByClass<UMOCarriedLightComponent>();
		EMOEquipmentSlot HeldSlot = EMOEquipmentSlot::RightHand;
		FMOEquippedItem Held;
		const FMOLightSourceProfile* Profile = Carried && Carried->FindUnlitHeld(HeldSlot, Held) ? UMOLightSourceSettings::Find(Held.ItemDefinitionId) : nullptr;
		if (!Profile)
		{
			return false;
		}
		TWeakObjectPtr<UMOCarriedLightComponent> WeakCarried(Carried);
		return Timed->Begin(LightCarried, FText::Format(LOCTEXT("LightingCarried", "Holding the {0} in the flames"), UMOItemDatabaseSettings::GetItemDisplayName(Held.ItemDefinitionId)),
			Profile->LightSeconds,
			[WeakCarried, WeakStation]()
			{
				UMOCarriedLightComponent* C = WeakCarried.Get();
				const AMOCraftingStationActor* S = WeakStation.Get();
				if (C && S && S->IsStationActive())
				{
					C->LightHeld();   // re-finds the held light: if it was put away meanwhile there is nothing to light
				}
			}, nullptr, 100.0f);
	}

	if (ActionId == LightFromCarried)
	{
		TWeakObjectPtr<UMOCarriedLightComponent> WeakCarried(Pawn->FindComponentByClass<UMOCarriedLightComponent>());
		return Timed->Begin(LightFromCarried, LOCTEXT("LightingFromFlame", "Holding the flame to the kindling"), LightFromFlameSeconds,
			[WeakPawn, WeakStation, WeakCarried]()
			{
				APawn* P = WeakPawn.Get();
				AMOCraftingStationActor* S = WeakStation.Get();
				const UMOCarriedLightComponent* C = WeakCarried.Get();
				if (!P || !S || !C || !C->IsLit() || S->CurrentFuel <= 0.0f)
				{
					return;   // the flame went out meanwhile, or the fuel is gone
				}
				const UMOWeatherIntegrationSubsystem* Weather = P->GetWorld() ? P->GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
				const float Chance = LightFromFlameChance(Weather && Weather->IsRaining());
				const bool bCaught = FMath::FRand() < Chance;
				if (bCaught)
				{
					S->SetStationActive(true);
					Notify(P, LOCTEXT("CaughtFromFlame", "The kindling catches from the flame."), EMONotificationType::Success);
				}
				else
				{
					Notify(P, LOCTEXT("FailedFromFlame", "The wet kindling smokes but will not take. Try again."), EMONotificationType::Warning);
				}
				UE_LOG(LogMOFramework, Log, TEXT("[MOFire] %s lighting %s from a carried %s: chance %.0f %% -> %s"), *P->GetName(), *S->GetName(), *C->GetLitItemId().ToString(),
					Chance * 100.0f, bCaught ? TEXT("CAUGHT") : TEXT("failed"));
			}, nullptr, 100.0f);
	}

	if (ActionId == PutOut)
	{
		// Smothered with earth or kicked apart: the fuel that was burning is spent.
		Station->CurrentFuel = 0.0f;
		Station->SetStationActive(false);
		return true;
	}

	FString FuelItemText;
	if (ActionId.ToString().Split(TEXT("."), nullptr, &FuelItemText) && ActionId.ToString().StartsWith(AddFuel.ToString()))
	{
		const FName FuelItem(*FuelItemText);
		const int32 Fits = AMOCraftingStationActor::ComputeAcceptedFuelItems(Station->CurrentFuel, Station->MaxFuel, Station->FuelPerItemUnit, Inventory->GetItemCountByDefinitionId(FuelItem));
		return Timed->Begin(AddFuel, FText::Format(LOCTEXT("AddingFuel", "Laying on {0}"), UMOItemDatabaseSettings::GetItemDisplayName(FuelItem)), SecondsPerFuelItem * Fits,
			[WeakPawn, WeakStation, FuelItem]()
			{
				AMOCraftingStationActor* S = WeakStation.Get();
				UMOInventoryComponent* Inv = FireInventoryOf(WeakPawn.Get());
				if (!S || !Inv)
				{
					return;
				}
				// The station takes only what fits; only that leaves the pack (AddFuel is transactional).
				const int32 Taken = S->AddFuel(FuelItem, Inv->GetItemCountByDefinitionId(FuelItem));
				if (Taken > 0)
				{
					Inv->RemoveItemByDefinitionId(FuelItem, Taken);
				}
				UE_LOG(LogMOFramework, Log, TEXT("[MOFire] %s laid %d x %s on %s: fuel %.0f / %.0f"), *GetNameSafe(WeakPawn.Get()), Taken, *FuelItem.ToString(), *S->GetName(),
					S->CurrentFuel, S->MaxFuel);
			}, nullptr, ReachCm);
	}

	if (ActionId == Light)
	{
		const FLightingMethod Method = FindLightingMethod(Pawn, Inventory);
		return Timed->Begin(Light, Method.IsImprovised() ? LOCTEXT("Twirling", "Twirling the hand drill")
				: FText::Format(LOCTEXT("Drilling", "Working the {0}"), UMOItemDatabaseSettings::GetItemDisplayName(Method.ToolItemId)),
			AttemptSeconds(Method),
			[WeakPawn, WeakStation, Method]()
			{
				APawn* P = WeakPawn.Get();
				AMOCraftingStationActor* S = WeakStation.Get();
				UMOInventoryComponent* Inv = FireInventoryOf(P);
				if (!P || !S || !Inv || !Inv->HasItem(TinderId))
				{
					return;
				}
				UMOSkillsComponent* Skills = P->FindComponentByClass<UMOSkillsComponent>();
				const UMOVitalsComponent* Vitals = P->FindComponentByClass<UMOVitalsComponent>();
				const UMOWeatherIntegrationSubsystem* Weather = P->GetWorld() ? P->GetWorld()->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
				const float Chance = LightChance(Method.Effectiveness, Skills ? Skills->GetSkillLevel(FiremakingSkill) : 0, Weather && Weather->IsRaining(),
					Vitals ? Vitals->GetWetnessLevel() : 0.0f);
				const bool bCaught = FMath::FRand() < Chance;
				// The tinder is spent either way; each attempt burns a socket into the hearth: an improvised drill wears a branch through, a kit loses durability.
				Inv->RemoveItemByDefinitionId(TinderId, 1);
				if (Method.IsImprovised())
				{
					Inv->RemoveItemByDefinitionId(DrillStickId, 1);
				}
				else
				{
					bool bWornOut = false;
					Inv->ReduceDurability(Method.ToolGuid, 1, bWornOut);
					if (bWornOut)
					{
						Notify(P, FText::Format(LOCTEXT("WornOut", "The {0} is worn through."), UMOItemDatabaseSettings::GetItemDisplayName(Method.ToolItemId)),
							EMONotificationType::Warning);
					}
				}
				if (Skills)
				{
					const int32 LevelBefore = Skills->GetSkillLevel(FiremakingSkill);
					const float XP = bCaught ? LitAttemptXP : FailedAttemptXP;   // a failed fire teaches too
					Skills->AddExperience(FiremakingSkill, XP);
					ShowPracticeToast(P, Skills, XP, LevelBefore);
				}
				if (bCaught && S->CurrentFuel > 0.0f)
				{
					S->SetStationActive(true);
					Notify(P, LOCTEXT("Caught", "An ember -- the tinder catches and the fire takes."), EMONotificationType::Success);
				}
				else
				{
					Notify(P, LOCTEXT("Failed", "The ember dies in the tinder. Try again with fresh tinder."), EMONotificationType::Warning);
				}
				UE_LOG(LogMOFramework, Log, TEXT("[MOFire] %s lighting %s (%s, effectiveness %.2f): chance %.0f %% -> %s"), *P->GetName(), *S->GetName(),
					Method.IsImprovised() ? TEXT("improvised hand drill") : *Method.ToolItemId.ToString(), Method.Effectiveness, Chance * 100.0f,
					bCaught ? TEXT("CAUGHT") : TEXT("failed"));
			}, nullptr, 100.0f);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
