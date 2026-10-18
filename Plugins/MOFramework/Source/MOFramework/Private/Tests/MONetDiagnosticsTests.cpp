// Co-op smoothness diagnostics (MONetDiagnostics.h): the pure parts -- which connection a sample reports, the [MONet] line the netdiag harness parses,
// the bug report's 60 s summary, and the counters' second-by-second reset. Headless-safe: no net driver, no world.

#include "Misc/AutomationTest.h"
#include "MONetDiagnostics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMONetConnSample MakeConn(int32 Out, int32 Cap, int32 Reps, int32 SatReps)
	{
		FMONetConnSample C;
		C.OutBytesPerSec = Out;
		C.RateCapBytesPerSec = Cap;
		C.Replications = Reps;
		C.SaturatedReplications = SatReps;
		C.TrackedFrames = Reps;
		C.RttMs = 20.0f;
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMONetDiag_BusiestAndLine,
	"MOFramework.Net.Diagnostics.BusiestAndLogLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMONetDiag_BusiestAndLine::RunTest(const FString& Parameters)
{
	FMONetSample S;
	S.bHost = true;
	S.FrameAvgMs = 20.0f;
	S.FrameMaxMs = 45.0f;
	S.Connections.Add(MakeConn(90000, 100000, 50, 0));
	S.Connections.Add(MakeConn(30000, 100000, 50, 12));   // fewer bytes but saturated: this is the one in trouble
	TestTrue(TEXT("the saturated connection is the busiest, not the one with more bytes"), S.Busiest() == &S.Connections[1]);
	S.TopOutBytes = { TPair<FName, int64>(TEXT("BP_Deer_C"), 4000), TPair<FName, int64>(TEXT("AMOWorldItem"), 900) };

	const FString Line = S.ToLogLine();
	TestTrue(TEXT("line starts with the [MONet] tag and the role"), Line.StartsWith(TEXT("[MONet] role=host ")));
	TestTrue(TEXT("fps from the average frame"), Line.Contains(TEXT(" fps=50.0 ")));
	TestTrue(TEXT("bytes and the cap of the busiest connection"), Line.Contains(TEXT(" out=30000 ")) && Line.Contains(TEXT(" cap=100000 ")) && Line.Contains(TEXT(" use=30% ")));
	TestTrue(TEXT("saturated replication share (12 of 50 = 24%)"), Line.Contains(TEXT(" satRepl=24% ")));
	TestTrue(TEXT("top classes as Class:bytes"), Line.Contains(TEXT("topOut=BP_Deer_C:4000,AMOWorldItem:900 ")));
	TestTrue(TEXT("an empty list prints a dash (the parser needs a token)"), Line.EndsWith(TEXT("topIn=-")));

	FMONetSample Empty;
	TestNull(TEXT("no connections: no busiest"), Empty.Busiest());
	TestTrue(TEXT("a sample with no connection still makes a parseable line"), Empty.ToLogLine().Contains(TEXT(" conns=0 out=0 ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMONetDiag_WindowSummary,
	"MOFramework.Net.Diagnostics.WindowSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMONetDiag_WindowSummary::RunTest(const FString& Parameters)
{
	TArray<FMONetSample> Window;
	for (int32 i = 0; i < 4; ++i)
	{
		FMONetSample& S = Window.AddDefaulted_GetRef();
		S.bHost = true;
		S.FrameAvgMs = i == 2 ? 40.0f : 16.0f;   // one slow second
		S.FrameMaxMs = i == 2 ? 120.0f : 20.0f;
		S.Connections.Add(MakeConn(i == 3 ? 100000 : 20000, 100000, 60, i == 3 ? 30 : 0));
		S.ServerMovesChecked = 60;
		S.ServerCorrections = i == 3 ? 5 : 0;
		S.ServerErrMaxCm = i == 3 ? 42.0f : 0.0f;
		S.RemotePawns = 1;
		S.RemoteUpdatesPerPawn = i == 3 ? 4.0f : 60.0f;
		S.RemoteMaxGapMs = i == 3 ? 400.0f : 30.0f;
		S.TopOutBytes = { TPair<FName, int64>(TEXT("BP_Deer_C"), 1000), TPair<FName, int64>(TEXT("AMOCharacter"), 500 * (i + 1)) };
	}
	const FMONetWindowSummary W = FMONetWindowSummary::Summarize(Window, 2);
	TestEqual(TEXT("seconds"), W.Seconds, 4);
	TestEqual(TEXT("worst fps is the slow second (25)"), W.FpsMin, 25.0f);
	TestEqual(TEXT("worst frame"), W.FrameMaxMs, 120.0f);
	TestEqual(TEXT("max bytes out"), W.OutMax, 100000.0f);
	TestEqual(TEXT("max use of the cap"), W.UseMaxPct, 100.0f);
	TestEqual(TEXT("one saturated second"), W.SaturatedSeconds, 1);
	TestEqual(TEXT("corrections summed"), W.ServerCorrections, 5);
	TestEqual(TEXT("moves summed"), W.ServerMovesChecked, 240);
	TestEqual(TEXT("worst error"), W.ServerErrMaxCm, 42.0f);
	TestEqual(TEXT("slowest remote second"), W.RemoteUpdatesMin, 4.0f);
	TestEqual(TEXT("longest remote gap"), W.RemoteMaxGapMs, 400.0f);
	TestEqual(TEXT("top classes summed over the window: AMOCharacter (500+1000+1500+2000) beats BP_Deer_C (4 x 1000)"),
		W.TopOutBytes.Num() > 0 ? W.TopOutBytes[0].Key : FName(), FName(TEXT("AMOCharacter")));
	TestEqual(TEXT("its total"), W.TopOutBytes.Num() > 0 ? W.TopOutBytes[0].Value : 0, static_cast<int64>(5000));

	const FMONetWindowSummary None = FMONetWindowSummary::Summarize({});
	TestEqual(TEXT("an empty window is all zero"), None.Seconds, 0);
	TestEqual(TEXT("an empty window has no fps"), None.FpsMin, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMONetDiag_CountersReset,
	"MOFramework.Net.Diagnostics.CountersReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMONetDiag_CountersReset::RunTest(const FString& Parameters)
{
	FMONetCounters::TakeAndReset();   // whatever a game left behind
	FMONetCounters::NoteServerMoveChecked(nullptr, false, FVector::ZeroVector);
	FMONetCounters::NoteServerMoveChecked(nullptr, true, FVector(30.0, 40.0, -10.0));
	FMONetCounters::NoteClientMoveResponse(true);
	FMONetCounters::NoteClientMoveResponse(false);
	FMONetCounters::NoteClientCorrectionSnap(nullptr, 12.0f);
	FMONetCounters::NoteChannelBits(TEXT("BP_Deer_C"), 80, true);
	FMONetCounters::NoteChannelBits(TEXT("BP_Deer_C"), 16, true);
	FMONetCounters::NoteChannelBits(TEXT("AMOWorldItem"), 8, false);

	const FMONetCounters::FSnapshot A = FMONetCounters::TakeAndReset();
	TestEqual(TEXT("moves checked"), A.ServerMovesChecked, 2);
	TestEqual(TEXT("corrections needed"), A.ServerCorrections, 1);
	TestEqual(TEXT("XY error of (30, 40)"), A.ServerErrXYSum, 50.0);
	TestEqual(TEXT("Z error"), A.ServerErrZSum, 10.0);
	TestEqual(TEXT("client corrections"), A.ClientCorrections, 1);
	TestEqual(TEXT("client acks"), A.ClientAcks, 1);
	TestEqual(TEXT("snap max"), A.ClientSnapMax, 12.0f);
	TestEqual(TEXT("bits out by class"), A.OutBits.FindRef(TEXT("BP_Deer_C")), static_cast<int64>(96));
	TestEqual(TEXT("bits in by class"), A.InBits.FindRef(TEXT("AMOWorldItem")), static_cast<int64>(8));

	const FMONetCounters::FSnapshot B = FMONetCounters::TakeAndReset();
	TestEqual(TEXT("the next second starts at zero"), B.ServerMovesChecked + B.ClientCorrections + B.OutBits.Num(), 0);
	return true;
}

#endif
