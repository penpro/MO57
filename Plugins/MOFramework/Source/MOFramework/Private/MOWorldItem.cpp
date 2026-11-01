#include "MOWorldItem.h"
#include "MOFramework.h"
#include "Engine/DataTable.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodySetup.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "MOSpawnClearance.h"
#include "MOIdentityComponent.h"
#include "MOInteractableComponent.h"
#include "MOItemComponent.h"

AMOWorldItem::AMOWorldItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Only enable when drop physics is active
	bReplicates = true;
	// The drop is simulated on the server (EnableDropPhysics lifts it 50-100 cm and lets it fall, SettleOnGround stops it): without movement replication a
	// joiner only ever saw the lifted spawn point -- "dropped items don't show as dropping on client, they hang in the air" (Steam playtest, Oct 16 2026).
	SetReplicatingMovement(true);

	// Item mesh is the root - this allows physics to move the whole actor
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	RootComponent = ItemMesh;
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ItemMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ItemMesh->SetSimulatePhysics(false); // Disabled by default, enabled on drop

	// No interaction proxy shape: the mesh itself is the target (pitfall 3 in the header).

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
	// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] OnHandleInteract called for '%s'"), *GetName());

	if (!InteractorController)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] OnHandleInteract: No controller"));
		return false;
	}

	bool bSuccess = false;

	if (bAddToInventoryOnInteract && ItemComponent)
	{
		bSuccess = ItemComponent->GiveToInteractorInventory(InteractorController);
		// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] GiveToInteractorInventory returned %s"), bSuccess ? TEXT("true") : TEXT("false"));

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
		// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace every pickup query
		// UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s is hidden"), *GetName());
		return false;
	}

	// Not pickupable if collision is disabled (already picked up)
	if (!GetActorEnableCollision())
	{
		// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace every pickup query
		// UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s has collision disabled"), *GetName());
		return false;
	}

	// Must have a valid item component with item data
	if (!IsValid(ItemComponent))
	{
		// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace every pickup query
		// UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s has invalid ItemComponent"), *GetName());
		return false;
	}

	// Must have a valid item definition
	if (ItemComponent->ItemDefinitionId.IsNone())
	{
		// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace every pickup query
		// UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] IsPickupable: %s has no ItemDefinitionId"), *GetName());
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

void AMOWorldItem::EnsureMeshAnswersSimpleQueries(UStaticMesh* Mesh)
{
	UBodySetup* BodySetup = IsValid(Mesh) ? Mesh->GetBodySetup() : nullptr;
	if (BodySetup && BodySetup->AggGeom.GetElementCount() == 0 && BodySetup->CollisionTraceFlag != CTF_UseComplexAsSimple)
	{
		BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] %s has no simple collision: its triangles answer simple queries"), *Mesh->GetName());
	}
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
			EnsureMeshAnswersSimpleQueries(LoadedMesh);
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

	// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace a falling drop every 0.5 s (per tick; its static map also grew one entry per dropped item, never removed)
	// // Log periodically (every 0.5s) to avoid spam
	// static TMap<AActor*, float> LastLogTimes;
	// float& LastLogTime = LastLogTimes.FindOrAdd(this);
	// if (CurrentTime - LastLogTime > 0.5f)
	// {
	// 	LastLogTime = CurrentTime;
	// 	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] Tick: %s at %s, Elapsed=%.2fs, IsSimulating=%s"),
	// 		*GetName(), *GetActorLocation().ToString(), ElapsedTime,
	// 		(IsValid(ItemMesh) && ItemMesh->IsSimulatingPhysics()) ? TEXT("true") : TEXT("false"));
	// }

	// Fell through the ground the drop found (its physics collision was not there yet): stop here and settle on it.
	if (bHasDropGround && GetActorLocation().Z < DropGroundLocation.Z - 150.0f)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] %s fell through the ground at %s (now %.0f cm below it): settling back on it"),
			*GetName(), *DropGroundLocation.ToString(), DropGroundLocation.Z - GetActorLocation().Z);
		SettleOnGround();
		return;
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
	// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] EnableDropPhysics called for %s at location %s"),
	// 	*GetName(), *GetActorLocation().ToString());

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

	// A mesh without simple collision already uses its triangles (EnsureMeshAnswersSimpleQueries, on every mesh apply).
	if (IsValid(CurrentMesh) && (!CurrentMesh->GetBodySetup() || CurrentMesh->GetBodySetup()->AggGeom.GetElementCount() == 0))
	{
		ItemMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
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

	bHasDropGround = false;
	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		DropGroundLocation = GroundHit.Location;
		bHasDropGround = true;
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

	StartDropSimulation();
}

void AMOWorldItem::FallFromHere()
{
	if (!HasAuthority() || bDropPhysicsActive || !IsValid(ItemMesh) || !GetWorld() || IsHidden())
	{
		return;
	}
	// The ground it lay on is not the ground any more: the "fell through" floor (pitfall 2b) is the terrain under it now. The voxel terrain only:
	// an object-type query for static objects also met the ocean's underwater volume (query-only, no channel blocks on it), whose top stands
	// metres over the shore -- items were "settled back on top" of it, 3 m higher at every dig (Nov 1, the first build of this fix).
	FHitResult Ground;
	const FVector Here = GetActorLocation();
	bHasDropGround = MOSpawnClearance::TraceVoxelGround(GetWorld(), static_cast<float>(Here.X), static_cast<float>(Here.Y), static_cast<float>(Here.Z + 100.0), static_cast<float>(Here.Z - 3000.0), this, Ground);
	DropGroundLocation = bHasDropGround ? Ground.ImpactPoint : Here;
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StartDropSimulation();
}

int32 AMOWorldItem::WakeItemsOver(UWorld* World, const FVector& Center, float RadiusCm)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return 0;
	}
	// The patch of ground a terraform changed is a disc (the brush): anything lying over it, up to a body's height above or below the brush centre
	// (a dig lowers the ground under items that stood on it; a raise or a flatten's fill lifts it into them).
	const float Reach = RadiusCm + 50.0f;
	int32 Woken = 0;
	for (TActorIterator<AMOWorldItem> It(World); It; ++It)
	{
		AMOWorldItem* Item = *It;
		const FVector L = Item->GetActorLocation();
		if (FVector::DistSquared2D(L, Center) <= FMath::Square(Reach) && FMath::Abs(L.Z - Center.Z) <= RadiusCm + 300.0f && !Item->bDropPhysicsActive && !Item->IsHidden())
		{
			Item->FallFromHere();
			++Woken;
		}
	}
	if (Woken > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] the ground changed at %s (radius %.0f): %d item(s) lying there fall onto it"), *Center.ToCompactString(), RadiusCm, Woken);
	}
	return Woken;
}

void AMOWorldItem::StartDropSimulation()
{
	UWorld* World = GetWorld();

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

		UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] EnableDropPhysics: CCD enabled, body awake"));
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] EnableDropPhysics: After SetSimulatePhysics(true), IsSimulating=%s"),
		ItemMesh->IsSimulatingPhysics() ? TEXT("true") : TEXT("false"));

	// Track drop physics state
	bDropPhysicsActive = true;
	DropPhysicsStartTime = World->GetTimeSeconds();

	// Enable tick to monitor physics
	SetActorTickEnabled(true);

	UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] EnableDropPhysics complete: bDropPhysicsActive=%s, StartTime=%.2f"),
		bDropPhysicsActive ? TEXT("true") : TEXT("false"), DropPhysicsStartTime);
}

void AMOWorldItem::SettleOnGround()
{
	// DEBUG (MOWorldItem, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] SettleOnGround called for %s at %s"),
	// 	*GetName(), *GetActorLocation().ToString());

	bDropPhysicsActive = false;
	SetActorTickEnabled(false);

	if (!IsValid(ItemMesh))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] SettleOnGround: ItemMesh invalid"));
		return;
	}

	// Disable physics simulation
	ItemMesh->SetSimulatePhysics(false);

	// Where the physics left it is where it rests -- on the ground or on another item -- unless that is no rest: under the ground (it fell through,
	// pitfall 2b) or with nothing under it (still in the air at the timeout). Then its bottom goes onto the first thing below it.
	// The old settle traced down from 200 cm above and put the item 10 cm over whatever it met first: another item over it as often as the ground.
	// Items dropped at one spot (a dig's spoil) each landed on the last one's top, a column of dirt piles standing in the air with gaps between
	// them (BR1101, Nov 1: "if you start another dig it spawns a dirt item before the bar completes" -- its screenshot).
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		FVector Origin, Extent;
		GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
		const double Bottom = Origin.Z - Extent.Z;
		const double PivotAboveBottom = GetActorLocation().Z - Bottom;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOWorldItemSettle), /*bTraceComplex=*/false, this);

		// The terrain alone (never another item, nor the ocean's query-only underwater volume -- see FallFromHere): from above both the item and
		// the ground its drop found.
		const double TopZ = FMath::Max(Origin.Z + Extent.Z, bHasDropGround ? DropGroundLocation.Z : Origin.Z) + 50.0;
		FHitResult Ground;
		const bool bGround = MOSpawnClearance::TraceVoxelGround(World, static_cast<float>(Origin.X), static_cast<float>(Origin.Y), static_cast<float>(TopZ), static_cast<float>(TopZ - 3000.0), this, Ground);
		// What is right under it, items included.
		FHitResult Under;
		const bool bUnder = World->LineTraceSingleByChannel(Under, Origin, FVector(Origin.X, Origin.Y, Bottom - 3000.0), ECC_WorldStatic, QueryParams);   // from its middle: an item resting a little sunk in the ground still finds it

		const bool bBelowGround = bGround && Bottom < Ground.ImpactPoint.Z - 5.0;
		const bool bResting = bUnder && Bottom - Under.ImpactPoint.Z <= RestingGapCm;
		if (bBelowGround || (!bResting && !bUnder && !bGround && bHasDropGround))
		{
			const double Floor = bGround ? Ground.ImpactPoint.Z : DropGroundLocation.Z;
			SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Floor + PivotAboveBottom));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] %s settled %.0f cm under the ground: back on top of it at %.0f"), *GetName(), Floor - Bottom, Floor);
		}
		else if (!bResting && bUnder)
		{
			SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Under.ImpactPoint.Z + PivotAboveBottom));
			UE_LOG(LogMOFramework, Log, TEXT("[MOWorldItem] %s was %.0f cm in the air at the timeout: down onto %s"), *GetName(), Bottom - Under.ImpactPoint.Z, *GetNameSafe(Under.GetActor()));
		}
		else if (!bResting)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWorldItem] No ground found below item at %s"), *GetActorLocation().ToString());
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
