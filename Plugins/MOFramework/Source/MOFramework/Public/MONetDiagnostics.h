/**
 * =============================================================================
 * MONetDiagnostics.h - how smooth a co-op session is, measured once a second on the host and on every joiner
 * =============================================================================
 *
 * WHY: "very bad rubber banding and animation latency at 9 ms ping" (Wes, Oct 17 2026) had nothing to measure it with. Ping can be low while the
 * host's channel to a joiner is saturated (the engine's per-client rate cap is 100 KB/s), while the server corrects every move, while the host
 * renders at 25 fps, or while a remote pawn's position arrives 5 times a second. Each of those is a different layer; this file tells them apart.
 *
 * WHAT IS SAMPLED (UMONetDiagnosticsSubsystem, every second, in any networked world):
 *   - the connection: bytes / packets per second in and out, the rate cap (CurrentNetSpeed: min of the joiner's requested speed and the host's
 *     MaxClientRate), how much of it is used, SATURATION (the engine's own per-connection analytics: replication passes and ticks that stopped
 *     early because the connection had sent its budget), packet loss, round-trip time, jitter, open channels;
 *   - the server: replication passes per second for the connection (a listen server replicates every frame: this is its net tick), active and
 *     dormant replicated objects;
 *   - this machine's frame time (average and worst frame of the second; the host's frame IS its server tick);
 *   - movement: on the host, ServerMoves checked and corrections needed for joiners' pawns and how far off they were (XY vs Z); on a joiner,
 *     corrections received and how far the pawn visibly snapped after replaying its moves; on both, how often each OTHER player's pawn got a
 *     position (a simulated proxy on a joiner, ServerMoves on the host) and the longest gap;
 *   - bytes per actor CLASS in and out (UMONetStatsActorChannel counts every actor-channel bunch), the top few.
 *
 * WHERE IT GOES (Steam test recipe: Docs/Coop_And_Networking.md section 7):
 *   - a [MONet] line per second in every co-op session without being asked (MO.Net.AutoStats 1; MO.Net.Stats 0|1 overrides), each stamped with the
 *     wall clock (utc=: lines up the host's and a joiner's logs across two PCs), play= (0 while the world builds) and the server clock (st=: world time,
 *     which stops at 0.4 s a frame, so it is not used to line logs up);
 *   - [MONetEvent] lines, always: a correction / snap >= 30 cm, a frame >= 250 ms while a player moves, a saturated second, a moving remote pawn silent for 500 ms;
 *   - MO.Net.Show 1: the numbers on screen (development builds); MO.Net.Trace 1: [MONetPos] lines (every player pawn, 20 Hz);
 *   - every bug report: Net.* rows (the last 60 s, the recent events, the server clock) -- the only record a Shipping build keeps.
 * Tools: `python -I Tools/ue_netdiag.py logs <host log> <joiner log>` reads two machines' logs; `python Tools/ue.py nettest netdiag` checks the
 * instruments on two local processes (controls that must fail), which cannot reproduce an internet connection.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MONetDiagnostics.generated.h"

class UWorld;
class UNetConnection;
class UNetDriver;

/** One connection's numbers over the last second (the host has one per joiner; a joiner has its connection to the host). */
struct MOFRAMEWORK_API FMONetConnSample
{
	FString Label;                 // the player's name on the host, "host" on a joiner
	int32 OutBytesPerSec = 0;
	int32 InBytesPerSec = 0;
	int32 OutPacketsPerSec = 0;
	int32 InPacketsPerSec = 0;
	int32 RateCapBytesPerSec = 0;  // CurrentNetSpeed of this connection
	float OutLossPct = 0.0f;
	float InLossPct = 0.0f;
	float RttMs = 0.0f;            // the connection's average round trip (AvgLag)
	float PingMs = 0.0f;           // what the player state says (what a ping display shows)
	float JitterMs = 0.0f;
	int32 Replications = 0;        // replication passes this second (host only)
	int32 SaturatedReplications = 0;
	int32 TrackedFrames = 0;
	int32 SaturatedFrames = 0;
	int32 OpenChannels = 0;

	float UsePct() const { return RateCapBytesPerSec > 0 ? 100.0f * OutBytesPerSec / RateCapBytesPerSec : 0.0f; }
	float SatReplPct() const { return Replications > 0 ? 100.0f * SaturatedReplications / Replications : 0.0f; }
	float SatFramePct() const { return TrackedFrames > 0 ? 100.0f * SaturatedFrames / TrackedFrames : 0.0f; }
};

/** Everything sampled in one second on this machine. */
struct MOFRAMEWORK_API FMONetSample
{
	double Time = 0.0;             // FPlatformTime::Seconds() at the sample
	double ServerTime = -1.0;      // the game state's server clock: world time, which stops at 0.4 s a frame, so it drifts seconds behind in hitches
	double Utc = 0.0;              // wall clock, Unix seconds with milliseconds: what lines up two PCs' logs (both synced by Windows time)
	bool bPlaying = true;          // false while this machine's world is still building behind the loading screen (UMOWorldReadinessSubsystem tracking)
	bool bHost = false;
	int32 Frames = 0;
	float FrameAvgMs = 0.0f;
	float FrameMaxMs = 0.0f;
	int32 MaxClientRate = 0;       // the net driver's configured cap per client
	int32 ActiveObjects = 0;       // host: replicated objects considered; joiner: open actor channels
	int32 DormantObjects = 0;
	TArray<FMONetConnSample> Connections;

	// movement (see FMONetCounters)
	int32 ServerMovesChecked = 0;
	int32 ServerCorrections = 0;
	float ServerErrAvgCm = 0.0f;
	float ServerErrMaxCm = 0.0f;
	float ServerErrAvgXYCm = 0.0f;
	float ServerErrAvgZCm = 0.0f;
	int32 ClientAcks = 0;
	int32 ClientCorrections = 0;
	int32 ClientSnaps = 0;
	float ClientSnapAvgCm = 0.0f;
	float ClientSnapMaxCm = 0.0f;
	int32 RemotePawns = 0;         // other players' pawns that got a position this second
	float RemoteUpdatesPerPawn = 0.0f;
	float RemoteMaxGapMs = 0.0f;

	TArray<TPair<FName, int64>> TopOutBytes;   // actor class -> bytes this second
	TArray<TPair<FName, int64>> TopInBytes;

	/** The busiest connection (most saturated, then most bytes out), or null. */
	const FMONetConnSample* Busiest() const;

	/** The [MONet] log line (pure: tested). */
	FString ToLogLine() const;
};

/** Summary of a window of samples (the bug report's last 60 s). Pure: tested. */
struct MOFRAMEWORK_API FMONetWindowSummary
{
	int32 Seconds = 0;
	float FpsMin = 0.0f, FpsAvg = 0.0f, FrameMaxMs = 0.0f;
	float OutAvg = 0.0f, OutMax = 0.0f, UseMaxPct = 0.0f;
	int32 SaturatedSeconds = 0;    // seconds with any saturated replication pass or tick
	float LossMaxPct = 0.0f;
	float RttAvgMs = 0.0f, RttMaxMs = 0.0f;
	int32 ServerCorrections = 0, ServerMovesChecked = 0;
	float ServerErrMaxCm = 0.0f;
	int32 ClientCorrections = 0;
	float ClientSnapMaxCm = 0.0f;
	float RemoteUpdatesMin = 0.0f;
	float RemoteMaxGapMs = 0.0f;
	TArray<TPair<FName, int64>> TopOutBytes;   // summed over the window
	TArray<TPair<FName, int64>> TopInBytes;

	static FMONetWindowSummary Summarize(const TArray<FMONetSample>& Samples, int32 TopN = 5);
};

/** Something a player would see as a hitch or a rubber band, kept for the bug report and logged as a [MONetEvent] line when it happens. */
struct MOFRAMEWORK_API FMONetEvent
{
	double Time = 0.0;             // FPlatformTime::Seconds()
	double ServerTime = -1.0;      // the server clock (see FMONetSample::ServerTime)
	double Utc = 0.0;              // wall clock (see FMONetSample::Utc)
	FString Text;
};

/**
 * Counters the movement and channel code feed (UMOCharacterMovementComponent, UMONetStatsActorChannel, the subsystem's own frame tick); the subsystem
 * reads and clears them once a second. Game thread only. Cheap enough to stay on: a few adds per move and per bunch.
 */
struct MOFRAMEWORK_API FMONetCounters
{
	/** Host: a joiner's ServerMove was checked against the server's position; ServerMinusClient is the disagreement when it needs a correction. */
	static void NoteServerMoveChecked(const AActor* Pawn, bool bNeedsCorrection, const FVector& ServerMinusClient);
	/** Joiner: the server acknowledged (bCorrection false) or corrected one of our moves. */
	static void NoteClientMoveResponse(bool bCorrection);
	/** Joiner: after a correction the pawn replayed its moves and ended up SnapCm from where it was drawn the frame before (the rubber band). */
	static void NoteClientCorrectionSnap(const AActor* Pawn, float SnapCm);
	/** Joiner: another player's pawn (a simulated proxy) received a new replicated position; bMoving = the server says it is moving (only then is a gap a stall). */
	static void NoteRemotePawnUpdate(const AActor* Pawn, bool bMoving);

	/** Records an event (see FMONetEvent) and logs it as [MONetEvent] at once, whatever MO.Net.Stats says: they are rare and they are what a report needs. */
	static void NoteEvent(const UObject* WorldContext, const FString& Text);
	/** The newest MaxRecentEvents events, oldest first. */
	static TArray<FMONetEvent> GetRecentEvents();
	/** The server clock of WorldContext's world (AGameStateBase::GetServerWorldTimeSeconds), or -1. */
	static double ServerTimeOf(const UObject* WorldContext);
	/** Wall clock now: Unix seconds with milliseconds (UTC). */
	static double UtcSeconds();

	static constexpr int32 MaxRecentEvents = 24;
	static constexpr float EventCorrectionCm = 30.0f;   // a correction / snap at least this long is visible
	static constexpr float EventFrameMs = 250.0f;       // a frame this long: the host extrapolates a joiner that stops sending moves after 250 ms
	static constexpr float EventRemoteGapMs = 500.0f;   // a moving remote pawn got no position for this long
	/** Any machine: an actor channel sent / received a bunch of Bits for an actor of this class. */
	static void NoteChannelBits(FName ActorClass, int64 Bits, bool bOutgoing);

	/** MO.Net.Stats 1 also writes the first few corrections of each second in detail ([MONetCorr] lines: the error, both movement modes, speeds). */
	static void SetDetailLogging(bool bOn);
	/** True when a [MONetCorr] line may be written now (detail logging on and this second's budget of DetailLinesPerSecond not spent). */
	static bool TakeDetailLine();
	static constexpr int32 DetailLinesPerSecond = 6;

	struct FSnapshot
	{
		int32 ServerMovesChecked = 0, ServerCorrections = 0;
		double ServerErrSum = 0.0, ServerErrXYSum = 0.0, ServerErrZSum = 0.0;
		float ServerErrMax = 0.0f;
		int32 ClientAcks = 0, ClientCorrections = 0, ClientSnaps = 0;
		double ClientSnapSum = 0.0;
		float ClientSnapMax = 0.0f;
		int32 RemotePawns = 0, RemoteUpdates = 0;
		float RemoteMaxGapMs = 0.0f;
		TMap<FName, int64> OutBits, InBits;
	};
	/** Returns the counts since the last call and starts a new second. */
	static FSnapshot TakeAndReset();
};

/** See the file comment. Lives on the game instance so it survives travel (host -> join -> leave). */
UCLASS()
class MOFRAMEWORK_API UMONetDiagnosticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UMONetDiagnosticsSubsystem* Get(const UObject* WorldContextObject);

	/** MO.Net.Stats 0|1: log every sample as a [MONet] line (an explicit choice; without one, MO.Net.AutoStats decides). */
	void SetLogging(bool bOn) { bLogging = bOn; bLoggingChosen = true; }
	/** Whether samples are logged now: the explicit choice, else MO.Net.AutoStats (on by default in co-op). */
	bool IsLogging() const;
	/** MO.Net.Trace: log [MONetPos] lines for every player pawn at TraceHz. */
	void SetTracing(bool bOn) { bTracing = bOn; }
	/** MO.Net.Show: the numbers on screen too (development builds draw screen messages; Shipping does not). */
	void SetShowing(bool bOn) { bShowing = bOn; }

	/** The samples of the last HistorySeconds (newest last). */
	const TArray<FMONetSample>& GetHistory() const { return History; }

	/** The bug report's Net.* rows (also MO.Net.Stats with no argument). */
	void DescribeForBugReport(TArray<TPair<FString, FString>>& Out) const;

	/** Measures one second ending now (public for the console verb and tests). */
	FMONetSample Sample(UWorld* World, double Now, double FrameSumSeconds, int32 Frames, double FrameMaxSeconds);

	static constexpr int32 HistorySeconds = 120;
	static constexpr float TraceHz = 20.0f;

private:
	bool Tick(float DeltaTime);
	UWorld* FindNetWorld() const;
	void TracePawns(UWorld* World, double Now) const;
	void WatchRemotePawns(UWorld* World);
	void ShowOnScreen(const FMONetSample& S) const;
	void SampleConnection(UNetConnection* Connection, bool bHost, FMONetConnSample& Out);

	FTSTicker::FDelegateHandle TickHandle;
	TArray<FMONetSample> History;
	bool bLogging = false;
	bool bLoggingChosen = false;
	bool bTracing = false;
	bool bShowing = false;
	/** Joiner: the last replicated position seen per remote player pawn (a change = an update arrived). */
	TMap<TWeakObjectPtr<const APawn>, FVector> LastRepLocation;
	TWeakObjectPtr<UNetDriver> DescribedDriver;

	double LastFrameTime = 0.0;
	double SecondStart = 0.0;
	double FrameSum = 0.0;
	double FrameMax = 0.0;
	int32 FrameCount = 0;
	double NextTraceTime = 0.0;

	/** Saturation analytics are cumulative per connection: the last values seen, to take the second's difference. */
	struct FSaturationSeen { uint32 Replications = 0, SaturatedReplications = 0, Frames = 0, SaturatedFrames = 0; };
	TMap<TWeakObjectPtr<UNetConnection>, FSaturationSeen> SaturationSeen;
};
