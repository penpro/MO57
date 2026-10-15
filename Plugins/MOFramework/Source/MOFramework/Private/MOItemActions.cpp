#include "MOItemActions.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOItemInstanceState.h"
#include "MOTimedActionComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOAnatomyComponent.h"
#include "MOWaterSubsystem.h"
#include "MOWaterSourceSettings.h"
#include "MOWaterPhysiology.h"
#include "MOAmbientEnvironmentProvider.h"
#include "MOFramework.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "MOItemActions"

const FName FMOItemActions::Drink(TEXT("Drink"));
const FName FMOItemActions::Fill(TEXT("Fill"));
const FName FMOItemActions::PourOut(TEXT("PourOut"));
const FName FMOItemActions::BoilOverFire(TEXT("BoilOverFire"));
const FName FMOItemActions::BoilWithStones(TEXT("BoilWithStones"));
const FName FMOItemActions::Use(TEXT("Use"));

namespace
{
	/** A sip from a container (ml) and how long it takes (game s). */
	constexpr float SipMl = 250.0f;
	constexpr float SipSeconds = 10.0f;
	/** Dunking a container at the water's edge and letting it fill. */
	constexpr float FillSeconds = 12.0f;
	/** Stone boiling a container (realistic floor: heating 4-6 stones in the fire ~15 min, then dropping them in with green-stick tongs until it boils, ~5 min). */
	constexpr float StoneBoilSeconds = 20.0f * 60.0f;
	/** A pot over the fire: about ten minutes to a rolling boil, plus the boil itself. */
	constexpr float FireBoilSeconds = 12.0f * 60.0f;
	/** Tending a fire: the pawn may move about it, not walk off. */
	constexpr float TendFireDriftCm = 300.0f;
	constexpr int32 StonesForBoiling = 4;
	/** Each heating cracks about one stone (archaeology's "fire-cracked rock"). */
	constexpr int32 StonesCrackedPerBoil = 1;

	const FName BoilingStoneIds[] = { FName(TEXT("Stone01")), FName(TEXT("RiverCobble01")) };

	bool GetDef(FName ItemId, FMOItemDefinitionRow& Out)
	{
		return UMOItemDatabaseSettings::GetItemDefinition(ItemId, Out);
	}

	FText LiquidName(const FMOLiquidContents& Liquid)
	{
		if (Liquid.bBoiled)
		{
			return Liquid.SalinityGPerL > 5.0f ? LOCTEXT("BoiledSea", "boiled sea water") : LOCTEXT("Boiled", "boiled water");
		}
		return UMOWaterSourceSettings::GetProfile(Liquid.Source).DisplayName.ToLower();
	}

	UMOInventoryComponent* InventoryOf(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	}
}

int32 FMOItemActions::CountBoilingStones(const UMOInventoryComponent* Inventory)
{
	int32 Count = 0;
	for (const FName& Id : BoilingStoneIds)
	{
		Count += Inventory ? Inventory->GetItemCountByDefinitionId(Id) : 0;
	}
	return Count;
}

bool FMOItemActions::IsFireWithinReach(const APawn* Pawn)
{
	const UMOAmbientEnvironmentRegistry* Registry = Pawn ? UMOAmbientEnvironmentRegistry::Get(Pawn) : nullptr;
	if (!Registry)
	{
		return false;
	}
	for (const IMOLocalHeatSource* Source : Registry->GetHeatSources())
	{
		if (Source && Source->IsHeatActive() && FVector::Dist(Source->GetHeatLocation(), Pawn->GetActorLocation()) <= TendFireDriftCm)
		{
			return true;
		}
	}
	return false;
}

bool FMOItemActions::FindReachableWater(const APawn* Pawn, FVector& OutPoint, uint8& OutKind)
{
	const UMOWaterSubsystem* Water = Pawn ? UMOWaterSubsystem::Get(Pawn) : nullptr;
	if (!Water)
	{
		return false;
	}
	// Rings out from the pawn's feet to the drinking reach: the nearest water it could kneel at.
	const FVector P = Pawn->GetActorLocation();
	const float Reach = GetDefault<UMOWaterSourceSettings>()->ReachCm;
	for (float R = 0.0f; R <= Reach; R += 50.0f)
	{
		const int32 N = R <= 0.0f ? 1 : 16;
		for (int32 K = 0; K < N; ++K)
		{
			const float A = K * 2.0f * UE_PI / N;
			const FVector2D Q(P.X + R * FMath::Cos(A), P.Y + R * FMath::Sin(A));
			const EMOWaterBodyKind Kind = Water->ClassifyWaterAt(Q.X, Q.Y);
			float Surface = 0.0f;
			if (Kind != EMOWaterBodyKind::None && Water->GetSurfaceZ(Q.X, Q.Y, Surface) && FMath::Abs(Surface - P.Z) <= Reach)
			{
				OutPoint = FVector(Q.X, Q.Y, Surface);
				OutKind = static_cast<uint8>(Kind);
				return true;
			}
		}
	}
	return false;
}

void FMOItemActions::GetActions(const APawn* Pawn, const UMOInventoryComponent* Inventory, const FGuid& ItemGuid, TArray<FMOItemActionOption>& Out)
{
	FMOInventoryEntry Entry;
	FMOItemDefinitionRow Def;
	if (!Pawn || !Inventory || !Inventory->TryGetEntryByGuid(ItemGuid, Entry) || !GetDef(Entry.ItemDefinitionId, Def) || Def.LiquidCapacityMl <= 0.0f)
	{
		return;
	}
	if (Entry.Quantity > 1)
	{
		return;   // one contents per entry: a stack of containers is split first (the data keeps them at a stack of 1)
	}
	const FMOLiquidContents& Liquid = Entry.Liquid;
	const bool bHasLiquid = !Liquid.IsEmpty();

	if (bHasLiquid)
	{
		FMOItemActionOption& DrinkOpt = Out.AddDefaulted_GetRef();
		DrinkOpt.Id = Drink;
		DrinkOpt.Label = FText::Format(LOCTEXT("DrinkFrom", "Drink ({0}, {1} ml)"), LiquidName(Liquid), FText::AsNumber(FMath::RoundToInt(Liquid.Millilitres)));
	}

	FVector WaterPoint;
	uint8 WaterKind = 0;
	const bool bWater = FindReachableWater(Pawn, WaterPoint, WaterKind);
	FMOItemActionOption& FillOpt = Out.AddDefaulted_GetRef();
	FillOpt.Id = Fill;
	if (bWater)
	{
		FillOpt.Label = FText::Format(LOCTEXT("FillFrom", "Fill with {0}"), UMOWaterSourceSettings::GetProfile(static_cast<EMOWaterBodyKind>(WaterKind)).DisplayName.ToLower());
		FillOpt.bEnabled = Liquid.Millilitres < Def.LiquidCapacityMl - 1.0f;
		FillOpt.DisabledReason = LOCTEXT("Full", "It is full.");
	}
	else
	{
		FillOpt.Label = LOCTEXT("Fill", "Fill");
		FillOpt.bEnabled = false;
		FillOpt.DisabledReason = LOCTEXT("NoWater", "Stand at the edge of a river or lake.");
	}

	if (bHasLiquid && !Liquid.bBoiled)
	{
		const bool bFire = IsFireWithinReach(Pawn);
		FMOItemActionOption& BoilOpt = Out.AddDefaulted_GetRef();
		if (Def.bHeatableOverFire)
		{
			BoilOpt.Id = BoilOverFire;
			BoilOpt.Label = LOCTEXT("BoilFire", "Boil over the fire (12 min)");
			BoilOpt.bEnabled = bFire;
			BoilOpt.DisabledReason = LOCTEXT("NeedFire", "Needs a burning fire within reach.");
		}
		else
		{
			const int32 Stones = CountBoilingStones(Inventory);
			BoilOpt.Id = BoilWithStones;
			BoilOpt.Label = LOCTEXT("BoilStones", "Boil with hot stones (20 min)");
			BoilOpt.bEnabled = bFire && Stones >= StonesForBoiling;
			BoilOpt.DisabledReason = !bFire ? LOCTEXT("NeedFireStones", "Needs a burning fire within reach to heat the stones.")
				: FText::Format(LOCTEXT("NeedStones", "Needs {0} dry stones to heat (you have {1})."), FText::AsNumber(StonesForBoiling), FText::AsNumber(Stones));
		}
	}

	if (bHasLiquid)
	{
		FMOItemActionOption& PourOpt = Out.AddDefaulted_GetRef();
		PourOpt.Id = PourOut;
		PourOpt.Label = LOCTEXT("PourOut", "Pour out");
	}
}

bool FMOItemActions::Perform(APawn* Pawn, const FGuid& ItemGuid, FName ActionId)
{
	UMOInventoryComponent* Inventory = InventoryOf(Pawn);
	if (!Pawn || !Pawn->HasAuthority() || !Inventory)
	{
		return false;
	}

	if (ActionId == Use)
	{
		UMOSurvivalStatsComponent* Stats = Pawn->FindComponentByClass<UMOSurvivalStatsComponent>();
		return Stats && Stats->ConsumeItem(Inventory, ItemGuid);
	}

	// Everything else must be on offer right now (the same checks the menu showed).
	TArray<FMOItemActionOption> Options;
	GetActions(Pawn, Inventory, ItemGuid, Options);
	const FMOItemActionOption* Option = Options.FindByPredicate([ActionId](const FMOItemActionOption& O) { return O.Id == ActionId; });
	if (!Option || !Option->bEnabled)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOItemActions] %s: %s refused (not on offer: %s)"), *Pawn->GetName(), *ActionId.ToString(),
			Option ? *Option->DisabledReason.ToString() : TEXT("no such action for this item"));
		return false;
	}
	FMOInventoryEntry Entry;
	FMOItemDefinitionRow Def;
	Inventory->TryGetEntryByGuid(ItemGuid, Entry);
	GetDef(Entry.ItemDefinitionId, Def);
	UMOTimedActionComponent* Timed = Pawn->FindComponentByClass<UMOTimedActionComponent>();
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	TWeakObjectPtr<UMOInventoryComponent> WeakInventory(Inventory);

	if (ActionId == PourOut)
	{
		return Inventory->SetLiquid(ItemGuid, FMOLiquidContents());
	}
	if (!Timed)
	{
		return false;
	}

	if (ActionId == Drink)
	{
		const float Ml = FMath::Min(SipMl, Entry.Liquid.Millilitres);
		return Timed->Begin(Drink, FText::Format(LOCTEXT("Drinking", "Drinking {0}"), LiquidName(Entry.Liquid)), SipSeconds * Ml / SipMl,
			[WeakPawn, WeakInventory, ItemGuid, Ml]()
			{
				APawn* P = WeakPawn.Get();
				UMOInventoryComponent* Inv = WeakInventory.Get();
				FMOInventoryEntry Now;
				if (!P || !Inv || !Inv->TryGetEntryByGuid(ItemGuid, Now) || Now.Liquid.Millilitres < Ml - 1.0f)
				{
					return;   // dropped, poured or traded away while drinking
				}
				if (UMOSurvivalStatsComponent* Stats = P->FindComponentByClass<UMOSurvivalStatsComponent>())
				{
					Stats->IngestWater(Ml, Now.Liquid.SalinityGPerL);
				}
				if (UMOAnatomyComponent* Anatomy = P->FindComponentByClass<UMOAnatomyComponent>())
				{
					Anatomy->ExposeToCondition(GetDefault<UMOWaterSourceSettings>()->IllnessCondition, FMOWaterPhysiology::IllnessChance(Ml, Now.Liquid.IllnessChancePerLitre));
				}
				Now.Liquid.Millilitres -= Ml;
				Inv->SetLiquid(ItemGuid, Now.Liquid.IsEmpty() ? FMOLiquidContents() : Now.Liquid);
			});
	}

	if (ActionId == Fill)
	{
		FVector WaterPoint;
		uint8 WaterKindByte = 0;
		FindReachableWater(Pawn, WaterPoint, WaterKindByte);
		const EMOWaterBodyKind Kind = static_cast<EMOWaterBodyKind>(WaterKindByte);
		const float Capacity = Def.LiquidCapacityMl;
		return Timed->Begin(Fill, FText::Format(LOCTEXT("Filling", "Filling with {0}"), UMOWaterSourceSettings::GetProfile(Kind).DisplayName.ToLower()), FillSeconds,
			[WeakPawn, WeakInventory, ItemGuid, Kind, Capacity]()
			{
				UMOInventoryComponent* Inv = WeakInventory.Get();
				FMOInventoryEntry Now;
				if (!WeakPawn.IsValid() || !Inv || !Inv->TryGetEntryByGuid(ItemGuid, Now))
				{
					return;
				}
				const FMOWaterSourceProfile Profile = UMOWaterSourceSettings::GetProfile(Kind);
				FMOLiquidContents Drawn;
				Drawn.Millilitres = FMath::Max(0.0f, Capacity - Now.Liquid.Millilitres);
				Drawn.SalinityGPerL = Profile.SalinityGPerL;
				Drawn.IllnessChancePerLitre = Profile.IllnessChancePerLitre;
				Drawn.Source = Kind;
				Now.Liquid.Mix(Drawn);
				Inv->SetLiquid(ItemGuid, Now.Liquid);
				UE_LOG(LogMOFramework, Log, TEXT("[MOItemActions] filled %s: %.0f ml of %s (illness %.2f %%/L)"), *Now.ItemDefinitionId.ToString(), Now.Liquid.Millilitres,
					*Profile.DisplayName.ToString(), Now.Liquid.IllnessChancePerLitre * 100.0f);
			});
	}

	if (ActionId == BoilOverFire || ActionId == BoilWithStones)
	{
		const bool bStones = ActionId == BoilWithStones;
		return Timed->Begin(ActionId, bStones ? LOCTEXT("StoneBoiling", "Boiling with hot stones") : LOCTEXT("FireBoiling", "Boiling over the fire"),
			bStones ? StoneBoilSeconds : FireBoilSeconds,
			[WeakPawn, WeakInventory, ItemGuid, bStones]()
			{
				APawn* P = WeakPawn.Get();
				UMOInventoryComponent* Inv = WeakInventory.Get();
				FMOInventoryEntry Now;
				if (!P || !Inv || !Inv->TryGetEntryByGuid(ItemGuid, Now) || Now.Liquid.IsEmpty() || !IsFireWithinReach(P))
				{
					return;   // the fire went out, or the container left: no boil
				}
				// A rolling boil kills what makes you ill; it leaves the salt (and concentrates it as some boils off).
				const float Kept = bStones ? 0.95f : 0.92f;
				Now.Liquid.Millilitres *= Kept;
				Now.Liquid.SalinityGPerL /= Kept;
				Now.Liquid.IllnessChancePerLitre = 0.0f;
				Now.Liquid.bBoiled = true;
				Inv->SetLiquid(ItemGuid, Now.Liquid);
				if (bStones)
				{
					// The heat cracks a stone or so each time (fire-cracked rock): it is no use for the next boil.
					int32 ToCrack = StonesCrackedPerBoil;
					for (const FName& Id : BoilingStoneIds)
					{
						const int32 Take = FMath::Min(ToCrack, Inv->GetItemCountByDefinitionId(Id));
						if (Take > 0 && Inv->RemoveItemByDefinitionId(Id, Take))
						{
							ToCrack -= Take;
						}
					}
				}
				UE_LOG(LogMOFramework, Log, TEXT("[MOItemActions] %s boiled %s (%s): %.0f ml left"), *P->GetName(), *Now.ItemDefinitionId.ToString(),
					bStones ? TEXT("stone boiling") : TEXT("over the fire"), Now.Liquid.Millilitres);
			}, nullptr, TendFireDriftCm);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
