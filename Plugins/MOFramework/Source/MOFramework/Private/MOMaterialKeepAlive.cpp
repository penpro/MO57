// Material keep-alive for UNCOOKED game processes (N-185): the harness instances (UnrealEditor.exe -game) only.
//
// An uncooked game compiles material shader maps on demand (r.ShaderCompiler.JobCacheDDC, on by default): a material loads with no shader
// map, and as its shaders arrive the engine swaps in a new shader map and recreates the material's uniform buffer
// (SetShaderMapsOnMaterialResources_RenderThread -> CacheUniformExpressions(true)), then re-creates the primitives it finds using the
// material. A Nanite shading pipeline of a primitive it misses keeps the OLD shaders and the OLD uniform buffer by raw pointer, freed with
// the old shader map: the joiner or the host died in Nanite::BasePass right after rock spires spawned for the first time in the process
// ("Invalid uniform buffer <whatever reused the memory> bound on ComputeShader at index 3", an access violation executing a heap address or
// reading 0x4; Oct 20-21). The proof that such pipelines exist: the Voxel plugin's material selection found two Nanite shading pipelines with
// different compute shaders for one material (Oct 21). A cooked build loads its shader maps with the material and never swaps them.
//
// The engine is a launcher install (no engine fix compiles: Docs/Engine_Patches.md #2), so this keeps every replaced material uniform buffer
// and every replaced material shader map alive: a stale pipeline then runs the old shaders on the old buffer instead of freed memory. It
// logs a summary of the replacements every few seconds ("[MOMaterialKeepAlive] ..."), the evidence to set against a crash, and lets go of
// everything at engine exit, before the RHI shuts down.
//
//   mo.Debug.MaterialKeepAlive 1|0   on by default in an uncooked game process; never active in the editor (PIE) or a cooked build

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "MaterialShared.h"
#include "Materials/MaterialRenderProxy.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "MOFramework.h"
#include "RenderingThread.h"
#include "RHI.h"

namespace MOMaterialKeepAlive
{
	static TAutoConsoleVariable<int32> CVarEnabled(
		TEXT("mo.Debug.MaterialKeepAlive"), 1,
		TEXT("Uncooked game processes (the harness instances) only: keep every replaced material uniform buffer and shader map alive and log ")
		TEXT("the replacements (N-185: Nanite shading pipelines that still point at replaced ones). 0 = off (A/B; set at launch)."),
		ECVF_Default);

	struct FHeldBuffers
	{
		FUniformBufferRHIRef UniformBuffer;
		FUniformBufferRHIRef UniformBufferVS;
		uint32 Pass = 0;
	};

	struct FHeldShaderMap
	{
		TRefCountPtr<FMaterialShaderMap> ShaderMap;
		uint32 Pass = 0;
	};

	// Render thread only.
	static TMap<const FMaterialRenderProxy*, FHeldBuffers> GBuffers;
	static TMap<const FMaterial*, FHeldShaderMap> GShaderMaps;   // keys are never dereferenced: a FMaterial may be gone, its key pruned
	static TArray<FUniformBufferRHIRef> GRetiredBuffers;
	static TArray<TRefCountPtr<FMaterialShaderMap>> GRetiredShaderMaps;
	static TMap<FString, int32> GBuffersSinceReport;   // material -> uniform buffers replaced since the last summary line
	static TMap<FString, int32> GShaderMapsSinceReport;   // material -> shader maps replaced since the last summary line
	static uint32 GPass = 0;
	static bool GExiting = false;   // game thread

	static void RetireBuffer(const FUniformBufferRHIRef& Old)
	{
		if (Old.IsValid())
		{
			GRetiredBuffers.Add(Old);
		}
	}

	static void RetireShaderMap(const TRefCountPtr<FMaterialShaderMap>& Old)
	{
		if (Old.IsValid())
		{
			GRetiredShaderMaps.Add(Old);
		}
	}

	static FString TopOf(TMap<FString, int32>& Counts, int32& OutTotal)
	{
		OutTotal = 0;
		for (const TPair<FString, int32>& Pair : Counts)
		{
			OutTotal += Pair.Value;
		}
		Counts.ValueSort([](int32 A, int32 B) { return A > B; });
		FString Top;
		int32 Listed = 0;
		for (const TPair<FString, int32>& Pair : Counts)
		{
			if (++Listed > 4)
			{
				break;
			}
			Top += FString::Printf(TEXT("%s%s x%d"), Top.IsEmpty() ? TEXT("") : TEXT(", "), *FPaths::GetBaseFilename(Pair.Key), Pair.Value);
		}
		return Top.IsEmpty() ? TEXT("none") : Top;
	}

	static void Scan_RenderThread(ERHIFeatureLevel::Type FeatureLevel)
	{
		const double Start = FPlatformTime::Seconds();
		++GPass;
		{
			FScopeLock Lock(&FMaterialRenderProxy::GetMaterialRenderProxyMapLock());
			for (const FMaterialRenderProxy* Proxy : FMaterialRenderProxy::GetMaterialRenderProxyMap())
			{
				const FUniformExpressionCache& Cache = Proxy->UniformExpressionCache[FeatureLevel];
				FHeldBuffers& Held = GBuffers.FindOrAdd(Proxy);
				Held.Pass = GPass;
				if (Held.UniformBuffer != Cache.UniformBuffer)
				{
					if (Held.UniformBuffer.IsValid())
					{
						++GBuffersSinceReport.FindOrAdd(Proxy->GetMaterialName());
						RetireBuffer(Held.UniformBuffer);
					}
					Held.UniformBuffer = Cache.UniformBuffer;
				}
				if (Held.UniformBufferVS != Cache.UniformBufferVS)
				{
					RetireBuffer(Held.UniformBufferVS);
					Held.UniformBufferVS = Cache.UniformBufferVS;
				}

				if (const FMaterial* Material = Proxy->GetMaterialNoFallback(FeatureLevel))
				{
					FMaterialShaderMap* ShaderMap = Material->GetRenderingThreadShaderMap();
					FHeldShaderMap& HeldMap = GShaderMaps.FindOrAdd(Material);
					if (HeldMap.Pass != GPass)   // several proxies share one FMaterial: once per pass
					{
						HeldMap.Pass = GPass;
						if (HeldMap.ShaderMap.GetReference() != ShaderMap)
						{
							if (HeldMap.ShaderMap.IsValid())
							{
								++GShaderMapsSinceReport.FindOrAdd(Proxy->GetMaterialName());
								RetireShaderMap(HeldMap.ShaderMap);
							}
							HeldMap.ShaderMap = ShaderMap;
						}
					}
				}
			}
		}
		// A proxy or a FMaterial gone (its material released): keep what it held too, a pipeline may still point at it
		for (auto It = GBuffers.CreateIterator(); It; ++It)
		{
			if (It->Value.Pass != GPass)
			{
				RetireBuffer(It->Value.UniformBuffer);
				RetireBuffer(It->Value.UniformBufferVS);
				It.RemoveCurrent();
			}
		}
		for (auto It = GShaderMaps.CreateIterator(); It; ++It)
		{
			if (It->Value.Pass != GPass)
			{
				RetireShaderMap(It->Value.ShaderMap);
				It.RemoveCurrent();
			}
		}

		// One summary line at most every 5 s (a line per replacement was ~20 000 lines in 25 min and slowed the run with -forcelogflush)
		const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
		static double LastReport = 0.0;
		static double WorstMs = 0.0;
		WorstMs = FMath::Max(WorstMs, Ms);
		if (((GBuffersSinceReport.Num() > 0 || GShaderMapsSinceReport.Num() > 0) && Start - LastReport > 5.0) || Start - LastReport > 120.0)
		{
			LastReport = Start;
			int32 NumBuffers = 0;
			int32 NumShaderMaps = 0;
			const FString TopBuffers = TopOf(GBuffersSinceReport, NumBuffers);
			const FString TopShaderMaps = TopOf(GShaderMapsSinceReport, NumShaderMaps);
			UE_LOG(LogMOFramework, Log, TEXT("[MOMaterialKeepAlive] since the last line: %d shader map(s) replaced (%s), %d uniform buffer(s) replaced (%s); ")
				TEXT("kept alive in all: %d shader maps, %d buffers; watching %d proxies, %d materials; scan worst %.2f ms"),
				NumShaderMaps, *TopShaderMaps, NumBuffers, *TopBuffers, GRetiredShaderMaps.Num(), GRetiredBuffers.Num(), GBuffers.Num(), GShaderMaps.Num(), WorstMs);
			GBuffersSinceReport.Reset();
			GShaderMapsSinceReport.Reset();
			WorstMs = 0.0;
		}
	}

	static FDelayedAutoRegisterHelper GRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		if (GIsEditor || !FApp::CanEverRender() || IsRunningCommandlet())
		{
			return;   // the editor (PIE included) and commandlets: not this
		}
		if (CVarEnabled.GetValueOnGameThread() == 0)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOMaterialKeepAlive] off (mo.Debug.MaterialKeepAlive 0)"));
			return;
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOMaterialKeepAlive] uncooked game process: keeping replaced material shader maps and uniform buffers alive (mo.Debug.MaterialKeepAlive)"));
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			if (!GExiting)
			{
				const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
				ENQUEUE_RENDER_COMMAND(MOMaterialKeepAlive)([FeatureLevel](FRHICommandListImmediate&)
				{
					Scan_RenderThread(FeatureLevel);
				});
			}
			return true;
		}));
		// Let go before the RHI shuts down: static containers released after it would release RHI resources into a dead RHI
		FCoreDelegates::OnEnginePreExit.AddLambda([]
		{
			GExiting = true;
			ENQUEUE_RENDER_COMMAND(MOMaterialKeepAliveRelease)([](FRHICommandListImmediate&)
			{
				GBuffers.Empty();
				GShaderMaps.Empty();
				GRetiredBuffers.Empty();
				GRetiredShaderMaps.Empty();
			});
			FlushRenderingCommands();
		});
	});
}

#endif // WITH_EDITOR
