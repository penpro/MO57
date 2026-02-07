#include "MOWorldItem.h"
#include "MOFramework.h"
#include "Engine/DataTable.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodySetup.h"
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

bool AMOWorldItem::IsPickupable() const
{
	// Not pickupable if hidden in game
	if (IsHidden())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s is hidden"), *GetName());
		return false;
	}

	// Not pickupable if collision is disabled (already picked up)
	if (!GetActorEnableCollision())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s has collision disabled"), *GetName());
		return false;
	}

	// Must have a valid item component with item data
	if (!IsValid(ItemComponent))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s has invalid ItemComponent"), *GetName());
		return false;
	}

	// Must have a valid item definition
	if (ItemComponent->ItemDefinitionId.IsNone())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s has no ItemDefinitionId"), *GetName());
		return false;
	}

	return true;
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

	// Only apply scale and rotation from definition, NOT location
	// Location would reset the actor position since ItemMesh is the root component
	const FTransform& DefTransform = ItemDefinitionRow->WorldVisual.RelativeTransform;
	ItemMesh->SetRelativeScale3D(DefTransform.GetScale3D());
	ItemMesh->SetRelativeRotation(DefTransform.GetRotation().Rotator());
	// Don't call: ItemMesh->SetRelativeLocation(DefTransform.GetLocation());
	// This would move the entire actor since ItemMesh is root

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

	// Ensure the mesh has collision - use complex as simple if no simple collision exists
	if (IsValid(CurrentMesh))
	{
		UBodySetup* BodySetup = CurrentMesh->GetBodySetup();
		// Check if mesh has simple collision
		if (!BodySetup || BodySetup->AggGeom.GetElementCount() == 0)
		{
			// No simple collision, use complex collision for physics
			ItemMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
			// In UE5, set complex as simple via the body setup collision type
			if (BodySetup)
			{
				BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics: Mesh has no simple collision, using complex as simple"));
		}
	}

	// Ensure collision is enabled before physics
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ItemMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ItemMesh->SetCollisionObjectType(ECC_PhysicsBody);

	// Find a safe spawn location - trace down to find ground and position above it
	FVector CurrentLocation = GetActorLocation();
	FHitResult GroundHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// Add random horizontal spread to prevent items stacking on each other
	const float RandomSpreadRadius = 50.0f;  // 50cm spread radius
	const float RandomAngle = FMath::RandRange(0.0f, 360.0f);
	const float RandomDistance = FMath::RandRange(0.0f, RandomSpreadRadius);
	const FVector RandomOffset(
		FMath::Cos(FMath::DegreesToRadians(RandomAngle)) * RandomDistance,
		FMath::Sin(FMath::DegreesToRadians(RandomAngle)) * RandomDistance,
		0.0f
	);

	// Random drop height between 50cm and 100cm
	const float RandomDropHeight = FMath::RandRange(50.0f, 100.0f);

	// Trace down to find the ground (from the randomized horizontal position)
	const FVector TraceStart = CurrentLocation + RandomOffset + FVector(0, 0, 150.0f);
	const FVector TraceEnd = CurrentLocation + RandomOffset - FVector(0, 0, 500.0f);

	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		// Position the item at random height above the ground
		FVector SafeLocation = GroundHit.Location + FVector(0, 0, RandomDropHeight);
		SetActorLocation(SafeLocation);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] EnableDropPhysics: Positioned at %s (%.0fcm above ground, spread %.0fcm)"),
			*SafeLocation.ToString(), RandomDropHeight, RandomDistance);
	}
	else
	{
		// No ground found, just apply random offset to current location
		SetActorLocation(CurrentLocation + RandomOffset + FVector(0, 0, RandomDropHeight));
	}

	// Force recreation of physics state to ensure body is properly created with new collision settings
	ItemMesh->RecreatePhysicsState();

	// Enable physics simulation
	ItemMesh->SetSimulatePhysics(true);

	// Configure the physics body for reliable collision
	if (FBodyInstance* BodyInstance = ItemMesh->GetBodyInstance())
	{
		// Enable CCD (Continuous Collision Detection) to prevent tunneling through geometry
		BodyInstance->SetUseCCD(true);

		// Wake the instance to ensure it starts simulating
		BodyInstance->WakeInstance();

		// Set linear damping to slow the fall slightly (helps with collision detection)
		BodyInstance->LinearDamping = 0.5f;

		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics: CCD enabled, body awake"));
	}

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

// ============================================================================
// IMOMaterialSourceInterface IMPLEMENTATION
// ============================================================================

bool AMOWorldItem::CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const
{
	if (!ItemComponent || !ItemComponent->bWorldItemActive)
	{
		return false;
	}

	// Check if this world item has the requested material
	if (ItemComponent->ItemDefinitionId != MaterialId)
	{
		return false;
	}

	return ItemComponent->Quantity >= Quantity;
}

int32 AMOWorldItem::GatherMaterial_Implementation(FName MaterialId, int32 Quantity)
{
	if (!ItemComponent || !ItemComponent->bWorldItemActive)
	{
		return 0;
	}

	if (ItemComponent->ItemDefinitionId != MaterialId)
	{
		return 0;
	}

	int32 Available = ItemComponent->Quantity;
	int32 ToGather = FMath::Min(Quantity, Available);

	if (ToGather > 0)
	{
		if (Available == ToGather)
		{
			// Fully consumed - deactivate and destroy
			ItemComponent->SetWorldItemActive(false);
			Destroy();
		}
		else
		{
			ItemComponent->Quantity = Available - ToGather;
		}
	}

	return ToGather;
}

int32 AMOWorldItem::GetMaterialSourcePriority_Implementation() const
{
	// World items have lowest priority (25)
	return 25;
}

// ============================================================================
// IMOIdentifiableInterface IMPLEMENTATION
// ============================================================================

UMOIdentityComponent* AMOWorldItem::GetIdentityComponent_Implementation() const
{
	return IdentityComponent;
}

FGuid AMOWorldItem::GetPersistentGuid_Implementation() const
{
	return IdentityComponent ? IdentityComponent->GetGuid() : FGuid();
}

bool AMOWorldItem::HasValidIdentity_Implementation() const
{
	return IdentityComponent && IdentityComponent->GetGuid().IsValid();
}
