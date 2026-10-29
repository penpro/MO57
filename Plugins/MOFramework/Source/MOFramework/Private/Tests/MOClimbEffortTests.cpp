// What a climb costs (F51, Wes Oct 29: "make it angle dependent, but also tie some skill or physical attribute to it ... the more muscle mass percentage
// of the body the better at climbing and the more skill the better"): the face's angle, the climber's training and strength against weight.

#include "Misc/AutomationTest.h"
#include "MOCharacter.h"
#include "MOMetabolismTypes.h"
#include "MOSurvivalStatsComponent.h"
#include "MOTraversal.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOTraversal_ClimbEffort,
	"MOFramework.Traversal.ClimbEffort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOTraversal_ClimbEffort::RunTest(const FString& Parameters)
{
	// The face's angle from level ground, from its outward normal.
	TestEqual(TEXT("a sheer face is 90 deg"), FMOTraversal::FaceAngleDeg(FVector(1.0, 0.0, 0.0)), 90.0f, 0.01f);
	TestEqual(TEXT("a slab leaning back 30 deg from level"), FMOTraversal::FaceAngleDeg(FVector(0.5, 0.0, 0.8660254)), 30.0f, 0.01f);
	TestEqual(TEXT("an overhang 135 deg"), FMOTraversal::FaceAngleDeg(FVector(0.70710678, 0.0, -0.70710678)), 135.0f, 0.01f);
	TestEqual(TEXT("no face known reads as sheer"), FMOTraversal::FaceAngleDeg(FVector::ZeroVector), 90.0f);

	// The climber: an untrained average body is the reference; more muscle for the weight and more training are cheaper and faster.
	const FMOTraversal::FClimbAbility Reference = FMOTraversal::ClimbAbility(0, FMOTraversal::ReferenceMuscleShare);
	TestEqual(TEXT("untrained, average build: cost x1"), Reference.CostScale, 1.0f, 1e-4f);
	TestEqual(TEXT("untrained, average build: speed x1"), Reference.SpeedScale, 1.0f, 1e-4f);
	TestEqual(TEXT("no body known counts as average"), FMOTraversal::ClimbAbility(0, 0.0f).CostScale, 1.0f, 1e-4f);
	FMOBodyComposition Heavy, Default, Lillian, Lean;
	Heavy.SetBody(35.0f, 25.0f);     // 25 kg of muscle at 35 % fat
	Default.SetBody(18.0f, 30.0f);   // UMOSurvivorBase's defaults
	Lillian.SetBody(12.4f, 28.5f);   // the femme base
	Lean.SetBody(10.0f, 40.0f);      // a strong, lean body
	const auto Share = [](const FMOBodyComposition& Body) { return Body.MuscleMass / Body.TotalWeight; };
	const float CostHeavy = FMOTraversal::ClimbAbility(0, Share(Heavy)).CostScale;
	const float CostDefault = FMOTraversal::ClimbAbility(0, Share(Default)).CostScale;
	const float CostLillian = FMOTraversal::ClimbAbility(0, Share(Lillian)).CostScale;
	const float CostLean = FMOTraversal::ClimbAbility(0, Share(Lean)).CostScale;
	TestTrue(FString::Printf(TEXT("more muscle for the weight is cheaper: %.2f (35 %% fat) > %.2f (default) > %.2f (Lillian) > %.2f (lean)"), CostHeavy, CostDefault,
		CostLillian, CostLean), CostHeavy > CostDefault && CostDefault > CostLillian && CostLillian > CostLean);
	TestTrue(FString::Printf(TEXT("the default survivor is about average (%.2f)"), CostDefault), FMath::IsNearlyEqual(CostDefault, 1.0f, 0.05f));
	TestTrue(TEXT("more muscle for the weight is faster"), FMOTraversal::ClimbAbility(0, Share(Lean)).SpeedScale > FMOTraversal::ClimbAbility(0, Share(Heavy)).SpeedScale);
	const FMOTraversal::FClimbAbility Level10 = FMOTraversal::ClimbAbility(10, FMOTraversal::ReferenceMuscleShare);
	TestEqual(TEXT("level 10 is ~63 % trained: cost x0.68"), Level10.CostScale, 1.0f - 0.5f * (1.0f - FMath::Exp(-1.0f)), 1e-3f);
	TestEqual(TEXT("level 10: speed x1.22"), Level10.SpeedScale, 1.0f + 0.35f * (1.0f - FMath::Exp(-1.0f)), 1e-3f);
	float LastCost = 2.0f;
	for (int32 Level = 0; Level <= 60; Level += 5)
	{
		const float Cost = FMOTraversal::ClimbAbility(Level, FMOTraversal::ReferenceMuscleShare).CostScale;
		TestTrue(FString::Printf(TEXT("level %d costs less than level %d"), Level, Level - 5), Cost < LastCost);
		LastCost = Cost;
	}
	TestTrue(TEXT("training never takes more than half off"), LastCost >= 0.5f);

	// The face, on the character's own curves: a slab rests as it climbs, a sheer face spends ~1/s, an overhang more.
	const AMOCharacter* Body = GetDefault<AMOCharacter>();
	const auto CostShare = [Body](float Deg) { return Body->ClimbingCostByFaceAngle.GetRichCurveConst()->Eval(Deg); };
	const auto Exertion = [Body](float Deg) { return Body->ClimbingExertionByFaceAngle.GetRichCurveConst()->Eval(Deg); };
	TestTrue(TEXT("steeper costs more: 60 < 75 < 90 < 110 < 140 deg"),
		CostShare(60.0f) < CostShare(75.0f) && CostShare(75.0f) < CostShare(90.0f) && CostShare(90.0f) < CostShare(110.0f) && CostShare(110.0f) < CostShare(140.0f));
	TestEqual(TEXT("a sheer face is the reference"), CostShare(90.0f), 1.0f, 1e-4f);
	const float AtRest = GetDefault<UMOSurvivalStatsComponent>()->Stamina.RegenRate;
	const float SlabNet = AtRest * Body->GetStaminaRecoveryFraction(Exertion(60.0f)) - Body->ClimbingStaminaCostPerSecond * CostShare(60.0f);
	TestTrue(FString::Printf(TEXT("a 60 deg slab nets %+.2f/s: it rests as it climbs"), SlabNet), SlabNet > 0.0f);
	TestEqual(TEXT("a sheer face recovers nothing"), Body->GetStaminaRecoveryFraction(Exertion(90.0f)), 0.0f, 1e-4f);
	const float SheerSeconds = GetDefault<UMOSurvivalStatsComponent>()->Stamina.Max / Body->ClimbingStaminaCostPerSecond;
	TestTrue(FString::Printf(TEXT("a full bar lasts %.0f s up a sheer face (untrained, average build)"), SheerSeconds), FMath::IsNearlyEqual(SheerSeconds, 100.0f, 1.0f));
	TestTrue(TEXT("an overhang is hung off the arms (exertion 85+ at 110 deg)"), Exertion(110.0f) >= 85.0f);
	return true;
}

#endif
