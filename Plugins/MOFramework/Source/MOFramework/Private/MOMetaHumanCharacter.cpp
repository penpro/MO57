#include "MOMetaHumanCharacter.h"
#include "MOFramework.h"
#include "MOAppearanceSubsystem.h"
#include "MOBodyShapeComponent.h"
#include "MOCharacterMovementComponent.h"
#include "MOMetabolismComponent.h"
#include "MOSurvivorBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AMOMetaHumanCharacter::AMOMetaHumanCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Face mesh follows Body via Leader Pose
	FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FaceMesh"));
	FaceMesh->SetupAttachment(GetMesh());

	// The body shapes: it sets the MO_Body_* weights on every skeletal mesh of the pawn that carries them (a base's body and face).
	BodyShape = CreateDefaultSubobject<UMOBodyShapeComponent>(TEXT("BodyShape"));

	// Update hand socket names for MetaHuman skeleton
	// MetaHuman uses different socket naming convention
	LeftHandSocketName = TEXT("hand_l");
	RightHandSocketName = TEXT("hand_r");

	// The movement the animation set was made for (GASP's SandboxCharacter_CMC; Wes, Oct 28: "match the animation project settings";
	// Docs/Animation_GASP.md section 3): its motion matching picks frames by these speeds. The gait's acceleration, braking, friction and turn
	// rate follow in UMOCharacterMovementComponent (bAnimationSetMovement); the creatures keep AMOCharacter's.
	WalkSpeed = 200.0f;
	JogSpeed = 500.0f;
	SprintSpeed = 700.0f;
	CrouchedWalkSpeed = 225.0f;
	if (UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(GetCharacterMovement()))
	{
		Move->bAnimationSetMovement = true;
		Move->JumpZVelocity = 500.0f;
		Move->AirControl = 0.25f;
		Move->MaxWalkSpeed = WalkSpeed;
		Move->MaxWalkSpeedCrouched = CrouchedWalkSpeed;
		Move->BrakingDecelerationFalling = 0.0f;   // no braking in the air
		Move->GroundFriction = 5.0f;
	}
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

	// The body: the base's meshes (a restored or replicated look that names one), then the shapes. The server copies the metabolism's body fat
	// and muscle into the shown body now and whenever it moves (it changes over days: a check every couple of seconds is plenty).
	ApplyBaseMeshes();
	ApplyBodyShape();
	if (HasAuthority())
	{
		RefreshShownBody();
		if (ShownBodyRefreshSeconds > 0.0f)
		{
			GetWorldTimerManager().SetTimer(ShownBodyTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { RefreshShownBody(); }),
				ShownBodyRefreshSeconds, /*bLoop=*/true, FMath::FRandRange(0.0f, ShownBodyRefreshSeconds));
		}
	}

	// Hair physics follows where the pawn is (near the world origin only, MO.Hair.Simulation 2): a groom that crosses the line is rebuilt once.
	// Staggered so a colony's pawns do not all check on the same frame.
	if (GetNetMode() != NM_DedicatedServer && HairSimulationCheckSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(HairSimulationTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UMOAppearanceSubsystem::ApplyHairSimulation(this);
		}), HairSimulationCheckSeconds, /*bLoop=*/true, FMath::FRandRange(0.0f, HairSimulationCheckSeconds));
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
	ApplyBaseMeshes();   // a new base's meshes first: the grooms bind to its face, the colours tint its materials
	ApplyGrooms(CurrentAppearance);
	UMOAppearanceSubsystem::ApplyColorsToActor(this, CurrentAppearance);
	ApplyBodyShape();
}

EMOBodySex AMOMetaHumanCharacter::GetBodySex() const
{
	const FMOSurvivorBase* Base = UMOSurvivorBaseSettings::Find(CurrentAppearance.BaseId);
	return Base && Base->Sex != EMOBodySex::Unspecified ? Base->Sex : Super::GetBodySex();
}

bool AMOMetaHumanCharacter::ApplyBaseMeshes()
{
	const FMOSurvivorBase* Base = UMOSurvivorBaseSettings::Find(CurrentAppearance.BaseId);
	if (!Base || !GetMesh() || !FaceMesh)
	{
		return false;
	}
	USkeletalMesh* Body = Cast<USkeletalMesh>(Base->Body.TryLoad());
	USkeletalMesh* Face = Cast<USkeletalMesh>(Base->Face.TryLoad());
	if (!Body || !Face)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAppearance] %s: base '%s' has no %s mesh (%s) -- the pawn keeps its own"), *GetName(), *Base->Id.ToString(),
			!Body ? TEXT("body") : TEXT("face"), *(!Body ? Base->Body : Base->Face).ToString());
		return false;
	}
	if (GetMesh()->GetSkeletalMeshAsset() == Body && FaceMesh->GetSkeletalMeshAsset() == Face)
	{
		return false;
	}
	// The old meshes' material instances (the colours' dynamic instances among them) must not stay on the new meshes: the colours make new ones
	// from the new meshes' materials (every caller applies them next).
	GetMesh()->EmptyOverrideMaterials();
	FaceMesh->EmptyOverrideMaterials();
	SetupMetaHumanMeshes(Body, Face);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAppearance] %s: base '%s' (%s, %s)"), *GetName(), *Base->Id.ToString(), *Body->GetName(), *Face->GetName());
	return true;
}

void AMOMetaHumanCharacter::ApplyBodyShape()
{
	if (!BodyShape)
	{
		return;
	}
	// A pawn with no base keeps its Blueprint's meshes, which carry no shapes: its neutral point is the composition's default.
	const FMOSurvivorBase* Base = UMOSurvivorBaseSettings::Find(CurrentAppearance.BaseId);
	const FMOBodyComposition Default;
	BodyShape->SetNeutralComposition(Base ? Base->BodyFatPercent : Default.BodyFatPercent, Base ? Base->MuscleMassKg : Default.MuscleMass);
	for (const FName Driver : BodyShape->GetDriverNames())
	{
		if (Driver != UMOBodyShapeComponent::BodyFat && Driver != UMOBodyShapeComponent::MuscleMass)
		{
			BodyShape->SetDriver(Driver, CurrentAppearance.GetBuildValue(Driver));
		}
	}
	BodyShape->SetComposition(ShownBodyFatPercent, ShownMuscleMassKg);
}

void AMOMetaHumanCharacter::ApplyBaseStartingBody()
{
	const FMOSurvivorBase* Base = UMOSurvivorBaseSettings::Find(CurrentAppearance.BaseId);
	UMOMetabolismComponent* Metabolism = HasAuthority() ? GetMetabolismComponent() : nullptr;
	if (!Base || !Metabolism)
	{
		return;
	}
	Metabolism->BodyComposition.SetBody(Base->BodyFatPercent, Base->MuscleMassKg);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAppearance] %s: base '%s' body -- %.1f %% body fat, %.1f kg of muscle, %.1f kg"), *GetName(), *Base->Id.ToString(),
		Metabolism->BodyComposition.BodyFatPercent, Metabolism->BodyComposition.MuscleMass, Metabolism->BodyComposition.TotalWeight);
	RefreshShownBody();
}

void AMOMetaHumanCharacter::RefreshShownBody()
{
	const UMOMetabolismComponent* Metabolism = HasAuthority() ? GetMetabolismComponent() : nullptr;
	if (!Metabolism)
	{
		return;
	}
	// Only a visible change goes out: a tenth of a percent of fat (half a percent of the heavy shape), a tenth of a kg of muscle.
	const FMOBodyComposition& Body = Metabolism->GetBodyComposition();
	if (FMath::Abs(Body.BodyFatPercent - ShownBodyFatPercent) < 0.1f && FMath::Abs(Body.MuscleMass - ShownMuscleMassKg) < 0.1f)
	{
		return;
	}
	ShownBodyFatPercent = Body.BodyFatPercent;
	ShownMuscleMassKg = Body.MuscleMass;
	OnRep_ShownBody();   // the server draws it too
}

void AMOMetaHumanCharacter::OnRep_ShownBody()
{
	if (BodyShape)
	{
		BodyShape->SetComposition(ShownBodyFatPercent, ShownMuscleMassKg);
	}
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
	DOREPLIFETIME(AMOMetaHumanCharacter, ShownBodyFatPercent);
	DOREPLIFETIME(AMOMetaHumanCharacter, ShownMuscleMassKg);
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
