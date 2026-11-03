#include "MOBuildPieceActor.h"

#include "MOBuildProgressComponent.h"
#include "MOWorldSaveGame.h"
#include "MOFramework.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace MOBuildPiece
{
	static const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");   // 100 cm, centred

	static UStaticMesh* Cube()
	{
		static TWeakObjectPtr<UStaticMesh> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadObject<UStaticMesh>(nullptr, CubePath);
		}
		return Cached.Get();
	}

	/** A hidden engine cube sized to the box: a collider or the snap box. */
	static UStaticMeshComponent* MakeBox(AActor* Owner, FName Name, USceneComponent* Parent, const FMOBuildPieceBox& Box)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner, Name);
		Comp->SetStaticMesh(Cube());
		Comp->SetupAttachment(Parent);
		FTransform T = Box.Transform;
		T.SetScale3D(Box.HalfSize / 50.0f);
		Comp->SetRelativeTransform(T);
		Comp->SetVisibility(false);
		Comp->SetCastShadow(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->RegisterComponent();
		Owner->AddInstanceComponent(Comp);
		return Comp;
	}
}

AMOBuildPieceActor::AMOBuildPieceActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;   // only while a door swings
	UnbuiltStagedPartStyle = EMOStagedPartStyle::Hidden;

	// The ghost material every buildable Blueprint uses (BP_BuildableBase's): a piece Blueprint is made from this class directly.
	GhostMaterialBase = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/MOFramework/UI/BuildSystem/M_BuildPreview.M_BuildPreview")));
}

void AMOBuildPieceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOBuildPieceActor, SupportLengthsCm);
	DOREPLIFETIME(AMOBuildPieceActor, bDoorOpen);
}

void AMOBuildPieceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildPieceComponents();
}

void AMOBuildPieceActor::BuildPieceComponents()
{
	if (bPieceBuilt || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	bPieceBuilt = true;

	USceneComponent* Root = GetRootComponent();
	USceneComponent* Structural = MeshComponent ? static_cast<USceneComponent*>(MeshComponent) : Root;

	const bool bDoor = HasDoor();
	if (bDoor)
	{
		DoorHingeComponent = NewObject<USceneComponent>(this, TEXT("DoorHinge"));
		DoorHingeComponent->SetupAttachment(Structural);
		DoorHingeComponent->SetRelativeTransform(DoorHinge);
		DoorHingeComponent->RegisterComponent();
		AddInstanceComponent(DoorHingeComponent);
	}
	// A leaf part is authored in the piece's frame: under the hinge it keeps that place (hinge^-1 * its transform).
	auto InLeaf = [this](const FTransform& InPiece) { return InPiece.GetRelativeTransform(DoorHinge); };

	PieceStagedNames.Reset();
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		const FMOBuildPiecePart& Part = Parts[i];
		if (!Part.Mesh)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOBuildPieceActor] %s: part %d (%s) has no mesh"), *GetClass()->GetName(), i, *Part.Name.ToString());
			continue;
		}
		const FName CompName(*FString::Printf(TEXT("Part_%02d_%s"), i, *Part.Name.ToString()));
		const bool bLeaf = Part.bDoorLeaf && DoorHingeComponent;
		USceneComponent* Parent = bLeaf ? DoorHingeComponent.Get() : Root;
		UStaticMeshComponent* Comp = nullptr;
		if (Part.Instances.Num() > 0)
		{
			UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, CompName);
			Ism->SetStaticMesh(Part.Mesh);
			Ism->SetupAttachment(Parent);
			for (const FTransform& Inst : Part.Instances)
			{
				Ism->AddInstance(bLeaf ? InLeaf(Inst) : Inst);
			}
			Comp = Ism;
		}
		else
		{
			Comp = NewObject<UStaticMeshComponent>(this, CompName);
			Comp->SetStaticMesh(Part.Mesh);
			Comp->SetupAttachment(Parent);
			Comp->SetRelativeTransform(bLeaf ? InLeaf(Part.Transform) : Part.Transform);
		}
		// Visual only: the colliders make the piece solid (AMOBuildableActor's passes leave a tagged part off).
		Comp->ComponentTags.Add(MO_NO_COLLISION_TAG);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
		PieceStagedNames.Add(CompName);
	}

	FBox ColliderBounds(EForceInit::ForceInit);
	for (int32 i = 0; i < Colliders.Num(); ++i)
	{
		const FMOBuildPieceBox& Box = Colliders[i];
		const bool bLeaf = Box.bDoorLeaf && DoorHingeComponent;
		FMOBuildPieceBox Local = Box;
		if (bLeaf)
		{
			Local.Transform = InLeaf(Box.Transform);
		}
		MOBuildPiece::MakeBox(this, FName(*FString::Printf(TEXT("Collider_%02d"), i)), bLeaf ? DoorHingeComponent.Get() : Structural, Local);
		if (!Box.bDoorLeaf)
		{
			ColliderBounds += FBox(-Box.HalfSize, Box.HalfSize).TransformBy(Box.Transform);
		}
	}

	FMOBuildPieceBox Snap = SnapBox;
	if (Snap.HalfSize.IsNearlyZero() && ColliderBounds.IsValid)
	{
		Snap.Transform = FTransform(ColliderBounds.GetCenter());
		Snap.HalfSize = ColliderBounds.GetExtent();
	}
	if (!Snap.HalfSize.IsNearlyZero())
	{
		UStaticMeshComponent* SnapComp = MOBuildPiece::MakeBox(this, TEXT("SnapBox"), Root, Snap);
		SnapComp->ComponentTags.Add(TEXT("MO_SnapBounds"));
		SnapComp->ComponentTags.Add(MO_NO_COLLISION_TAG);
		SnapComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (Parts.Num() > 0)
	{
		StagedBuildParts = PieceStagedNames;
		UnbuiltStagedPartStyle = EMOStagedPartStyle::Hidden;
	}
	else
	{
		// A Blueprint that carries its own parts (the stick floor, reparented here for its supports) keeps its own staging.
		PieceStagedNames = StagedBuildParts;
	}
	RefreshAfterPartsChanged();
}

// ============================================================================
// SUPPORTS
// ============================================================================

int32 AMOBuildPieceActor::ItemsForPost(float LengthCm, float CmPerItem)
{
	if (LengthCm <= 0.0f)
	{
		return 0;
	}
	return FMath::Max(1, FMath::CeilToInt(LengthCm / FMath::Max(1.0f, CmPerItem)));
}

bool AMOBuildPieceActor::HasSupportNear(const FVector& WorldPoint, float RadiusCm) const
{
	for (int32 i = 0; i < SupportPoints.Num() && i < SupportLengthsCm.Num(); ++i)
	{
		if (SupportLengthsCm[i] <= 0.0f)
		{
			continue;
		}
		const FVector P = GetActorTransform().TransformPosition(SupportPoints[i]);
		if (FVector::DistSquared2D(P, WorldPoint) <= RadiusCm * RadiusCm)
		{
			return true;
		}
	}
	return false;
}

TArray<float> AMOBuildPieceActor::MeasureSupports() const
{
	TArray<float> Lengths;
	UWorld* World = GetWorld();
	if (!bGrowsSupports || !World)
	{
		return Lengths;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MOPieceSupport), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);
	// The first SOLID below: something a person cannot walk through (the ground, a finished building) -- a plan, grass, a person are traced
	// past (the shelter rays' rule). By channel, not object type: the voxel terrain is not WorldStatic.
	auto SolidBelow = [World](const FVector& From, const FVector& To, FCollisionQueryParams Query, FHitResult& Out)
	{
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			if (!World->LineTraceSingleByChannel(Out, From, To, ECC_Visibility, Query))
			{
				return false;
			}
			const UPrimitiveComponent* Comp = Out.GetComponent();
			if (!Comp)
			{
				return false;
			}
			const AActor* Owner = Comp->GetOwner();
			if (Comp->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && !(Owner && Owner->IsA<APawn>()))
			{
				return true;
			}
			Query.AddIgnoredComponent(Comp);
		}
		return false;
	};
	const FTransform& Xf = GetActorTransform();
	for (const FVector& Local : SupportPoints)
	{
		const FVector Top = Xf.TransformPosition(Local);
		// A neighbour's post already carries this corner (two floors side by side share it).
		bool bShared = false;
		for (TActorIterator<AMOBuildPieceActor> It(World); It && !bShared; ++It)
		{
			if (*It != this && !It->IsGhostMode() && It->HasSupportNear(Top, SupportShareCm))
			{
				bShared = true;
			}
		}
		float Length = 0.0f;
		FHitResult Hit;
		if (!bShared && SolidBelow(Top + FVector(0, 0, 2.0f), Top - FVector(0, 0, SupportMaxCm), Params, Hit))
		{
			const float Drop = Top.Z - Hit.ImpactPoint.Z;
			Length = Drop >= SupportMinCm ? Drop + 4.0f : 0.0f;   // the foot sits a few cm into the ground
		}
		Lengths.Add(Length);
	}
	return Lengths;
}

void AMOBuildPieceActor::ApplySupports()
{
	for (UStaticMeshComponent* Old : SupportComponents)
	{
		if (IsValid(Old))
		{
			RemoveInstanceComponent(Old);
			Old->DestroyComponent();
		}
	}
	SupportComponents.Reset();

	TArray<FName> Staged;
	TArray<FMOBuildPart> Extra;
	float ExtraSeconds = 0.0f;
	int32 Items = 0;
	int32 Posts = 0;
	if (SupportMesh)
	{
		// On the root, not under MeshComponent: snapping measures a floor by its MeshComponent subtree when it has no snap box (the stick floor),
		// and a floor with posts in it read 75 cm tall -- not flat -- so a wall beside it went coplanar, half sunk (Nov 3, check_buildings).
		// Built, a post still blocks: a part outside the subtree gets BlockAll (no Blueprint template; RestoreAuthoredCollision).
		USceneComponent* Structural = GetRootComponent();
		for (int32 i = 0; i < SupportPoints.Num() && i < SupportLengthsCm.Num(); ++i)
		{
			const float Length = SupportLengthsCm[i];
			if (Length <= 0.0f)
			{
				continue;
			}
			const FName Name(*FString::Printf(TEXT("Support_%02d"), i));
			UStaticMeshComponent* Post = NewObject<UStaticMeshComponent>(this, Name);
			Post->SetStaticMesh(SupportMesh);
			Post->SetupAttachment(Structural);
			// Its foot at the point's drop, its top under the point; a post is solid once built (its mesh's simple collision).
			Post->SetRelativeTransform(FTransform(FRotator(0.0f, 37.0f * i, 0.0f), SupportPoints[i] - FVector(0, 0, Length), FVector(1.0f, 1.0f, Length / 100.0f)));
			Post->SetCanEverAffectNavigation(false);
			Post->RegisterComponent();
			AddInstanceComponent(Post);
			SupportComponents.Add(Post);
			Staged.Add(Name);
			Items += ItemsForPost(Length, SupportCmPerItem);
			++Posts;
		}
	}
	if (Items > 0 && !SupportItem.IsNone())
	{
		FMOBuildPart Part;
		Part.ItemDefinitionId = SupportItem;
		Part.Quantity = Items;
		Extra.Add(Part);
	}
	if (Posts > 0 && !SupportLashingItem.IsNone())
	{
		FMOBuildPart Part;
		Part.ItemDefinitionId = SupportLashingItem;
		Part.Quantity = Posts;
		Extra.Add(Part);
	}
	ExtraSeconds = Posts * SupportSecondsPerPost;
	if (BuildProgressComponent && !IsGhostMode())
	{
		BuildProgressComponent->SetExtraWork(Extra, ExtraSeconds);
	}

	// The posts go up first, then the piece.
	Staged.Append(PieceStagedNames);
	StagedBuildParts = Staged;
	RefreshAfterPartsChanged();
}

void AMOBuildPieceActor::OnRep_SupportLengths()
{
	BuildPieceComponents();
	ApplySupports();
}

void AMOBuildPieceActor::UpdatePreviewSupports()
{
	if (!bGrowsSupports || !IsGhostMode())
	{
		return;
	}
	const FTransform Now = GetActorTransform();
	if (Now.Equals(PreviewSupportsAt, 1.0f))
	{
		return;
	}
	PreviewSupportsAt = Now;
	const TArray<float> Lengths = MeasureSupports();
	if (Lengths != SupportLengthsCm)
	{
		SupportLengthsCm = Lengths;
		ApplySupports();
	}
}

void AMOBuildPieceActor::InitializeBuilding(FName InRecipeId)
{
	BuildPieceComponents();
	if (HasAuthority() && bGrowsSupports && GetBuildState() == EMOBuildState::Ghost && SupportLengthsCm.IsEmpty())
	{
		SupportLengthsCm = MeasureSupports();   // the server measures once, where it was placed; the lengths replicate and are saved
	}
	Super::InitializeBuilding(InRecipeId);
	if (bGrowsSupports)
	{
		ApplySupports();   // after the recipe: its work joins the recipe's (either order works: SetExtraWork)
	}
}

// ============================================================================
// DOOR
// ============================================================================

bool AMOBuildPieceActor::HasDoor() const
{
	return Parts.ContainsByPredicate([](const FMOBuildPiecePart& P) { return P.bDoorLeaf; })
		|| Colliders.ContainsByPredicate([](const FMOBuildPieceBox& B) { return B.bDoorLeaf; });
}

void AMOBuildPieceActor::SetDoorOpen(bool bOpen)
{
	if (!HasAuthority() || !HasDoor() || !IsComplete() || bDoorOpen == bOpen)
	{
		return;
	}
	bDoorOpen = bOpen;
	OnRep_DoorOpen();
	UE_LOG(LogMOFramework, Log, TEXT("[MOBuildPieceActor] %s: door %s"), *GetName(), bOpen ? TEXT("opened") : TEXT("closed"));
}

void AMOBuildPieceActor::OnCompleteInteracted_Implementation(AController* Controller)
{
	Super::OnCompleteInteracted_Implementation(Controller);
	if (HasDoor())
	{
		SetDoorOpen(!bDoorOpen);
	}
}

void AMOBuildPieceActor::OnRep_DoorOpen()
{
	if (DoorSwingSeconds <= 0.0f || !HasActorBegunPlay())
	{
		ApplyDoorYaw(bDoorOpen ? DoorOpenYawDeg : 0.0f);
		return;
	}
	SetActorTickEnabled(true);
}

void AMOBuildPieceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bDoorOpen ? DoorOpenYawDeg : 0.0f;
	const float Step = FMath::Abs(DoorOpenYawDeg) / FMath::Max(0.05f, DoorSwingSeconds) * DeltaSeconds;
	const float Next = FMath::Abs(Target - DoorYaw) <= Step ? Target : DoorYaw + FMath::Sign(Target - DoorYaw) * Step;
	ApplyDoorYaw(Next);
	if (FMath::IsNearlyEqual(Next, Target))
	{
		SetActorTickEnabled(false);
	}
}

void AMOBuildPieceActor::ApplyDoorYaw(float Yaw)
{
	DoorYaw = Yaw;
	if (DoorHingeComponent)
	{
		FTransform T = DoorHinge;
		T.SetRotation(DoorHinge.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)));
		DoorHingeComponent->SetRelativeTransform(T);
	}
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

void AMOBuildPieceActor::BuildSaveData(FMOPersistedBuildingRecord& OutRecord) const
{
	Super::BuildSaveData(OutRecord);
	OutRecord.SupportLengthsCm = SupportLengthsCm;
	OutRecord.bDoorOpen = bDoorOpen;
}

void AMOBuildPieceActor::ApplySaveData(const FMOPersistedBuildingRecord& InRecord)
{
	BuildPieceComponents();
	// The posts first: the recipe re-initialised below takes their work, so the saved per-part consumption lines up.
	SupportLengthsCm = InRecord.SupportLengthsCm;
	if (bGrowsSupports)
	{
		ApplySupports();
	}
	Super::ApplySaveData(InRecord);
	bDoorOpen = InRecord.bDoorOpen && HasDoor();
	ApplyDoorYaw(bDoorOpen ? DoorOpenYawDeg : 0.0f);
}
