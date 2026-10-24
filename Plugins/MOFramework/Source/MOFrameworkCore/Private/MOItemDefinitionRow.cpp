// Fill out your copyright notice in the Description page of Project Settings.


#include "MOItemDefinitionRow.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult FMOItemDefinitionRow::IsDataValid(FDataValidationContext& Context) const
{
	// The editor's data validation runs this for every row when DT_Items is saved or validated (DECISIONS F38).
	if (IsEquippableWithoutSlot())
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s is equippable but has no EquipmentSlotType, so no slot can take it: give it a slot, or clear bEquippable (DECISIONS F38)"),
			*ItemId.ToString())));
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}
#endif
