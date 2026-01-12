#include "MOIdentityComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UMOIdentityComponent::UMOIdentityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOIdentityComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// Runtime safety net: server assigns if missing (spawned actors, restored actors that did not get seeded).
	if (OwnerActor->HasAuthority() && !StableGuid.IsValid())
	{
		StableGuid = FGuid::NewGuid();
		OnGuidAvailable.Broadcast(StableGuid);
	}
}

void UMOIdentityComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// When a level is loaded in the editor, OnRegister is a reliable place to seed instance GUIDs
	// if they are missing and then mark the level dirty so it persists.
	EnsureGuidForEditorInstanceIfMissing(false);
#endif
}

void UMOIdentityComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

#if WITH_EDITOR
	// Covers the first time the component is created on an instance in editor workflows.
	EnsureGuidForEditorInstanceIfMissing(false);
#endif
}

#if WITH_EDITOR
void UMOIdentityComponent::PostEditImport()
{
	Super::PostEditImport();

	// This is called on placing/dropping, copy-paste, and common duplication workflows.
	// It is explicitly recommended to cover duplication cases for components. :contentReference[oaicite:2]{index=2}
	EnsureGuidForEditorInstanceIfMissing(true);
}
#endif

void UMOIdentityComponent::EnsureGuidForEditorInstanceIfMissing(bool bBroadcast)
{
#if WITH_EDITOR
	// Never touch templates, CDOs, or archetypes.
	if (IsTemplate() || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (!World)
	{
		return;
	}

	// Only seed in editor worlds, not during PIE/game worlds.
	// PIE duplicates should keep their existing GUID (NonPIEDuplicateTransient helps with this).
	if (World->IsGameWorld())
	{
		return;
	}

	if (StableGuid.IsValid())
	{
		return;
	}

	StableGuid = FGuid::NewGuid();

	// Make sure the owning level/package is dirtied so the GUID persists after save.
	OwnerActor->Modify();
	OwnerActor->MarkPackageDirty();

	Modify();
	MarkPackageDirty();

	if (bBroadcast)
	{
		OnGuidAvailable.Broadcast(StableGuid);
	}
#endif
}

void UMOIdentityComponent::EnsureGuidForAuthorityIfMissing(bool bBroadcast)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (StableGuid.IsValid())
	{
		return;
	}

	StableGuid = FGuid::NewGuid();

	if (bBroadcast)
	{
		OnGuidAvailable.Broadcast(StableGuid);
	}
}

void UMOIdentityComponent::SetGuid(const FGuid& InGuid)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	StableGuid = InGuid;
	OnGuidAvailable.Broadcast(StableGuid);
}

FGuid UMOIdentityComponent::GetOrCreateGuid()
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		EnsureGuidForAuthorityIfMissing(true);
	}

	return StableGuid;
}

void UMOIdentityComponent::RegenerateGuid()
{
	StableGuid = FGuid::NewGuid();
	OnGuidAvailable.Broadcast(StableGuid);

#if WITH_EDITOR
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->Modify();
		OwnerActor->MarkPackageDirty();
	}
	Modify();
	MarkPackageDirty();
#endif
}

void UMOIdentityComponent::OnRep_StableGuid()
{
	if (StableGuid.IsValid())
	{
		OnGuidAvailable.Broadcast(StableGuid);
	}
}

void UMOIdentityComponent::HandleOwnerDestroyed(AActor* /*DestroyedActor*/)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (StableGuid.IsValid())
	{
		OnOwnerDestroyedWithGuid.Broadcast(StableGuid);
	}
}

void UMOIdentityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMOIdentityComponent, DisplayName);
	DOREPLIFETIME(UMOIdentityComponent, StableGuid);
}
