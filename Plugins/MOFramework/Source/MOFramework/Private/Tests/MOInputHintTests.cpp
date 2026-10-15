/**
 * Key glyphs in tutorial hints (FMOInputHintFormatter::FormatKeyGlyph / FormatKeysGlyph).
 * Run: python Tools/ue.py auto --filter "MOFramework.UI.InputHintGlyphs"
 *
 * Found Oct 15 2026 by a first-ten-minutes pass: a new player's first hint read "Use [W] to walk, and [Mouse XY 2D-Axis] to look
 * around" -- the move action's first key only, and the engine's editor label for the mouse.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "InputCoreTypes.h"
#include "MOInputHintFormatter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInputHint_Glyphs,
	"MOFramework.UI.InputHintGlyphs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOInputHint_Glyphs::RunTest(const FString& Parameters)
{
	using F = FMOInputHintFormatter;

	TestEqual(TEXT("a button key"), F::FormatKeyGlyph(EKeys::E), FString(TEXT("[E]")));
	TestEqual(TEXT("no binding"), F::FormatKeyGlyph(FKey()), FString(TEXT("[unbound]")));
	TestEqual(TEXT("the mouse axis reads as the mouse"), F::FormatKeyGlyph(EKeys::Mouse2D), FString(TEXT("[Mouse]")));
	TestEqual(TEXT("a pad stick reads as a stick"), F::FormatKeyGlyph(EKeys::Gamepad_Left2D), FString(TEXT("[Left Stick]")));

	// Move = W, S, A, D in the mapping context's order (plus the pad's left stick): the keyboard set, named the way players say it.
	TestEqual(TEXT("WASD in any mapping order"), F::FormatKeysGlyph({ EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::Gamepad_Left2D }), FString(TEXT("[WASD]")));
	TestEqual(TEXT("the arrow keys"), F::FormatKeysGlyph({ EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right }), FString(TEXT("[Arrow Keys]")));
	TestEqual(TEXT("move as the project binds it (WASD, then the arrows as an alternative): the first set"),
		F::FormatKeysGlyph({ EKeys::W, EKeys::S, EKeys::A, EKeys::D, EKeys::Up, EKeys::Down, EKeys::Right, EKeys::Left }), FString(TEXT("[WASD]")));
	TestEqual(TEXT("... and the arrows when they come first"),
		F::FormatKeysGlyph({ EKeys::Up, EKeys::Down, EKeys::Right, EKeys::Left, EKeys::W, EKeys::S, EKeys::A, EKeys::D }), FString(TEXT("[Arrow Keys]")));
	TestEqual(TEXT("any other set is listed"), F::FormatKeysGlyph({ EKeys::Q, EKeys::E }), FString(TEXT("[Q/E]")));
	TestEqual(TEXT("one key is one glyph"), F::FormatKeysGlyph({ EKeys::SpaceBar }), F::FormatKeyGlyph(EKeys::SpaceBar));
	TestEqual(TEXT("an axis key stands alone (look = mouse, not mouse/stick)"), F::FormatKeysGlyph({ EKeys::Mouse2D, EKeys::Gamepad_Right2D }), FString(TEXT("[Mouse]")));
	TestEqual(TEXT("a pad-first binding lists the pad"), F::FormatKeysGlyph({ EKeys::Gamepad_Right2D, EKeys::Mouse2D }), FString(TEXT("[Right Stick]")));
	TestEqual(TEXT("nothing bound"), F::FormatKeysGlyph({}), FString(TEXT("[unbound]")));
	return true;
}

#endif
