#include "MOItemDatabaseSettings.h"

#include "Engine/DataTable.h"

UDataTable* UMOItemDatabaseSettings::GetItemDefinitionsDataTable() const
{
	return ItemDefinitionsDataTable.LoadSynchronous();
}
