// Dev verbs for the swimming unit. Output lines start with [MOQUERY] WATER.
//
//   MO.Water.Swim                        the local pawn's water state: oracle surface, immersion, movement mode, head under, oxygen reserve, SpO2, stamina
//   MO.Water.GoToLake [rank=0] [dry]     teleport the local pawn into the rank-th nearest upland lake (centre, a metre under the surface so it floats up); `dry` = onto the bank a few metres from the shore
//   MO.Water.SetBreath <0..1>            set the local pawn's oxygen reserve (tests)
//   MO.Water.Stroke [seconds=5] [up|down] hold a swimming stroke along the camera direction (or straight up / down) for a few seconds: the held keys, for tests

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "MOFramework.h"
#include "MORespirationComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOVitalsComponent.h"
#include "MOWaterSubsystem.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"

namespace
{
	AMOCharacter* LocalMOPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AMOCharacter>(PC->GetPawn()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GSwimCmd(
		TEXT("MO.Water.Swim"),
		TEXT("Dev: log the local pawn's water state (oracle surface, immersion, mode, head under, oxygen reserve, SpO2, stamina)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Swim FAILED: no local MO pawn"));
				return;
			}
			const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(Pawn->GetCharacterMovement());
			float Surface = 0.0f;
			const bool bHave = Move && Move->GetWaterSurfaceZ(Surface);
			const UMORespirationComponent* Breath = Pawn->GetRespirationComponent();
			const UMOVitalsComponent* Vitals = Pawn->GetVitalsComponent();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOQUERY] WATER Swim pos=(%.0f, %.0f, %.0f) surface=%s%.0f immersion=%.2f depth=%.0fcm mode=%s swimming=%d headUnder=%d | airwayClosed=%d reserve=%.2f spo2=%.1f asphyxiating=%.1fs | stamina=%.1f speed=%.0f cm/s"),
				Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z,
				bHave ? TEXT("") : TEXT("(none) "), Surface, Move ? Move->GetImmersion() : 0.0f, Move ? Move->GetWaterDepthCm() : 0.0f,
				Move ? *UEnum::GetValueAsString(Move->MovementMode.GetValue()) : TEXT("?"), Pawn->IsSwimming() ? 1 : 0, (Move && Move->IsHeadUnderwater()) ? 1 : 0,
				(Breath && Breath->IsAirwayClosed()) ? 1 : 0, Breath ? Breath->GetOxygenReserve() : -1.0f, Vitals ? Vitals->Vitals.SpO2 : -1.0f, Breath ? Breath->GetSecondsAsphyxiating() : 0.0f,
				Pawn->GetSurvivalStatsComponent() ? Pawn->GetSurvivalStatsComponent()->GetStatCurrent(TEXT("Stamina")) : -1.0f, Pawn->GetVelocity().Size());
		}));

	FAutoConsoleCommandWithWorldAndArgs GGoToLakeCmd(
		TEXT("MO.Water.GoToLake"),
		TEXT("Dev: teleport the local pawn into the rank-th nearest upland lake. Usage: MO.Water.GoToLake [rank=0] [dry]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
			if (!Pawn || !Seeds || !Seeds->HasActiveSeed())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToLake FAILED: needs a local MO pawn and an active world seed"));
				return;
			}
			int32 Rank = 0;
			bool bDry = false;
			for (const FString& A : Args)
			{
				if (A.IsNumeric())
				{
					Rank = FMath::Max(0, FCString::Atoi(*A));
				}
				else if (A.Equals(TEXT("dry"), ESearchCase::IgnoreCase))
				{
					bDry = true;
				}
			}
			const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
			const FVector Here = Pawn->GetActorLocation();
			TArray<FMOWorldGen::FLake> Lakes;
			FMOWorldGen::GetUplandLakesNear(*P, Here.X, Here.Y, 600000.0, Seeds->GetActiveTerrainSeed(), Lakes);
			Lakes.Sort([&Here](const FMOWorldGen::FLake& A, const FMOWorldGen::FLake& B)
			{
				return FVector2D::DistSquared(A.Center, FVector2D(Here.X, Here.Y)) < FVector2D::DistSquared(B.Center, FVector2D(Here.X, Here.Y));
			});
			if (!Lakes.IsValidIndex(Rank))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToLake FAILED: %d lake(s) within 6 km, rank %d asked"), Lakes.Num(), Rank);
				return;
			}
			const FMOWorldGen::FLake& L = Lakes[Rank];
			FVector Dest;
			if (bDry)
			{
				// 1.15 radii out along the long axis, on the ground there + 2.5 m (the terrain streams in under the pawn)
				const double X = L.Center.X + 1.15 * L.RadiusCm * L.CosAngle, Y = L.Center.Y + 1.15 * L.RadiusCm * L.SinAngle;
				Dest = FVector(X, Y, FMOWorldGen::SampleHeightCm(*P, X, Y, Seeds->GetActiveTerrainSeed(), P->Tuning.VoxelSizeCm) + 250.0);
			}
			else
			{
				Dest = FVector(L.Center.X, L.Center.Y, L.SurfaceCm - 60.0f);   // a body length under the surface: it floats up into the swim
			}
			Pawn->SetActorLocation(Dest, false, nullptr, ETeleportType::ResetPhysics);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToLake rank %d: lake at (%.0f, %.0f) radius %.0f surface %.0f depth %.0f -> pawn at (%.0f, %.0f, %.0f)%s"),
				Rank, L.Center.X, L.Center.Y, L.RadiusCm, L.SurfaceCm, L.DepthCm, Dest.X, Dest.Y, Dest.Z, bDry ? TEXT(" (bank)") : TEXT(""));
		}));

	FAutoConsoleCommandWithWorldAndArgs GStrokeCmd(
		TEXT("MO.Water.Stroke"),
		TEXT("Dev: hold a swimming stroke for a few seconds. Usage: MO.Water.Stroke [seconds=5] [up|down]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			if (!Pawn)
			{
				return;
			}
			double Seconds = 5.0;
			int32 Vertical = 0;
			for (const FString& A : Args)
			{
				if (A.IsNumeric())
				{
					Seconds = FMath::Clamp((double)FCString::Atof(*A), 0.5, 120.0);
				}
				else if (A.Equals(TEXT("up"), ESearchCase::IgnoreCase))
				{
					Vertical = 1;
				}
				else if (A.Equals(TEXT("down"), ESearchCase::IgnoreCase))
				{
					Vertical = -1;
				}
			}
			const double EndTime = FPlatformTime::Seconds() + Seconds;
			TWeakObjectPtr<AMOCharacter> Weak(Pawn);
			const double StartTime = FPlatformTime::Seconds();
			TSharedRef<double> LastLog = MakeShared<double>(0.0);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, EndTime, StartTime, LastLog, Vertical](float)
			{
				AMOCharacter* P = Weak.Get();
				if (!P || FPlatformTime::Seconds() >= EndTime)
				{
					return false;
				}
				if (FPlatformTime::Seconds() - *LastLog >= 0.5)
				{
					*LastLog = FPlatformTime::Seconds();
					const UCharacterMovementComponent* M = P->GetCharacterMovement();
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Stroke t=%.1f z=%.0f v=(%.0f, %.0f, %.0f) mode=%s | accel=(%.0f, %.0f, %.0f) maxSpeed=%.0f maxAccel=%.0f buoyancy=%.2f immersion=%.2f input=%.2f"), FPlatformTime::Seconds() - StartTime, P->GetActorLocation().Z, P->GetVelocity().X, P->GetVelocity().Y, P->GetVelocity().Z, P->IsSwimming() ? TEXT("swim") : TEXT("other"), M->GetCurrentAcceleration().X, M->GetCurrentAcceleration().Y, M->GetCurrentAcceleration().Z, M->GetMaxSpeed(), M->GetMaxAcceleration(), M->Buoyancy, M->ImmersionDepth(), P->GetPendingMovementInputVector().Z);
				}
				if (Vertical != 0)
				{
					P->AddMovementInput(FVector::UpVector, (float)Vertical);
				}
				else if (P->GetController())
				{
					P->AddMovementInput(P->GetController()->GetControlRotation().Vector(), 1.0f);
				}
				return true;
			}), 0.0f);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Stroke for %.1f s (%s)"), Seconds, Vertical > 0 ? TEXT("up") : Vertical < 0 ? TEXT("down") : TEXT("forward along the camera"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GSetBreathCmd(
		TEXT("MO.Water.SetBreath"),
		TEXT("Dev: set the local pawn's oxygen reserve (0..1). Usage: MO.Water.SetBreath <reserve>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			if (Pawn && Pawn->GetRespirationComponent() && Args.Num() > 0 && Args[0].IsNumeric())
			{
				Pawn->GetRespirationComponent()->DebugSetOxygenReserve(FCString::Atof(*Args[0]));
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER SetBreath %.2f"), Pawn->GetRespirationComponent()->GetOxygenReserve());
			}
		}));
}

#endif // !UE_BUILD_SHIPPING
