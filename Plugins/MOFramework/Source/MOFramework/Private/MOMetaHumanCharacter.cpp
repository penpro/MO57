#include "MOMetaHumanCharacter.h"
#include "MOFramework.h"
#include "MOAppearanceSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AMOMetaHumanCharacter::AMOMetaHumanCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

void AMOMetaHumanCharacter::SetAppearanceLook(const FMOCharacterAppearance& Look)
{
	CurrentAppearance.CopyLookFrom(Look);
	ApplyGrooms(CurrentAppearance);
	UMOAppearanceSubsystem::ApplyColorsToActor(this, CurrentAppearance);
}

bool AMOMetaHumanCharacter::ApplyGrooms(const FMOCharacterAppearance& Look)
{
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bGiveUp = GroomWaitStartSeconds >= 0.0 && Now - GroomWaitStartSeconds > GroomReadyTimeoutSeconds;
	bool bPending = false, bChanged = false;
	const bool bLoaded = UMOAppearanceSubsystem::ApplyGroomsToCharacter(this, Look, !bGiveUp, &bPending, &bChanged);
	if (bChanged)
	{
		UMOAppearanceSubsystem::ApplyColorsToActor(this, CurrentAppearance);
		if (GroomWaitStartSeconds >= 0.0)
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOAppearance] %s: groom swapped in after %.1f s of loading (the old one stayed on meanwhile)"), *GetName(), Now - GroomWaitStartSeconds);
		}
	}
	if (bGiveUp)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAppearance] %s: a groom was not ready after %.0f s -- swapped in anyway"), *GetName(), GroomReadyTimeoutSeconds);
	}
	if (!bPending || !World)
	{
		GroomWaitStartSeconds = -1.0;
		return bLoaded;
	}
	if (GroomWaitStartSeconds < 0.0)
	{
		GroomWaitStartSeconds = Now;
	}
	if (!World->GetTimerManager().IsTimerActive(GroomRetryTimer))
	{
		World->GetTimerManager().SetTimer(GroomRetryTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ApplyGrooms(CurrentAppearance); }), GroomRetrySeconds, false);
	}
	return bLoaded;
}

void AMOMetaHumanCharacter::OnRep_CurrentAppearance()
{
	ApplyCurrentAppearance();
}

void AMOMetaHumanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOMetaHumanCharacter, CurrentAppearance);
}

void AMOMetaHumanCharacter::ApplySkinLook()
{
	Super::ApplySkinLook();
	ApplyGrooms(CurrentAppearance);
	UMOAppearanceSubsystem::ApplyColorsToActor(this, CurrentAppearance);
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
