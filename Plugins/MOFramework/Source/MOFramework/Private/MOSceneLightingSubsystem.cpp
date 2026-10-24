#include "MOSceneLightingSubsystem.h"
#include "Actors/OceanologyInfiniteOceanActor.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "MOFramework.h"

namespace
{
	TAutoConsoleVariable<float> CVarSkylightLeaking(TEXT("mo.Light.SkylightLeaking"), 0.1f,
		TEXT("Lumen Skylight Leaking in every game world (MOSceneLightingSubsystem): the share of the sky light that still reaches places the scene occludes, "
			 "the light a forest floor gets through and between the leaves. 0 = none (a closed canopy leaves the floor near black); caves stay dark either "
			 "way (the sky actor zeroes the sky light inside)."), ECVF_Default);

	TAutoConsoleVariable<float> CVarInteriorSkyLight(TEXT("mo.Light.InteriorSkyLight"), 0.35f,
		TEXT("Ultra Dynamic Sky's 'Sky Light Intensity Multiplier in Interiors' (MOSceneLightingSubsystem sets it on the sky actor): the sky light left "
			 "where its occlusion test reads enclosed. 0 = black (a cave; also a dense forest taken for one, which switched dark and light as the test "
			 "flipped: Wes, Oct 24 2026, \"i'd rather things be a little unnaturally bright than to have it being like a lgiht switch\")."), ECVF_Default);

	// The ocean's shoreline wetness (a 1 km decal Oceanology lays along the waterline): its preset made everything within 4 m of the water 75 %
	// METALLIC at roughness 0.1, beach rocks and the pawn's skin read as chrome (Wes, Oct 24 2026, BR24-18: "the wetness effect goes way too high on
	// things and just looks metallic instead of wet"). Wet rock and skin are darker and glossier, never metallic.
	TAutoConsoleVariable<float> CVarShoreWetMetallic(TEXT("mo.Water.ShoreWetMetallic"), 0.0f, TEXT("The ocean's shoreline wetness: metallic (the preset's 0.75 read as chrome)."), ECVF_Default);
	TAutoConsoleVariable<float> CVarShoreWetRoughness(TEXT("mo.Water.ShoreWetRoughness"), 0.25f, TEXT("The ocean's shoreline wetness: roughness of the wet band (the preset's 0.1)."), ECVF_Default);
	TAutoConsoleVariable<float> CVarShoreWetRadius(TEXT("mo.Water.ShoreWetRadius"), 100.0f, TEXT("The ocean's shoreline wetness: how far (cm) about the waterline the ground is wet (the preset's 400)."), ECVF_Default);

	/** A Blueprint number variable of the sky actor, by its (spaced) name; false if it has none. */
	bool SetSkyNumber(UObject* Sky, const TCHAR* Name, double Value)
	{
		FProperty* Prop = Sky ? Sky->GetClass()->FindPropertyByName(FName(Name)) : nullptr;
		if (FDoubleProperty* D = CastField<FDoubleProperty>(Prop))
		{
			D->SetPropertyValue_InContainer(Sky, Value);
			return true;
		}
		if (FFloatProperty* F = CastField<FFloatProperty>(Prop))
		{
			F->SetPropertyValue_InContainer(Sky, static_cast<float>(Value));
			return true;
		}
		return false;
	}
}

bool UMOSceneLightingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UMOSceneLightingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Holder = InWorld.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Holder)
	{
		return;
	}
	SkyLeak = NewObject<UPostProcessComponent>(Holder, TEXT("MOSkylightLeak"));
	SkyLeak->bUnbound = true;
	SkyLeak->Priority = -1.0f;   // under the sky actor's (1), the water's (98-99) and any level volume: they win where they set it
	Holder->SetRootComponent(SkyLeak);
	SkyLeak->RegisterComponent();
	Apply();
	CVarChangedHandle = CVarSkylightLeaking->OnChangedDelegate().AddWeakLambda(this, [this](IConsoleVariable*) { Apply(); });
	InteriorChangedHandle = CVarInteriorSkyLight->OnChangedDelegate().AddWeakLambda(this, [this](IConsoleVariable*) { Apply(); });
	// The ocean loads its preset (wetness included) when it begins play, after this: set the shoreline wetness on the next tick.
	InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { ApplyShoreWetness(); }));
	for (IConsoleVariable* Var : { CVarShoreWetMetallic.AsVariable(), CVarShoreWetRoughness.AsVariable(), CVarShoreWetRadius.AsVariable() })
	{
		WetChangedHandles.Add(Var->OnChangedDelegate().AddWeakLambda(this, [this](IConsoleVariable*) { ApplyShoreWetness(); }));
	}
}

void UMOSceneLightingSubsystem::ApplyShoreWetness()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	int32 Oceans = 0;
	for (TActorIterator<AOceanologyInfiniteOcean> It(World); It; ++It)
	{
		It->Wetness.MoistureMetallic = FMath::Clamp(CVarShoreWetMetallic.GetValueOnGameThread(), 0.0f, 1.0f);
		It->Wetness.MoistureRoughness = FMath::Clamp(CVarShoreWetRoughness.GetValueOnGameThread(), 0.0f, 1.0f);
		It->Wetness.WetRadius = FMath::Max(CVarShoreWetRadius.GetValueOnGameThread(), 0.0f);
		It->InitShorelineWetness();   // pushes the struct into its decal's material
		++Oceans;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOSceneLighting] shoreline wetness on %d ocean(s): metallic %.2f roughness %.2f radius %.0f cm"), Oceans,
		CVarShoreWetMetallic.GetValueOnGameThread(), CVarShoreWetRoughness.GetValueOnGameThread(), CVarShoreWetRadius.GetValueOnGameThread());
}

void UMOSceneLightingSubsystem::Apply()
{
	if (!SkyLeak)
	{
		return;
	}
	const float Leak = FMath::Max(CVarSkylightLeaking.GetValueOnGameThread(), 0.0f);
	SkyLeak->Settings.bOverride_LumenSkylightLeaking = Leak > 0.0f;
	SkyLeak->Settings.LumenSkylightLeaking = Leak;

	// The sky actor's interior floor (a level actor: found each time, so a later sky or a changed cvar applies).
	const float Interior = FMath::Clamp(CVarInteriorSkyLight.GetValueOnGameThread(), 0.0f, 1.0f);
	bool bSky = false;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("Ultra_Dynamic_Sky")))
			{
				bSky |= SetSkyNumber(*It, TEXT("Sky Light Intensity Multiplier in Interiors"), Interior);
			}
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOSceneLighting] Lumen skylight leaking %.2f; interior sky light %.2f%s"), Leak, Interior, bSky ? TEXT("") : TEXT(" (no sky actor)"));
}

void UMOSceneLightingSubsystem::Deinitialize()
{
	if (CVarChangedHandle.IsValid())
	{
		CVarSkylightLeaking->OnChangedDelegate().Remove(CVarChangedHandle);
	}
	if (InteriorChangedHandle.IsValid())
	{
		CVarInteriorSkyLight->OnChangedDelegate().Remove(InteriorChangedHandle);
	}
	IConsoleVariable* WetVars[] = { CVarShoreWetMetallic.AsVariable(), CVarShoreWetRoughness.AsVariable(), CVarShoreWetRadius.AsVariable() };
	for (int32 I = 0; I < WetChangedHandles.Num() && I < 3; ++I)
	{
		WetVars[I]->OnChangedDelegate().Remove(WetChangedHandles[I]);
	}
	WetChangedHandles.Reset();
	Super::Deinitialize();
}
