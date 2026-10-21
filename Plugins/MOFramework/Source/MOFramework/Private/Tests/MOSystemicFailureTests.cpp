// Whole-body failures kill only when SUSTAINED, and say what they were (UMOVitalsComponent::StepSystemicFailure). Until Oct 2026 a stopped heart, an SpO2
// under 50 % or 20-30 % of the blood left was broadcast every tick to no listener: none of them ever killed. These are the clock's rules.

#include "Misc/AutomationTest.h"
#include "MOVitalsComponent.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UMOVitalsComponent* HealthyVitals()
	{
		UMOVitalsComponent* V = NewObject<UMOVitalsComponent>();
		V->Vitals.MaxBloodVolume = 5000.0f;
		V->Vitals.BloodVolume = 5000.0f;
		V->Vitals.HeartRate = 72.0f;
		V->Vitals.SpO2 = 98.0f;
		V->Vitals.RespiratoryRate = 14.0f;
		return V;
	}

	struct FFailureLimits
	{
		float Arrest = 300.0f, Resp = 240.0f;
		FFailureLimits()
		{
			if (IConsoleVariable* A = IConsoleManager::Get().FindConsoleVariable(TEXT("mo.Medical.CardiacArrestDeathSeconds"))) { Arrest = A->GetFloat(); }
			if (IConsoleVariable* R = IConsoleManager::Get().FindConsoleVariable(TEXT("mo.Medical.RespiratoryFailureDeathSeconds"))) { Resp = R->GetFloat(); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOVitals_SystemicFailure_SustainedAndReversible,
	"MOFramework.Medical.Vitals.SystemicFailure.SustainedAndReversible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOVitals_SystemicFailure_SustainedAndReversible::RunTest(const FString& Parameters)
{
	const FFailureLimits L;

	// A healthy body never fails, however long it runs.
	UMOVitalsComponent* Healthy = HealthyVitals();
	EMOSystemicFailure Any = EMOSystemicFailure::None;
	for (int32 I = 0; I < 100; ++I)
	{
		Any = Healthy->StepSystemicFailure(60.0f);
		if (Any != EMOSystemicFailure::None) { break; }
	}
	TestEqual(TEXT("a healthy body runs 100 minutes without a fatal failure"), Any, EMOSystemicFailure::None);

	// 24 % of the blood left: circulatory failure. Not fatal before the limit; recovery resets the clock.
	UMOVitalsComponent* V = HealthyVitals();
	V->Vitals.BloodVolume = 1200.0f;
	TestEqual(TEXT("halfway through the window: alive"), V->StepSystemicFailure(L.Arrest * 0.5f), EMOSystemicFailure::None);
	TestTrue(TEXT("the circulatory clock runs"), V->GetCirculatoryFailureSeconds() > 0.0f);
	V->Vitals.BloodVolume = 4000.0f;   // a transfusion
	TestEqual(TEXT("recovered: alive"), V->StepSystemicFailure(1.0f), EMOSystemicFailure::None);
	TestEqual(TEXT("recovery resets the clock"), V->GetCirculatoryFailureSeconds(), 0.0f);
	V->Vitals.BloodVolume = 1200.0f;
	TestEqual(TEXT("bleeding again, a fresh window: not fatal just short of the limit"), V->StepSystemicFailure(L.Arrest - 1.0f), EMOSystemicFailure::None);
	TestEqual(TEXT("sustained past the limit: fatal, and it was the blood"), V->StepSystemicFailure(2.0f), EMOSystemicFailure::Exsanguination);
	TestEqual(TEXT("fatal only once"), V->StepSystemicFailure(60.0f), EMOSystemicFailure::None);
	TestEqual(TEXT("the fatal cause is kept"), V->GetFatalSystemicFailure(), EMOSystemicFailure::Exsanguination);

	// The heart stops with the blood still there: cardiac arrest, not a bleed-out.
	UMOVitalsComponent* Heart = HealthyVitals();
	Heart->Vitals.HeartRate = 4.0f;
	Heart->StepSystemicFailure(L.Arrest - 1.0f);
	TestEqual(TEXT("asystole with full blood volume is a cardiac arrest"), Heart->StepSystemicFailure(2.0f), EMOSystemicFailure::CardiacArrest);

	// No oxygen: respiratory failure on its own (shorter) clock.
	UMOVitalsComponent* Lungs = HealthyVitals();
	Lungs->Vitals.SpO2 = 42.0f;
	TestEqual(TEXT("hypoxaemia: alive just short of its limit"), Lungs->StepSystemicFailure(L.Resp - 1.0f), EMOSystemicFailure::None);
	TestEqual(TEXT("hypoxaemia sustained past its limit is a respiratory failure"), Lungs->StepSystemicFailure(2.0f), EMOSystemicFailure::RespiratoryFailure);

	// Heat stroke (N-063, Oct 20 2026): a dose of minutes at 42 C, each degree above doubling it; cooling under 41 C resets it.
	float HeatLimitMin = 60.0f;
	if (IConsoleVariable* H = IConsoleManager::Get().FindConsoleVariable(TEXT("mo.Medical.HeatStrokeDeathMinutesAt42"))) { HeatLimitMin = H->GetFloat(); }
	TestEqual(TEXT("no heat dose under 42 C"), UMOVitalsComponent::HeatDoseRatePerMinute(41.9f), 0.0f);
	TestEqual(TEXT("42 C: one minute's dose a minute"), UMOVitalsComponent::HeatDoseRatePerMinute(42.0f), 1.0f);
	TestEqual(TEXT("43 C: twice as fast"), UMOVitalsComponent::HeatDoseRatePerMinute(43.0f), 2.0f);
	UMOVitalsComponent* Hot = HealthyVitals();
	Hot->Vitals.BodyTemperature = 42.0f;
	TestEqual(TEXT("42 C: alive just short of the dose"), Hot->StepSystemicFailure((HeatLimitMin - 1.0f) * 60.0f), EMOSystemicFailure::None);
	Hot->Vitals.BodyTemperature = 40.5f;   // cooled
	TestEqual(TEXT("cooled under 41 C: alive"), Hot->StepSystemicFailure(60.0f), EMOSystemicFailure::None);
	TestEqual(TEXT("... and the dose is reset"), Hot->GetHeatStrokeDoseMinutes(), 0.0f);
	Hot->Vitals.BodyTemperature = 43.0f;
	TestEqual(TEXT("43 C: alive just short of half the time"), Hot->StepSystemicFailure((HeatLimitMin * 0.5f - 1.0f) * 60.0f), EMOSystemicFailure::None);
	TestEqual(TEXT("43 C for half the time is fatal heat stroke"), Hot->StepSystemicFailure(120.0f), EMOSystemicFailure::HeatStroke);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
