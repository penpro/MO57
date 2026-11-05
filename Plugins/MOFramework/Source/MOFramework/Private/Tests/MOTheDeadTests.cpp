#include "Misc/AutomationTest.h"
#include "MOCharacter.h"
#include "MOColonyManagerSubsystem.h"
#include "MOIdentityComponent.h"
#include "MORecruitmentComponent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The same flag EnterDeadState sets; the real path needs a world with a mesh and a movement component. */
	bool MarkDead(FAutomationTestBase& Test, AMOCharacter* Someone)
	{
		const FBoolProperty* Dead = FindFProperty<FBoolProperty>(AMOCharacter::StaticClass(), TEXT("bIsDead"));
		if (!Test.TestNotNull(TEXT("the character has its dead flag"), Dead))
		{
			return false;
		}
		Dead->SetPropertyValue_InContainer(Someone, true);
		return true;
	}
}

// The dead stayed on the roster (N-303): GetColonyRoster asked only for the recruited state, which a death never changed, so a corpse was courted, kept its bed and showed "Idle".
// Expected: the same colonist, dead, is off it; failing before: IsPossessable alone said yes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColonyRosterIsTheLiving, "MOFramework.Colony.RosterIsTheLiving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOColonyRosterIsTheLiving::RunTest(const FString& Parameters)
{
	AMOCharacter* Someone = NewObject<AMOCharacter>(GetTransientPackage());
	UMOIdentityComponent* Identity = Someone->FindComponentByClass<UMOIdentityComponent>();
	UMORecruitmentComponent* Recruit = Someone->FindComponentByClass<UMORecruitmentComponent>();
	if (!TestNotNull(TEXT("a character has an identity"), Identity) || !TestNotNull(TEXT("and a recruitment"), Recruit))
	{
		return false;
	}
	Identity->SetGuid(FGuid::NewGuid());
	TestFalse(TEXT("a stranger is no colonist"), UMOColonyManagerSubsystem::IsLivingColonist(Someone));
	Recruit->ForceRecruit();
	TestTrue(TEXT("recruited and alive: on the roster (the control)"), UMOColonyManagerSubsystem::IsLivingColonist(Someone));
	if (MarkDead(*this, Someone))
	{
		TestTrue(TEXT("the recruited state stands at a death"), Recruit->IsPossessable());
		TestFalse(TEXT("the same colonist, dead: off the roster"), UMOColonyManagerSubsystem::IsLivingColonist(Someone));
	}
	TestFalse(TEXT("nobody is on no roster"), UMOColonyManagerSubsystem::IsLivingColonist(nullptr));
	return true;
}

// A stranger's corpse could be recruited (N-304): the Dead state ResolveInteraction tests for is set only when an ask times out.
// Expected: "Unavailable" ("... is dead.") and the state unchanged; failing before: no quest, so ForceRecruit, and the corpse joined.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOColonyTheDeadCannotBeRecruited, "MOFramework.Colony.TheDeadCannotBeRecruited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOColonyTheDeadCannotBeRecruited::RunTest(const FString& Parameters)
{
	AMOCharacter* Stranger = NewObject<AMOCharacter>(GetTransientPackage());
	AMOCharacter* Player = NewObject<AMOCharacter>(GetTransientPackage());
	UMORecruitmentComponent* Recruit = Stranger->FindComponentByClass<UMORecruitmentComponent>();
	if (!TestNotNull(TEXT("a character has a recruitment"), Recruit) || !MarkDead(*this, Stranger))
	{
		return false;
	}
	const FMORecruitmentResult Result = Recruit->BeginInteractionDetailed(Player);
	TestEqual(TEXT("a corpse answers that it is dead"), static_cast<int32>(Result.Outcome), static_cast<int32>(EMORecruitmentOutcome::Unavailable));
	TestFalse(TEXT("and does not join"), Recruit->IsPossessable());
	return true;
}

#endif
