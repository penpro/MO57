#include "MONetDiagnostics.h"
#include "MOFramework.h"
#include "MOBugReport.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "MOControllableInterface.h"
#include "MOWorldReadinessSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/NetworkObjectList.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

// ============================================================================
// FMONetCounters
// ============================================================================

namespace
{
	struct FRemoteCadence
	{
		double LastUpdate = 0.0;
		int32 Updates = 0;          // this second
		float MaxGapMs = 0.0f;      // this second
		bool bMoving = false;       // the last update said the pawn was moving: a silence after it is a stall, not a pawn standing still
	};

	FMONetCounters::FSnapshot GCounts;
	TMap<TWeakObjectPtr<const AActor>, FRemoteCadence> GRemote;
	bool GDetailLogging = false;
	int32 GDetailLinesLeft = 0;
	TArray<FMONetEvent> GEvents;

	void NoteRemote(const AActor* Pawn, bool bMoving)
	{
		if (!Pawn)
		{
			return;
		}
		const double Now = FPlatformTime::Seconds();
		FRemoteCadence& Cadence = GRemote.FindOrAdd(Pawn);
		if (Cadence.LastUpdate > 0.0 && Cadence.bMoving)
		{
			Cadence.MaxGapMs = FMath::Max(Cadence.MaxGapMs, static_cast<float>((Now - Cadence.LastUpdate) * 1000.0));
		}
		Cadence.LastUpdate = Now;
		Cadence.bMoving = bMoving;
		Cadence.Updates += bMoving ? 1 : 0;
	}
}

double FMONetCounters::ServerTimeOf(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : -1.0;
}

double FMONetCounters::UtcSeconds()
{
	return static_cast<double>((FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTotalMilliseconds()) / 1000.0;
}

void FMONetCounters::NoteEvent(const UObject* WorldContext, const FString& Text)
{
	FMONetEvent& Event = GEvents.AddDefaulted_GetRef();
	Event.Time = FPlatformTime::Seconds();
	Event.ServerTime = ServerTimeOf(WorldContext);
	Event.Utc = UtcSeconds();
	Event.Text = Text;
	if (GEvents.Num() > MaxRecentEvents)
	{
		GEvents.RemoveAt(0, GEvents.Num() - MaxRecentEvents);
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MONetEvent] st=%.2f utc=%.3f %s"), Event.ServerTime, Event.Utc, *Text);
}

TArray<FMONetEvent> FMONetCounters::GetRecentEvents()
{
	return GEvents;
}

void FMONetCounters::NoteServerMoveChecked(const AActor* Pawn, bool bNeedsCorrection, const FVector& ServerMinusClient)
{
	++GCounts.ServerMovesChecked;
	// on the host a joiner's pawn moves when its ServerMoves arrive: that is its update cadence (a joiner keeps sending moves while standing still)
	NoteRemote(Pawn, Pawn && Pawn->GetVelocity().SizeSquared() > 100.0);
	if (bNeedsCorrection)
	{
		++GCounts.ServerCorrections;
		const float Err = ServerMinusClient.Size();
		GCounts.ServerErrSum += Err;
		GCounts.ServerErrXYSum += ServerMinusClient.Size2D();
		GCounts.ServerErrZSum += FMath::Abs(ServerMinusClient.Z);
		GCounts.ServerErrMax = FMath::Max(GCounts.ServerErrMax, Err);
		if (Err >= EventCorrectionCm)
		{
			NoteEvent(Pawn, FString::Printf(TEXT("host corrected joiner pawn %s by %.0f cm (dx %.0f dy %.0f dz %.0f)"),
				*GetNameSafe(Pawn), Err, ServerMinusClient.X, ServerMinusClient.Y, ServerMinusClient.Z));
		}
	}
}

void FMONetCounters::NoteClientMoveResponse(bool bCorrection)
{
	++(bCorrection ? GCounts.ClientCorrections : GCounts.ClientAcks);
}

void FMONetCounters::NoteClientCorrectionSnap(const AActor* Pawn, float SnapCm)
{
	++GCounts.ClientSnaps;
	GCounts.ClientSnapSum += SnapCm;
	GCounts.ClientSnapMax = FMath::Max(GCounts.ClientSnapMax, SnapCm);
	if (SnapCm >= EventCorrectionCm)
	{
		NoteEvent(Pawn, FString::Printf(TEXT("joiner rubber-banded %.0f cm (a server correction)"), SnapCm));
	}
}

void FMONetCounters::NoteRemotePawnUpdate(const AActor* Pawn, bool bMoving)
{
	NoteRemote(Pawn, bMoving);
}

void FMONetCounters::NoteChannelBits(FName ActorClass, int64 Bits, bool bOutgoing)
{
	(bOutgoing ? GCounts.OutBits : GCounts.InBits).FindOrAdd(ActorClass) += Bits;
}

void FMONetCounters::SetDetailLogging(bool bOn)
{
	GDetailLogging = bOn;
}

bool FMONetCounters::TakeDetailLine()
{
	if (!GDetailLogging || GDetailLinesLeft <= 0)
	{
		return false;
	}
	--GDetailLinesLeft;
	return true;
}

FMONetCounters::FSnapshot FMONetCounters::TakeAndReset()
{
	FSnapshot Out = MoveTemp(GCounts);
	GCounts = FSnapshot();
	GDetailLinesLeft = DetailLinesPerSecond;
	const double Now = FPlatformTime::Seconds();
	for (auto It = GRemote.CreateIterator(); It; ++It)
	{
		FRemoteCadence& Cadence = It.Value();
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		// Counted: a pawn that got positions this second, or a MOVING pawn that went silent (its open gap is a stall). A pawn standing still sends
		// nothing and is not slow.
		if (Cadence.Updates > 0 || Cadence.bMoving)
		{
			const float OpenGapMs = Cadence.bMoving ? static_cast<float>((Now - Cadence.LastUpdate) * 1000.0) : 0.0f;
			Out.RemoteMaxGapMs = FMath::Max(Out.RemoteMaxGapMs, FMath::Max(Cadence.MaxGapMs, OpenGapMs));
			Out.RemoteUpdates += Cadence.Updates;
			++Out.RemotePawns;
		}
		Cadence.Updates = 0;
		Cadence.MaxGapMs = 0.0f;
	}
	return Out;
}

// ============================================================================
// FMONetSample / FMONetWindowSummary (pure)
// ============================================================================

namespace
{
	TArray<TPair<FName, int64>> TopBytes(const TMap<FName, int64>& Bits, int32 N)
	{
		TArray<TPair<FName, int64>> Out;
		for (const TPair<FName, int64>& Pair : Bits)
		{
			Out.Emplace(Pair.Key, (Pair.Value + 7) / 8);
		}
		Out.Sort([](const TPair<FName, int64>& A, const TPair<FName, int64>& B) { return A.Value > B.Value; });
		if (Out.Num() > N)
		{
			Out.SetNum(N);
		}
		return Out;
	}

	FString JoinTop(const TArray<TPair<FName, int64>>& Top)
	{
		if (Top.IsEmpty())
		{
			return TEXT("-");
		}
		TArray<FString> Parts;
		for (const TPair<FName, int64>& Pair : Top)
		{
			Parts.Add(FString::Printf(TEXT("%s:%lld"), *Pair.Key.ToString(), Pair.Value));
		}
		return FString::Join(Parts, TEXT(","));
	}
}

const FMONetConnSample* FMONetSample::Busiest() const
{
	const FMONetConnSample* Best = nullptr;
	for (const FMONetConnSample& C : Connections)
	{
		if (!Best || C.SaturatedReplications + C.SaturatedFrames > Best->SaturatedReplications + Best->SaturatedFrames
			|| (C.SaturatedReplications + C.SaturatedFrames == Best->SaturatedReplications + Best->SaturatedFrames && C.OutBytesPerSec > Best->OutBytesPerSec))
		{
			Best = &C;
		}
	}
	return Best;
}

FString FMONetSample::ToLogLine() const
{
	const FMONetConnSample Empty;
	const FMONetConnSample& C = Busiest() ? *Busiest() : Empty;
	const float Fps = FrameAvgMs > 0.0f ? 1000.0f / FrameAvgMs : 0.0f;
	return FString::Printf(
		TEXT("[MONet] role=%s st=%.2f utc=%.3f play=%d fps=%.1f frame=%.1f/%.1fms conns=%d out=%d in=%d cap=%d use=%.0f%% pkts=%d/%d loss=%.1f/%.1f%% rtt=%.0fms ping=%.0fms jit=%.1fms ")
		TEXT("reps=%d satRepl=%.0f%% satFrames=%.0f%% chans=%d objs=%d dorm=%d | moves=%d corr=%d err=%.1f/%.1fcm xy=%.1f z=%.1f | acks=%d recv=%d snaps=%d snap=%.1f/%.1fcm ")
		TEXT("| remote=%d upd=%.1f gap=%.0fms | topOut=%s | topIn=%s"),
		bHost ? TEXT("host") : TEXT("client"), ServerTime, Utc, bPlaying ? 1 : 0, Fps, FrameAvgMs, FrameMaxMs, Connections.Num(), C.OutBytesPerSec, C.InBytesPerSec, C.RateCapBytesPerSec, C.UsePct(),
		C.OutPacketsPerSec, C.InPacketsPerSec, C.OutLossPct, C.InLossPct, C.RttMs, C.PingMs, C.JitterMs,
		C.Replications, C.SatReplPct(), C.SatFramePct(), C.OpenChannels, ActiveObjects, DormantObjects,
		ServerMovesChecked, ServerCorrections, ServerErrAvgCm, ServerErrMaxCm, ServerErrAvgXYCm, ServerErrAvgZCm,
		ClientAcks, ClientCorrections, ClientSnaps, ClientSnapAvgCm, ClientSnapMaxCm,
		RemotePawns, RemoteUpdatesPerPawn, RemoteMaxGapMs, *JoinTop(TopOutBytes), *JoinTop(TopInBytes));
}

FMONetWindowSummary FMONetWindowSummary::Summarize(const TArray<FMONetSample>& Samples, int32 TopN)
{
	FMONetWindowSummary S;
	S.Seconds = Samples.Num();
	if (Samples.IsEmpty())
	{
		return S;
	}
	S.FpsMin = TNumericLimits<float>::Max();
	S.RemoteUpdatesMin = TNumericLimits<float>::Max();
	double FpsSum = 0.0, OutSum = 0.0, RttSum = 0.0;
	int32 RemoteSeconds = 0;
	TMap<FName, int64> OutTotals, InTotals;
	for (const FMONetSample& X : Samples)
	{
		const float Fps = X.FrameAvgMs > 0.0f ? 1000.0f / X.FrameAvgMs : 0.0f;
		S.FpsMin = FMath::Min(S.FpsMin, Fps);
		FpsSum += Fps;
		S.FrameMaxMs = FMath::Max(S.FrameMaxMs, X.FrameMaxMs);
		if (const FMONetConnSample* C = X.Busiest())
		{
			OutSum += C->OutBytesPerSec;
			S.OutMax = FMath::Max(S.OutMax, static_cast<float>(C->OutBytesPerSec));
			S.UseMaxPct = FMath::Max(S.UseMaxPct, C->UsePct());
			S.SaturatedSeconds += (C->SaturatedReplications > 0 || C->SaturatedFrames > 0) ? 1 : 0;
			S.LossMaxPct = FMath::Max(S.LossMaxPct, FMath::Max(C->OutLossPct, C->InLossPct));
			RttSum += C->RttMs;
			S.RttMaxMs = FMath::Max(S.RttMaxMs, C->RttMs);
		}
		S.ServerCorrections += X.ServerCorrections;
		S.ServerMovesChecked += X.ServerMovesChecked;
		S.ServerErrMaxCm = FMath::Max(S.ServerErrMaxCm, X.ServerErrMaxCm);
		S.ClientCorrections += X.ClientCorrections;
		S.ClientSnapMaxCm = FMath::Max(S.ClientSnapMaxCm, X.ClientSnapMaxCm);
		if (X.RemotePawns > 0)
		{
			++RemoteSeconds;
			S.RemoteUpdatesMin = FMath::Min(S.RemoteUpdatesMin, X.RemoteUpdatesPerPawn);
			S.RemoteMaxGapMs = FMath::Max(S.RemoteMaxGapMs, X.RemoteMaxGapMs);
		}
		for (const TPair<FName, int64>& P : X.TopOutBytes) { OutTotals.FindOrAdd(P.Key) += P.Value * 8; }
		for (const TPair<FName, int64>& P : X.TopInBytes) { InTotals.FindOrAdd(P.Key) += P.Value * 8; }
	}
	S.FpsAvg = static_cast<float>(FpsSum / Samples.Num());
	S.OutAvg = static_cast<float>(OutSum / Samples.Num());
	S.RttAvgMs = static_cast<float>(RttSum / Samples.Num());
	if (RemoteSeconds == 0)
	{
		S.RemoteUpdatesMin = 0.0f;
	}
	S.TopOutBytes = TopBytes(OutTotals, TopN);
	S.TopInBytes = TopBytes(InTotals, TopN);
	return S;
}

// ============================================================================
// UMONetDiagnosticsSubsystem
// ============================================================================

void UMONetDiagnosticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMOBugReportSubsystem>();
	if (UMOBugReportSubsystem* Reports = GetGameInstance()->GetSubsystem<UMOBugReportSubsystem>())
	{
		TWeakObjectPtr<UMONetDiagnosticsSubsystem> WeakThis(this);
		Reports->RegisterContributor(TEXT("Net"), [WeakThis](const UWorld*, FMOBugReportFields& Out)
		{
			if (const UMONetDiagnosticsSubsystem* Self = WeakThis.Get())
			{
				Self->DescribeForBugReport(Out);
			}
		});
	}
	LastFrameTime = SecondStart = FPlatformTime::Seconds();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMONetDiagnosticsSubsystem::Tick));
}

void UMONetDiagnosticsSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

UMONetDiagnosticsSubsystem* UMONetDiagnosticsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UMONetDiagnosticsSubsystem>() : nullptr;
}

UWorld* UMONetDiagnosticsSubsystem::FindNetWorld() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !World->GetNetDriver())
	{
		return nullptr;
	}
	const ENetMode Mode = World->GetNetMode();
	return (Mode == NM_ListenServer || Mode == NM_Client || Mode == NM_DedicatedServer) ? World : nullptr;
}

bool UMONetDiagnosticsSubsystem::Tick(float /*DeltaTime*/)
{
	const double Now = FPlatformTime::Seconds();
	const double Frame = Now - LastFrameTime;
	LastFrameTime = Now;
	FrameSum += Frame;
	FrameMax = FMath::Max(FrameMax, Frame);
	++FrameCount;

	UWorld* World = FindNetWorld();
	if (World)
	{
		if (Frame * 1000.0 >= FMONetCounters::EventFrameMs && Frame < 10.0)   // (longer: a debugger or a suspended window, not play)
		{
			// A joiner that stops sending moves for 250 ms is extrapolated by the host (AGameNetworkManager MAXCLIENTUPDATEINTERVAL): a frame this long
			// while a player moves is the measured cause of a rubber band (Docs/Coop_And_Networking.md section 7). Only then is it an event: a world
			// building behind the loading screen has second-long frames that rubber-band nobody.
			float FastestPlayer = 0.0f;
			if (const AGameStateBase* GameState = World->GetGameState())
			{
				for (const APlayerState* PS : GameState->PlayerArray)
				{
					const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
					FastestPlayer = Pawn ? FMath::Max(FastestPlayer, static_cast<float>(Pawn->GetVelocity().Size2D())) : FastestPlayer;
				}
			}
			const UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(World);
			if (FastestPlayer > 10.0f && !(Readiness && Readiness->IsTracking()))
			{
				FMONetCounters::NoteEvent(World, FString::Printf(TEXT("%s frame took %.0f ms (a player moving at %.0f cm/s)"),
					World->GetNetMode() == NM_Client ? TEXT("joiner") : TEXT("host"), Frame * 1000.0, FastestPlayer));
			}
		}
		WatchRemotePawns(World);
		if (bTracing && Now >= NextTraceTime)
		{
			TracePawns(World, Now);
			NextTraceTime = Now + 1.0 / TraceHz;
		}
	}
	if (Now - SecondStart < 1.0)
	{
		return true;
	}
	if (World)
	{
		UNetDriver* Driver = World->GetNetDriver();
		if (Driver != DescribedDriver.Get())
		{
			// once per driver, always: the facts every number below depends on (and the trap check: a bad channel path leaves co-op with no actors)
			DescribedDriver = Driver;
			const FChannelDefinition* ActorDef = Driver->ChannelDefinitionMap.Find(NAME_Actor);
			UE_LOG(LogMOFramework, Warning, TEXT("[MONet] net driver %s (%s): actor channel %s, MaxClientRate %d, MaxInternetClientRate %d, NetServerMaxTickRate %d"),
				*Driver->GetClass()->GetName(), Driver->IsServer() ? TEXT("host") : TEXT("client"),
				ActorDef && ActorDef->ChannelClass ? *ActorDef->ChannelClass->GetName() : TEXT("NONE (check [/Script/Engine.NetDriver] ChannelDefinitions)"),
				Driver->MaxClientRate, Driver->MaxInternetClientRate, Driver->GetNetServerMaxTickRate());
		}
		FMONetSample S = Sample(World, Now, FrameSum, FrameCount, FrameMax);
		const bool bLog = IsLogging();
		FMONetCounters::SetDetailLogging(bLog);
		if (bLog)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("%s"), *S.ToLogLine());
		}
		if (bShowing)
		{
			ShowOnScreen(S);
		}
		History.Add(MoveTemp(S));
		if (History.Num() > HistorySeconds)
		{
			History.RemoveAt(0, History.Num() - HistorySeconds);
		}
	}
	else
	{
		FMONetCounters::TakeAndReset();   // a menu or a standalone world: nothing to attribute the counts to
		SaturationSeen.Reset();
	}
	SecondStart = Now;
	FrameSum = FrameMax = 0.0;
	FrameCount = 0;
	return true;
}

void UMONetDiagnosticsSubsystem::SampleConnection(UNetConnection* C, bool bHost, FMONetConnSample& Out)
{
	Out.OutBytesPerSec = C->OutBytesPerSecond;
	Out.InBytesPerSec = C->InBytesPerSecond;
	Out.OutPacketsPerSec = C->OutPacketsPerSecond;
	Out.InPacketsPerSec = C->InPacketsPerSecond;
	Out.RateCapBytesPerSec = C->CurrentNetSpeed;
	Out.OutLossPct = 100.0f * C->GetOutLossPercentage().GetLossPercentage();
	Out.InLossPct = 100.0f * C->GetInLossPercentage().GetLossPercentage();
	Out.RttMs = C->AvgLag * 1000.0f;
	Out.JitterMs = C->GetAverageJitterInMS();
	Out.OpenChannels = C->OpenChannels.Num();
	const APlayerController* PC = C->PlayerController;
	if (!PC && !bHost)
	{
		PC = C->Driver && C->Driver->GetWorld() ? C->Driver->GetWorld()->GetFirstPlayerController() : nullptr;
	}
	Out.PingMs = PC && PC->PlayerState ? PC->PlayerState->GetPingInMilliseconds() : 0.0f;

	// cumulative engine analytics -> this second's difference (a reset by the engine shows as a smaller total: take it as is)
	const FNetConnectionSaturationAnalytics& Sat = C->GetSaturationAnalytics();
	FSaturationSeen& Seen = SaturationSeen.FindOrAdd(C);
	auto Delta = [](uint32 Current, uint32& Last) { const uint32 D = Current >= Last ? Current - Last : Current; Last = Current; return static_cast<int32>(D); };
	Out.Replications = Delta(Sat.GetNumberOfReplications(), Seen.Replications);
	Out.SaturatedReplications = Delta(Sat.GetNumberOfSaturatedReplications(), Seen.SaturatedReplications);
	Out.TrackedFrames = Delta(Sat.GetNumberOfTrackedFrames(), Seen.Frames);
	Out.SaturatedFrames = Delta(Sat.GetNumberOfSaturatedFrames(), Seen.SaturatedFrames);
}

FMONetSample UMONetDiagnosticsSubsystem::Sample(UWorld* World, double Now, double FrameSumSeconds, int32 Frames, double FrameMaxSeconds)
{
	FMONetSample S;
	S.Time = Now;
	S.ServerTime = FMONetCounters::ServerTimeOf(World);
	S.Utc = FMONetCounters::UtcSeconds();
	if (const UMOWorldReadinessSubsystem* Readiness = UMOWorldReadinessSubsystem::Get(World))
	{
		S.bPlaying = !Readiness->IsTracking();
	}
	S.Frames = Frames;
	S.FrameAvgMs = Frames > 0 ? static_cast<float>(1000.0 * FrameSumSeconds / Frames) : 0.0f;
	S.FrameMaxMs = static_cast<float>(1000.0 * FrameMaxSeconds);
	if (UNetDriver* Driver = World ? World->GetNetDriver() : nullptr)
	{
		S.bHost = Driver->IsServer();
		S.MaxClientRate = Driver->MaxClientRate;
		if (S.bHost)
		{
			S.ActiveObjects = Driver->GetNetworkObjectList().GetActiveObjects().Num();
			S.DormantObjects = Driver->GetNetworkObjectList().GetDormantObjectsOnAllConnections().Num();
			for (int32 i = 0; i < Driver->ClientConnections.Num(); ++i)
			{
				if (UNetConnection* C = Driver->ClientConnections[i])
				{
					FMONetConnSample& CS = S.Connections.AddDefaulted_GetRef();
					CS.Label = FString::Printf(TEXT("joiner%d"), i);
					SampleConnection(C, true, CS);
				}
			}
		}
		else if (Driver->ServerConnection)
		{
			FMONetConnSample& CS = S.Connections.AddDefaulted_GetRef();
			CS.Label = TEXT("host");
			SampleConnection(Driver->ServerConnection, false, CS);
			S.ActiveObjects = CS.OpenChannels;
		}
	}

	FMONetCounters::FSnapshot C = FMONetCounters::TakeAndReset();
	S.ServerMovesChecked = C.ServerMovesChecked;
	S.ServerCorrections = C.ServerCorrections;
	if (C.ServerCorrections > 0)
	{
		S.ServerErrAvgCm = static_cast<float>(C.ServerErrSum / C.ServerCorrections);
		S.ServerErrAvgXYCm = static_cast<float>(C.ServerErrXYSum / C.ServerCorrections);
		S.ServerErrAvgZCm = static_cast<float>(C.ServerErrZSum / C.ServerCorrections);
		S.ServerErrMaxCm = C.ServerErrMax;
	}
	S.ClientAcks = C.ClientAcks;
	S.ClientCorrections = C.ClientCorrections;
	S.ClientSnaps = C.ClientSnaps;
	if (C.ClientSnaps > 0)
	{
		S.ClientSnapAvgCm = static_cast<float>(C.ClientSnapSum / C.ClientSnaps);
		S.ClientSnapMaxCm = C.ClientSnapMax;
	}
	S.RemotePawns = C.RemotePawns;
	S.RemoteUpdatesPerPawn = C.RemotePawns > 0 ? static_cast<float>(C.RemoteUpdates) / C.RemotePawns : 0.0f;
	S.RemoteMaxGapMs = C.RemoteMaxGapMs;
	S.TopOutBytes = TopBytes(C.OutBits, 6);
	S.TopInBytes = TopBytes(C.InBits, 6);

	if (const FMONetConnSample* Busy = S.Busiest(); Busy && (Busy->SaturatedReplications > 0 || Busy->SaturatedFrames > 0))
	{
		FMONetCounters::NoteEvent(World, FString::Printf(TEXT("%s channel to %s saturated: %d of %d replication passes cut short, %d B/s of a %d cap"),
			S.bHost ? TEXT("host") : TEXT("joiner"), *Busy->Label, Busy->SaturatedReplications, Busy->Replications, Busy->OutBytesPerSec, Busy->RateCapBytesPerSec));
	}
	if (S.RemoteMaxGapMs >= FMONetCounters::EventRemoteGapMs)
	{
		FMONetCounters::NoteEvent(World, FString::Printf(TEXT("%s waited %.0f ms for a moving player's position"), S.bHost ? TEXT("host") : TEXT("joiner"), S.RemoteMaxGapMs));
	}
	return S;
}

void UMONetDiagnosticsSubsystem::TracePawns(UWorld* World, double Now) const
{
	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return;
	}
	const double ServerTime = GameState->GetServerWorldTimeSeconds();
	const double Utc = FMONetCounters::UtcSeconds();
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const FVector A = Pawn->GetActorLocation();
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const FVector M = Character && Character->GetMesh() ? Character->GetMesh()->GetComponentLocation() : A;
		const FVector V = Pawn->GetVelocity();
		TCHAR Gait = TEXT('?'), MoveGait = TEXT('?');
		if (const AMOCharacter* MO = Cast<AMOCharacter>(Pawn))
		{
			Gait = MO->IsSprinting() ? TEXT('S') : TEXT('-');   // the character's own sprint flag (what other systems read); jog is in the moves (mgait)
			if (const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(MO->GetCharacterMovement()))
			{
				MoveGait = Move->WantsToSprint() ? TEXT('S') : (Move->WantsToJog() ? TEXT('J') : TEXT('W'));
			}
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MONetPos] t=%.4f st=%.3f utc=%.3f id=%d local=%d role=%d ax=%.1f ay=%.1f az=%.1f mx=%.1f my=%.1f mz=%.1f v=%.0f gait=%c mgait=%c"),
			Now, ServerTime, Utc, PS->GetPlayerId(), Pawn->IsLocallyControlled() ? 1 : 0, static_cast<int32>(Pawn->GetLocalRole()), A.X, A.Y, A.Z, M.X, M.Y, M.Z, V.Size2D(), Gait, MoveGait);
	}
}

void UMONetDiagnosticsSubsystem::WatchRemotePawns(UWorld* World)
{
	// Joiner: an update of another player's pawn = its replicated position changed (read every frame; works whatever path the engine applies it by,
	// and whether or not the proxy is extrapolated between updates).
	if (World->GetNetMode() != NM_Client)
	{
		return;
	}
	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return;
	}
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		if (!Pawn || Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			continue;
		}
		const FRepMovement& Rep = Pawn->GetReplicatedMovement();
		FVector& Last = LastRepLocation.FindOrAdd(Pawn, FVector(TNumericLimits<float>::Max()));
		if (!Rep.Location.Equals(Last, 0.01))
		{
			Last = Rep.Location;
			FMONetCounters::NoteRemotePawnUpdate(Pawn, Rep.LinearVelocity.SizeSquared() > 100.0);   // moving faster than 10 cm/s
		}
	}
	for (auto It = LastRepLocation.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void UMONetDiagnosticsSubsystem::ShowOnScreen(const FMONetSample& S) const
{
	if (!GEngine)
	{
		return;
	}
	const FMONetConnSample Empty;
	const FMONetConnSample& C = S.Busiest() ? *S.Busiest() : Empty;
	const float Fps = S.FrameAvgMs > 0.0f ? 1000.0f / S.FrameAvgMs : 0.0f;
	const int32 KeyBase = 0x4D4F4E00;   // fixed keys, so each second replaces the last lines
	const TArray<FMONetEvent> Events = FMONetCounters::GetRecentEvents();
	const FString LastEvent = Events.IsEmpty() ? FString(TEXT("none")) :
		FString::Printf(TEXT("%.0fs ago: %s"), FPlatformTime::Seconds() - Events.Last().Time, *Events.Last().Text);
	const bool bTrouble = C.SaturatedReplications > 0 || S.ServerCorrections > 2 || S.ClientSnapMaxCm >= FMONetCounters::EventCorrectionCm;
	const FColor Color = bTrouble ? FColor::Orange : FColor::Cyan;
	GEngine->AddOnScreenDebugMessage(KeyBase + 3, 1.5f, Color, FString::Printf(TEXT("NET last event: %s"), *LastEvent));
	GEngine->AddOnScreenDebugMessage(KeyBase + 2, 1.5f, Color, S.bHost
		? FString::Printf(TEXT("NET joiner moves %d/s, corrected %d (max %.0f cm) | joiner pawn: worst gap %.0f ms"), S.ServerMovesChecked, S.ServerCorrections, S.ServerErrMaxCm, S.RemoteMaxGapMs)
		: FString::Printf(TEXT("NET corrections %d, snap max %.0f cm | host pawn: %.0f positions/s, worst gap %.0f ms"), S.ClientCorrections, S.ClientSnapMaxCm, S.RemoteUpdatesPerPawn, S.RemoteMaxGapMs));
	GEngine->AddOnScreenDebugMessage(KeyBase + 1, 1.5f, Color, FString::Printf(TEXT("NET out %.1f kB/s in %.1f kB/s of a %.0f kB/s cap (%.0f%%), saturated %.0f%% | loss %.1f/%.1f%%"),
		C.OutBytesPerSec / 1000.0f, C.InBytesPerSec / 1000.0f, C.RateCapBytesPerSec / 1000.0f, C.UsePct(), C.SatReplPct(), C.OutLossPct, C.InLossPct));
	GEngine->AddOnScreenDebugMessage(KeyBase + 0, 1.5f, Color, FString::Printf(TEXT("NET %s | %.0f fps, worst frame %.0f ms | rtt %.0f ms, ping %.0f ms | server clock %.1f"),
		S.bHost ? TEXT("HOST") : TEXT("JOINER"), Fps, S.FrameMaxMs, C.RttMs, C.PingMs, S.ServerTime));
}

bool UMONetDiagnosticsSubsystem::IsLogging() const
{
	static const IConsoleVariable* AutoStats = IConsoleManager::Get().FindConsoleVariable(TEXT("MO.Net.AutoStats"));
	return bLoggingChosen ? bLogging : (AutoStats && AutoStats->GetInt() != 0);
}

void UMONetDiagnosticsSubsystem::DescribeForBugReport(TArray<TPair<FString, FString>>& Out) const
{
	if (History.IsEmpty())
	{
		Out.Emplace(TEXT("Mode"), TEXT("not networked in the last two minutes"));
		return;
	}
	const FMONetSample& Last = History.Last();
	Out.Emplace(TEXT("Mode"), Last.bHost ? FString::Printf(TEXT("host (listen server), %d joiner(s)"), Last.Connections.Num()) : FString(TEXT("joiner")));
	Out.Emplace(TEXT("SampledAgoSec"), FString::Printf(TEXT("%.0f"), FPlatformTime::Seconds() - Last.Time));   // > a few s: the report was written after leaving the session
	const FMONetConnSample* Busiest = Last.Busiest();
	Out.Emplace(TEXT("RateCap"), FString::Printf(TEXT("MaxClientRate %d B/s; this connection %d B/s"), Last.MaxClientRate, Busiest ? Busiest->RateCapBytesPerSec : 0));

	TArray<FMONetSample> Window;
	for (int32 i = FMath::Max(0, History.Num() - 60); i < History.Num(); ++i)
	{
		Window.Add(History[i]);
	}
	const FMONetWindowSummary W = FMONetWindowSummary::Summarize(Window);
	Out.Emplace(TEXT("Last60s.Bandwidth"), FString::Printf(TEXT("out avg %.0f max %.0f B/s (max %.0f%% of the cap); %d of %d s saturated; loss max %.1f%%; rtt avg %.0f max %.0f ms"),
		W.OutAvg, W.OutMax, W.UseMaxPct, W.SaturatedSeconds, W.Seconds, W.LossMaxPct, W.RttAvgMs, W.RttMaxMs));
	Out.Emplace(TEXT("Last60s.Frames"), FString::Printf(TEXT("fps min %.0f avg %.0f, worst frame %.0f ms"), W.FpsMin, W.FpsAvg, W.FrameMaxMs));
	Out.Emplace(TEXT("Last60s.Movement"), Last.bHost
		? FString::Printf(TEXT("host: %d corrections of %d joiner moves checked (max error %.0f cm)"), W.ServerCorrections, W.ServerMovesChecked, W.ServerErrMaxCm)
		: FString::Printf(TEXT("joiner: %d corrections received, max visible snap %.0f cm"), W.ClientCorrections, W.ClientSnapMaxCm));
	Out.Emplace(TEXT("Last60s.RemotePawns"), FString::Printf(TEXT("slowest second %.1f updates per pawn, longest gap %.0f ms"), W.RemoteUpdatesMin, W.RemoteMaxGapMs));
	Out.Emplace(TEXT("Last60s.TopOut"), JoinTop(W.TopOutBytes));
	Out.Emplace(TEXT("Last60s.TopIn"), JoinTop(W.TopInBytes));
	Out.Emplace(TEXT("Clock"), FString::Printf(TEXT("UTC %s, server clock %.1f (match the host's and a joiner's reports by the UTC time)"),
		*FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d %H:%M:%S")), Last.ServerTime));
	TArray<FString> EventTexts;
	const double Now = FPlatformTime::Seconds();
	for (const FMONetEvent& Event : FMONetCounters::GetRecentEvents())
	{
		EventTexts.Add(FString::Printf(TEXT("%.0fs ago: %s"), Now - Event.Time, *Event.Text));
	}
	Out.Emplace(TEXT("Events"), EventTexts.IsEmpty() ? FString(TEXT("none this session (no visible correction, long frame, saturation or remote stall)"))
		: FString::Join(EventTexts, TEXT(" | ")));
	Out.Emplace(TEXT("Now"), Last.ToLogLine());
}

// ============================================================================
// Console verbs
// ============================================================================

namespace
{
	UMONetDiagnosticsSubsystem* SubsystemFor(UWorld* World)
	{
		return UMONetDiagnosticsSubsystem::Get(World);
	}

	bool ParseOnOff(const TArray<FString>& Args, bool& bOut)
	{
		if (Args.Num() < 1)
		{
			return false;
		}
		bOut = Args[0] == TEXT("1") || Args[0].Equals(TEXT("on"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("true"), ESearchCase::IgnoreCase);
		return true;
	}

	FAutoConsoleCommandWithWorldAndArgs GNetStatsCmd(
		TEXT("MO.Net.Stats"),
		TEXT("Co-op smoothness numbers. MO.Net.Stats 1 logs one [MONet] line per second on this machine (bytes vs the rate cap, saturation, loss, rtt, corrections, ")
		TEXT("frame time, remote pawn update rate, top actor classes by bytes); 0 stops. With no argument: prints the last 60 s summary (the bug report's Net.* rows)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMONetDiagnosticsSubsystem* Diag = SubsystemFor(World);
			if (!Diag)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MONet] no diagnostics subsystem (no game instance)"));
				return;
			}
			bool bOn = false;
			if (ParseOnOff(Args, bOn))
			{
				Diag->SetLogging(bOn);
				UE_LOG(LogMOFramework, Warning, TEXT("[MONet] stats logging %s"), bOn ? TEXT("ON") : TEXT("off"));
				return;
			}
			TArray<TPair<FString, FString>> Rows;
			Diag->DescribeForBugReport(Rows);
			for (const TPair<FString, FString>& Row : Rows)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MONet] Net.%s = %s"), *Row.Key, *Row.Value);
			}
		}));

	TAutoConsoleVariable<int32> CVarNetAutoStats(
		TEXT("MO.Net.AutoStats"), 1,
		TEXT("1 (default): in a co-op session every machine logs one [MONet] line a second without being asked (MO.Net.Stats 0|1 overrides). 0: only when asked."),
		ECVF_Default);

	FAutoConsoleCommandWithWorldAndArgs GNetShowCmd(
		TEXT("MO.Net.Show"),
		TEXT("MO.Net.Show 1 draws this machine's co-op numbers on screen every second (fps, round trip, bytes vs the cap, saturation, corrections, the other ")
		TEXT("player's position rate and the last hitch / rubber-band event); 0 hides them. Development builds only (Shipping draws no screen messages)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMONetDiagnosticsSubsystem* Diag = SubsystemFor(World);
			bool bOn = true;
			ParseOnOff(Args, bOn);
			if (Diag)
			{
				Diag->SetShowing(bOn);
				UE_LOG(LogMOFramework, Warning, TEXT("[MONet] on-screen numbers %s"), bOn ? TEXT("ON") : TEXT("off"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GNetTraceCmd(
		TEXT("MO.Net.Trace"),
		TEXT("MO.Net.Trace 1 logs a [MONetPos] line per player pawn 20 times a second (actor and mesh position, speed, gait as this machine sees it), for the ")
		TEXT("netdiag harness to line up the host's and the joiner's views; 0 stops."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMONetDiagnosticsSubsystem* Diag = SubsystemFor(World);
			bool bOn = false;
			if (Diag && ParseOnOff(Args, bOn))
			{
				Diag->SetTracing(bOn);
				UE_LOG(LogMOFramework, Warning, TEXT("[MONet] pawn trace %s"), bOn ? TEXT("ON") : TEXT("off"));
			}
		}));

	/**
	 * The netdiag path: hold "forward" and change gait on a fixed schedule (walk, jog, sprint, jog, walk, sprint ... 6-8 s each), turning 90 degrees at each
	 * phase change and whenever the pawn has not got anywhere for 1.5 s (a random world can face a cliff). Everything runs on the core ticker, never inside the
	 * console call: an RPC sent inside a bridge script call runs locally (Docs/Coop_And_Networking.md section 6).
	 */
	struct FNetPathRun
	{
		TWeakObjectPtr<APawn> Pawn;
		double Start = 0.0, End = 0.0;
		int32 Phase = -1;
		double BlockCheckAt = 0.0;
		FVector BlockCheckPos = FVector::ZeroVector;
		double NextGaitFix = 0.0;
	};

	enum class ENetPathGait : uint8 { Walk, Jog, Sprint };
	const ENetPathGait GNetPathPhases[] = { ENetPathGait::Walk, ENetPathGait::Jog, ENetPathGait::Sprint, ENetPathGait::Jog, ENetPathGait::Walk, ENetPathGait::Sprint, ENetPathGait::Jog };
	const float GNetPathPhaseSeconds[] = { 8.0f, 8.0f, 6.0f, 8.0f, 8.0f, 6.0f, 8.0f };

	void TurnPawn(APawn* Pawn, float Yaw)
	{
		if (AController* Controller = Pawn->GetController())
		{
			FRotator Rot = Controller->GetControlRotation();
			Rot.Yaw += Yaw;
			Controller->SetControlRotation(Rot);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GNetPathCmd(
		TEXT("MO.Test.NetPath"),
		TEXT("TEST: walk / jog / sprint the local pawn along a scripted path for N seconds (netdiag), first turning by Yaw degrees (the host walks the other way ")
		TEXT("from the joiner, so the two pawns do not bump). Usage: MO.Test.NetPath <seconds> [yaw]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Pawn || !Pawn->GetClass()->ImplementsInterface(UMOControllableInterface::StaticClass()))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL NetPath: no controllable local pawn"));
				return;
			}
			const float Seconds = Args.Num() > 0 ? FMath::Max(1.0f, FCString::Atof(*Args[0])) : 60.0f;
			const float StartYaw = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
			TSharedRef<FNetPathRun> Run = MakeShared<FNetPathRun>();
			Run->Pawn = Pawn;
			Run->Start = FPlatformTime::Seconds();
			Run->End = Run->Start + Seconds;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run, StartYaw](float)
			{
				APawn* P = Run->Pawn.Get();
				AMOCharacter* MO = Cast<AMOCharacter>(P);
				const UMOCharacterMovementComponent* Move = MO ? Cast<UMOCharacterMovementComponent>(MO->GetCharacterMovement()) : nullptr;
				const bool bJogging = Move && Move->WantsToJog() && !MO->IsSprinting();   // the jog the moves carry (set where the input happens)
				const double Now = FPlatformTime::Seconds();
				if (P && Run->Phase < 0 && StartYaw != 0.0f)
				{
					TurnPawn(P, StartYaw);
				}
				if (!P || Now >= Run->End)
				{
					if (MO && MO->IsSprinting())
					{
						IMOControllableInterface::Execute_RequestSprintEnd(P);
					}
					if (bJogging)
					{
						IMOControllableInterface::Execute_RequestToggleJog(P);
					}
					UE_LOG(LogMOFramework, Warning, TEXT("[NETPATH] end"));
					return false;
				}
				// which phase are we in (the schedule repeats)
				double Elapsed = Now - Run->Start;
				double Cycle = 0.0;
				for (const float S : GNetPathPhaseSeconds) { Cycle += S; }
				Elapsed = FMath::Fmod(Elapsed, Cycle);
				int32 Phase = 0;
				for (double Acc = GNetPathPhaseSeconds[0]; Elapsed >= Acc && Phase + 1 < UE_ARRAY_COUNT(GNetPathPhases); Acc += GNetPathPhaseSeconds[++Phase]) {}
				const ENetPathGait Want = GNetPathPhases[Phase];
				if (Phase != Run->Phase)
				{
					if (Run->Phase >= 0)
					{
						TurnPawn(P, 90.0f);
					}
					Run->Phase = Phase;
					UE_LOG(LogMOFramework, Warning, TEXT("[NETPATH] phase %d gait %s at %s"), Phase,
						Want == ENetPathGait::Sprint ? TEXT("sprint") : (Want == ENetPathGait::Jog ? TEXT("jog") : TEXT("walk")), *P->GetActorLocation().ToCompactString());
				}
				// gait: reconcile at most twice a second (a toggle takes a frame to show)
				if (MO && Now >= Run->NextGaitFix)
				{
					Run->NextGaitFix = Now + 0.5;
					if (Want == ENetPathGait::Sprint && !MO->IsSprinting())
					{
						IMOControllableInterface::Execute_RequestSprintStart(P);
					}
					else if (Want != ENetPathGait::Sprint && MO->IsSprinting())
					{
						IMOControllableInterface::Execute_RequestSprintEnd(P);
					}
					else if (Want != ENetPathGait::Sprint && (Want == ENetPathGait::Jog) != bJogging)
					{
						IMOControllableInterface::Execute_RequestToggleJog(P);
					}
				}
				// blocked for 1.5 s -> turn
				if (Now >= Run->BlockCheckAt)
				{
					const FVector Here = P->GetActorLocation();
					if (Run->BlockCheckAt > 0.0 && FVector::Dist2D(Here, Run->BlockCheckPos) < 100.0)
					{
						TurnPawn(P, 90.0f);
						UE_LOG(LogMOFramework, Warning, TEXT("[NETPATH] blocked -> turning 90 degrees"));
					}
					Run->BlockCheckPos = Here;
					Run->BlockCheckAt = Now + 1.5;
				}
				IMOControllableInterface::Execute_RequestMove(P, FVector2D(0.0, 1.0));
				return true;
			}));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PASS NetPath: %.0f s"), Seconds);
		}));

	/** NEGATIVE CONTROL for the saturation measure: a rate cap low enough that the host's channel to each joiner must saturate. 0 = back to MaxClientRate. */
	FAutoConsoleCommandWithWorldAndArgs GNetTestRateCapCmd(
		TEXT("MO.Net.TestRateCap"),
		TEXT("TEST ONLY (host): set every joiner connection's rate cap (CurrentNetSpeed, bytes/s); 0 = the net driver's MaxClientRate. Usage: MO.Net.TestRateCap <bytesPerSec|0>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
			if (!Driver || !Driver->IsServer() || Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MONet] TestRateCap: needs a host world and a value"));
				return;
			}
			const int32 Requested = FCString::Atoi(*Args[0]);
			const int32 Cap = Requested > 0 ? FMath::Max(Requested, 1800) : Driver->MaxClientRate;   // the engine's own floor (APlayerController::SetNetSpeed)
			for (int32 i = 0; i < Driver->ClientConnections.Num(); ++i)
			{
				if (UNetConnection* C = Driver->ClientConnections[i])
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MONet] TestRateCap: joiner%d %d -> %d B/s"), i, C->CurrentNetSpeed, Cap);
					C->CurrentNetSpeed = Cap;
				}
			}
		}), ECVF_Cheat);
}
