// Performance bench: frame-time percentiles per biome, measured on a PACKAGED build (the editor throttles PIE to ~3 fps when its window is not foreground, so editor numbers are meaningless).
//
//   MO.Perf.Bench [seconds=15] [label]            sample the current spot: avg / p50 / p95 / p99 / worst frame ms, GPU / game / render thread ms, draw calls, memory
//   MO.Perf.Tour [settle_s=75] [bench_s=24] [quit=1] [profile_gpu=0] [fixed_yaw=0] [biome ...]
//                                                   for each biome (default: all eight): teleport to a deep-inside dry spot, wait for the terrain + PCG to settle, then bench it; log one summary table;
//                                                   quit the game at the end (quit=0 keeps it running). The tour waits for a game world with a pawn and a world seed, so it can be queued
//                                                   BEFORE the world exists:  MO57.exe -ExecCmds="MO.Perf.Tour; MO.Session.Host PerfTour 1"
//
// Output lines start with [MOPERF]. Not compiled into Shipping builds (the cheats are dev tools).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "MOFramework.h"
#include "MOWorldSeedSubsystem.h"
#include "RHI.h"
#include "RHIStats.h"
#include "RenderTimer.h"

namespace
{
	struct FBenchSample
	{
		TArray<float> FrameMs;
		double SumGpuMs = 0.0, SumGameMs = 0.0, SumRenderMs = 0.0;
		int64 SumDraws = 0;
		int32 GpuFrames = 0;
		double MemMb = 0.0;
	};

	struct FBenchResult
	{
		FString Label;
		int32 Frames = 0;
		float AvgMs = 0, P50 = 0, P95 = 0, P99 = 0, Worst = 0;
		float Gpu = 0, Game = 0, Render = 0;
		int32 Draws = 0;
		double MemMb = 0;
	};

	float PercentileOf(const TArray<float>& Sorted, float P)
	{
		if (Sorted.Num() == 0)
		{
			return 0.0f;
		}
		const int32 I = FMath::Clamp(FMath::CeilToInt(P * (float)Sorted.Num()) - 1, 0, Sorted.Num() - 1);
		return Sorted[I];
	}

	FBenchResult Summarize(const FString& Label, FBenchSample& S)
	{
		FBenchResult R;
		R.Label = Label;
		R.Frames = S.FrameMs.Num();
		if (R.Frames == 0)
		{
			return R;
		}
		S.FrameMs.Sort();
		double Sum = 0.0;
		for (float M : S.FrameMs)
		{
			Sum += M;
		}
		R.AvgMs = (float)(Sum / R.Frames);
		R.P50 = PercentileOf(S.FrameMs, 0.50f);
		R.P95 = PercentileOf(S.FrameMs, 0.95f);
		R.P99 = PercentileOf(S.FrameMs, 0.99f);
		R.Worst = S.FrameMs.Last();
		R.Gpu = S.GpuFrames > 0 ? (float)(S.SumGpuMs / S.GpuFrames) : 0.0f;
		R.Game = (float)(S.SumGameMs / R.Frames);
		R.Render = (float)(S.SumRenderMs / R.Frames);
		R.Draws = (int32)(S.SumDraws / R.Frames);
		R.MemMb = S.MemMb;
		return R;
	}

	void LogResult(const FBenchResult& R)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOPERF] %-18s frames=%d fps=%.1f avg=%.1fms p50=%.1f p95=%.1f p99=%.1f worst=%.1f | gpu=%.1fms game=%.1fms render=%.1fms | draws=%d | mem=%.0fMB"),
			*R.Label, R.Frames, R.AvgMs > 0.0f ? 1000.0f / R.AvgMs : 0.0f, R.AvgMs, R.P50, R.P95, R.P99, R.Worst, R.Gpu, R.Game, R.Render, R.Draws, R.MemMb);
	}

	float GBenchYawForLog = -1.0f;   // the tour's camera heading while sampling (hitch lines say where the camera pointed)

	/** One frame's numbers into the sample (called once per engine tick). */
	void AddFrame(FBenchSample& S)
	{
		const float Ms = (float)(FApp::GetDeltaTime() * 1000.0);
		if (Ms <= 0.0f)
		{
			return;
		}
		if (Ms > 50.0f)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] HITCH %.0f ms while sampling (yaw %.0f)"), Ms, GBenchYawForLog);
		}
		S.FrameMs.Add(Ms);
		const uint32 Gpu = RHIGetGPUFrameCycles(0);
		if (Gpu > 0)
		{
			S.SumGpuMs += FPlatformTime::ToMilliseconds(Gpu);
			++S.GpuFrames;
		}
		S.SumGameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		S.SumRenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		S.SumDraws += GNumDrawCallsRHI[0];
		S.MemMb = (double)FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0);
	}

	UWorld* FindGameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* W = Ctx.World();
			if (W && (Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE) && W->GetFirstPlayerController() && W->GetFirstPlayerController()->GetPawn())
			{
				const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(W);
				if (Seeds && Seeds->HasActiveSeed())
				{
					return W;
				}
			}
		}
		return nullptr;
	}

	// ----- MO.Perf.Bench: one spot -----
	struct FBenchRun
	{
		FBenchSample Sample;
		double EndTime = 0.0;
		FString Label;
		FTSTicker::FDelegateHandle Handle;
	};
	TSharedPtr<FBenchRun> GBench;

	// ----- MO.Perf.Tour: every biome -----
	struct FTourRun
	{
		TArray<FString> Biomes;
		int32 Index = -1;
		double SettleS = 75.0, BenchS = 24.0;
		bool bQuit = true;
		bool bProfileGpu = false;   // 4th number: 1 = run the engine's ProfileGPU once per biome (the GPU pass tree lands in the log)
		bool bProfiled = false;
		bool bFixedYaw = false;   // 5th number: 1 = do not turn the camera while sampling (separates 'first time a view is drawn' hitches from steady-state cost)
		int32 SettleHitches = 0;      // frames over 50 ms while the terrain / PCG settles after the teleport (world streaming cost), per biome
		float SettleWorstMs = 0.0f;
		int32 SettleFrames = 0;
		enum class EPhase { WaitWorld, Settle, Sample } Phase = EPhase::WaitWorld;
		double PhaseStart = 0.0;
		FBenchSample Sample;
		TArray<FBenchResult> Results;
		FTSTicker::FDelegateHandle Handle;
	};
	TSharedPtr<FTourRun> GTour;

	void FinishTour(FTourRun& T)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] ===== TOUR SUMMARY (%d biomes) ====="), T.Results.Num());
		float WorstP95 = 0.0f, WorstP99 = 0.0f;
		for (const FBenchResult& R : T.Results)
		{
			LogResult(R);
			WorstP95 = FMath::Max(WorstP95, R.P95);
			WorstP99 = FMath::Max(WorstP99, R.P99);
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] TOUR DONE worst p95=%.1fms (%.0f fps) worst p99=%.1fms (%.0f fps)"), WorstP95, WorstP95 > 0 ? 1000.0f / WorstP95 : 0.0f, WorstP99, WorstP99 > 0 ? 1000.0f / WorstP99 : 0.0f);
		if (T.bQuit)
		{
			FPlatformMisc::RequestExit(false);
		}
	}

	bool TickTour(float)
	{
		if (!GTour.IsValid())
		{
			return false;
		}
		FTourRun& T = *GTour;
		const double Now = FPlatformTime::Seconds();
		UWorld* World = FindGameWorld();
		switch (T.Phase)
		{
		case FTourRun::EPhase::WaitWorld:
		{
			if (!World)
			{
				return true;
			}
			++T.Index;
			if (!T.Biomes.IsValidIndex(T.Index))
			{
				FinishTour(T);
				GTour.Reset();
				return false;
			}
			GEngine->Exec(World, *FString::Printf(TEXT("MO.WorldGen.FindBiomeSpot %s 40 go"), *T.Biomes[T.Index]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] -> %s (settling %.0f s)"), *T.Biomes[T.Index], T.SettleS);
			T.Phase = FTourRun::EPhase::Settle;
			T.PhaseStart = Now;
			return true;
		}
		case FTourRun::EPhase::Settle:
		{
			const float FrameMs = (float)(FApp::GetDeltaTime() * 1000.0);
			++T.SettleFrames;
			T.SettleWorstMs = FMath::Max(T.SettleWorstMs, FrameMs);
			if (FrameMs > 50.0f)
			{
				++T.SettleHitches;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] HITCH %.0f ms while settling at %s (t=+%.0f s)"), FrameMs, *T.Biomes[T.Index], Now - T.PhaseStart);
			}
		}
			if (Now - T.PhaseStart >= T.SettleS)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] %s settle: %d frames, %d over 50 ms, worst %.0f ms"), *T.Biomes[T.Index], T.SettleFrames, T.SettleHitches, T.SettleWorstMs);
				T.SettleHitches = 0; T.SettleWorstMs = 0.0f; T.SettleFrames = 0;
				T.Sample = FBenchSample();
				T.Phase = FTourRun::EPhase::Sample;
				T.PhaseStart = Now;
			}
			return true;
		case FTourRun::EPhase::Sample:
		{
			// Turn the camera through three headings so the bench sees forest, open ground and the horizon.
			if (World && World->GetFirstPlayerController())
			{
				const double Frac = FMath::Clamp((Now - T.PhaseStart) / FMath::Max(T.BenchS, 1.0), 0.0, 0.999);
				FRotator Rot = World->GetFirstPlayerController()->GetControlRotation();
				Rot.Yaw = T.bFixedYaw ? 0.0f : (float)(Frac * 360.0);
				GBenchYawForLog = Rot.Yaw;
				Rot.Pitch = -6.0f;
				World->GetFirstPlayerController()->SetControlRotation(Rot);
			}
			AddFrame(T.Sample);
			if (T.bProfileGpu && !T.bProfiled && World && Now - T.PhaseStart >= T.BenchS * 0.5)
			{
				T.bProfiled = true;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] ProfileGPU for %s follows"), *T.Biomes[T.Index]);
				GEngine->Exec(World, TEXT("ProfileGPU"));
			}
			if (Now - T.PhaseStart >= T.BenchS)
			{
				T.Results.Add(Summarize(T.Biomes[T.Index], T.Sample));
				LogResult(T.Results.Last());
				T.Phase = FTourRun::EPhase::WaitWorld;
				T.bProfiled = false;
			}
			return true;
		}
		}
		return true;
	}

	bool TickBench(float)
	{
		if (!GBench.IsValid())
		{
			return false;
		}
		AddFrame(GBench->Sample);
		if (FPlatformTime::Seconds() >= GBench->EndTime)
		{
			LogResult(Summarize(GBench->Label, GBench->Sample));
			GBench.Reset();
			return false;
		}
		return true;
	}

	FAutoConsoleCommand GBenchCmd(
		TEXT("MO.Perf.Bench"),
		TEXT("Dev: sample frame times at the current spot. Usage: MO.Perf.Bench [seconds=15] [label]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (GBench.IsValid())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] a bench is already running"));
				return;
			}
			GBench = MakeShared<FBenchRun>();
			const double Secs = Args.Num() > 0 && Args[0].IsNumeric() ? FMath::Clamp((double)FCString::Atof(*Args[0]), 2.0, 600.0) : 15.0;
			GBench->Label = Args.Num() > 1 ? Args[1] : TEXT("spot");
			GBench->EndTime = FPlatformTime::Seconds() + Secs;
			GBench->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickBench), 0.0f);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] benching '%s' for %.0f s"), *GBench->Label, Secs);
		}));

	FAutoConsoleCommand GDelayCmd(
		TEXT("MO.Delay"),
		TEXT("Dev: run a console command after N seconds (works from -ExecCmds, which runs once at startup). Usage: MO.Delay <seconds> <command ...>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2 || !Args[0].IsNumeric())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] MO.Delay usage: MO.Delay <seconds> <command ...>"));
				return;
			}
			const double Seconds = FMath::Clamp((double)FCString::Atof(*Args[0]), 0.0, 36000.0);
			FString Command;
			for (int32 I = 1; I < Args.Num(); ++I)
			{
				Command += (I > 1 ? TEXT(" ") : TEXT("")) + Args[I];
			}
			const double At = FPlatformTime::Seconds() + Seconds;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([At, Command](float)
			{
				if (FPlatformTime::Seconds() < At)
				{
					return true;
				}
				UWorld* World = FindGameWorld();
				if (!World && GEngine)
				{
					World = GEngine->GetWorldContexts().Num() > 0 ? GEngine->GetWorldContexts()[0].World() : nullptr;
				}
				if (GEngine)
				{
					GEngine->Exec(World, *Command);
				}
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] MO.Delay ran: %s"), *Command);
				return false;
			}), 0.25f);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] will run '%s' in %.0f s"), *Command, Seconds);
		}));

	FAutoConsoleCommand GTourCmd(
		TEXT("MO.Perf.Tour"),
		TEXT("Dev: bench every biome (teleport, settle, sample), log a summary, quit. Usage: MO.Perf.Tour [settle_s=75] [bench_s=24] [quit=1] [profile_gpu=0] [fixed_yaw=0] [BiomeId ...]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (GTour.IsValid())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] a tour is already running"));
				return;
			}
			GTour = MakeShared<FTourRun>();
			int32 NumericIdx = 0;
			for (const FString& A : Args)
			{
				if (A.IsNumeric())
				{
					const double V = (double)FCString::Atof(*A);
					if (NumericIdx == 0) { GTour->SettleS = FMath::Clamp(V, 0.0, 600.0); }
					else if (NumericIdx == 1) { GTour->BenchS = FMath::Clamp(V, 2.0, 600.0); }
					else if (NumericIdx == 2) { GTour->bQuit = V != 0.0; }
					else if (NumericIdx == 3) { GTour->bProfileGpu = V != 0.0; }
					else if (NumericIdx == 4) { GTour->bFixedYaw = V != 0.0; }
					++NumericIdx;
				}
				else
				{
					GTour->Biomes.Add(A);
				}
			}
			if (GTour->Biomes.Num() == 0)
			{
				GTour->Biomes = { TEXT("TemperateForest"), TEXT("Meadow"), TEXT("RockyHighland"), TEXT("ShoreStrand"), TEXT("DryScrub"), TEXT("LocustWoodland"), TEXT("AlpineHeath"), TEXT("Rainforest"), TEXT("AlpineRock") };
			}
			GTour->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickTour), 0.0f);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOPERF] tour queued: %d biomes, settle %.0f s, bench %.0f s, quit=%d (waits for a game world)"), GTour->Biomes.Num(), GTour->SettleS, GTour->BenchS, GTour->bQuit ? 1 : 0);
		}));
}

#endif // !UE_BUILD_SHIPPING
