// The water is Oceanology (see MOOceanologyIntegration.h). Two things are pinned here, both found the hard way on 2026-10-11:
//  1. The level oceans: each shipped level has an Oceanology ocean and NO Epic water body (two oceans in one level draw on top of each other and the Epic one carries its own, different underwater look).
//  2. The underwater look: the ocean's post process is bounded by a box with no collision, which UE 5.8 can never measure the camera against, so the deep-blue underwater fog never appeared. The
//     integration subsystem gives the box a (query-only, ignore-everything) body; this test spawns the real ocean actor in a game world and asks the engine's own question, the one the renderer asks
//     every frame: does this post process volume encompass the camera?

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Actors/OceanologyInfiniteOceanActor.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "MOOceanologyIntegration.h"
#include "OceanologyRuntimeSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWater_LevelOceans,
	"MOFramework.Water.LevelOceans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWater_LevelOceans::RunTest(const FString& Parameters)
{
	for (const TCHAR* MapPath : { TEXT("/Game/VoxelExamples/PCGScattering/MOPCGScattering"), TEXT("/Game/Penumbra/Maps/LoadingLevel") })
	{
		if (!FPackageName::DoesPackageExist(MapPath))
		{
			AddWarning(FString::Printf(TEXT("%s is not in this build; its ocean was not checked"), MapPath));
			continue;
		}
		UPackage* Package = LoadPackage(nullptr, MapPath, LOAD_None);
		UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
		if (!World || !World->PersistentLevel)
		{
			AddError(FString::Printf(TEXT("could not load %s"), MapPath));
			continue;
		}
		int32 Oceans = 0, EpicWater = 0;
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (!Actor || Actor->HasAnyFlags(RF_Transient))
			{
				continue;   // the Water plugin spawns a transient BuoyancyManager into every world it initialises: not level data
			}
			Oceans += Actor->IsA<AOceanologyInfiniteOcean>() ? 1 : 0;
			if (Actor->GetClass()->GetOutermost()->GetName().StartsWith(TEXT("/Script/Water")))
			{
				++EpicWater;
				AddInfo(FString::Printf(TEXT("%s: Epic water actor %s of class %s, transient=%d, outer=%s"), MapPath, *Actor->GetName(), *Actor->GetClass()->GetPathName(), Actor->HasAnyFlags(RF_Transient) ? 1 : 0, *Actor->GetOuter()->GetName()));
			}
		}
		TestEqual(*FString::Printf(TEXT("%s has exactly one Oceanology ocean"), MapPath), Oceans, 1);
		TestEqual(*FString::Printf(TEXT("%s has no Epic water actors"), MapPath), EpicWater, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWater_UnderwaterVolumeEncompassesCamera,
	"MOFramework.Water.UnderwaterVolumeEncompassesCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWater_UnderwaterVolumeEncompassesCamera::RunTest(const FString& Parameters)
{
	// The renderer's question (DoPostProcessVolume), asked at the middle of the ocean's underwater volume (deep under the water, comfortably inside it): a bounded volume applies when the distance to
	// the view is >= 0 and within the blend radius.
	auto Applies = [](AOceanologyInfiniteOcean* Ocean)
	{
		float Distance = -1.0f;
		Ocean->UnderwaterPostProcessComponent->EncompassesPoint(Ocean->UnderwaterBoxVolumeComponent->GetComponentLocation(), 0.0f, &Distance);
		return Distance >= 0.0f && Distance <= Ocean->UnderwaterPostProcessComponent->BlendRadius;
	};
	// The infinite ocean FOLLOWS the camera: it is moved across the world every few seconds, and the volume goes with it.
	auto Follow = [](AOceanologyInfiniteOcean* Ocean, const FVector& Where) { Ocean->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics); };

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, TEXT("MOOceanologyTestWorld"));
	if (!TestNotNull(TEXT("a game world for the test"), World))
	{
		return false;
	}
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	IConsoleVariable* FixSwitch = IConsoleManager::Get().FindConsoleVariable(TEXT("MO.Water.FixUnderwaterVolume"));
	if (!TestNotNull(TEXT("the MO.Water.FixUnderwaterVolume switch exists"), FixSwitch))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}
	const int32 SwitchBefore = FixSwitch->GetInt();

	// 1. The ocean as the project runs it: the integration subsystem (a game-world subsystem, so it lives in this world) sees the ocean spawn and fixes its underwater volume.
	AOceanologyInfiniteOcean* Fixed = World->SpawnActor<AOceanologyInfiniteOcean>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (TestNotNull(TEXT("the ocean actor spawned"), Fixed) && TestNotNull(TEXT("it has an underwater volume"), Fixed->UnderwaterBoxVolumeComponent.Get()) && TestNotNull(TEXT("it has an underwater post process"), Fixed->UnderwaterPostProcessComponent.Get()))
	{
		TestFalse(TEXT("the ocean's post process is bounded (it is the underwater volume's job to say where the underwater look applies)"), Fixed->UnderwaterPostProcessComponent->bUnbound);
		TestTrue(TEXT("the integration subsystem made the underwater volume queryable"), MOOceanology::IsUnderwaterVolumeQueryable(*Fixed));
		TestTrue(TEXT("a camera inside the underwater volume is encompassed by the underwater post process"), Applies(Fixed));
		Follow(Fixed, FVector(1400000.0, -1020000.0, 0.0));
		TestTrue(TEXT("... after the ocean followed the camera far from where it spawned"), Applies(Fixed));
		Follow(Fixed, FVector(1500000.0, -1100000.0, 0.0));
		TestTrue(TEXT("... and again"), Applies(Fixed));
		TestEqual(TEXT("the volume ignores every channel (a camera trace or a pawn must never hit the ocean-wide box): Pawn"), Fixed->UnderwaterBoxVolumeComponent->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		TestEqual(TEXT("... Visibility"), Fixed->UnderwaterBoxVolumeComponent->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
		TestEqual(TEXT("... WorldStatic (ground traces)"), Fixed->UnderwaterBoxVolumeComponent->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Ignore);
		TestFalse(TEXT("... and it makes no overlap events"), Fixed->UnderwaterBoxVolumeComponent->GetGenerateOverlapEvents());
	}

	// 2. CONTROL: the ocean as the plugin ships it (the fix switched off while it spawns), registered with no collision like the level's ocean. Once it has moved the engine's volume test answers
	// "infinitely far" for a camera in the middle of it: the bug. If this starts passing, the engine was fixed and the integration can go.
	FixSwitch->Set(0, ECVF_SetByCode);
	AOceanologyInfiniteOcean* Plain = World->SpawnActor<AOceanologyInfiniteOcean>(FVector(0.0, 0.0, 0.0), FRotator::ZeroRotator);
	FixSwitch->Set(SwitchBefore, ECVF_SetByCode);
	if (TestNotNull(TEXT("the control ocean spawned"), Plain) && TestNotNull(TEXT("it has an underwater volume"), Plain->UnderwaterBoxVolumeComponent.Get()))
	{
		TestFalse(TEXT("the control ocean has the plugin's own no-collision volume"), MOOceanology::IsUnderwaterVolumeQueryable(*Plain));
		Follow(Plain, FVector(1600000.0, -1200000.0, 0.0));
		float PlainDistance = -9.0f;
		Plain->UnderwaterPostProcessComponent->EncompassesPoint(Plain->UnderwaterBoxVolumeComponent->GetComponentLocation(), 0.0f, &PlainDistance);
		// NOT asserted: this headless world builds a physics body for the no-collision box (state created, distance 0) where the real game does not (box physics state 0, distance FLT_MAX: the
		// bug, reproduced in play with MO.Water.FixUnderwaterVolume 0 + MO.Water.Underwater, 2026-10-11). The contract the game depends on is the queryable volume asserted above and below.
		AddInfo(FString::Printf(TEXT("control (information only): the plugin's own no-collision box answers distance %.1f for a camera in the middle of it; physics state created %d"), PlainDistance, Plain->UnderwaterBoxVolumeComponent->IsPhysicsStateCreated() ? 1 : 0));

		// The fix, applied to it after the fact (what happens to a level-placed ocean at begin play): idempotent, and it keeps working as the ocean follows the camera.
		TestTrue(TEXT("the fix applies to a no-collision volume"), MOOceanology::EnsureUnderwaterVolumeQueryable(*Plain));
		TestFalse(TEXT("... and does nothing the second time"), MOOceanology::EnsureUnderwaterVolumeQueryable(*Plain));
		TestTrue(TEXT("after the fix the camera is encompassed"), Applies(Plain));
		Follow(Plain, FVector(1700000.0, -1300000.0, 0.0));
		TestTrue(TEXT("... and it keeps being encompassed as the ocean follows the camera"), Applies(Plain));
	}

	FixSwitch->Set(SwitchBefore, ECVF_SetByCode);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

// Oceanology loads its materials, effects and sounds through SOFT paths in UOceanologyRuntimeSettings, which the cooker does not follow. Missing from the cook, they are fine in the editor and fatal in a packaged
// build: the first upland lake to spawn crashed in UOceanologyUnderwaterComponent::InitBubbles (null bubbles material; found by the packaged two-process test, 2026-10-11). Every soft path of the settings that points
// into the plugin must lie under a directory the project always cooks (Config/DefaultGame.ini, DirectoriesToAlwaysCook).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWater_OceanologyAssetsAreCooked,
	"MOFramework.Water.OceanologyAssetsAreCooked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWater_OceanologyAssetsAreCooked::RunTest(const FString& Parameters)
{
	TArray<FString> CookDirectories;
	{
		TArray<FString> Entries;
		GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("DirectoriesToAlwaysCook"), Entries, GGameIni);
		for (const FString& Entry : Entries)
		{
			// (Path="/Oceanology_Plugin/Design/Lake")
			int32 Start = Entry.Find(TEXT("\""));
			int32 End = Start == INDEX_NONE ? INDEX_NONE : Entry.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start + 1);
			if (Start != INDEX_NONE && End != INDEX_NONE)
			{
				CookDirectories.Add(Entry.Mid(Start + 1, End - Start - 1));
			}
		}
	}
	TestTrue(TEXT("the project's always-cook directories were read from DefaultGame.ini"), CookDirectories.Num() > 3);

	const UOceanologyRuntimeSettings* Settings = GetDefault<UOceanologyRuntimeSettings>();
	int32 Checked = 0, Uncovered = 0;
	for (TFieldIterator<FSoftObjectProperty> It(UOceanologyRuntimeSettings::StaticClass()); It; ++It)
	{
		const FSoftObjectPath Path = It->GetPropertyValue_InContainer(Settings).ToSoftObjectPath();
		const FString Package = Path.GetLongPackageName();
		if (Path.IsNull() || !Package.StartsWith(TEXT("/Oceanology_Plugin/")))
		{
			continue;
		}
		++Checked;
		bool bCovered = false;
		for (const FString& Dir : CookDirectories)
		{
			bCovered = bCovered || Package.StartsWith(Dir + TEXT("/"));
		}
		if (!bCovered)
		{
			++Uncovered;
			AddError(FString::Printf(TEXT("%s (setting %s) is loaded at run time but lies under no DirectoriesToAlwaysCook entry: a packaged build will not have it"), *Path.ToString(), *It->GetName()));
		}
	}
	AddInfo(FString::Printf(TEXT("%d Oceanology soft asset paths in the runtime settings checked, %d uncovered"), Checked, Uncovered));
	TestTrue(TEXT("the runtime settings point at plugin assets (the check is not vacuous)"), Checked > 20);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
