// The audio NaN guard's block rule (FMOAudioNaNGuard::SanitizeBlock): a block with any invalid sample (non-finite, or beyond +12 dBFS) goes out
// silent WHOLE -- zeroing only the invalid samples left the garbage around them, a full-scale buzz (Oct 22) -- and a clean block passes untouched.
// Run: python Tools/ue.py auto --filter "MOFramework.Audio.NaNGuard"

#include "Misc/AutomationTest.h"
#include "MOAudioNaNGuard.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAudioNaNGuard_CorruptBlockGoesSilentWhole, "MOFramework.Audio.NaNGuard.CorruptBlockGoesSilentWhole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAudioNaNGuard_CorruptBlockGoesSilentWhole::RunTest(const FString& Parameters)
{
	auto Run = [](TArray<float> In, TArray<float>& Out) -> int64
	{
		Out.SetNumUninitialized(In.Num());
		return FMOAudioNaNGuard::SanitizeBlock(In.GetData(), Out.GetData(), In.Num());
	};
	auto AllZero = [](const TArray<float>& Block)
	{
		return Block.FindByPredicate([](float S) { return S != 0.0f; }) == nullptr;
	};

	TArray<float> Out;
	const TArray<float> Clean = { 0.25f, -0.8f, 1.5f, -3.9f, 0.0f };   // loud but real: up to +12 dBFS passes
	TestEqual(TEXT("a clean block: nothing invalid"), Run(Clean, Out), int64(0));
	TestTrue(TEXT("... and it passes untouched"), Out == Clean);

	TestEqual(TEXT("one NaN"), Run({ 0.2f, NAN, 0.3f, -0.4f }, Out), int64(1));
	TestTrue(TEXT("... silences the whole block (the samples around it are garbage too)"), AllZero(Out));

	TestEqual(TEXT("an infinity"), Run({ 0.1f, -INFINITY, 0.1f }, Out), int64(1));
	TestTrue(TEXT("... silences the whole block"), AllZero(Out));

	TestEqual(TEXT("UDW's finite garbage: 13.6 and 1e24 (measured)"), Run({ 0.5f, 13.6f, -0.7f, 1.0e24f }, Out), int64(2));
	TestTrue(TEXT("... silences the whole block"), AllZero(Out));

	TestEqual(TEXT("just over +12 dBFS is invalid"), Run({ 4.01f, 0.1f }, Out), int64(1));

	TArray<float> InPlace = { 0.3f, NAN, 0.6f };
	TestEqual(TEXT("in place (In == Out)"), FMOAudioNaNGuard::SanitizeBlock(InPlace.GetData(), InPlace.GetData(), InPlace.Num()), int64(1));
	TestTrue(TEXT("... silences the whole block"), AllZero(InPlace));
	return true;
}

#endif
