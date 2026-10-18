#include "MOIngredientReservation.h"
#include "MOInventoryComponent.h"
#include "MOItemDefinitionRow.h"
#include "MOWaterSourceSettings.h"

namespace MOIngredientPlans
{
namespace Detail
{
	FMOIngredientOperationResult Failure(EMOIngredientFailure Kind, FName Id = NAME_None, int32 Missing = 0)
	{
		FMOIngredientOperationResult Result;
		Result.Failure = Kind;
		Result.IngredientId = Id;
		Result.MissingUnits = Missing;
		return Result;
	}
	bool ValidWater(const FMOLiquidContents& Water)
	{
		return FMath::IsFinite(Water.Millilitres) && Water.Millilitres >= 0.0f
			&& FMath::IsFinite(Water.SalinityGPerL) && Water.SalinityGPerL >= 0.0f
			&& FMath::IsFinite(Water.IllnessChancePerLitre) && Water.IllnessChancePerLitre >= 0.0f;
	}
	bool SameWater(const FMOLiquidContents& A, const FMOLiquidContents& B)
	{
		return A.Source == B.Source && A.bBoiled == B.bBoiled && A.SalinityGPerL == B.SalinityGPerL
			&& A.IllnessChancePerLitre == B.IllnessChancePerLitre;
	}
	bool ValidLot(const FMOIngredientLot& Lot)
	{
		return !Lot.IngredientId.IsNone() && FMath::IsFinite(Lot.MeasureMl) && Lot.MeasureMl >= 0.0f
			&& ValidWater(Lot.State.Liquid) && (Lot.IsLiquid() ? Lot.Quantity == 0
				: (Lot.ItemGuid.IsValid() && !Lot.ItemDefinitionId.IsNone() && Lot.Quantity >= 0));
	}
	bool Exhausted(const FMOIngredientLot& Lot) { return ValidLot(Lot) && !Lot.HasContents(); }

}

FMOIngredientOperationResult CreateProducts(const TMap<FName, int32>& Quantities,
	FMOIngredientReservation& OutProducts, FDefinitionLookup Definition)
{
	if (!OutProducts.IsEmpty()) return Detail::Failure(EMOIngredientFailure::InvalidInput);
	FMOIngredientReservation Products;
	for (const auto& Pair : Quantities)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0) return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
		FMOItemDefinitionRow Item;
		if (!Definition(Pair.Key, Item)) return Detail::Failure(EMOIngredientFailure::UnknownDefinition, Pair.Key);
		if (!FMath::IsFinite(Item.LiquidMeasureMl) || Item.LiquidMeasureMl < 0) return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
		FMOIngredientLot Lot;
		Lot.IngredientId = Pair.Key;
		if (Item.LiquidMeasureMl > 0)
		{
			Lot.MeasureMl = Item.LiquidMeasureMl;
			Lot.bMayMixCompatibleLiquids = true;
			Lot.State.Liquid.Millilitres = Pair.Value * Item.LiquidMeasureMl;
			if (!FMath::IsFinite(Lot.State.Liquid.Millilitres)) return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
			// Existing definition policy for freshly produced water. This never reconstructs a refund.
			Lot.State.Liquid.bBoiled = Item.bMeasureNeedsSafeWater;
			if (!Item.bMeasureNeedsSafeWater)
			{
				const auto Profile = UMOWaterSourceSettings::GetProfile(EMOWaterBodyKind::Pond);
				Lot.State.Liquid.Source = EMOWaterBodyKind::Pond;
				Lot.State.Liquid.SalinityGPerL = Profile.SalinityGPerL;
				Lot.State.Liquid.IllnessChancePerLitre = Profile.IllnessChancePerLitre;
			}
		}
		else
		{
			Lot.ItemDefinitionId = Pair.Key;
			Lot.ItemGuid = FGuid::NewGuid();
			Lot.Quantity = Pair.Value;
			if (Item.bIsTool && Item.MaxDurability > 0) Lot.State.Durability = Item.MaxDurability;
		}
		Products.Lots.Add(MoveTemp(Lot));
	}
	OutProducts = MoveTemp(Products);
	return {};
}

FMOIngredientOperationResult Reserve(TArray<FMOInventoryEntry>& Entries, const TMap<FName, int32>& Requirements,
	FMOIngredientReservation& OutReservation, FDefinitionLookup Definition, const TSet<FGuid>* ExcludedPhysicalItems)
{
	if (!OutReservation.IsEmpty()) return Detail::Failure(EMOIngredientFailure::InvalidInput);
	TSet<FGuid> Identities;
	for (const auto& Entry : Entries)
	{
		if (!Entry.ItemGuid.IsValid() || Entry.Quantity <= 0 || !Detail::ValidWater(Entry.Liquid)) return Detail::Failure(EMOIngredientFailure::InvalidInput);
		if (Identities.Contains(Entry.ItemGuid)) return Detail::Failure(EMOIngredientFailure::IdentityConflict);
		Identities.Add(Entry.ItemGuid);
	}
	struct FRequest { FName Id; int32 Units; FMOItemDefinitionRow Definition; };
	TArray<FRequest> Requests;
	for (const auto& Pair : Requirements)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0) return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
		FRequest Request;
		Request.Id = Pair.Key;
		Request.Units = Pair.Value;
		if (!Definition(Pair.Key, Request.Definition)) return Detail::Failure(EMOIngredientFailure::UnknownDefinition, Pair.Key);
		if (!FMath::IsFinite(Request.Definition.LiquidMeasureMl) || Request.Definition.LiquidMeasureMl < 0)
			return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
		Requests.Add(MoveTemp(Request));
	}
	Requests.Sort([](const FRequest& A, const FRequest& B)
	{
		// A physical container consumed as an ingredient cannot also supply separately reserved water.
		auto Priority = [](const FRequest& Request) { return Request.Definition.LiquidMeasureMl <= 0 ? 0 : (Request.Definition.bMeasureNeedsSafeWater ? 1 : 2); };
		return Priority(A) == Priority(B) ? A.Id.LexicalLess(B.Id) : Priority(A) < Priority(B);
	});
	TArray<FMOInventoryEntry> Working = Entries;
	FMOIngredientReservation Reserved;
	for (const FRequest& Request : Requests)
	{
		int32 Remaining = Request.Units;
		for (auto& Entry : Working)
		{
			if (Remaining <= 0) break;
			if (Entry.ItemDefinitionId != Request.Id || Entry.Quantity <= 0) continue;
			if (ExcludedPhysicalItems && ExcludedPhysicalItems->Contains(Entry.ItemGuid)) continue;
			const int32 Taken = FMath::Min(Remaining, Entry.Quantity);
			FMOIngredientLot Lot;
			Lot.IngredientId = Request.Id;
			Lot.ItemDefinitionId = Entry.ItemDefinitionId;
			Lot.ItemGuid = Taken == Entry.Quantity ? Entry.ItemGuid : FGuid::NewGuid();
			Lot.Quantity = Taken;
			Lot.State.Durability = Entry.CurrentDurability;
			Lot.State.Liquid = Entry.Liquid;
			Reserved.Lots.Add(MoveTemp(Lot));
			Entry.Quantity -= Taken;
			Remaining -= Taken;
		}
		if (Remaining <= 0) continue;
		const float Measure = Request.Definition.LiquidMeasureMl;
		if (Measure <= 0) return Detail::Failure(EMOIngredientFailure::MissingIngredients, Request.Id, Remaining);
		TArray<int32> Containers;
		for (int32 Index = 0; Index < Working.Num(); ++Index)
		{
			const auto& Entry = Working[Index];
			FMOItemDefinitionRow Container;
			if (Entry.Quantity > 0 && Entry.Liquid.IsSuitableMeasure(Request.Definition.bMeasureNeedsSafeWater)
				&& Definition(Entry.ItemDefinitionId, Container) && Container.LiquidCapacityMl > 0) Containers.Add(Index);
		}
		Containers.Sort([&Working](int32 A, int32 B) { return Working[A].Liquid.Millilitres > Working[B].Liquid.Millilitres; });
		double NeedMl = double(Remaining) * Measure;
		for (int32 Index : Containers)
		{
			if (NeedMl <= 0.0) break;
			auto& Entry = Working[Index];
			const float Taken = float(FMath::Min(NeedMl, double(Entry.Liquid.Millilitres)));
			FMOIngredientLot Lot;
			Lot.IngredientId = Request.Id;
			Lot.SourceContainerGuid = Entry.ItemGuid;
			Lot.MeasureMl = Measure;
			Lot.State.Liquid = Entry.Liquid;
			Lot.State.Liquid.Millilitres = Taken;
			Reserved.Lots.Add(MoveTemp(Lot));
			Entry.Liquid.Millilitres = FMath::Max(0.0f, Entry.Liquid.Millilitres - Taken);
			NeedMl -= Taken;
			// Keep even sub-ml remainders; they still exist and must survive a refund/save.
			if (Entry.Liquid.Millilitres == 0.0f) Entry.Liquid = FMOLiquidContents();
		}
		if (NeedMl > MeasureToleranceMl(Measure))
			return Detail::Failure(EMOIngredientFailure::MissingIngredients, Request.Id, FMath::CeilToInt(NeedMl / Measure));
	}
	Working.RemoveAll([](const auto& Entry) { return Entry.Quantity <= 0; });
	Entries = MoveTemp(Working);
	OutReservation = MoveTemp(Reserved);
	return {};
}

FMOIngredientOperationResult Consume(FMOIngredientReservation& Reservation, const TMap<FName, int32>& Requirements)
{
	FMOIngredientReservation Working = Reservation;
	for (const auto& Lot : Working.Lots)
		if (!Detail::ValidLot(Lot)) return Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
	for (const auto& Pair : Requirements)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0) return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
		double Need = Pair.Value, Tolerance = 0.0;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (auto& Lot : Working.Lots)
			{
				if (Lot.IngredientId != Pair.Key || Lot.IsLiquid() != (Pass == 1) || !Lot.HasContents() || Need <= 0.0) continue;
				if (Lot.IsLiquid())
				{
					if (!FMath::IsFinite(Lot.MeasureMl) || !Detail::ValidWater(Lot.State.Liquid)) return Detail::Failure(EMOIngredientFailure::InvalidInput, Pair.Key);
					const float TakenMl = float(FMath::Min(Need * Lot.MeasureMl, double(Lot.State.Liquid.Millilitres)));
					Lot.State.Liquid.Millilitres = FMath::Max(0.0f, Lot.State.Liquid.Millilitres - TakenMl);
					Need -= double(TakenMl) / Lot.MeasureMl;
					Tolerance = FMath::Max(Tolerance, MeasureToleranceMl(Lot.MeasureMl) / Lot.MeasureMl);
				}
				else
				{
					const int32 Taken = FMath::Min(Lot.Quantity, FMath::FloorToInt(Need));
					Lot.Quantity -= Taken;
					Need -= Taken;
				}
			}
		}
		if (Need > Tolerance) return Detail::Failure(EMOIngredientFailure::MissingIngredients, Pair.Key, FMath::CeilToInt(Need));
	}
	Working.Lots.RemoveAll(Detail::Exhausted);
	Reservation = MoveTemp(Working);
	return {};
}

FMOIngredientOperationResult CarrySourceContainers(TArray<FMOInventoryEntry>& SourceEntries,
	FMOIngredientReservation& Transfer, TArray<FGuid>& OutCarriedContainers, FDefinitionLookup Definition)
{
	TArray<FMOInventoryEntry> Working = SourceEntries;
	FMOIngredientReservation Carried = Transfer;
	TArray<FGuid> Containers;
	for (const auto& Lot : Transfer.Lots)
	{
		if (!Detail::ValidLot(Lot)) return Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
		if (Lot.IsLiquid() && Lot.HasContents())
		{
			if (!Lot.SourceContainerGuid.IsValid()) return Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
			Containers.AddUnique(Lot.SourceContainerGuid);
		}
	}
	for (const FGuid& Guid : Containers)
	{
		const int32 Index = Working.IndexOfByPredicate([&Guid](const auto& Entry) { return Entry.ItemGuid == Guid; });
		if (Index == INDEX_NONE || Working[Index].Quantity != 1) return Detail::Failure(EMOIngredientFailure::IdentityConflict);
		const auto Entry = Working[Index];
		FMOItemDefinitionRow Vessel;
		if (!Definition(Entry.ItemDefinitionId, Vessel)) return Detail::Failure(EMOIngredientFailure::UnknownDefinition, Entry.ItemDefinitionId);
		if (!FMath::IsFinite(Vessel.LiquidCapacityMl) || Vessel.LiquidCapacityMl <= 0.0f || !Detail::ValidWater(Entry.Liquid))
			return Detail::Failure(EMOIngredientFailure::InvalidInput, Entry.ItemDefinitionId);
		FMOLiquidContents Contents = Entry.Liquid;
		for (const auto& Lot : Transfer.Lots)
		{
			if (!Lot.IsLiquid() || Lot.SourceContainerGuid != Guid || !Lot.HasContents()) continue;
			if (Contents.Millilitres > 0.0f && !Detail::SameWater(Contents, Lot.State.Liquid))
				return Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
			const float Total = Contents.Millilitres + Lot.State.Liquid.Millilitres;
			if (!FMath::IsFinite(Total)) return Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
			Contents = Lot.State.Liquid;
			Contents.Millilitres = Total;
		}
		FMOIngredientLot VesselLot;
		VesselLot.IngredientId = Entry.ItemDefinitionId;
		VesselLot.ItemDefinitionId = Entry.ItemDefinitionId;
		VesselLot.ItemGuid = Entry.ItemGuid;
		VesselLot.Quantity = 1;
		VesselLot.State.Durability = Entry.CurrentDurability;
		VesselLot.State.Liquid = Contents;
		Carried.Lots.RemoveAll([&Guid](const auto& Lot) { return Lot.IsLiquid() && Lot.SourceContainerGuid == Guid; });
		Carried.Lots.Add(MoveTemp(VesselLot));
		Working.RemoveAt(Index);
	}
	SourceEntries = MoveTemp(Working);
	Transfer = MoveTemp(Carried);
	OutCarriedContainers = MoveTemp(Containers);
	return {};
}

FMOIngredientOperationResult CarryItems(TArray<FMOInventoryEntry>& SourceEntries, FMOIngredientReservation& Transfer, const TArray<FGuid>& ItemGuids)
{
	TArray<FMOInventoryEntry> Working = SourceEntries;
	FMOIngredientReservation Carried = Transfer;
	TSet<FGuid> Seen;
	for (const auto& Guid : ItemGuids)
	{
		if (!Guid.IsValid() || Seen.Contains(Guid)) return Detail::Failure(EMOIngredientFailure::InvalidInput);
		Seen.Add(Guid);
		// CarrySourceContainers may already own this exact vessel, including the liquid requirements it supplies.
		if (Carried.Lots.ContainsByPredicate([&Guid](const auto& Lot) { return !Lot.IsLiquid() && Lot.ItemGuid == Guid; })) continue;
		const int32 Index = Working.IndexOfByPredicate([&Guid](const auto& Entry) { return Entry.ItemGuid == Guid; });
		if (Index == INDEX_NONE) return Detail::Failure(EMOIngredientFailure::IdentityConflict);
		const auto& Entry = Working[Index];
		if (Entry.Quantity <= 0 || Entry.ItemDefinitionId.IsNone() || !Detail::ValidWater(Entry.Liquid)) return Detail::Failure(EMOIngredientFailure::InvalidInput);
		FMOIngredientLot Lot;
		Lot.IngredientId = Entry.ItemDefinitionId;
		Lot.ItemDefinitionId = Entry.ItemDefinitionId;
		Lot.ItemGuid = Entry.ItemGuid;
		Lot.Quantity = Entry.Quantity;
		Lot.State.Durability = Entry.CurrentDurability;
		Lot.State.Liquid = Entry.Liquid;
		Carried.Lots.Add(MoveTemp(Lot));
		Working.RemoveAt(Index);
	}
	SourceEntries = MoveTemp(Working);
	Transfer = MoveTemp(Carried);
	return {};
}

FMOIngredientOperationResult Return(TArray<FMOInventoryEntry>& Entries, int32 SlotCount,
	FMOIngredientReservation& Reservation, FDefinitionLookup Definition)
{
	FMOIngredientOperationResult Result;
	for (auto& Lot : Reservation.Lots)
	{
		if (!Detail::ValidLot(Lot))
		{
			Result = Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
			continue;
		}
		if (!Lot.HasContents()) continue;
		if (Lot.IsLiquid())
		{
			// Original vessel first, then other empty/same-profile vessels. Returning water never silently spoils another profile.
			TArray<int32> Candidates;
			for (int32 Index = 0; Index < Entries.Num(); ++Index) Candidates.Add(Index);
			Candidates.StableSort([&Entries, &Lot](int32 A, int32 B)
			{
				auto Priority = [&Lot](const auto& Entry) { return Entry.ItemGuid == Lot.SourceContainerGuid ? 0 : (Entry.Liquid.Millilitres > 0 ? 1 : 2); };
				return Priority(Entries[A]) < Priority(Entries[B]);
			});
			for (int32 Index : Candidates)
			{
				if (Lot.State.Liquid.Millilitres <= 0.0f) break;
				auto& Entry = Entries[Index];
				FMOItemDefinitionRow Container;
				if (!Definition(Entry.ItemDefinitionId, Container) || !FMath::IsFinite(Container.LiquidCapacityMl)
					|| Container.LiquidCapacityMl <= 0.0f || !Detail::ValidWater(Entry.Liquid)) continue;
				if (Entry.Liquid.Millilitres > 0.0f && !Detail::SameWater(Entry.Liquid, Lot.State.Liquid)
					&& (!Lot.bMayMixCompatibleLiquids || Entry.Liquid.IsSuitableMeasure(true) != Lot.State.Liquid.IsSuitableMeasure(true))) continue;
				const float Placed = FMath::Min(Lot.State.Liquid.Millilitres, FMath::Max(0.0f, Container.LiquidCapacityMl - Entry.Liquid.Millilitres));
				if (Placed <= 0.0f) continue;
				if (Lot.bMayMixCompatibleLiquids && Entry.Liquid.Millilitres > 0.0f)
				{
					auto Part = Lot.State.Liquid;
					Part.Millilitres = Placed;
					Entry.Liquid.Mix(Part);
				}
				else
				{
					const float Total = Entry.Liquid.Millilitres + Placed;
					Entry.Liquid = Lot.State.Liquid;
					Entry.Liquid.Millilitres = Total;
				}
				Lot.State.Liquid.Millilitres = FMath::Max(0.0f, Lot.State.Liquid.Millilitres - Placed);
			}
		}
		else
		{
			FMOItemDefinitionRow Item;
			if (!Lot.ItemGuid.IsValid() || Lot.Quantity <= 0)
			{
				Result = Detail::Failure(EMOIngredientFailure::InvalidInput, Lot.IngredientId);
				continue;
			}
			if (!Definition(Lot.ItemDefinitionId, Item))
			{
				Result = Detail::Failure(EMOIngredientFailure::UnknownDefinition, Lot.IngredientId);
				continue;
			}
			if (Entries.ContainsByPredicate([&Lot](const auto& Entry) { return Entry.ItemGuid == Lot.ItemGuid; }))
			{
				Result = Detail::Failure(EMOIngredientFailure::IdentityConflict, Lot.IngredientId);
				continue;
			}
			const int32 MaxStack = UMOInventoryComponent::GetItemStackLimit(Item);
			if (MaxStack > 1)
			{
				for (auto& Entry : Entries)
				{
					if (Lot.Quantity <= 0) break;
					if (!UMOInventoryComponent::HasMatchingStackState(Entry, Lot.ItemDefinitionId, Lot.State)) continue;
					const int32 Placed = FMath::Min(Lot.Quantity, FMath::Max(0, MaxStack - Entry.Quantity));
					Entry.Quantity += Placed;
					Lot.Quantity -= Placed;
				}
			}
			while (Lot.Quantity > 0 && Entries.Num() < FMath::Max(1, SlotCount))
			{
				FMOInventoryEntry Entry;
				Entry.ItemGuid = Lot.ItemGuid;
				Entry.ItemDefinitionId = Lot.ItemDefinitionId;
				Entry.CurrentDurability = Lot.State.Durability;
				Entry.Liquid = Lot.State.Liquid;
				Entry.Quantity = FMath::Min(Lot.Quantity, MaxStack);
				Lot.Quantity -= Entry.Quantity;
				Entries.Add(MoveTemp(Entry));
				if (Lot.Quantity > 0) Lot.ItemGuid = FGuid::NewGuid();
			}
		}
	}
	Reservation.Lots.RemoveAll(Detail::Exhausted);
	if (!Reservation.IsEmpty() && Result.IsComplete()) Result = Detail::Failure(EMOIngredientFailure::Capacity, Reservation.Lots[0].IngredientId);
	return Result;
}
}
