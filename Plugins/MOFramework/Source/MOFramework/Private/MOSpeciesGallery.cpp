#include "MOSpeciesGallery.h"
#include "MOFramework.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOPCGBiomeSpawnerSettings.h"
#include "MOResourceDatabaseSettings.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/CollisionProfile.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr int32 GalleryColumns = 8;
	constexpr float GalleryGapCm = 500.0f;
	constexpr float GalleryDropSeconds = 1.5f;
	constexpr float GalleryStandableLiftCm = 50.0f;

	// The gallery is a TEST, not a copy of the world's placement (Wes: "you can't have the rock stuff embedded in the ground if we're testing collision" and
	// "make the assets larger to help make it obvious when they block"): everything is shown at this multiple of its mid scale, and a standable species is not
	// buried but lifted until its lowest point is GalleryStandableLiftCm above the floor -- landing on it and falling through it can no longer look alike.
	TAutoConsoleVariable<float> CVarGalleryScale(TEXT("mo.Gallery.Scale"), 2.0f,
		TEXT("Species gallery: every entry at this multiple of its mid scale (MO.Gallery rebuilds with it)."), ECVF_Default);
}

AMOSpeciesGallery::AMOSpeciesGallery()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(RootComponent);
	Floor->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grid(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (Cube.Succeeded())
	{
		Floor->SetStaticMesh(Cube.Object);
	}
	if (Grid.Succeeded())
	{
		Floor->SetMaterial(0, Grid.Object);   // a 1 m grid: sizes read at a glance
	}
}

AMOSpeciesGallery* AMOSpeciesGallery::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<AMOSpeciesGallery> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

int32 AMOSpeciesGallery::Build(FName InBiomeId)
{
	BiomeId = InBiomeId;
	for (UActorComponent* Component : BuiltComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	BuiltComponents.Reset();
	Entries.Reset();

	const UMOBiomeDatabaseSettings* BiomeSettings = GetDefault<UMOBiomeDatabaseSettings>();
	const UDataTable* Biomes = BiomeSettings ? BiomeSettings->GetBiomeDefinitionsDataTable() : nullptr;
	if (!Biomes)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] no DT_Biomes (Project Settings > Biome Database)"));
		return 0;
	}
	const FName Decorative = GetDefault<UMOPCGBiomeSpawnerSettings>()->DecorativeCollisionProfile.Name;

	struct FPending
	{
		FString Name;
		UStaticMesh* Mesh = nullptr;
		const FMOBiomeSpeciesEntry* Species = nullptr;
		FName Profile;
		EMOSpeciesContact Expected = EMOSpeciesContact::WalkThrough;
		float Scale = 1.0f;
		FBoxSphereBounds Bounds;
		float Radius = 0.0f;
	};
	TArray<FPending> Pending;
	TSet<FString> Seen;
	Biomes->ForeachRow<FMOBiomeDefinitionRow>(TEXT("MOGallery"), [&](const FName& Row, const FMOBiomeDefinitionRow& Def)
	{
		if (!InBiomeId.IsNone() && Row != InBiomeId)
		{
			return;
		}
		const auto Add = [&](const FMOBiomeSpeciesEntry& Species)
		{
			UStaticMesh* Mesh = Species.Mesh.LoadSynchronous();
			if (!Mesh)
			{
				return;
			}
			const FString Name = Row.ToString() + TEXT("/") + Mesh->GetName();
			if (Seen.Contains(Name))
			{
				return;
			}
			Seen.Add(Name);
			const FMOResourceNodeDefinitionRow* Resource = Species.ResourceNodeId.IsNone() ? nullptr : UMOResourceDatabaseSettings::GetResourceDefinition(Species.ResourceNodeId);
			FPending P;
			P.Name = Name;
			P.Mesh = Mesh;
			P.Species = &Species;
			P.Profile = FMOSpeciesPlacement::CollisionProfile(Species.Collision, Resource, Decorative);
			P.Expected = FMOSpeciesPlacement::ExpectedContact(Species, Resource, P.Profile);
			P.Scale = 0.5f * (Species.MinScale + Species.MaxScale) * FMath::Max(0.1f, CVarGalleryScale.GetValueOnGameThread());
			P.Bounds = Mesh->GetBounds();
			P.Radius = FMath::Max(100.0f, static_cast<float>(FMath::Max(P.Bounds.BoxExtent.X, P.Bounds.BoxExtent.Y)) * P.Scale * Species.WidthScale);
			Pending.Add(P);
		};
		for (const FMOBiomeSpeciesEntry& Species : Def.Species)
		{
			Add(Species);
		}
		for (const FMOBiomeSpeciesEntry& Species : Def.WaterEdgeSpecies)
		{
			Add(Species);
		}
	});
	Pending.Sort([](const FPending& A, const FPending& B)
	{
		return A.Expected != B.Expected ? static_cast<uint8>(A.Expected) > static_cast<uint8>(B.Expected) : A.Radius > B.Radius;   // standable first, big first
	});

	// Rows of GalleryColumns along +X; rows step along +Y. The visitor stands at the -Y edge looking +Y.
	const FVector Origin = GetActorLocation();
	FloorTopZ = static_cast<float>(Origin.Z);
	float RowY = 0.0f, MaxX = 0.0f;
	for (int32 RowStart = 0; RowStart < Pending.Num(); RowStart += GalleryColumns)
	{
		const int32 RowEnd = FMath::Min(RowStart + GalleryColumns, Pending.Num());
		float RowRadius = 0.0f;
		for (int32 I = RowStart; I < RowEnd; ++I)
		{
			RowRadius = FMath::Max(RowRadius, Pending[I].Radius);
		}
		float X = 0.0f;
		for (int32 I = RowStart; I < RowEnd; ++I)
		{
			const FPending& P = Pending[I];
			const FVector Center(Origin.X + X + P.Radius, Origin.Y + RowY + RowRadius, FloorTopZ);
			X += 2.0f * P.Radius + GalleryGapCm;
			// Standable: resting ABOVE the floor (its lowest point GalleryStandableLiftCm up), never buried. The rest: as the spawner places them (upright, pivot at the base).
			const float Offset = P.Expected == EMOSpeciesContact::Standable
				? GalleryStandableLiftCm - static_cast<float>(P.Bounds.Origin.Z - P.Bounds.BoxExtent.Z) * P.Scale
				: FMOSpeciesPlacement::BurialOffsetCm(*P.Species, P.Bounds, P.Scale);

			UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
			Instances->SetStaticMesh(P.Mesh);
			Instances->SetCollisionProfileName(P.Profile);
			Instances->SetupAttachment(RootComponent);
			Instances->RegisterComponent();
			const float Width = P.Scale * P.Species->WidthScale;   // as the spawner widens it
			Instances->AddInstance(FTransform(FQuat::Identity, Center + FVector(0.0, 0.0, Offset), FVector(Width, Width, P.Scale)), /*bWorldSpace=*/true);
			BuiltComponents.Add(Instances);

			UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
			Text->SetupAttachment(RootComponent);
			Text->RegisterComponent();
			Text->SetWorldLocationAndRotation(FVector(Center.X, Center.Y - P.Radius - 120.0, FloorTopZ + 20.0), FRotator(0.0, -90.0, 0.0));
			Text->SetHorizontalAlignment(EHTA_Center);
			Text->SetWorldSize(32.0f);
			Text->SetText(FText::FromString(FString::Printf(TEXT("%d  %s\n%s  %s"), Entries.Num() + 1, *P.Mesh->GetName(), *P.Profile.ToString(), FMOSpeciesPlacement::ContactName(P.Expected))));
			BuiltComponents.Add(Text);

			FEntry E;
			E.Name = P.Name;
			E.MeshPath = P.Mesh->GetPathName();
			E.Component = Instances;
			E.Text = Text;
			E.Center = Center;
			E.TopZ = FloorTopZ + Offset + static_cast<float>(P.Bounds.Origin.Z + P.Bounds.BoxExtent.Z) * P.Scale;
			E.RadiusCm = P.Radius;
			E.Profile = P.Profile;
			E.Expected = P.Expected;
			Entries.Add(E);
		}
		MaxX = FMath::Max(MaxX, X);
		RowY += 2.0f * RowRadius + GalleryGapCm + 300.0f;
	}

	// The floor: 1.5 m of margin around everything, its top at the actor's height.
	FloorArea = FBox2D(FVector2D(Origin.X - 1500.0, Origin.Y - 1500.0), FVector2D(Origin.X + MaxX + 1000.0, Origin.Y + RowY + 1000.0));
	const FVector2D Size = FloorArea.GetSize();
	Floor->SetWorldLocation(FVector(FloorArea.GetCenter().X, FloorArea.GetCenter().Y, FloorTopZ - 50.0));
	Floor->SetWorldScale3D(FVector(Size.X / 100.0, Size.Y / 100.0, 1.0));
	return Entries.Num();
}

FTransform AMOSpeciesGallery::GetViewpoint() const
{
	return FTransform(FRotator(-10.0, 90.0, 0.0), FVector(FloorArea.GetCenter().X, FloorArea.Min.Y + 400.0, FloorTopZ + 120.0));
}

void AMOSpeciesGallery::StartContactTest(ACharacter* Character)
{
	if (!Character || Entries.Num() == 0)
	{
		return;
	}
	TestCharacter = Character;
	TestStartTransform = Character->GetActorTransform();
	TestIndex = 0;
	SetActorTickEnabled(true);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] contact test: %d species of %s"), Entries.Num(), BiomeId.IsNone() ? TEXT("every biome") : *BiomeId.ToString());
	BeginEntry(0);
}

void AMOSpeciesGallery::BeginEntry(int32 Index)
{
	// Only what should BLOCK is tested (Wes: "we don't really need to test things that are walk thru ... the only ones that are finicky are the ones that might not
	// have simple collision or hulls"): a walk-through species has a non-blocking profile, and the profile is the whole story.
	while (Entries.IsValidIndex(Index) && Entries[Index].Expected == EMOSpeciesContact::WalkThrough)
	{
		Entries[Index].Result = TEXT("not tested (walk-through profile)");
		++Index;
	}
	TestIndex = Index;
	if (!TestCharacter.IsValid() || !Entries.IsValidIndex(Index))
	{
		FinishTest();
		return;
	}
	Probe = 0;
	CentreResult.Reset();
	BeginProbe();
}

void AMOSpeciesGallery::BeginProbe()
{
	ACharacter* Character = TestCharacter.Get();
	if (!Character)
	{
		FinishTest();
		return;
	}
	const FEntry& E = Entries[TestIndex];
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	Move->StopMovementImmediately();
	bWalking = E.Expected != EMOSpeciesContact::Standable;
	if (bWalking)
	{
		// Walk in along -X from beyond its reach: through its centre, or (probe 1) past it at 60 % of its reach, where only a canopy or a too-wide hull would stop her.
		const double Side = Probe == 1 ? FMath::Max(0.6 * E.RadiusCm, 100.0) : 0.0;
		Character->SetActorLocationAndRotation(FVector(E.Center.X + E.RadiusCm + 250.0, E.Center.Y + Side, FloorTopZ + HalfHeight + 5.0), FRotator(0.0, 180.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Dropped from a little above its top, onto its middle.
		Character->SetActorLocationAndRotation(FVector(E.Center.X, E.Center.Y, E.TopZ + HalfHeight + 120.0), FRotator(0.0, 90.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
	}
	Move->SetMovementMode(MOVE_Falling);
	PhaseStartSeconds = GetWorld()->GetTimeSeconds();
}

void AMOSpeciesGallery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (TestIndex == INDEX_NONE)
	{
		return;
	}
	ACharacter* Character = TestCharacter.Get();
	if (!Character)
	{
		FinishTest();
		return;
	}
	const FEntry& E = Entries[TestIndex];
	const double Elapsed = GetWorld()->GetTimeSeconds() - PhaseStartSeconds;
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (bWalking)
	{
		Character->AddMovementInput(FVector(-1.0, 0.0, 0.0), 1.0f);
		const double Needed = (2.0 * E.RadiusCm + 600.0) / FMath::Max(Move->GetMaxSpeed(), 100.0f) + 1.0;
		if (Elapsed < Needed)
		{
			return;
		}
		const double Ahead = Character->GetActorLocation().X - E.Center.X;   // > 0: stopped before the centre; < 0: went past it
		if (E.Expected == EMOSpeciesContact::BlocksWalk && Probe == 0)
		{
			const bool bStopped = Ahead > 10.0;
			CentreResult = bStopped ? FString::Printf(TEXT("centre: stopped %.0f cm out"), Ahead) : FString::Printf(TEXT("centre: walked through (%.0f cm past)"), -Ahead);
			if (!bStopped)
			{
				FinishEntry(false, CentreResult);
				return;
			}
			Probe = 1;
			BeginProbe();
		}
		else if (E.Expected == EMOSpeciesContact::BlocksWalk)
		{
			const bool bPassed = Ahead < -50.0;
			FinishEntry(bPassed, CentreResult + (bPassed ? FString(TEXT("; off-centre: walked past")) : FString::Printf(TEXT("; off-centre: STOPPED %.0f cm out (the canopy / hull is too wide)"), Ahead)));
		}
		else
		{
			FinishEntry(Ahead < -50.0, Ahead < -50.0 ? FString(TEXT("walked through")) : FString::Printf(TEXT("blocked %.0f cm before its centre"), Ahead));
		}
		return;
	}

	if (Elapsed < GalleryDropSeconds)
	{
		return;
	}
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector L = Character->GetActorLocation();
	const double Feet = L.Z - HalfHeight;
	const UPrimitiveComponent* FloorComponent = Move->CurrentFloor.HitResult.GetComponent();
	// How far below the VISIBLE surface the feet are (a complex trace sees the render triangles): standing on a hull that is lower than the rock = sinking in.
	FHitResult Visible;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MOGalleryVisible), /*bTraceComplex=*/true);
	Params.AddIgnoredActor(Character);
	double Sunk = 0.0;
	if (GetWorld()->LineTraceSingleByChannel(Visible, FVector(L.X, L.Y, E.TopZ + 500.0), FVector(L.X, L.Y, FloorTopZ - 100.0), ECC_Visibility, Params) && Visible.GetComponent() == E.Component)
	{
		Sunk = Visible.ImpactPoint.Z - Feet;
	}
	if (!Move->IsMovingOnGround())
	{
		FinishEntry(false, FString::Printf(TEXT("still falling after %.1f s"), GalleryDropSeconds));
	}
	else if (FloorComponent == Floor)
	{
		const double Moved = FVector2D::Distance(FVector2D(L), FVector2D(E.Center));
		FinishEntry(false, Moved > 60.0 ? FString::Printf(TEXT("slid off it onto the floor (%.0f cm from where she landed: its top is too steep there)"), Moved)
		                                : FString(TEXT("fell through it to the floor")));
	}
	else if (FloorComponent != E.Component)
	{
		FinishEntry(false, FString::Printf(TEXT("landed on %s"), *GetNameSafe(FloorComponent)));
	}
	else if (Sunk > 30.0)
	{
		FinishEntry(false, FString::Printf(TEXT("stands %.0f cm inside it (the collision is lower than the rock)"), Sunk));
	}
	else
	{
		FinishEntry(true, FString::Printf(TEXT("stands on it (feet %.0f cm from the visible surface)"), FMath::Abs(Sunk)));
	}
}

void AMOSpeciesGallery::FinishEntry(bool bPass, const FString& Result)
{
	FEntry& E = Entries[TestIndex];
	E.bPass = bPass;
	E.Result = Result;
	if (E.Text)
	{
		E.Text->SetText(FText::FromString(FString::Printf(TEXT("%d  %s\n%s  %s\n%s: %s"), TestIndex + 1, *FPaths::GetBaseFilename(E.MeshPath), *E.Profile.ToString(),
			FMOSpeciesPlacement::ContactName(E.Expected), bPass ? TEXT("PASS") : TEXT("FAIL"), *Result)));
		E.Text->SetTextRenderColor(bPass ? FColor(90, 230, 90) : FColor(255, 70, 60));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] %s %s expected=%s profile=%s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *E.Name, FMOSpeciesPlacement::ContactName(E.Expected),
		*E.Profile.ToString(), *Result);
	BeginEntry(TestIndex + 1);   // skips walk-through entries, finishes after the last
}

void AMOSpeciesGallery::FinishTest()
{
	int32 Passed = 0, Tested = 0;
	FString Tsv = TEXT("species\tmesh\tprofile\texpected\tresult\tdetail\n");
	for (const FEntry& E : Entries)
	{
		const bool bTested = E.Expected != EMOSpeciesContact::WalkThrough;
		Tested += bTested ? 1 : 0;
		Passed += (bTested && E.bPass) ? 1 : 0;
		Tsv += FString::Printf(TEXT("%s\t%s\t%s\t%s\t%s\t%s\n"), *E.Name, *E.MeshPath, *E.Profile.ToString(), FMOSpeciesPlacement::ContactName(E.Expected),
			!bTested ? TEXT("SKIP") : E.Result.IsEmpty() ? TEXT("-") : (E.bPass ? TEXT("PASS") : TEXT("FAIL")), *E.Result);
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Logs") / FString::Printf(TEXT("MOGallery_%s.tsv"), BiomeId.IsNone() ? TEXT("all") : *BiomeId.ToString());
	FFileHelper::SaveStringToFile(Tsv, *Path);
	UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] contact test done: %d / %d PASS, %d walk-through not tested (%s)"), Passed, Tested, Entries.Num() - Tested, *Path);
	if (ACharacter* Character = TestCharacter.Get())
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->SetActorTransform(TestStartTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	TestIndex = INDEX_NONE;
	SetActorTickEnabled(false);
}

// ============================================================================
// Console verbs
// ============================================================================

namespace
{
	ACharacter* GalleryVisitor(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GGalleryCommand(
		TEXT("MO.Gallery"),
		TEXT("Dev: lay out every species of a biome on a test floor high over the ocean and stand there (MO.Gallery.Test drops the character on each). Usage: MO.Gallery [BiomeId|all]  (no argument: the biome you stand in)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ACharacter* Visitor = GalleryVisitor(World);
			if (!Visitor || World->GetNetMode() == NM_Client)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] needs a local character on the host / standalone"));
				return;
			}
			FName Biome = NAME_None;
			FVector Origin(0.0, 0.0, 500000.0);
			if (FMOWorldGenParamsProvider::HasPublished())
			{
				const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
				const double Corner = P->Tuning.WorldHalfSizeCm * 0.8;
				Origin = FVector(P->GetWorldCentre().X + Corner, P->GetWorldCentre().Y + Corner, 300000.0);   // 3 km over the island's ocean corner: nothing of the world in the way
				const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
				if (Args.Num() == 0 && Seeds && Seeds->HasActiveSeed())
				{
					const FVector L = Visitor->GetActorLocation();
					Biome = P->GetBiomeId(FMOWorldGen::SampleColumn(*P, L.X, L.Y, Seeds->GetActiveTerrainSeed(), P->Tuning.VoxelSizeCm).BiomeIndex);
				}
			}
			if (Args.Num() > 0 && !Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
			{
				Biome = FName(*Args[0]);
			}
			FTransform Return = Visitor->GetActorTransform();
			if (AMOSpeciesGallery* Old = AMOSpeciesGallery::Find(World))
			{
				Return = Old->ReturnTransform;
				Old->Destroy();
			}
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AMOSpeciesGallery* Gallery = World->SpawnActor<AMOSpeciesGallery>(Origin, FRotator::ZeroRotator, Spawn);
			const int32 Count = Gallery ? Gallery->Build(Biome) : 0;
			if (Count == 0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] %s: no species"), Biome.IsNone() ? TEXT("all") : *Biome.ToString());
				if (Gallery)
				{
					Gallery->Destroy();
				}
				return;
			}
			Gallery->ReturnTransform = Return;
			const FTransform View = Gallery->GetViewpoint();
			Visitor->GetCharacterMovement()->StopMovementImmediately();
			Visitor->SetActorLocationAndRotation(View.GetLocation(), FRotator(0.0, View.Rotator().Yaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* Controller = Visitor->GetController())
			{
				Controller->SetControlRotation(View.Rotator());
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] %s: %d species laid out (MO.Gallery.Test drops the character on each, MO.Gallery.Clear goes back)"),
				Biome.IsNone() ? TEXT("every biome") : *Biome.ToString(), Count);
		}));

	FAutoConsoleCommandWithWorldAndArgs GGalleryTestCommand(
		TEXT("MO.Gallery.Test"),
		TEXT("Dev: drop / walk the character on every species of the gallery (standable: stands on it; blocks-walk: stopped; walk-through: passes). PASS/FAIL on the labels, the log and Saved/Logs/MOGallery_<biome>.tsv."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			AMOSpeciesGallery* Gallery = AMOSpeciesGallery::Find(World);
			ACharacter* Visitor = GalleryVisitor(World);
			if (!Gallery || !Visitor)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOGallery] no gallery (MO.Gallery first) or no local character"));
				return;
			}
			Gallery->StartContactTest(Visitor);
		}));

	FAutoConsoleCommandWithWorldAndArgs GGalleryClearCommand(
		TEXT("MO.Gallery.Clear"),
		TEXT("Dev: remove the species gallery and go back to where you were."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			AMOSpeciesGallery* Gallery = AMOSpeciesGallery::Find(World);
			if (!Gallery)
			{
				return;
			}
			if (ACharacter* Visitor = GalleryVisitor(World))
			{
				Visitor->GetCharacterMovement()->StopMovementImmediately();
				Visitor->SetActorTransform(Gallery->ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
			}
			Gallery->Destroy();
		}));
}
