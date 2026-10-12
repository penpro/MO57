// UI design tokens: the colour parser, token references, and the accessibility swaps (colour-blind block, high contrast, reduced motion, text scale), on a hand-built token set (no asset needed),
// plus a check of the SHIPPED table: every token a runtime caller names must exist in DT_UITokens (a typo is a loud magenta, but a test is louder).

#include "Misc/AutomationTest.h"
#include "MOUITokens.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMOUITokenSet MakeUITestTokens()
	{
		TMap<FName, FString> M;
		M.Add("color.accent", TEXT("#6CB8A9"));
		M.Add("color.hud.plate", TEXT("#0E0D0BD1"));
		M.Add("color.hud.outline", TEXT("#000000CC"));
		M.Add("text.world.outline.color", TEXT("color.hud.outline"));
		M.Add("color.severity.critical", TEXT("#EE6550"));
		M.Add("colorblind.color.severity.critical", TEXT("#E890C4"));
		M.Add("color.text.primary", TEXT("#EEE7D7"));
		M.Add("color.text.secondary", TEXT("#BDB3A0"));
		M.Add("opacity.hud.plate", TEXT("0.82"));
		M.Add("motion.base", TEXT("140"));
		M.Add("motion.fast", TEXT("80"));
		M.Add("motion.pulse.critical", TEXT("1200"));
		M.Add("motion.ease.out", TEXT("cubic-bezier(0.2,0,0,1)"));
		M.Add("type.body.umgSize", TEXT("14"));
		M.Add("type.body.lineHeight", TEXT("26"));
		M.Add("type.body.weight", TEXT("400"));
		M.Add("control.height.min", TEXT("44"));
		M.Add("space.4", TEXT("16"));
		M.Add("font.face.IBMPlexSans.400", TEXT("/Engine/EngineFonts/Faces/RobotoRegular.RobotoRegular"));
		M.Add("font.face.IBMPlexSans.600", TEXT("/Engine/EngineFonts/Faces/RobotoBold.RobotoBold"));
		M.Add("font.face.Bitter.600", TEXT("/Engine/EngineFonts/Faces/RobotoBold.RobotoBold"));
		M.Add("loop.a", TEXT("loop.b"));
		M.Add("loop.b", TEXT("loop.a"));
		FMOUITokenSet S;
		S.Set(M);
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUITokens_ParseAndReference,
	"MOFramework.UITokens.ParseAndReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUITokens_ParseAndReference::RunTest(const FString& Parameters)
{
	const FMOUITokenSet S = MakeUITestTokens();
	const FMOUIAccessibility Plain;

	FLinearColor C;
	TestTrue(TEXT("RRGGBB parses"), FMOUITokenSet::ParseColor(TEXT("#6CB8A9"), C));
	TestTrue(TEXT("sRGB -> linear: 0x6C (108) is ~0.15"), FMath::IsNearlyEqual(C.R, 0.150f, 0.005f));
	TestEqual(TEXT("opaque alpha"), C.A, 1.0f);
	TestTrue(TEXT("RRGGBBAA parses"), FMOUITokenSet::ParseColor(TEXT("#0E0D0BD1"), C));
	TestTrue(TEXT("alpha is linear 0xD1/255"), FMath::IsNearlyEqual(C.A, 209.0f / 255.0f, 0.002f));
	TestFalse(TEXT("5 digits rejected"), FMOUITokenSet::ParseColor(TEXT("#6CB8A"), C));
	TestFalse(TEXT("non-hex rejected"), FMOUITokenSet::ParseColor(TEXT("#GGGGGG"), C));
	TestFalse(TEXT("a number is not a colour"), FMOUITokenSet::ParseColor(TEXT("48"), C));

	bool bFound = false;
	S.Color("color.accent", Plain, &bFound);
	TestTrue(TEXT("a real token is found"), bFound);
	const FLinearColor Missing = S.Color("color.does.not.exist", Plain, &bFound);
	TestFalse(TEXT("a missing token is reported"), bFound);
	TestEqual(TEXT("and reads loud magenta"), Missing, FLinearColor(1, 0, 1, 1));

	const FLinearColor Outline = S.Color("text.world.outline.color", Plain, &bFound);
	TestTrue(TEXT("a token that names another token resolves"), bFound && FMath::IsNearlyEqual(Outline.A, 204.0f / 255.0f, 0.002f));

	S.Color("loop.a", Plain, &bFound);
	TestFalse(TEXT("a reference loop terminates (not found)"), bFound);

	TestEqual(TEXT("number"), S.Number("space.4", Plain), 16.0f);
	TestEqual(TEXT("missing number takes the default"), S.Number("space.99", Plain, 7.0f), 7.0f);
	TestEqual(TEXT("a string token is not a number"), S.Number("motion.ease.out", Plain, 3.0f), 3.0f);
	TestEqual(TEXT("string"), S.String("motion.ease.out", Plain), FString(TEXT("cubic-bezier(0.2,0,0,1)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUITokens_FontKeyAndMargin,
	"MOFramework.UITokens.FontKeyAndMargin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUITokens_FontKeyAndMargin::RunTest(const FString& Parameters)
{
	const TArray<FString> Keys = { TEXT("Bitter|600"), TEXT("Bitter|700"), TEXT("IBM Plex Sans|400"), TEXT("IBM Plex Sans|600"), TEXT("IBM Plex Mono|500") };
	TestEqual(TEXT("exact weight"), FMOUITokenSet::PickFontKey(Keys, TEXT("IBM Plex Sans"), 400), FString(TEXT("IBM Plex Sans|400")));
	TestEqual(TEXT("exact weight 600"), FMOUITokenSet::PickFontKey(Keys, TEXT("Bitter"), 600), FString(TEXT("Bitter|600")));
	TestEqual(TEXT("no 500 in Plex Sans: a tie goes to the heavier face"), FMOUITokenSet::PickFontKey(Keys, TEXT("IBM Plex Sans"), 500), FString(TEXT("IBM Plex Sans|600")));
	TestEqual(TEXT("nearest weight wins (Bitter 400 -> 600)"), FMOUITokenSet::PickFontKey(Keys, TEXT("Bitter"), 400), FString(TEXT("Bitter|600")));
	TestEqual(TEXT("a family with no key"), FMOUITokenSet::PickFontKey(Keys, TEXT("Papyrus"), 400), FString());

	{
		const TArray<FString> FromTokens = MakeUITestTokens().FontFaceKeys();
		TestEqual(TEXT("three font.face tokens -> three keys"), FromTokens.Num(), 3);
		TestTrue(TEXT("key form is <Family>|<Weight>"), FromTokens.Contains(TEXT("IBMPlexSans|600")) && FromTokens.Contains(TEXT("Bitter|600")));
		TestEqual(TEXT("a medium weight picks the heavier Plex face"), FMOUITokenSet::PickFontKey(FromTokens, TEXT("IBMPlexSans"), 500), FString(TEXT("IBMPlexSans|600")));
	}

	FVector4 M;
	TestTrue(TEXT("margin parses"), FMOUITokenSet::ParseMargin(TEXT("0.125,0.125,0.125,0.1875"), M));
	TestEqual(TEXT("bottom"), (float)M.W, 0.1875f);
	TestFalse(TEXT("3 numbers rejected"), FMOUITokenSet::ParseMargin(TEXT("1,2,3"), M));
	TestFalse(TEXT("text rejected"), FMOUITokenSet::ParseMargin(TEXT("a,b,c,d"), M));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUITokens_Accessibility,
	"MOFramework.UITokens.Accessibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUITokens_Accessibility::RunTest(const FString& Parameters)
{
	const FMOUITokenSet S = MakeUITestTokens();

	FMOUIAccessibility CB;
	CB.bColorBlind = true;
	const FLinearColor Plain = S.Color("color.severity.critical", FMOUIAccessibility());
	const FLinearColor Alt = S.Color("color.severity.critical", CB);
	TestTrue(TEXT("colour-blind mode reads the colorblind. block"), !Plain.Equals(Alt, 0.01f) && Alt.B > Alt.G);   // reddish purple, not orange-red
	TestTrue(TEXT("non-severity tokens are untouched by colour-blind mode"), S.Color("color.accent", CB).Equals(S.Color("color.accent", FMOUIAccessibility())));

	FMOUIAccessibility HC;
	HC.bHighContrast = true;
	TestEqual(TEXT("HUD plate opacity 0.82 -> 0.95"), S.Number("opacity.hud.plate", HC), 0.95f);
	TestTrue(TEXT("secondary text becomes primary"), S.Color("color.text.secondary", HC).Equals(S.Color("color.text.primary", HC)));
	TestEqual(TEXT("normal plate opacity"), S.Number("opacity.hud.plate", FMOUIAccessibility()), 0.82f);

	FMOUIAccessibility RM;
	RM.bReducedMotion = true;
	TestEqual(TEXT("motion.base -> 0"), S.Number("motion.base", RM), 0.0f);
	TestEqual(TEXT("critical pulse -> 0 (static double border)"), S.Number("motion.pulse.critical", RM), 0.0f);
	TestEqual(TEXT("fast fades stay"), S.Number("motion.fast", RM), 80.0f);
	TestEqual(TEXT("strings under motion.* are untouched"), S.String("motion.ease.out", RM), FString(TEXT("cubic-bezier(0.2,0,0,1)")));

	FMOUIAccessibility Big;
	Big.TextScale = 1.5f;
	TestEqual(TEXT("type size x1.5"), S.Number("type.body.umgSize", Big), 21.0f);
	TestEqual(TEXT("line height x1.5"), S.Number("type.body.lineHeight", Big), 39.0f);
	TestEqual(TEXT("control height x1.5"), S.Number("control.height.min", Big), 66.0f);
	TestEqual(TEXT("weight is not scaled"), S.Number("type.body.weight", Big), 400.0f);
	TestEqual(TEXT("spacing is not scaled"), S.Number("space.4", Big), 16.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOUITokens_ShippedTable,
	"MOFramework.UITokens.ShippedTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOUITokens_ShippedTable::RunTest(const FString& Parameters)
{
	UMOUITokens::Reload();
	if (!UMOUITokens::IsLoaded())
	{
		AddError(TEXT("DT_UITokens did not load (Project Settings > MO UI Theme > Token Table)"));
		return false;
	}
	const TCHAR* Colors[] = { TEXT("color.surface.1"), TEXT("color.accent"), TEXT("color.focus.ring"), TEXT("color.severity.ok"), TEXT("color.severity.notice"), TEXT("color.severity.warning"),
		TEXT("color.severity.critical"), TEXT("color.severity.info"), TEXT("color.text.primary"), TEXT("color.text.secondary"), TEXT("color.hud.plate"), TEXT("color.hud.outline"), TEXT("color.paper.surface") };
	for (const TCHAR* Name : Colors)
	{
		const FLinearColor C = UMOUITokens::Color(FName(Name));
		TestFalse(*FString::Printf(TEXT("token %s exists and is a colour"), Name), C.Equals(FLinearColor(1, 0, 1, 1)));
	}
	const TCHAR* Numbers[] = { TEXT("space.4"), TEXT("radius.1"), TEXT("opacity.hud.plate"), TEXT("motion.base"), TEXT("motion.pulse.critical"), TEXT("control.height.min"), TEXT("row.height"), TEXT("slot.size"),
		TEXT("type.body.umgSize"), TEXT("type.title.umgSize"), TEXT("type.hud.umgSize"), TEXT("type.numeric.umgSize") };
	for (const TCHAR* Name : Numbers)
	{
		const float V = UMOUITokens::Number(FName(Name), -12345.0f);
		TestTrue(*FString::Printf(TEXT("token %s exists and is a number"), Name), V != -12345.0f);
	}
	TestEqual(TEXT("body is 18 px at 1080p = 14 UMG points"), UMOUITokens::Number("type.body.umgSize"), 14.0f);
	TestTrue(TEXT("the shipped severity colours differ from the colour-blind block"), []()
	{
		const FLinearColor A = UMOUITokens::Color("color.severity.critical");
		UMOUITokens::SetAccessibility(true, false, false, 1.0f);
		const FLinearColor B = UMOUITokens::Color("color.severity.critical");
		UMOUITokens::SetAccessibility(false, false, false, 1.0f);
		return !A.Equals(B, 0.01f);
	}());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
