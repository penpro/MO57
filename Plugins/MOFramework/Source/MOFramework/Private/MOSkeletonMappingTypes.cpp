#include "MOSkeletonMappingTypes.h"
#include "Animation/Skeleton.h"

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

TMap<FName, FMOBoneMapping> UMOSkeletonMapping::UE5MannequinMap;
TMap<FName, FMOBoneMapping> UMOSkeletonMapping::MetaHumanMap;
bool UMOSkeletonMapping::bMappingsInitialized = false;

// ============================================================================
// FMOSkeletonMappingConfig Implementation
// ============================================================================

EMOBodyPartType FMOSkeletonMappingConfig::GetBodyPartForBone(FName BoneName) const
{
	// Try exact match first
	if (const FMOBoneMapping* Mapping = FindMapping(BoneName))
	{
		return Mapping->BodyPart;
	}

	// Try fuzzy matching for common patterns
	FString BoneStr = BoneName.ToString().ToLower();

	// Head region
	if (BoneStr.Contains(TEXT("head"))) return EMOBodyPartType::Head;
	if (BoneStr.Contains(TEXT("neck"))) return EMOBodyPartType::SpineCervical;

	// Spine
	if (BoneStr.Contains(TEXT("spine")))
	{
		if (BoneStr.Contains(TEXT("01")) || BoneStr.Contains(TEXT("02")))
			return EMOBodyPartType::SpineLumbar;
		return EMOBodyPartType::SpineThoracic;
	}
	if (BoneStr.Contains(TEXT("pelvis"))) return EMOBodyPartType::SpineLumbar;

	// Arms - Left
	if (BoneStr.Contains(TEXT("clavicle_l"))) return EMOBodyPartType::ShoulderLeft;
	if (BoneStr.Contains(TEXT("upperarm_l"))) return EMOBodyPartType::UpperArmLeft;
	if (BoneStr.Contains(TEXT("lowerarm_l"))) return EMOBodyPartType::ForearmLeft;
	if (BoneStr.Contains(TEXT("hand_l"))) return EMOBodyPartType::HandLeft;

	// Arms - Right
	if (BoneStr.Contains(TEXT("clavicle_r"))) return EMOBodyPartType::ShoulderRight;
	if (BoneStr.Contains(TEXT("upperarm_r"))) return EMOBodyPartType::UpperArmRight;
	if (BoneStr.Contains(TEXT("lowerarm_r"))) return EMOBodyPartType::ForearmRight;
	if (BoneStr.Contains(TEXT("hand_r"))) return EMOBodyPartType::HandRight;

	// Legs - Left
	if (BoneStr.Contains(TEXT("thigh_l"))) return EMOBodyPartType::ThighLeft;
	if (BoneStr.Contains(TEXT("calf_l"))) return EMOBodyPartType::CalfLeft;
	if (BoneStr.Contains(TEXT("foot_l"))) return EMOBodyPartType::FootLeft;

	// Legs - Right
	if (BoneStr.Contains(TEXT("thigh_r"))) return EMOBodyPartType::ThighRight;
	if (BoneStr.Contains(TEXT("calf_r"))) return EMOBodyPartType::CalfRight;
	if (BoneStr.Contains(TEXT("foot_r"))) return EMOBodyPartType::FootRight;

	// Fingers - Left
	if (BoneStr.Contains(TEXT("thumb")) && BoneStr.Contains(TEXT("_l"))) return EMOBodyPartType::ThumbLeft;
	if (BoneStr.Contains(TEXT("index")) && BoneStr.Contains(TEXT("_l"))) return EMOBodyPartType::IndexFingerLeft;
	if (BoneStr.Contains(TEXT("middle")) && BoneStr.Contains(TEXT("_l"))) return EMOBodyPartType::MiddleFingerLeft;
	if (BoneStr.Contains(TEXT("ring")) && BoneStr.Contains(TEXT("_l"))) return EMOBodyPartType::RingFingerLeft;
	if (BoneStr.Contains(TEXT("pinky")) && BoneStr.Contains(TEXT("_l"))) return EMOBodyPartType::PinkyFingerLeft;

	// Fingers - Right
	if (BoneStr.Contains(TEXT("thumb")) && BoneStr.Contains(TEXT("_r"))) return EMOBodyPartType::ThumbRight;
	if (BoneStr.Contains(TEXT("index")) && BoneStr.Contains(TEXT("_r"))) return EMOBodyPartType::IndexFingerRight;
	if (BoneStr.Contains(TEXT("middle")) && BoneStr.Contains(TEXT("_r"))) return EMOBodyPartType::MiddleFingerRight;
	if (BoneStr.Contains(TEXT("ring")) && BoneStr.Contains(TEXT("_r"))) return EMOBodyPartType::RingFingerRight;
	if (BoneStr.Contains(TEXT("pinky")) && BoneStr.Contains(TEXT("_r"))) return EMOBodyPartType::PinkyFingerRight;

	return DefaultBodyPart;
}

// ============================================================================
// UMOSkeletonMapping - Initialization
// ============================================================================

void UMOSkeletonMapping::EnsureMappingsInitialized()
{
	if (bMappingsInitialized)
	{
		return;
	}

	// ========================================================================
	// UE5 MANNEQUIN MAPPINGS (89 bones)
	// ========================================================================

	auto AddUE5Mapping = [](FName Bone, EMOBodyPartType Part, EMOBodyRegion Region, int32 Priority = 0)
	{
		UE5MannequinMap.Add(Bone, FMOBoneMapping(Bone, Part, Region, Priority));
	};

	// ROOT & PELVIS
	AddUE5Mapping(TEXT("root"), EMOBodyPartType::SpineLumbar, EMOBodyRegion::Torso);
	AddUE5Mapping(TEXT("pelvis"), EMOBodyPartType::SpineLumbar, EMOBodyRegion::Torso);

	// SPINE
	AddUE5Mapping(TEXT("spine_01"), EMOBodyPartType::SpineLumbar, EMOBodyRegion::Torso);
	AddUE5Mapping(TEXT("spine_02"), EMOBodyPartType::SpineLumbar, EMOBodyRegion::Torso);
	AddUE5Mapping(TEXT("spine_03"), EMOBodyPartType::SpineThoracic, EMOBodyRegion::Torso);
	AddUE5Mapping(TEXT("spine_04"), EMOBodyPartType::SpineThoracic, EMOBodyRegion::Torso);
	AddUE5Mapping(TEXT("spine_05"), EMOBodyPartType::SpineThoracic, EMOBodyRegion::Torso);

	// NECK & HEAD
	AddUE5Mapping(TEXT("neck_01"), EMOBodyPartType::SpineCervical, EMOBodyRegion::Head);
	AddUE5Mapping(TEXT("neck_02"), EMOBodyPartType::SpineCervical, EMOBodyRegion::Head);
	AddUE5Mapping(TEXT("head"), EMOBodyPartType::Head, EMOBodyRegion::Head, 10);

	// LEFT ARM
	AddUE5Mapping(TEXT("clavicle_l"), EMOBodyPartType::ShoulderLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("upperarm_l"), EMOBodyPartType::UpperArmLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("upperarm_twist_01_l"), EMOBodyPartType::UpperArmLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("lowerarm_l"), EMOBodyPartType::ForearmLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("lowerarm_twist_01_l"), EMOBodyPartType::ForearmLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("hand_l"), EMOBodyPartType::HandLeft, EMOBodyRegion::LeftArm);

	// LEFT HAND FINGERS
	AddUE5Mapping(TEXT("thumb_01_l"), EMOBodyPartType::ThumbLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("thumb_02_l"), EMOBodyPartType::ThumbLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("thumb_03_l"), EMOBodyPartType::ThumbLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("index_metacarpal_l"), EMOBodyPartType::HandLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("index_01_l"), EMOBodyPartType::IndexFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("index_02_l"), EMOBodyPartType::IndexFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("index_03_l"), EMOBodyPartType::IndexFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("middle_metacarpal_l"), EMOBodyPartType::HandLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("middle_01_l"), EMOBodyPartType::MiddleFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("middle_02_l"), EMOBodyPartType::MiddleFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("middle_03_l"), EMOBodyPartType::MiddleFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("ring_metacarpal_l"), EMOBodyPartType::HandLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("ring_01_l"), EMOBodyPartType::RingFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("ring_02_l"), EMOBodyPartType::RingFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("ring_03_l"), EMOBodyPartType::RingFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("pinky_metacarpal_l"), EMOBodyPartType::HandLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("pinky_01_l"), EMOBodyPartType::PinkyFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("pinky_02_l"), EMOBodyPartType::PinkyFingerLeft, EMOBodyRegion::LeftArm);
	AddUE5Mapping(TEXT("pinky_03_l"), EMOBodyPartType::PinkyFingerLeft, EMOBodyRegion::LeftArm);

	// RIGHT ARM
	AddUE5Mapping(TEXT("clavicle_r"), EMOBodyPartType::ShoulderRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("upperarm_r"), EMOBodyPartType::UpperArmRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("upperarm_twist_01_r"), EMOBodyPartType::UpperArmRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("lowerarm_r"), EMOBodyPartType::ForearmRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("lowerarm_twist_01_r"), EMOBodyPartType::ForearmRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("hand_r"), EMOBodyPartType::HandRight, EMOBodyRegion::RightArm);

	// RIGHT HAND FINGERS
	AddUE5Mapping(TEXT("thumb_01_r"), EMOBodyPartType::ThumbRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("thumb_02_r"), EMOBodyPartType::ThumbRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("thumb_03_r"), EMOBodyPartType::ThumbRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("index_metacarpal_r"), EMOBodyPartType::HandRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("index_01_r"), EMOBodyPartType::IndexFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("index_02_r"), EMOBodyPartType::IndexFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("index_03_r"), EMOBodyPartType::IndexFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("middle_metacarpal_r"), EMOBodyPartType::HandRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("middle_01_r"), EMOBodyPartType::MiddleFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("middle_02_r"), EMOBodyPartType::MiddleFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("middle_03_r"), EMOBodyPartType::MiddleFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("ring_metacarpal_r"), EMOBodyPartType::HandRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("ring_01_r"), EMOBodyPartType::RingFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("ring_02_r"), EMOBodyPartType::RingFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("ring_03_r"), EMOBodyPartType::RingFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("pinky_metacarpal_r"), EMOBodyPartType::HandRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("pinky_01_r"), EMOBodyPartType::PinkyFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("pinky_02_r"), EMOBodyPartType::PinkyFingerRight, EMOBodyRegion::RightArm);
	AddUE5Mapping(TEXT("pinky_03_r"), EMOBodyPartType::PinkyFingerRight, EMOBodyRegion::RightArm);

	// LEFT LEG
	AddUE5Mapping(TEXT("thigh_l"), EMOBodyPartType::ThighLeft, EMOBodyRegion::LeftLeg);
	AddUE5Mapping(TEXT("thigh_twist_01_l"), EMOBodyPartType::ThighLeft, EMOBodyRegion::LeftLeg);
	AddUE5Mapping(TEXT("calf_l"), EMOBodyPartType::CalfLeft, EMOBodyRegion::LeftLeg);
	AddUE5Mapping(TEXT("calf_twist_01_l"), EMOBodyPartType::CalfLeft, EMOBodyRegion::LeftLeg);
	AddUE5Mapping(TEXT("foot_l"), EMOBodyPartType::FootLeft, EMOBodyRegion::LeftLeg);
	AddUE5Mapping(TEXT("ball_l"), EMOBodyPartType::FootLeft, EMOBodyRegion::LeftLeg);

	// RIGHT LEG
	AddUE5Mapping(TEXT("thigh_r"), EMOBodyPartType::ThighRight, EMOBodyRegion::RightLeg);
	AddUE5Mapping(TEXT("thigh_twist_01_r"), EMOBodyPartType::ThighRight, EMOBodyRegion::RightLeg);
	AddUE5Mapping(TEXT("calf_r"), EMOBodyPartType::CalfRight, EMOBodyRegion::RightLeg);
	AddUE5Mapping(TEXT("calf_twist_01_r"), EMOBodyPartType::CalfRight, EMOBodyRegion::RightLeg);
	AddUE5Mapping(TEXT("foot_r"), EMOBodyPartType::FootRight, EMOBodyRegion::RightLeg);
	AddUE5Mapping(TEXT("ball_r"), EMOBodyPartType::FootRight, EMOBodyRegion::RightLeg);

	// ========================================================================
	// METAHUMAN ADDITIONAL MAPPINGS
	// ========================================================================
	// MetaHuman includes all UE5 Mannequin bones plus many additional ones

	// Copy all UE5 mappings to MetaHuman
	MetaHumanMap = UE5MannequinMap;

	auto AddMetaHumanMapping = [](FName Bone, EMOBodyPartType Part, EMOBodyRegion Region, int32 Priority = 0)
	{
		MetaHumanMap.Add(Bone, FMOBoneMapping(Bone, Part, Region, Priority));
	};

	// FACE BONES (MetaHuman specific)
	AddMetaHumanMapping(TEXT("jaw"), EMOBodyPartType::Jaw, EMOBodyRegion::Head);
	AddMetaHumanMapping(TEXT("FACIAL_C_Jaw"), EMOBodyPartType::Jaw, EMOBodyRegion::Head);
	AddMetaHumanMapping(TEXT("FACIAL_L_Eye"), EMOBodyPartType::EyeLeft, EMOBodyRegion::Head, 5);
	AddMetaHumanMapping(TEXT("FACIAL_R_Eye"), EMOBodyPartType::EyeRight, EMOBodyRegion::Head, 5);
	AddMetaHumanMapping(TEXT("FACIAL_L_Ear"), EMOBodyPartType::EarLeft, EMOBodyRegion::Head);
	AddMetaHumanMapping(TEXT("FACIAL_R_Ear"), EMOBodyPartType::EarRight, EMOBodyRegion::Head);

	// Additional twist bones
	AddMetaHumanMapping(TEXT("upperarm_twist_02_l"), EMOBodyPartType::UpperArmLeft, EMOBodyRegion::LeftArm);
	AddMetaHumanMapping(TEXT("upperarm_twist_02_r"), EMOBodyPartType::UpperArmRight, EMOBodyRegion::RightArm);
	AddMetaHumanMapping(TEXT("lowerarm_twist_02_l"), EMOBodyPartType::ForearmLeft, EMOBodyRegion::LeftArm);
	AddMetaHumanMapping(TEXT("lowerarm_twist_02_r"), EMOBodyPartType::ForearmRight, EMOBodyRegion::RightArm);
	AddMetaHumanMapping(TEXT("thigh_twist_02_l"), EMOBodyPartType::ThighLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("thigh_twist_02_r"), EMOBodyPartType::ThighRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("calf_twist_02_l"), EMOBodyPartType::CalfLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("calf_twist_02_r"), EMOBodyPartType::CalfRight, EMOBodyRegion::RightLeg);

	// Toe bones (MetaHuman has more detailed toes)
	AddMetaHumanMapping(TEXT("bigtoe_01_l"), EMOBodyPartType::BigToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("bigtoe_02_l"), EMOBodyPartType::BigToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("indextoe_01_l"), EMOBodyPartType::SecondToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("indextoe_02_l"), EMOBodyPartType::SecondToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("middletoe_01_l"), EMOBodyPartType::ThirdToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("middletoe_02_l"), EMOBodyPartType::ThirdToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("ringtoe_01_l"), EMOBodyPartType::FourthToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("ringtoe_02_l"), EMOBodyPartType::FourthToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("pinkytoe_01_l"), EMOBodyPartType::PinkyToeLeft, EMOBodyRegion::LeftLeg);
	AddMetaHumanMapping(TEXT("pinkytoe_02_l"), EMOBodyPartType::PinkyToeLeft, EMOBodyRegion::LeftLeg);

	AddMetaHumanMapping(TEXT("bigtoe_01_r"), EMOBodyPartType::BigToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("bigtoe_02_r"), EMOBodyPartType::BigToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("indextoe_01_r"), EMOBodyPartType::SecondToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("indextoe_02_r"), EMOBodyPartType::SecondToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("middletoe_01_r"), EMOBodyPartType::ThirdToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("middletoe_02_r"), EMOBodyPartType::ThirdToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("ringtoe_01_r"), EMOBodyPartType::FourthToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("ringtoe_02_r"), EMOBodyPartType::FourthToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("pinkytoe_01_r"), EMOBodyPartType::PinkyToeRight, EMOBodyRegion::RightLeg);
	AddMetaHumanMapping(TEXT("pinkytoe_02_r"), EMOBodyPartType::PinkyToeRight, EMOBodyRegion::RightLeg);

	bMappingsInitialized = true;
}

// ============================================================================
// UMOSkeletonMapping - Mapping Lookup
// ============================================================================

EMOBodyPartType UMOSkeletonMapping::BoneToBodyPart(FName BoneName)
{
	EnsureMappingsInitialized();

	// Try MetaHuman map first (superset of UE5 Mannequin)
	if (const FMOBoneMapping* Mapping = MetaHumanMap.Find(BoneName))
	{
		return Mapping->BodyPart;
	}

	// Fuzzy match as fallback
	FString BoneStr = BoneName.ToString().ToLower();

	// Head
	if (BoneStr.Contains(TEXT("head"))) return EMOBodyPartType::Head;
	if (BoneStr.Contains(TEXT("jaw"))) return EMOBodyPartType::Jaw;
	if (BoneStr.Contains(TEXT("eye_l")) || BoneStr.Contains(TEXT("l_eye"))) return EMOBodyPartType::EyeLeft;
	if (BoneStr.Contains(TEXT("eye_r")) || BoneStr.Contains(TEXT("r_eye"))) return EMOBodyPartType::EyeRight;
	if (BoneStr.Contains(TEXT("neck"))) return EMOBodyPartType::SpineCervical;

	// Spine
	if (BoneStr.Contains(TEXT("spine")) || BoneStr.Contains(TEXT("pelvis")))
	{
		if (BoneStr.Contains(TEXT("01")) || BoneStr.Contains(TEXT("02")) || BoneStr.Contains(TEXT("pelvis")))
			return EMOBodyPartType::SpineLumbar;
		return EMOBodyPartType::SpineThoracic;
	}

	// Arms
	if (BoneStr.Contains(TEXT("clavicle")) || BoneStr.Contains(TEXT("shoulder")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::ShoulderLeft : EMOBodyPartType::ShoulderRight;
	if (BoneStr.Contains(TEXT("upperarm")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::UpperArmLeft : EMOBodyPartType::UpperArmRight;
	if (BoneStr.Contains(TEXT("lowerarm")) || BoneStr.Contains(TEXT("forearm")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::ForearmLeft : EMOBodyPartType::ForearmRight;
	if (BoneStr.Contains(TEXT("hand")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::HandLeft : EMOBodyPartType::HandRight;

	// Fingers
	if (BoneStr.Contains(TEXT("thumb")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::ThumbLeft : EMOBodyPartType::ThumbRight;
	if (BoneStr.Contains(TEXT("index")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::IndexFingerLeft : EMOBodyPartType::IndexFingerRight;
	if (BoneStr.Contains(TEXT("middle")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::MiddleFingerLeft : EMOBodyPartType::MiddleFingerRight;
	if (BoneStr.Contains(TEXT("ring")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::RingFingerLeft : EMOBodyPartType::RingFingerRight;
	if (BoneStr.Contains(TEXT("pinky")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::PinkyFingerLeft : EMOBodyPartType::PinkyFingerRight;

	// Legs
	if (BoneStr.Contains(TEXT("thigh")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::ThighLeft : EMOBodyPartType::ThighRight;
	if (BoneStr.Contains(TEXT("calf")) || BoneStr.Contains(TEXT("shin")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::CalfLeft : EMOBodyPartType::CalfRight;
	if (BoneStr.Contains(TEXT("foot")) || BoneStr.Contains(TEXT("ball")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::FootLeft : EMOBodyPartType::FootRight;

	// Toes
	if (BoneStr.Contains(TEXT("bigtoe")))
		return BoneStr.Contains(TEXT("_l")) ? EMOBodyPartType::BigToeLeft : EMOBodyPartType::BigToeRight;

	// Default to torso
	return EMOBodyPartType::Torso;
}

EMOBodyRegion UMOSkeletonMapping::BoneToBodyRegion(FName BoneName)
{
	EnsureMappingsInitialized();

	if (const FMOBoneMapping* Mapping = MetaHumanMap.Find(BoneName))
	{
		return Mapping->Region;
	}

	// Fuzzy match
	FString BoneStr = BoneName.ToString().ToLower();

	if (BoneStr.Contains(TEXT("head")) || BoneStr.Contains(TEXT("neck")) ||
		BoneStr.Contains(TEXT("jaw")) || BoneStr.Contains(TEXT("eye")) ||
		BoneStr.Contains(TEXT("ear")))
	{
		return EMOBodyRegion::Head;
	}

	if (BoneStr.Contains(TEXT("_l")))
	{
		if (BoneStr.Contains(TEXT("arm")) || BoneStr.Contains(TEXT("hand")) ||
			BoneStr.Contains(TEXT("clavicle")) || BoneStr.Contains(TEXT("thumb")) ||
			BoneStr.Contains(TEXT("index")) || BoneStr.Contains(TEXT("middle")) ||
			BoneStr.Contains(TEXT("ring")) || BoneStr.Contains(TEXT("pinky")))
		{
			return EMOBodyRegion::LeftArm;
		}
		if (BoneStr.Contains(TEXT("thigh")) || BoneStr.Contains(TEXT("calf")) ||
			BoneStr.Contains(TEXT("foot")) || BoneStr.Contains(TEXT("toe")))
		{
			return EMOBodyRegion::LeftLeg;
		}
	}

	if (BoneStr.Contains(TEXT("_r")))
	{
		if (BoneStr.Contains(TEXT("arm")) || BoneStr.Contains(TEXT("hand")) ||
			BoneStr.Contains(TEXT("clavicle")) || BoneStr.Contains(TEXT("thumb")) ||
			BoneStr.Contains(TEXT("index")) || BoneStr.Contains(TEXT("middle")) ||
			BoneStr.Contains(TEXT("ring")) || BoneStr.Contains(TEXT("pinky")))
		{
			return EMOBodyRegion::RightArm;
		}
		if (BoneStr.Contains(TEXT("thigh")) || BoneStr.Contains(TEXT("calf")) ||
			BoneStr.Contains(TEXT("foot")) || BoneStr.Contains(TEXT("toe")))
		{
			return EMOBodyRegion::RightLeg;
		}
	}

	return EMOBodyRegion::Torso;
}

TArray<FName> UMOSkeletonMapping::GetBonesForBodyPart(EMOBodyPartType BodyPart)
{
	EnsureMappingsInitialized();

	TArray<FName> Result;
	for (const auto& Pair : MetaHumanMap)
	{
		if (Pair.Value.BodyPart == BodyPart)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

// ============================================================================
// UMOSkeletonMapping - Configuration Getters
// ============================================================================

FMOSkeletonMappingConfig UMOSkeletonMapping::GetUE5MannequinMapping()
{
	EnsureMappingsInitialized();

	FMOSkeletonMappingConfig Config;
	Config.SkeletonType = EMOSkeletonType::UE5Mannequin;

	for (const auto& Pair : UE5MannequinMap)
	{
		Config.Mappings.Add(Pair.Value);
	}

	return Config;
}

FMOSkeletonMappingConfig UMOSkeletonMapping::GetMetaHumanMapping()
{
	EnsureMappingsInitialized();

	FMOSkeletonMappingConfig Config;
	Config.SkeletonType = EMOSkeletonType::MetaHuman;

	for (const auto& Pair : MetaHumanMap)
	{
		Config.Mappings.Add(Pair.Value);
	}

	return Config;
}

EMOSkeletonType UMOSkeletonMapping::DetectSkeletonType(const USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		return EMOSkeletonType::Custom;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int32 BoneCount = RefSkeleton.GetNum();

	// MetaHuman has ~342 bones
	if (BoneCount > 200)
	{
		// Check for MetaHuman-specific bones
		if (RefSkeleton.FindBoneIndex(TEXT("FACIAL_C_Jaw")) != INDEX_NONE ||
			RefSkeleton.FindBoneIndex(TEXT("FACIAL_L_Eye")) != INDEX_NONE)
		{
			return EMOSkeletonType::MetaHuman;
		}
	}

	// UE5 Mannequin has ~89 bones
	if (BoneCount >= 70 && BoneCount <= 120)
	{
		// Check for UE5 Mannequin bones
		if (RefSkeleton.FindBoneIndex(TEXT("spine_05")) != INDEX_NONE)
		{
			return EMOSkeletonType::UE5Mannequin;
		}
	}

	return EMOSkeletonType::Custom;
}

// ============================================================================
// UMOSkeletonMapping - Physics Asset Helpers
// ============================================================================

void UMOSkeletonMapping::GetRecommendedPhysicsBody(
	EMOBodyPartType BodyPart,
	FString& OutPrimitiveType,
	FVector& OutDimensions)
{
	// Provide recommended physics body setup for hit detection
	// Dimensions are approximate and should be adjusted per character

	switch (BodyPart)
	{
	case EMOBodyPartType::Head:
		OutPrimitiveType = TEXT("Sphere");
		OutDimensions = FVector(12.0f, 0.0f, 0.0f);  // Radius
		break;

	case EMOBodyPartType::Torso:
	case EMOBodyPartType::SpineThoracic:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(18.0f, 25.0f, 0.0f);  // Radius, HalfHeight
		break;

	case EMOBodyPartType::SpineLumbar:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(15.0f, 12.0f, 0.0f);
		break;

	case EMOBodyPartType::SpineCervical:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(6.0f, 8.0f, 0.0f);
		break;

	case EMOBodyPartType::UpperArmLeft:
	case EMOBodyPartType::UpperArmRight:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(5.0f, 15.0f, 0.0f);
		break;

	case EMOBodyPartType::ForearmLeft:
	case EMOBodyPartType::ForearmRight:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(4.0f, 12.0f, 0.0f);
		break;

	case EMOBodyPartType::HandLeft:
	case EMOBodyPartType::HandRight:
		OutPrimitiveType = TEXT("Box");
		OutDimensions = FVector(4.0f, 8.0f, 2.0f);  // HalfExtent
		break;

	case EMOBodyPartType::ThighLeft:
	case EMOBodyPartType::ThighRight:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(8.0f, 22.0f, 0.0f);
		break;

	case EMOBodyPartType::CalfLeft:
	case EMOBodyPartType::CalfRight:
		OutPrimitiveType = TEXT("Capsule");
		OutDimensions = FVector(6.0f, 20.0f, 0.0f);
		break;

	case EMOBodyPartType::FootLeft:
	case EMOBodyPartType::FootRight:
		OutPrimitiveType = TEXT("Box");
		OutDimensions = FVector(5.0f, 12.0f, 3.0f);
		break;

	default:
		// Fingers, toes, and other small parts
		OutPrimitiveType = TEXT("Sphere");
		OutDimensions = FVector(2.0f, 0.0f, 0.0f);
		break;
	}
}

EMOBodyPartType UMOSkeletonMapping::GetParentBodyPart(EMOBodyPartType BodyPart)
{
	switch (BodyPart)
	{
	// Head hierarchy
	case EMOBodyPartType::Brain:
	case EMOBodyPartType::EyeLeft:
	case EMOBodyPartType::EyeRight:
	case EMOBodyPartType::EarLeft:
	case EMOBodyPartType::EarRight:
	case EMOBodyPartType::Jaw:
		return EMOBodyPartType::Head;

	case EMOBodyPartType::Head:
		return EMOBodyPartType::SpineCervical;

	// Spine hierarchy
	case EMOBodyPartType::SpineCervical:
		return EMOBodyPartType::SpineThoracic;
	case EMOBodyPartType::SpineThoracic:
		return EMOBodyPartType::Torso;
	case EMOBodyPartType::SpineLumbar:
		return EMOBodyPartType::Torso;

	// Organs -> Torso
	case EMOBodyPartType::Heart:
	case EMOBodyPartType::LungLeft:
	case EMOBodyPartType::LungRight:
	case EMOBodyPartType::Liver:
	case EMOBodyPartType::Stomach:
	case EMOBodyPartType::Intestines:
	case EMOBodyPartType::KidneyLeft:
	case EMOBodyPartType::KidneyRight:
		return EMOBodyPartType::Torso;

	// Left arm hierarchy
	case EMOBodyPartType::ShoulderLeft:
		return EMOBodyPartType::Torso;
	case EMOBodyPartType::UpperArmLeft:
		return EMOBodyPartType::ShoulderLeft;
	case EMOBodyPartType::ElbowLeft:
	case EMOBodyPartType::ForearmLeft:
		return EMOBodyPartType::UpperArmLeft;
	case EMOBodyPartType::WristLeft:
	case EMOBodyPartType::HandLeft:
		return EMOBodyPartType::ForearmLeft;
	case EMOBodyPartType::ThumbLeft:
	case EMOBodyPartType::IndexFingerLeft:
	case EMOBodyPartType::MiddleFingerLeft:
	case EMOBodyPartType::RingFingerLeft:
	case EMOBodyPartType::PinkyFingerLeft:
		return EMOBodyPartType::HandLeft;

	// Right arm hierarchy (same structure)
	case EMOBodyPartType::ShoulderRight:
		return EMOBodyPartType::Torso;
	case EMOBodyPartType::UpperArmRight:
		return EMOBodyPartType::ShoulderRight;
	case EMOBodyPartType::ElbowRight:
	case EMOBodyPartType::ForearmRight:
		return EMOBodyPartType::UpperArmRight;
	case EMOBodyPartType::WristRight:
	case EMOBodyPartType::HandRight:
		return EMOBodyPartType::ForearmRight;
	case EMOBodyPartType::ThumbRight:
	case EMOBodyPartType::IndexFingerRight:
	case EMOBodyPartType::MiddleFingerRight:
	case EMOBodyPartType::RingFingerRight:
	case EMOBodyPartType::PinkyFingerRight:
		return EMOBodyPartType::HandRight;

	// Left leg hierarchy
	case EMOBodyPartType::HipLeft:
		return EMOBodyPartType::SpineLumbar;
	case EMOBodyPartType::ThighLeft:
		return EMOBodyPartType::HipLeft;
	case EMOBodyPartType::KneeLeft:
	case EMOBodyPartType::CalfLeft:
		return EMOBodyPartType::ThighLeft;
	case EMOBodyPartType::AnkleLeft:
	case EMOBodyPartType::FootLeft:
		return EMOBodyPartType::CalfLeft;
	case EMOBodyPartType::BigToeLeft:
	case EMOBodyPartType::SecondToeLeft:
	case EMOBodyPartType::ThirdToeLeft:
	case EMOBodyPartType::FourthToeLeft:
	case EMOBodyPartType::PinkyToeLeft:
		return EMOBodyPartType::FootLeft;

	// Right leg hierarchy (same structure)
	case EMOBodyPartType::HipRight:
		return EMOBodyPartType::SpineLumbar;
	case EMOBodyPartType::ThighRight:
		return EMOBodyPartType::HipRight;
	case EMOBodyPartType::KneeRight:
	case EMOBodyPartType::CalfRight:
		return EMOBodyPartType::ThighRight;
	case EMOBodyPartType::AnkleRight:
	case EMOBodyPartType::FootRight:
		return EMOBodyPartType::CalfRight;
	case EMOBodyPartType::BigToeRight:
	case EMOBodyPartType::SecondToeRight:
	case EMOBodyPartType::ThirdToeRight:
	case EMOBodyPartType::FourthToeRight:
	case EMOBodyPartType::PinkyToeRight:
		return EMOBodyPartType::FootRight;

	default:
		return EMOBodyPartType::None;
	}
}

TArray<EMOBodyPartType> UMOSkeletonMapping::GetChildBodyParts(EMOBodyPartType BodyPart)
{
	TArray<EMOBodyPartType> Children;

	// Iterate all body parts and find ones whose parent is BodyPart
	for (uint8 i = 0; i < static_cast<uint8>(EMOBodyPartType::MAX); ++i)
	{
		EMOBodyPartType Part = static_cast<EMOBodyPartType>(i);
		if (GetParentBodyPart(Part) == BodyPart)
		{
			Children.Add(Part);
		}
	}

	return Children;
}

// ============================================================================
// UMOSkeletonMapping - Vital Organ Queries
// ============================================================================

bool UMOSkeletonMapping::IsVitalOrgan(EMOBodyPartType BodyPart)
{
	switch (BodyPart)
	{
	case EMOBodyPartType::Brain:
	case EMOBodyPartType::Heart:
	case EMOBodyPartType::LungLeft:
	case EMOBodyPartType::LungRight:
		return true;
	default:
		return false;
	}
}

bool UMOSkeletonMapping::IsLimb(EMOBodyPartType BodyPart)
{
	switch (BodyPart)
	{
	// Arms
	case EMOBodyPartType::ShoulderLeft:
	case EMOBodyPartType::ShoulderRight:
	case EMOBodyPartType::UpperArmLeft:
	case EMOBodyPartType::UpperArmRight:
	case EMOBodyPartType::ElbowLeft:
	case EMOBodyPartType::ElbowRight:
	case EMOBodyPartType::ForearmLeft:
	case EMOBodyPartType::ForearmRight:
	case EMOBodyPartType::WristLeft:
	case EMOBodyPartType::WristRight:
	case EMOBodyPartType::HandLeft:
	case EMOBodyPartType::HandRight:
	// Legs
	case EMOBodyPartType::HipLeft:
	case EMOBodyPartType::HipRight:
	case EMOBodyPartType::ThighLeft:
	case EMOBodyPartType::ThighRight:
	case EMOBodyPartType::KneeLeft:
	case EMOBodyPartType::KneeRight:
	case EMOBodyPartType::CalfLeft:
	case EMOBodyPartType::CalfRight:
	case EMOBodyPartType::AnkleLeft:
	case EMOBodyPartType::AnkleRight:
	case EMOBodyPartType::FootLeft:
	case EMOBodyPartType::FootRight:
		return true;
	default:
		return false;
	}
}

bool UMOSkeletonMapping::IsExtremity(EMOBodyPartType BodyPart)
{
	switch (BodyPart)
	{
	// Fingers
	case EMOBodyPartType::ThumbLeft:
	case EMOBodyPartType::ThumbRight:
	case EMOBodyPartType::IndexFingerLeft:
	case EMOBodyPartType::IndexFingerRight:
	case EMOBodyPartType::MiddleFingerLeft:
	case EMOBodyPartType::MiddleFingerRight:
	case EMOBodyPartType::RingFingerLeft:
	case EMOBodyPartType::RingFingerRight:
	case EMOBodyPartType::PinkyFingerLeft:
	case EMOBodyPartType::PinkyFingerRight:
	// Toes
	case EMOBodyPartType::BigToeLeft:
	case EMOBodyPartType::BigToeRight:
	case EMOBodyPartType::SecondToeLeft:
	case EMOBodyPartType::SecondToeRight:
	case EMOBodyPartType::ThirdToeLeft:
	case EMOBodyPartType::ThirdToeRight:
	case EMOBodyPartType::FourthToeLeft:
	case EMOBodyPartType::FourthToeRight:
	case EMOBodyPartType::PinkyToeLeft:
	case EMOBodyPartType::PinkyToeRight:
		return true;
	default:
		return false;
	}
}
