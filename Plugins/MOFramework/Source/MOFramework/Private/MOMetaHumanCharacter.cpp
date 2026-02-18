#include "MOMetaHumanCharacter.h"
#include "MOFramework.h"
#include "MOAppearanceSubsystem.h"
#include "Components/SkeletalMeshComponent.h"

AMOMetaHumanCharacter::AMOMetaHumanCharacter()
{
	// Face mesh follows Body via Leader Pose
	FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FaceMesh"));
	FaceMesh->SetupAttachment(GetMesh());

	// Update hand socket names for MetaHuman skeleton
	// MetaHuman uses different socket naming convention
	LeftHandSocketName = TEXT("hand_l");
	RightHandSocketName = TEXT("hand_r");
}

void AMOMetaHumanCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSetupLeaderPose)
	{
		SetupLeaderPoseComponents();
	}

	if (bAutoApplyAppearance && CurrentAppearance.IsValid())
	{
		ApplyCurrentAppearance();
	}
}

void AMOMetaHumanCharacter::SetupLeaderPoseComponents()
{
	// The Body mesh (inherited Mesh component) is the leader
	// Face mesh follows the body's animation via SetLeaderPoseComponent
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (!BodyMesh)
	{
		return;
	}

	if (FaceMesh && FaceMesh->GetSkeletalMeshAsset())
	{
		FaceMesh->SetLeaderPoseComponent(BodyMesh);
	}

	UE_LOG(LogMOFramework, Log, TEXT("AMOMetaHumanCharacter: Set up Face mesh to follow Body via Leader Pose"));
}

void AMOMetaHumanCharacter::SetupMetaHumanMeshes(USkeletalMesh* BodyMesh, USkeletalMesh* FaceSkeletalMesh)
{
	if (BodyMesh)
	{
		GetMesh()->SetSkeletalMesh(BodyMesh);
		UE_LOG(LogMOFramework, Log, TEXT("AMOMetaHumanCharacter: Set Body mesh to %s"), *BodyMesh->GetName());
	}

	if (FaceSkeletalMesh && FaceMesh)
	{
		FaceMesh->SetSkeletalMesh(FaceSkeletalMesh);
		UE_LOG(LogMOFramework, Log, TEXT("AMOMetaHumanCharacter: Set Face mesh to %s"), *FaceSkeletalMesh->GetName());
	}

	// Re-establish leader pose relationship after mesh changes
	SetupLeaderPoseComponents();
}

void AMOMetaHumanCharacter::SetAppearance(const FMOCharacterAppearance& NewAppearance)
{
	CurrentAppearance = NewAppearance;
	ApplyCurrentAppearance();
}

void AMOMetaHumanCharacter::ApplyCurrentAppearance()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMOAppearanceSubsystem* AppearanceSubsystem = GI->GetSubsystem<UMOAppearanceSubsystem>())
		{
			AppearanceSubsystem->ApplyAppearance(this, CurrentAppearance);
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("AMOMetaHumanCharacter::ApplyCurrentAppearance: No AppearanceSubsystem found"));
		}
	}
}
