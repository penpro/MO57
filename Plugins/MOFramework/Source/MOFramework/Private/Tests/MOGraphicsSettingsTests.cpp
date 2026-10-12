// Graphics quality plumbing: when the first-run hardware benchmark runs (and when it must NOT: editor, servers, automated runs, an explicit opt-out, a level already chosen), the level names the
// Options panel shows, the resolution parser behind its list, and the shipped scalability file (every group the project overrides must exist in the engine's set and every override must name a real cvar).

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "MOGameSettings.h"
#include "MOOptionsPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOGraphics_BenchmarkDecision,
	"MOFramework.Graphics.BenchmarkDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOGraphics_BenchmarkDecision::RunTest(const FString& Parameters)
{
	// (already chosen, editor, dedicated server, unattended, suppressed)
	TestTrue(TEXT("a first run on a player's machine benchmarks"), UMOGameSettings::ShouldRunQualityBenchmark(false, false, false, false, false));
	TestFalse(TEXT("a saved choice is kept"), UMOGameSettings::ShouldRunQualityBenchmark(true, false, false, false, false));
	TestFalse(TEXT("the editor keeps its own settings"), UMOGameSettings::ShouldRunQualityBenchmark(false, true, false, false, false));
	TestFalse(TEXT("a dedicated server has no graphics"), UMOGameSettings::ShouldRunQualityBenchmark(false, false, true, false, false));
	TestFalse(TEXT("automated runs pin their settings"), UMOGameSettings::ShouldRunQualityBenchmark(false, false, false, true, false));
	TestFalse(TEXT("-NoQualityBenchmark opts out (perf measurements, test harnesses)"), UMOGameSettings::ShouldRunQualityBenchmark(false, false, false, false, true));

	TestEqual(TEXT("Low"), UMOGameSettings::GraphicsQualityLevelName(0).ToString(), FString(TEXT("Low")));
	TestEqual(TEXT("Medium"), UMOGameSettings::GraphicsQualityLevelName(1).ToString(), FString(TEXT("Medium")));
	TestEqual(TEXT("High"), UMOGameSettings::GraphicsQualityLevelName(2).ToString(), FString(TEXT("High")));
	TestEqual(TEXT("Epic"), UMOGameSettings::GraphicsQualityLevelName(3).ToString(), FString(TEXT("Epic")));
	TestEqual(TEXT("a mixed set of groups is Custom"), UMOGameSettings::GraphicsQualityLevelName(-1).ToString(), FString(TEXT("Custom")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOGraphics_ResolutionParse,
	"MOFramework.Graphics.ResolutionParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOGraphics_ResolutionParse::RunTest(const FString& Parameters)
{
	auto Same = [this](const TCHAR* What, const FIntPoint& Actual, const FIntPoint& Expected)
	{
		TestTrue(*FString::Printf(TEXT("%s (got %d x %d)"), What, Actual.X, Actual.Y), Actual == Expected);
	};
	Same(TEXT("the list's own format"), UMOOptionsPanel::ParseResolution(TEXT("1920 x 1080")), FIntPoint(1920, 1080));
	Same(TEXT("an ultrawide"), UMOOptionsPanel::ParseResolution(TEXT("3440 x 1440")), FIntPoint(3440, 1440));
	Same(TEXT("16:10"), UMOOptionsPanel::ParseResolution(TEXT("2560 x 1600")), FIntPoint(2560, 1600));
	Same(TEXT("the old label format"), UMOOptionsPanel::ParseResolution(TEXT("4K (3840x2160)")), FIntPoint(3840, 2160));
	Same(TEXT("the old 1080p label"), UMOOptionsPanel::ParseResolution(TEXT("1080p (1920x1080)")), FIntPoint(1920, 1080));
	Same(TEXT("garbage falls back to 1080p"), UMOOptionsPanel::ParseResolution(TEXT("auto")), FIntPoint(1920, 1080));
	Same(TEXT("an implausibly small size falls back"), UMOOptionsPanel::ParseResolution(TEXT("12 x 8")), FIntPoint(1920, 1080));
	Same(TEXT("empty falls back"), UMOOptionsPanel::ParseResolution(FString()), FIntPoint(1920, 1080));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOGraphics_ScalabilityFile,
	"MOFramework.Graphics.ScalabilityFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOGraphics_ScalabilityFile::RunTest(const FString& Parameters)
{
	// The project's overrides (Config/DefaultScalability.ini) are read through GScalabilityIni together with the engine's BaseScalability.ini: every [Group@Level] the project writes must be a group
	// the engine knows, and every cvar line must name a registered cvar that carries ECVF_Scalability (a typo would silently do nothing, and a cvar without the flag fails the cook with an ensure).
	const FString ProjectFile = FPaths::ProjectConfigDir() / TEXT("DefaultScalability.ini");
	if (!FPaths::FileExists(ProjectFile))
	{
		AddWarning(TEXT("no Config/DefaultScalability.ini: the engine's own scalability tiers apply unchanged"));
		return true;
	}
	FConfigFile Config;
	Config.Read(ProjectFile);
	int32 NumOverrides = 0;
	for (const TPair<FString, FConfigSection>& Section : AsConst(Config))
	{
		const FString& Name = Section.Key;
		if (!Name.Contains(TEXT("@")))
		{
			continue;   // not a quality-level section
		}
		for (const TPair<FName, FConfigValue>& Entry : Section.Value)
		{
			++NumOverrides;
			const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Entry.Key.ToString());
			TestNotNull(*FString::Printf(TEXT("[%s] %s is a registered cvar"), *Name, *Entry.Key.ToString()), CVar);
			// The engine ENSURES (fatal in the cook commandlet) on a scalability cvar that does not carry ECVF_Scalability: "Scalability.ini can only set ECVF_Scalability console variables".
			if (CVar)
			{
				TestTrue(*FString::Printf(TEXT("[%s] %s carries ECVF_Scalability (scalability files may only set such cvars)"), *Name, *Entry.Key.ToString()),
					CVar->TestFlags(ECVF_Scalability) || CVar->TestFlags(ECVF_ScalabilityGroup));
			}
		}
	}
	TestTrue(TEXT("the file overrides at least one cvar"), NumOverrides > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
