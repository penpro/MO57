#include "MOWorldItem.h"
#include "MOFramework.h"
#include "Engine/DataTable.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "MOIdentityComponent.h"
#include "MOInteractableComponent.h"
#include "MOItemComponent.h"

AMOWorldItem::AMOWorldItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Only enable when drop physics is active
	bReplicates = true;

	// Item mesh is the root - this allows physics to move the whole actor
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	RootComponent = ItemMesh;
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ItemMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ItemMesh->SetSimulatePhysics(false); // Disabled by default, enabled on drop

	// Interaction collision sphere - attached to mesh so it moves with physics
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(ItemMesh);
	InteractionSphere->SetSphereRadius(InteractionSphereRadius);
	InteractionSphere->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f)); // Centered on mesh
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetGenerateOverlapEvents(false);

	// Scene root kept for compatibility but not used as root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(ItemMesh);

	// MO Components
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("IdentityComponent"));
	ItemComponent = CreateDefaultSubobject<UMOItemComponent>(TEXT("ItemComponent"));
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("InteractableComponent"));

	// Bind to OnInteracted to handle the interaction
	InteractableComponent->OnHandleInteract.BindUObject(this, &AMOWorldItem::OnHandleInteract);
}

bool AMOWorldItem::OnHandleInteract(AController* InteractorController)
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] OnHandleInteract called for '%s'"), *GetName());

	if (!InteractorController)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] OnHandleInteract: No controller"));
		return false;
	}

	bool bSuccess = false;

	if (bAddToInventoryOnInteract && ItemComponent)
	{
		bSuccess = ItemComponent->GiveToInteractorInventory(InteractorController);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] GiveToInteractorInventory returned %s"), bSuccess ? TEXT("true") : TEXT("false"));

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

#if WITH_EDITOR
void AMOWorldItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyItemDefinitionToWorldMesh();
}

void AMOWorldItem::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// This catches changes to subobject properties (like ItemComponent->ItemDefinitionId)
	ApplyItemDefinitionToWorldMesh();
}
#endif

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
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] No item definitions DataTable set (ActorOverride empty and Settings empty). ItemDefinitionId=%s"), *ItemDefinitionId.ToString());
		return false;
	}

	const FMOItemDefinitionRow* ItemDefinitionRow = ItemDefinitionsTable->FindRow<FMOItemDefinitionRow>(ItemDefinitionId, TEXT("MOWorldItem::ApplyItemDefinitionToWorldMesh"), false);
	if (!ItemDefinitionRow)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] ItemDefinitionId '%s' not found in DataTable '%s'"), *ItemDefinitionId.ToString(), *GetNameSafe(ItemDefinitionsTable));
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
	// Don't set physics from definition here - let EnableDropPhysics control it when dropped
	// ItemMesh->SetSimulatePhysics(ItemDefinitionRow->WorldVisual.bSimulatePhysics);

	// Optionally set the replicated DisplayName from the definition.
	if (HasAuthority() && IsValid(IdentityComponent) && !ItemDefinitionRow->DisplayName.IsEmpty())
	{
		IdentityComponent->SetDisplayName(ItemDefinitionRow->DisplayName);
	}

	return true;
}

void AMOWorldItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDropPhysicsActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float ElapsedTime = CurrentTime - DropPhysicsStartTime;

	// Log periodically (every 0.5s) to avoid spam
	static TMap<AActor*, float> LastLogTimes;
	float& LastLogTime = LastLogTimes.FindOrAdd(this);
	if (CurrentTime - LastLogTime > 0.5f)
	{
		LastLogTime = CurrentTime;
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] Tick: %s at %s, Elapsed=%.2fs, IsSimulating=%s"),
			*GetName(), *GetActorLocation().ToString(), ElapsedTime,
			(IsValid(ItemMesh) && ItemMesh->IsSimulatingPhysics()) ? TEXT("true") : TEXT("false"));
	}

	// Check if timeout has expired - always run physics for the full duration
	if (ElapsedTime >= DropPhysicsTimeout)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] Drop physics timeout reached (%.1fs), settling on ground"), ElapsedTime);
		SettleOnGround();
		return;
	}

	// No early rest check - let physics run for full duration so item can bounce/roll naturally
}

void AMOWorldItem::EnableDropPhysics()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics called for %s at location %s"),
		*GetName(), *GetActorLocation().ToString());

	if (!IsValid(ItemMesh))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics: ItemMesh is INVALID"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics: No World"));
		return;
	}

	UStaticMesh* CurrentMesh = ItemMesh->GetStaticMesh();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics: ItemMesh has StaticMesh=%s, SimulatePhysics=%s"),
		IsValid(CurrentMesh) ? *CurrentMesh->GetName() : TEXT("NULL"),
		ItemMesh->IsSimulatingPhysics() ? TEXT("true") : TEXT("false"));

	// Enable physics simulation - collision is already set up in constructor
	ItemMesh->SetSimulatePhysics(true);

	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics: After SetSimulatePhysics(true), IsSimulating=%s"),
		ItemMesh->IsSimulatingPhysics() ? TEXT("true") : TEXT("false"));

	// Track drop physics state
	bDropPhysicsActive = true;
	DropPhysicsStartTime = World->GetTimeSeconds();

	// Enable tick to monitor physics
	SetActorTickEnabled(true);

	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics complete: bDropPhysicsActive=%s, StartTime=%.2f"),
		bDropPhysicsActive ? TEXT("true") : TEXT("false"), DropPhysicsStartTime);
}

void AMOWorldItem::SettleOnGround()
{
	UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] SettleOnGround called for %s at %s"),
		*GetName(), *GetActorLocation().ToString());

	bDropPhysicsActive = false;
	SetActorTickEnabled(false);

	if (!IsValid(ItemMesh))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] SettleOnGround: ItemMesh invalid"));
		return;
	}

	// Disable physics simulation
	ItemMesh->SetSimulatePhysics(false);

	// Get current location
	FVector CurrentLocation = GetActorLocation();

	// Trace down to find the ground
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		const FVector TraceStart = CurrentLocation;
		const FVector TraceEnd = CurrentLocation - FVector(0.0f, 0.0f, 1000.0f);

		if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			// Check if we're already close to the ground (within 50cm)
			const float DistanceToGround = FVector::Dist(CurrentLocation, HitResult.Location);
			if (DistanceToGround > 50.0f)
			{
				// Teleport to just above the ground
				const FVector GroundLocation = HitResult.Location + FVector(0.0f, 0.0f, 10.0f);
				SetActorLocation(GroundLocation);
				UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] Settled on ground at %s (was %.1fcm above)"),
					*GroundLocation.ToString(), DistanceToGround);
			}
			else
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] Already on ground (%.1fcm away)"), DistanceToGround);
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] No ground found below item at %s"), *CurrentLocation.ToString());
		}
	}

	// Keep collision enabled for interaction traces but disable physics response
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
