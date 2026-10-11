/**
 * MOMenuFlyby.cpp - The main menu's living backdrop (see the header)
 */

#include "MOMenuFlyby.h"
#include "MOFramework.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMOMenuFlyby::AMOMenuFlyby()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 72.0f;

	// The loop is a fixed path in WORLD space. It must not ride along with the camera: attached normally it would move with this actor every tick (the actor is the
	// camera that flies along it), and the lap would run away at exponential speed (measured: 390 km from the origin after a minute).
	Loop = CreateDefaultSubobject<USplineComponent>(TEXT("Loop"));
	Loop->SetupAttachment(Root);
	Loop->SetAbsolute(true, true, true);
	Loop->SetClosedLoop(true);
}

float AMOMenuFlyby::GroundAt(const FVector2D& XY) const
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	// Coarse sampling (octaves finer than 2 x 50 m skipped): the camera does not care about metre-scale roughness, and this runs every frame.
	return FMath::Max(FMOWorldGen::SampleHeightCm(*Params, XY.X, XY.Y, TerrainSeed, 5000.0f), Params->Tuning.SeaLevelCm);
}

void AMOMenuFlyby::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	// A backdrop is for a screen: a dedicated server that happens to start on the menu map has nothing to show and must not generate a world for it.
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}

	// The backdrop terrain is a normal generated world: same seed handling as a game, minus replication.
	if (UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World))
	{
		Seeds->ApplyBackdropSeed(BackdropWorldSeed, VoxelSeedParameterName);
	}
	TerrainSeed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(BackdropWorldSeed);

	// Clear skies, like the start of a game (queued until the weather bridge has registered).
	if (UMOWeatherIntegrationSubsystem* Weather = World->GetSubsystem<UMOWeatherIntegrationSubsystem>())
	{
		Weather->ApplyNewGameStartConditions();
	}

	BuildLoop();
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTarget(this);
	}
}

void AMOMenuFlyby::BuildLoop()
{
	const FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::Get();
	FMOCoastalSpawn Coast;
	FVector2D Anchor = FVector2D::ZeroVector;
	if (FMOWorldGen::FindCoastalSpawn(*Params, TerrainSeed, Coast))
	{
		Anchor = Coast.Location;
		const float Yaw = FMath::DegreesToRadians(Coast.YawDegrees);
		SeaDirection = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOMenuFlyby] Seed %d has no coast in range: flying a loop over the origin"), BackdropWorldSeed);
	}
	const FVector2D Across(-SeaDirection.Y, SeaDirection.X);
	const double L = (double)LoopSizeCm;

	// The anchor is the new-game coast: the shore the generator picked BECAUSE the far side of the water has the best skyline. The lap runs along that shore (a pass of 2 x L right at the
	// water's edge, where the view across the bay is) and comes back through the land behind it (the trees and the hills). Coordinates in the (toward-sea, across) frame, in units of L.
	// A flight over open sea alone is a blue screen: the whole loop is shifted along the sea direction until about 70% of its length is over land (the generator knows which).
	static constexpr double Frame[][2] = { { -0.10, -1.00 }, { -0.10, -0.35 }, { -0.04, 0.30 }, { -0.10, 1.00 }, { -0.55, 1.25 }, { -0.95, 0.60 }, { -0.95, -0.45 }, { -0.55, -1.25 } };
	constexpr int32 NumFrame = UE_ARRAY_COUNT(Frame);
	auto PointAt = [&](double Shift, int32 I) { return Anchor + SeaDirection * ((Frame[I][0] + Shift) * L) + Across * (Frame[I][1] * L); };
	double BestShift = 0.0, BestScore = TNumericLimits<double>::Max();
	for (const double Shift : { 0.15, 0.05, -0.05, -0.15, -0.3, -0.45 })
	{
		int32 Land = 0, Total = 0;
		for (int32 I = 0; I < NumFrame; ++I)
		{
			const FVector2D A = PointAt(Shift, I), B = PointAt(Shift, (I + 1) % NumFrame);
			for (int32 K = 0; K < 12; ++K, ++Total)
			{
				const FVector2D Q = FMath::Lerp(A, B, (double)K / 12.0);
				Land += FMOWorldGen::SampleHeightCm(*Params, Q.X, Q.Y, TerrainSeed, 5000.0f) > Params->Tuning.SeaLevelCm + 50.0f ? 1 : 0;
			}
		}
		const double Score = FMath::Abs((double)Land / (double)Total - 0.70);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestShift = Shift;
		}
	}
	Loop->SetWorldTransform(FTransform::Identity);
	Loop->ClearSplinePoints(false);
	LoopCentre = Anchor + SeaDirection * ((-0.45 + BestShift) * L);
	for (int32 I = 0; I < NumFrame; ++I)
	{
		const FVector2D XY = PointAt(BestShift, I);
		Loop->AddSplinePoint(FVector(XY.X, XY.Y, 0.0), ESplineCoordinateSpace::World, false);
	}
	Loop->SetClosedLoop(true, false);
	Loop->UpdateSpline();
	// Where the flight starts: the loop's first point can be over the sea (the shift above only keeps ~70% of the WHOLE loop on land), and the menu would open on bare water for
	// minutes at 9 m/s. Start at the first point that has planted ground under the camera and open water ahead -- the picture the loop was laid for.
	DistanceAlongLoop = 0.0f;
	const float SeaCm = Params->Tuning.SeaLevelCm;
	for (float D = 0.0f; D < Loop->GetSplineLength(); D += 20000.0f)
	{
		const FVector P = Loop->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World);
		if (FMOWorldGen::SampleHeightCm(*Params, P.X, P.Y, TerrainSeed, 5000.0f) < SeaCm + 400.0f)
		{
			continue;
		}
		bool bWaterAhead = false;
		for (const double ReachCm : { 30000.0, 60000.0, 100000.0, 150000.0 })
		{
			const FVector2D Q = FVector2D(P.X, P.Y) + SeaDirection * ReachCm;
			bWaterAhead = bWaterAhead || FMOWorldGen::SampleHeightCm(*Params, Q.X, Q.Y, TerrainSeed, 5000.0f) < SeaCm - 50.0f;
		}
		if (bWaterAhead)
		{
			DistanceAlongLoop = D;
			break;
		}
	}
	bLoopReady = true;
	UE_LOG(LogMOFramework, Log, TEXT("[MOMenuFlyby] Seed %d: loop of %.1f km laid over (%.0f, %.0f), the sea lies toward yaw %.0f, the flight starts %.1f km along it"), BackdropWorldSeed, Loop->GetSplineLength() / 100000.0f,
		Anchor.X, Anchor.Y, FMath::RadiansToDegrees(FMath::Atan2(SeaDirection.Y, SeaDirection.X)), DistanceAlongLoop / 100000.0f);
}

void AMOMenuFlyby::JumpToFraction(float Fraction)
{
	if (bLoopReady)
	{
		DistanceAlongLoop = FMath::Clamp(Fraction, 0.0f, 1.0f) * Loop->GetSplineLength();
		bHaveZ = false;   // snap the height too
	}
}

void AMOMenuFlyby::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLoopReady || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}
	Age += DeltaSeconds;

	// Keep (or win back) the view: the menu's controller may set its own target while it builds its widgets.
	TimeSinceViewCheck += DeltaSeconds;
	if (TimeSinceViewCheck >= 2.0f)
	{
		TimeSinceViewCheck = 0.0f;
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->GetViewTarget() != this)
			{
				PC->SetViewTarget(this);
			}
		}
	}

	DistanceAlongLoop = FMath::Fmod(DistanceAlongLoop + SpeedCmPerSecond * DeltaSeconds, FMath::Max(Loop->GetSplineLength(), 1.0f));
	const FVector OnLoop = Loop->GetLocationAtDistanceAlongSpline(DistanceAlongLoop, ESplineCoordinateSpace::World);
	const FVector2D Here(OnLoop.X, OnLoop.Y);
	const FVector Tangent = Loop->GetDirectionAtDistanceAlongSpline(DistanceAlongLoop, ESplineCoordinateSpace::World);

	// Height: the highest ground within the next 150 m of flight (the camera rises BEFORE a ridge), eased so it never jerks.
	float Ahead = GroundAt(Here);
	for (const float LookCm : { 3000.0f, 6000.0f, 10000.0f, 15000.0f })
	{
		Ahead = FMath::Max(Ahead, GroundAt(Here + FVector2D(Tangent.X, Tangent.Y) * (double)LookCm));
	}
	const float TargetZ = Ahead + ClearanceCm;
	SmoothedZ = bHaveZ ? FMath::FInterpTo(SmoothedZ, TargetZ, DeltaSeconds, 0.5f) : TargetZ;
	bHaveZ = true;

	// Heading: along the flight, turned toward the sea; a slow sway (two incommensurate periods) so it never feels railed.
	const FVector2D Flight(Tangent.X, Tangent.Y);
	const FVector2D ToSea = (SeaDirection * 100000.0 + (LoopCentre - Here)).GetSafeNormal();
	const double FlightYaw = FMath::Atan2(Flight.Y, Flight.X);
	double Delta = FMath::Atan2(ToSea.Y, ToSea.X) - FlightYaw;
	Delta = FMath::UnwindRadians(Delta);
	const float Sway = 2.5f * FMath::Sin(Age * 0.11f) + 1.2f * FMath::Sin(Age * 0.27f + 1.3f);
	const float Yaw = FMath::RadiansToDegrees(FlightYaw + Delta * LookTowardSea) + Sway;
	const float Pitch = PitchDegrees + 1.5f * FMath::Sin(Age * 0.17f + 0.6f);

	SetActorLocationAndRotation(FVector(Here.X, Here.Y, SmoothedZ), FRotator(Pitch, Yaw, 0.0f));
}
