/**
 * MOAmbientEnvironmentSubsystem.cpp - What the local player hears, from where they stand (see the header)
 */

#include "MOAmbientEnvironmentSubsystem.h"
#include "MOAudioSubsystem.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOWorldSeedSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

bool UMOAmbientEnvironmentSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	// Played worlds with a listener: not the editor preview, not a dedicated server.
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

TStatId UMOAmbientEnvironmentSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMOAmbientEnvironmentSubsystem, STATGROUP_Tickables);
}

float UMOAmbientEnvironmentSubsystem::DistanceToWaterCm(const FMOWorldGenParams& Params, int32 TerrainSeed, double X, double Y, float MaxCm, float& OutSurfaceCm)
{
	const FMOWorldGenTuning& T = Params.Tuning;
	float Best = MaxCm * 2.0f;   // "none within MaxCm"
	OutSurfaceCm = T.SeaLevelCm;

	// Upland lakes first: their surface is their own height and the ellipse is the water. The distance to the shore is approximated by the long radius, close enough for "within ~50 m".
	TArray<FMOWorldGen::FLake> Lakes;
	FMOWorldGen::GetUplandLakesNear(Params, X, Y, (double)MaxCm + 20000.0, TerrainSeed, Lakes);
	for (const FMOWorldGen::FLake& L : Lakes)
	{
		const float D = FMath::Max((float)FVector2D::Distance(FVector2D(X, Y), L.Center) - L.RadiusCm, 0.0f);
		if (D <= MaxCm && D < Best)
		{
			Best = D;
			OutSurfaceCm = L.SurfaceCm;
		}
	}

	// Sea-level water (sea, rivers, ponds, sea-level lakes): the ground under it is below the sea plane. The listener's own cell, then rings at FIXED radii (1/8 .. 8/8 of MaxCm) in 8 directions.
	// The rings do not depend on the caller or on any previous state: the same position always measures the same distance (see the FLAPPING note in the header).
	auto IsWet = [&](double Px, double Py) { return FMOWorldGen::SampleHeightCm(Params, Px, Py, TerrainSeed, T.VoxelSizeCm) < T.SeaLevelCm - 20.0f; };
	if (IsWet(X, Y))
	{
		OutSurfaceCm = T.SeaLevelCm;
		return 0.0f;
	}
	static constexpr float Dir[8][2] = { { 1.0f, 0.0f }, { 0.7071f, 0.7071f }, { 0.0f, 1.0f }, { -0.7071f, 0.7071f }, { -1.0f, 0.0f }, { -0.7071f, -0.7071f }, { 0.0f, -1.0f }, { 0.7071f, -0.7071f } };
	for (int32 Ring = 1; Ring <= 8; ++Ring)
	{
		const float R = MaxCm * (float)Ring / 8.0f;
		if (R >= Best)
		{
			break;   // an upland lake is already nearer than every remaining ring
		}
		for (int32 K = 0; K < 8; ++K)
		{
			if (IsWet(X + (double)(Dir[K][0] * R), Y + (double)(Dir[K][1] * R)))
			{
				OutSurfaceCm = T.SeaLevelCm;
				return R;
			}
		}
	}
	return Best;
}

EMOAmbientState UMOAmbientEnvironmentSubsystem::Classify(const FMOWorldGenParams& Params, int32 TerrainSeed, const FVector& Where, EMOAmbientState Previous)
{
	const FMOWorldGenTuning& T = Params.Tuning;

	// Underground: how far below the surface the listener is. A sinkhole's shaft counts (the surface over it is where the ground would be).
	const float Ground = FMOWorldGen::SampleHeightCm(Params, Where.X, Where.Y, TerrainSeed, T.VoxelSizeCm);
	const float Depth = Ground - (float)Where.Z;
	const float CaveLimit = Previous == EMOAmbientState::Cave ? CaveDepthCm * 0.66f : CaveDepthCm;
	if (Depth > CaveLimit)
	{
		return EMOAmbientState::Cave;
	}

	// Near water, and not far above it (a cliff top over the sea does not hear the shore). ONE measurement of the distance, compared with the entry distance coming from the open and the
	// exit distance coming from Water: a position's distance is the same whichever state asks, so it settles in one step and cannot flap.
	const float ExitCm = WaterRadiusCm * 1.6f;
	float Surface = 0.0f;
	const float Distance = DistanceToWaterCm(Params, TerrainSeed, Where.X, Where.Y, ExitCm, Surface);
	const float Limit = Previous == EMOAmbientState::Water ? ExitCm : WaterRadiusCm;
	if (Distance <= Limit && Where.Z < Surface + 3000.0f)
	{
		return EMOAmbientState::Water;
	}
	return EMOAmbientState::Outdoor_Day;
}

void UMOAmbientEnvironmentSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeSinceEvaluate += DeltaTime;
	TimeSinceChange += DeltaTime;
	if (TimeSinceEvaluate < 0.75f)
	{
		return;
	}
	TimeSinceEvaluate = 0.0f;

	UWorld* World = GetWorld();
	const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
	UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(World);
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Seeds || !Seeds->HasActiveSeed() || !Audio || !Pawn || !FMOWorldGenParamsProvider::HasPublished())
	{
		return;
	}
	const EMOAmbientState Current = Audio->GetAmbientState();
	if (Current == EMOAmbientState::Indoor)
	{
		return;   // a building decides its own sound
	}

	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	const EMOAmbientState Wanted = Classify(*Params, Seeds->GetActiveTerrainSeed(), Pawn->GetActorLocation(), Current);
	if (Wanted == EMOAmbientState::Cave || Wanted == EMOAmbientState::Water)
	{
		if (Wanted != Current && TimeSinceChange >= MinDwellSeconds)
		{
			Audio->SetAmbientState(Wanted);
			TimeSinceChange = 0.0f;
		}
		return;
	}
	// In the open: back to what the clock says, but only when we are the ones who moved it to Cave / Water -- and not sooner than MinDwellSeconds after the last change.
	if ((Current == EMOAmbientState::Cave || Current == EMOAmbientState::Water) && TimeSinceChange >= MinDwellSeconds)
	{
		const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(World);
		Audio->SetAmbientState(!Clock || Clock->IsDaytime() ? EMOAmbientState::Outdoor_Day : EMOAmbientState::Outdoor_Night);
		TimeSinceChange = 0.0f;
	}
}
