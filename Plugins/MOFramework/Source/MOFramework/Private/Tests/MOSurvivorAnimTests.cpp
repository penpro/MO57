/**
 * The survivor's animation setup that lives outside code (Oct 27 2026; Wes: "the leg animations are broken now that the new character work
 * has been done ... something to do with the foot IKs").
 * Run: python Tools/ue.py auto --filter "MOFramework.Animation.FootIKVirtualBones"
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Animation/Skeleton.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOAnimation_FootIKVirtualBones,
	"MOFramework.Animation.FootIKVirtualBones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOAnimation_FootIKVirtualBones::RunTest(const FString& Parameters)
{
	// The foot IK rig (CR_MH_F, run by ABP_MH_F) plants the feet on these: a project addition the MetaHuman Creator's own skeleton does not
	// have, so a refresh of /Game/MetaHumans/Common wipes them and the legs stop walking (Oct 27). Put back with
	// Content/Python/ensure_foot_ik_virtual_bones.py.
	const USkeleton* Skeleton = LoadObject<USkeleton>(nullptr,
		TEXT("/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel.metahuman_base_skel"));
	if (!TestNotNull(TEXT("the MetaHuman body skeleton loads"), Skeleton))
	{
		return false;
	}
	for (const TCHAR* Name : { TEXT("VB root_root"), TEXT("VB VB root_root_foot_l"), TEXT("VB VB root_root_foot_r") })
	{
		TestTrue(FString::Printf(TEXT("the skeleton carries %s (run Content/Python/ensure_foot_ik_virtual_bones.py)"), Name),
			Skeleton->GetVirtualBones().ContainsByPredicate([Name](const FVirtualBone& Bone) { return Bone.VirtualBoneName == FName(Name); }));
	}
	return true;
}

#endif
