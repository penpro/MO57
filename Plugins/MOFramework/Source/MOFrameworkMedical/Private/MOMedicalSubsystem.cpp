#include "MOMedicalSubsystem.h"
#include "MOMedicalDatabaseSettings.h"
#include "Engine/DataTable.h"

void UMOMedicalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load DataTable references from settings
	if (const UMOMedicalDatabaseSettings* Settings = UMOMedicalDatabaseSettings::Get())
	{
		BodyPartDefinitionsTable = Settings->BodyPartDefinitionsTable;
		WoundTypeDefinitionsTable = Settings->WoundTypeDefinitionsTable;
		ConditionDefinitionsTable = Settings->ConditionDefinitionsTable;
		MedicalTreatmentsTable = Settings->MedicalTreatmentsTable;
	}

	// Caches will be built on first access
	bCachesBuilt = false;
}

void UMOMedicalSubsystem::Deinitialize()
{
	CachedBodyPartDefs.Empty();
	CachedWoundTypeDefs.Empty();
	CachedConditionDefs.Empty();
	CachedTreatmentDefs.Empty();

	Super::Deinitialize();
}

// ============================================================================
// BODY PART LOOKUPS
// ============================================================================

bool UMOMedicalSubsystem::GetBodyPartDefinition(EMOBodyPartType PartType, FMOBodyPartDefinitionRow& OutDefinition) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	if (const FMOBodyPartDefinitionRow* Found = CachedBodyPartDefs.Find(PartType))
	{
		OutDefinition = *Found;
		return true;
	}

	// Return defaults if not found
	OutDefinition = FMOBodyPartDefinitionRow();
	OutDefinition.PartType = PartType;
	OutDefinition.DisplayName = FText::FromString(UEnum::GetValueAsString(PartType));
	return false;
}

TArray<FMOBodyPartDefinitionRow> UMOMedicalSubsystem::GetAllBodyPartDefinitions() const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<FMOBodyPartDefinitionRow> Result;
	CachedBodyPartDefs.GenerateValueArray(Result);
	return Result;
}

TArray<EMOBodyPartType> UMOMedicalSubsystem::GetChildBodyParts(EMOBodyPartType ParentPart) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<EMOBodyPartType> Children;

	for (const auto& Pair : CachedBodyPartDefs)
	{
		if (Pair.Value.ParentPart == ParentPart)
		{
			Children.Add(Pair.Key);
		}
	}

	return Children;
}

bool UMOMedicalSubsystem::IsVitalBodyPart(EMOBodyPartType PartType) const
{
	FMOBodyPartDefinitionRow Def;
	if (GetBodyPartDefinition(PartType, Def))
	{
		return Def.bInstantDeathOnDestruction || Def.DeathTimerOnDestruction > 0.0f;
	}

	// Default vital parts if no DataTable
	switch (PartType)
	{
	case EMOBodyPartType::Brain:
	case EMOBodyPartType::Heart:
		return true;
	case EMOBodyPartType::LungLeft:
	case EMOBodyPartType::LungRight:
		return true;
	default:
		return false;
	}
}

// ============================================================================
// WOUND TYPE LOOKUPS
// ============================================================================

bool UMOMedicalSubsystem::GetWoundTypeDefinition(EMOWoundType WoundType, FMOWoundTypeDefinitionRow& OutDefinition) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	if (const FMOWoundTypeDefinitionRow* Found = CachedWoundTypeDefs.Find(WoundType))
	{
		OutDefinition = *Found;
		return true;
	}

	// Return defaults
	OutDefinition = FMOWoundTypeDefinitionRow();
	OutDefinition.WoundType = WoundType;
	OutDefinition.DisplayName = FText::FromString(UEnum::GetValueAsString(WoundType));
	return false;
}

TArray<FMOWoundTypeDefinitionRow> UMOMedicalSubsystem::GetAllWoundTypeDefinitions() const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<FMOWoundTypeDefinitionRow> Result;
	CachedWoundTypeDefs.GenerateValueArray(Result);
	return Result;
}

// ============================================================================
// CONDITION LOOKUPS
// ============================================================================

bool UMOMedicalSubsystem::GetConditionDefinition(EMOConditionType ConditionType, FMOConditionDefinitionRow& OutDefinition) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	if (const FMOConditionDefinitionRow* Found = CachedConditionDefs.Find(ConditionType))
	{
		OutDefinition = *Found;
		return true;
	}

	// Return defaults
	OutDefinition = FMOConditionDefinitionRow();
	OutDefinition.ConditionType = ConditionType;
	OutDefinition.DisplayName = FText::FromString(UEnum::GetValueAsString(ConditionType));
	return false;
}

TArray<FMOConditionDefinitionRow> UMOMedicalSubsystem::GetAllConditionDefinitions() const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<FMOConditionDefinitionRow> Result;
	CachedConditionDefs.GenerateValueArray(Result);
	return Result;
}

// ============================================================================
// TREATMENT LOOKUPS
// ============================================================================

bool UMOMedicalSubsystem::GetTreatmentDefinition(FName TreatmentId, FMOMedicalTreatmentRow& OutDefinition) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	if (const FMOMedicalTreatmentRow* Found = CachedTreatmentDefs.Find(TreatmentId))
	{
		OutDefinition = *Found;
		return true;
	}

	return false;
}

TArray<FMOMedicalTreatmentRow> UMOMedicalSubsystem::GetAllTreatmentDefinitions() const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<FMOMedicalTreatmentRow> Result;
	CachedTreatmentDefs.GenerateValueArray(Result);
	return Result;
}

TArray<FMOMedicalTreatmentRow> UMOMedicalSubsystem::GetTreatmentsForWoundType(EMOWoundType WoundType) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<FMOMedicalTreatmentRow> Result;

	for (const auto& Pair : CachedTreatmentDefs)
	{
		if (Pair.Value.TreatsWoundTypes.Contains(WoundType))
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

TArray<FMOMedicalTreatmentRow> UMOMedicalSubsystem::GetTreatmentsForCondition(EMOConditionType ConditionType) const
{
	const_cast<UMOMedicalSubsystem*>(this)->BuildCaches();

	TArray<FMOMedicalTreatmentRow> Result;

	for (const auto& Pair : CachedTreatmentDefs)
	{
		if (Pair.Value.TreatsConditions.Contains(ConditionType))
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

// ============================================================================
// CASCADE CALCULATIONS
// ============================================================================

void UMOMedicalSubsystem::CalculateWoundParameters(
	EMOWoundType WoundType,
	float Severity,
	EMOBodyPartType BodyPart,
	float& OutBleedRate,
	float& OutInfectionRisk,
	float& OutPain
) const
{
	// Get wound type definition
	FMOWoundTypeDefinitionRow WoundDef;
	GetWoundTypeDefinition(WoundType, WoundDef);

	// Get body part definition
	FMOBodyPartDefinitionRow PartDef;
	GetBodyPartDefinition(BodyPart, PartDef);

	// Calculate bleed rate
	// Base rate * severity * body part multiplier
	OutBleedRate = WoundDef.BaseBleedRate * (Severity / 100.0f) * PartDef.BleedMultiplier;

	// Calculate infection risk
	// Base risk * body part multiplier
	OutInfectionRisk = WoundDef.BaseInfectionRisk * PartDef.InfectionMultiplier;

	// Calculate pain
	// Severity * pain multiplier
	OutPain = Severity * WoundDef.PainMultiplier;
}

float UMOMedicalSubsystem::CalculateTreatmentEffectiveness(
	FName TreatmentId,
	int32 MedicSkillLevel,
	bool bIsSelfTreatment,
	EMOBodyPartType BodyPart
) const
{
	FMOMedicalTreatmentRow Treatment;
	if (!GetTreatmentDefinition(TreatmentId, Treatment))
	{
		return 0.0f;  // Unknown treatment
	}

	// Check minimum skill requirement
	if (MedicSkillLevel < Treatment.MinimumSkillLevel)
	{
		return 0.0f;  // Not skilled enough
	}

	// Base effectiveness
	float Effectiveness = 1.0f;

	// Skill scaling
	// Higher skill = better results
	float SkillBonus = (MedicSkillLevel - Treatment.MinimumSkillLevel) * Treatment.QualitySkillScaling;
	Effectiveness += SkillBonus;

	// Self-treatment penalty
	if (bIsSelfTreatment)
	{
		// Check if body part is unreachable for self
		if (Treatment.UnreachableForSelf.Contains(BodyPart))
		{
			return 0.0f;  // Can't self-treat this area
		}

		Effectiveness *= (1.0f - Treatment.SelfTreatmentPenalty);
	}

	return FMath::Max(0.0f, Effectiveness);
}

float UMOMedicalSubsystem::CalculateHealingRateMultiplier(
	float NutritionMultiplier,
	bool bIsInfected,
	bool bIsBandaged,
	bool bIsSutured
) const
{
	float Multiplier = NutritionMultiplier;

	// Infection significantly slows healing
	if (bIsInfected)
	{
		Multiplier *= 0.25f;
	}

	// Bandaging helps
	if (bIsBandaged)
	{
		Multiplier *= 1.5f;
	}

	// Suturing helps deep wounds heal properly
	if (bIsSutured)
	{
		Multiplier *= 2.0f;
	}

	return FMath::Max(0.1f, Multiplier);
}

// ============================================================================
// UTILITY
// ============================================================================

FText UMOMedicalSubsystem::GetBodyPartDisplayName(EMOBodyPartType PartType) const
{
	FMOBodyPartDefinitionRow Def;
	if (GetBodyPartDefinition(PartType, Def))
	{
		return Def.DisplayName;
	}

	// Fallback to enum name
	return FText::FromString(UEnum::GetValueAsString(PartType).Replace(TEXT("EMOBodyPartType::"), TEXT("")));
}

FText UMOMedicalSubsystem::GetWoundTypeDisplayName(EMOWoundType WoundType) const
{
	FMOWoundTypeDefinitionRow Def;
	if (GetWoundTypeDefinition(WoundType, Def))
	{
		return Def.DisplayName;
	}

	return FText::FromString(UEnum::GetValueAsString(WoundType).Replace(TEXT("EMOWoundType::"), TEXT("")));
}

FText UMOMedicalSubsystem::GetConditionDisplayName(EMOConditionType ConditionType) const
{
	FMOConditionDefinitionRow Def;
	if (GetConditionDefinition(ConditionType, Def))
	{
		return Def.DisplayName;
	}

	return FText::FromString(UEnum::GetValueAsString(ConditionType).Replace(TEXT("EMOConditionType::"), TEXT("")));
}

FText UMOMedicalSubsystem::GetConsciousnessDisplayName(EMOConsciousnessLevel Level) const
{
	switch (Level)
	{
	case EMOConsciousnessLevel::Alert:
		return FText::FromString(TEXT("Alert"));
	case EMOConsciousnessLevel::Confused:
		return FText::FromString(TEXT("Confused"));
	case EMOConsciousnessLevel::Drowsy:
		return FText::FromString(TEXT("Drowsy"));
	case EMOConsciousnessLevel::Unconscious:
		return FText::FromString(TEXT("Unconscious"));
	case EMOConsciousnessLevel::Comatose:
		return FText::FromString(TEXT("Comatose"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

FText UMOMedicalSubsystem::GetBloodLossStageDisplayName(EMOBloodLossStage Stage) const
{
	switch (Stage)
	{
	case EMOBloodLossStage::None:
		return FText::FromString(TEXT("Normal"));
	case EMOBloodLossStage::Class1:
		return FText::FromString(TEXT("Class I (15-30%)"));
	case EMOBloodLossStage::Class2:
		return FText::FromString(TEXT("Class II (30-40%)"));
	case EMOBloodLossStage::Class3:
		return FText::FromString(TEXT("Class III (>40%)"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UMOMedicalSubsystem::BuildCaches()
{
	if (bCachesBuilt)
	{
		return;
	}

	// Load and cache body part definitions
	if (UDataTable* BodyPartTable = LoadDataTable(BodyPartDefinitionsTable))
	{
		TArray<FMOBodyPartDefinitionRow*> Rows;
		BodyPartTable->GetAllRows(TEXT(""), Rows);

		for (FMOBodyPartDefinitionRow* Row : Rows)
		{
			if (Row && Row->PartType != EMOBodyPartType::None)
			{
				CachedBodyPartDefs.Add(Row->PartType, *Row);
			}
		}
	}

	// Load and cache wound type definitions
	if (UDataTable* WoundTable = LoadDataTable(WoundTypeDefinitionsTable))
	{
		TArray<FMOWoundTypeDefinitionRow*> Rows;
		WoundTable->GetAllRows(TEXT(""), Rows);

		for (FMOWoundTypeDefinitionRow* Row : Rows)
		{
			if (Row && Row->WoundType != EMOWoundType::None)
			{
				CachedWoundTypeDefs.Add(Row->WoundType, *Row);
			}
		}
	}

	// Load and cache condition definitions
	if (UDataTable* ConditionTable = LoadDataTable(ConditionDefinitionsTable))
	{
		TArray<FMOConditionDefinitionRow*> Rows;
		ConditionTable->GetAllRows(TEXT(""), Rows);

		for (FMOConditionDefinitionRow* Row : Rows)
		{
			if (Row && Row->ConditionType != EMOConditionType::None)
			{
				CachedConditionDefs.Add(Row->ConditionType, *Row);
			}
		}
	}

	// Load and cache treatment definitions
	if (UDataTable* TreatmentTable = LoadDataTable(MedicalTreatmentsTable))
	{
		TArray<FMOMedicalTreatmentRow*> Rows;
		TreatmentTable->GetAllRows(TEXT(""), Rows);

		for (FMOMedicalTreatmentRow* Row : Rows)
		{
			if (Row && !Row->TreatmentId.IsNone())
			{
				CachedTreatmentDefs.Add(Row->TreatmentId, *Row);
			}
		}
	}

	bCachesBuilt = true;
}

UDataTable* UMOMedicalSubsystem::LoadDataTable(TSoftObjectPtr<UDataTable>& TablePtr) const
{
	if (TablePtr.IsNull())
	{
		return nullptr;
	}

	if (TablePtr.IsValid())
	{
		return TablePtr.Get();
	}

	// Synchronous load
	return TablePtr.LoadSynchronous();
}
