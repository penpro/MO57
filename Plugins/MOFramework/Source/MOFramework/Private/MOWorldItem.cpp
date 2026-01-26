#include "MOWorldItem.h"
#include "Engine/DataTable.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"

#include "Components/StaticMeshComponent.h"
#include "MOIdentityComponent.h"
#include "MOInteractableComponent.h"
#include "MOItemComponent.h"

AMOWorldItem::AMOWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Visual mesh - must block visibility for interaction traces
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootComponent);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ItemMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ItemMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// MO Components
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("IdentityComponent"));
	ItemComponent = CreateDefaultSubobject<UMOItemComponent>(TEXT("ItemComponent"));
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("InteractableComponent"));

	// Bind to OnInteracted to handle the interaction
	InteractableComponent->OnHandleInteract.BindUObject(this, &AMOWorldItem::OnHandleInteract);
}

bool AMOWorldItem::OnHandleInteract(AController* InteractorController)
{
	UE_LOG(LogTemp, Warning, TEXT("[MOWorldItem] OnHandleInteract called for '%s'"), *GetName());

	if (!InteractorController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOWorldItem] OnHandleInteract: No controller"));
		return false;
	}

	bool bSuccess = false;

	if (bAddToInventoryOnInteract && ItemComponent)
	{
		bSuccess = ItemComponent->GiveToInteractorInventory(InteractorController);
		UE_LOG(LogTemp, Warning, TEXT("[MOWorldItem] GiveToInteractorInventory returned %s"), bSuccess ? TEXT("true") : TEXT("false"));

		if (bSuccess)
		{
			if (bHideOnPickup)
			{
				SetActorHiddenInGame(true);
				SetActorEnableCollision(false);
			}

			if (bDestroyAfterPickup)
			{
				Destroy();
			}
		}
	}

	return bSuccess;
}
namespace
{
	UDataTable* ResolveItemDefinitionsDataTable(const TSoftObjectPtr<UDataTable>& ActorOverrideDataTable)
	{
		if (!ActorOverrideDataTable.IsNull())
		{
			if (UDataTable* LoadedOverride = ActorOverrideDataTable.LoadSynchronous())
			{
				return LoadedOverride;
			}
		}

		const UMOItemDatabaseSettings* ItemDefinitionsSettings = GetDefault<UMOItemDatabaseSettings>();
		if (IsValid(ItemDefinitionsSettings))
		{
			return ItemDefinitionsSettings->GetItemDefinitionsDataTable();
		}

		return nullptr;
	}
}

void AMOWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(ItemComponent))
	{
		ItemComponent->OnItemDefinitionIdChanged.AddDynamic(this, &AMOWorldItem::HandleItemDefinitionIdChanged);
	}

	ApplyItemDefinitionToWorldMesh();
}

void AMOWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyItemDefinitionToWorldMesh();
}

void AMOWorldItem::HandleItemDefinitionIdChanged(FName NewItemDefinitionId)
{
	ApplyItemDefinitionToWorldMesh();
}

bool AMOWorldItem::ApplyItemDefinitionToWorldMesh()
{
	if (!IsValid(ItemComponent) || !IsValid(ItemMesh))
	{
		return false;
	}

	const FName ItemDefinitionId = ItemComponent->ItemDefinitionId;
	if (ItemDefinitionId.IsNone())
	{
		return false;
	}

	UDataTable* ItemDefinitionsTable = ResolveItemDefinitionsDataTable(ItemDefinitionsDataTable);
	if (!IsValid(ItemDefinitionsTable))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOWorldItem] No item definitions DataTable set (ActorOverride empty and Settings empty). ItemDefinitionId=%s"), *ItemDefinitionId.ToString());
		return false;
	}

	const FMOItemDefinitionRow* ItemDefinitionRow = ItemDefinitionsTable->FindRow<FMOItemDefinitionRow>(ItemDefinitionId, TEXT("MOWorldItem::ApplyItemDefinitionToWorldMesh"), false);
	if (!ItemDefinitionRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOWorldItem] ItemDefinitionId '%s' not found in DataTable '%s'"), *ItemDefinitionId.ToString(), *GetNameSafe(ItemDefinitionsTable));
		return false;
	}

	if (!ItemDefinitionRow->WorldVisual.StaticMesh.IsNull())
	{
		UStaticMesh* LoadedMesh = ItemDefinitionRow->WorldVisual.StaticMesh.LoadSynchronous();
		if (IsValid(LoadedMesh))
		{
			ItemMesh->SetStaticMesh(LoadedMesh);
		}
	}

	if (!ItemDefinitionRow->WorldVisual.MaterialOverride.IsNull())
	{
		UMaterialInterface* LoadedMaterial = ItemDefinitionRow->WorldVisual.MaterialOverride.LoadSynchronous();
		if (IsValid(LoadedMaterial))
		{
			ItemMesh->SetMaterial(0, LoadedMaterial);
		}
	}

	ItemMesh->SetRelativeTransform(ItemDefinitionRow->WorldVisual.RelativeTransform);
	ItemMesh->SetSimulatePhysics(ItemDefinitionRow->WorldVisual.bSimulatePhysics);

	// Optionally set the replicated DisplayName from the definition.
	if (HasAuthority() && IsValid(IdentityComponent) && !ItemDefinitionRow->DisplayName.IsEmpty())
	{
		IdentityComponent->SetDisplayName(ItemDefinitionRow->DisplayName);
	}

	return true;
}
