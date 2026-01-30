#include "MOMedicalDatabaseSettings.h"
#include "Engine/DataTable.h"
#include "MOFramework.h"

const UMOMedicalDatabaseSettings* UMOMedicalDatabaseSettings::Get()
{
	return GetDefault<UMOMedicalDatabaseSettings>();
}

UDataTable* UMOMedicalDatabaseSettings::GetBodyPartDefinitionsTable() const
{
	if (BodyPartDefinitionsTable.IsNull())
	{
		return nullptr;
	}

	if (BodyPartDefinitionsTable.IsValid())
	{
		return BodyPartDefinitionsTable.Get();
	}

	return BodyPartDefinitionsTable.LoadSynchronous();
}

UDataTable* UMOMedicalDatabaseSettings::GetWoundTypeDefinitionsTable() const
{
	if (WoundTypeDefinitionsTable.IsNull())
	{
		return nullptr;
	}

	if (WoundTypeDefinitionsTable.IsValid())
	{
		return WoundTypeDefinitionsTable.Get();
	}

	return WoundTypeDefinitionsTable.LoadSynchronous();
}

UDataTable* UMOMedicalDatabaseSettings::GetConditionDefinitionsTable() const
{
	if (ConditionDefinitionsTable.IsNull())
	{
		return nullptr;
	}

	if (ConditionDefinitionsTable.IsValid())
	{
		return ConditionDefinitionsTable.Get();
	}

	return ConditionDefinitionsTable.LoadSynchronous();
}

UDataTable* UMOMedicalDatabaseSettings::GetMedicalTreatmentsTable() const
{
	if (MedicalTreatmentsTable.IsNull())
	{
		return nullptr;
	}

	if (MedicalTreatmentsTable.IsValid())
	{
		return MedicalTreatmentsTable.Get();
	}

	return MedicalTreatmentsTable.LoadSynchronous();
}

bool UMOMedicalDatabaseSettings::IsConfigured()
{
	const UMOMedicalDatabaseSettings* Settings = Get();
	if (!Settings)
	{
		return false;
	}

	// At minimum, body part definitions should be configured
	return !Settings->BodyPartDefinitionsTable.IsNull();
}

void UMOMedicalDatabaseSettings::ValidateConfiguration()
{
	const UMOMedicalDatabaseSettings* Settings = Get();
	if (!Settings)
	{
		return;
	}

	if (Settings->BodyPartDefinitionsTable.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("MO Medical Database: BodyPartDefinitionsTable is not configured. "
			"Configure it in Project Settings -> Plugins -> MO Medical Database."));
	}

	if (Settings->WoundTypeDefinitionsTable.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("MO Medical Database: WoundTypeDefinitionsTable is not configured. "
			"Wound types will use default values."));
	}

	if (Settings->ConditionDefinitionsTable.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("MO Medical Database: ConditionDefinitionsTable is not configured. "
			"Conditions will use default values."));
	}

	if (Settings->MedicalTreatmentsTable.IsNull())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("MO Medical Database: MedicalTreatmentsTable is not configured. "
			"Medical treatments will not be available."));
	}
}
