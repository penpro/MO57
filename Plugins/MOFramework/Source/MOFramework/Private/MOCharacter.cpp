#include "MOCharacter.h"
#include "MOBuildingComponent.h"
#include "MOAIController.h"
#include "MOCreature.h"
#include "MOCreatureDefinitionRow.h"
#include "Net/UnrealNetwork.h"
#include "MOFramework.h"
#include "MOSpawnClearance.h"
#include "HAL/IConsoleManager.h"
#include "MOInterruptibleInterface.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MOCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/SpringArmComponent.h"
#include "MOIdentityComponent.h"
#include "MOPersistenceSubsystem.h"
#include "MOInteractorComponent.h"
#include "MOInventoryComponent.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOVitalsComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOAnatomyComponent.h"
#include "MOAdrenalineComponent.h"
#include "MORespirationComponent.h"
#include "MOFootstepComponent.h"
#include "MOAudioSubsystem.h"
#include "MOCraftingQueueComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOEquipmentComponent.h"
#include "GroomComponent.h"
#include "MOCombatComponent.h"
#include "MORecruitmentComponent.h"
#include "MOPersonalityComponent.h"
#include "MOCharacterHistoryComponent.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOInteractableComponent.h"
#include "MOUIContractInterface.h"
#include "MOMedicalTreatmentActions.h"
#include "MOFireActions.h"
#include "MONotificationComponent.h"
#include "MOCraftingTypes.h"
#include "MOTerraformingComponent.h"
#include "MOTimedActionComponent.h"
#include "MOCarriedLightComponent.h"
#include "MOEyeAdaptationComponent.h"
#include "MOSurvivorController.h"
#include "AIController.h"
#include "MOUIManagerComponent.h"
#include "MOPlayerController.h"
#include "MOModeIndicatorWidget.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOGameSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NavigationInvokerComponent.h"
#include "Engine/DataTable.h"
#include "Perception/AISense_Hearing.h"
#include "VoxelWorld.h"
#include "Collision/VoxelCollisionInvoker.h"
#include "Render/VoxelLODVolumeComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"

AMOCharacter::AMOCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMOCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Capsule defaults (same as ThirdPerson template)
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates (let movement handle it)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Camera boom (spring arm)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	// Keep SocketOffset zeroed - shoulder offset is handled via FollowCamera relative location

	// Follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	// Default to right shoulder view - toggled via ToggleCameraShoulder()
	FollowCamera->SetRelativeLocation(FVector(0.0f, 80.0f, 0.0f));

	// Movement defaults (same as ThirdPerson template)
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Crouching: without bCanCrouch ACharacter::Crouch() does nothing at all.
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(CrouchedHalfHeightCm);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchedWalkSpeed;
	CrouchIdleAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_Idle_MH.MM_Unarmed_Crouch_Idle_MH")));
	CrouchWalkAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Crouch/MM_Unarmed_Crouch_Walk_Fwd_MH.MM_Unarmed_Crouch_Walk_Fwd_MH")));

	// MO Components - Core Identity
	IdentityComponent = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("IdentityComponent"));

	// MO Components - Inventory & Crafting
	InventoryComponent = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("InventoryComponent"));
	CraftingQueueComponent = CreateDefaultSubobject<UMOCraftingQueueComponent>(TEXT("CraftingQueueComponent"));
	RecipeDiscoveryComponent = CreateDefaultSubobject<UMORecipeDiscoveryComponent>(TEXT("RecipeDiscoveryComponent"));

	// MO Components - Interaction
	InteractorComponent = CreateDefaultSubobject<UMOInteractorComponent>(TEXT("InteractorComponent"));

	// MO Components - Skills & Knowledge
	SkillsComponent = CreateDefaultSubobject<UMOSkillsComponent>(TEXT("SkillsComponent"));
	KnowledgeComponent = CreateDefaultSubobject<UMOKnowledgeComponent>(TEXT("KnowledgeComponent"));

	// MO Components - Survival Stats & Body Systems
	SurvivalStatsComponent = CreateDefaultSubobject<UMOSurvivalStatsComponent>(TEXT("SurvivalStatsComponent"));
	VitalsComponent = CreateDefaultSubobject<UMOVitalsComponent>(TEXT("VitalsComponent"));
	MetabolismComponent = CreateDefaultSubobject<UMOMetabolismComponent>(TEXT("MetabolismComponent"));
	MentalStateComponent = CreateDefaultSubobject<UMOMentalStateComponent>(TEXT("MentalStateComponent"));
	AnatomyComponent = CreateDefaultSubobject<UMOAnatomyComponent>(TEXT("AnatomyComponent"));
	AdrenalineComponent = CreateDefaultSubobject<UMOAdrenalineComponent>(TEXT("AdrenalineComponent"));
	RespirationComponent = CreateDefaultSubobject<UMORespirationComponent>(TEXT("RespirationComponent"));
	FootstepComponent = CreateDefaultSubobject<UMOFootstepComponent>(TEXT("FootstepComponent"));

	// Swim animations (retargeted Oceanology set, Content/Characters/Swim): the default for every MO pawn that has a MetaHuman-skeleton body.
	SwimTreadAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swim_Surface_Idle_MH.Swim_Surface_Idle_MH")));
	SwimSlowAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swim_Surface_Slow_Forward_MH.Swim_Surface_Slow_Forward_MH")));
	SwimCrawlAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swim_Surface_Forward_MH.Swim_Surface_Forward_MH")));
	SwimUnderIdleAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swimming_Underwater_MH.Swimming_Underwater_MH")));
	SwimUnderForwardAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swim_Underwater_Forward_MH.Swim_Underwater_Forward_MH")));
	SwimUnderUpAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swim_Underwater_Forward_Up_MH.Swim_Underwater_Forward_Up_MH")));
	SwimUnderDownAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/MOFramework/Characters/Swim/Swim_Underwater_Forward_Down_MH.Swim_Underwater_Forward_Down_MH")));

	// MO Components - Terraforming
	TerraformingComponent = CreateDefaultSubobject<UMOTerraformingComponent>(TEXT("TerraformingComponent"));

	// MO Components - timed body actions (drinking at water, ...)
	TimedActionComponent = CreateDefaultSubobject<UMOTimedActionComponent>(TEXT("TimedActionComponent"));

	// MO Components - a carried flame (torch, lamp)
	CarriedLightComponent = CreateDefaultSubobject<UMOCarriedLightComponent>(TEXT("CarriedLightComponent"));
	EyeAdaptationComponent = CreateDefaultSubobject<UMOEyeAdaptationComponent>(TEXT("EyeAdaptationComponent"));

	// MO Components - Equipment
	EquipmentComponent = CreateDefaultSubobject<UMOEquipmentComponent>(TEXT("EquipmentComponent"));

	// MO Components - Recruitment
	RecruitmentComponent = CreateDefaultSubobject<UMORecruitmentComponent>(TEXT("RecruitmentComponent"));

	// MO Components - Survivor Job Queue (for task assignment when recruited)
	JobQueueComponent = CreateDefaultSubobject<UMOSurvivorJobQueueComponent>(TEXT("JobQueueComponent"));

	// MO Components - Colony (V1): personality axes + life-event history.
	// Traits start zeroed; the colony upkeep tick randomizes them the first
	// time a recruited villager is seen (villagers become people on joining).
	PersonalityComponent = CreateDefaultSubobject<UMOPersonalityComponent>(TEXT("PersonalityComponent"));
	HistoryComponent = CreateDefaultSubobject<UMOCharacterHistoryComponent>(TEXT("HistoryComponent"));

	// MO Components - Interactable (allows other pawns to interact with this character)
	InteractableComponent = CreateDefaultSubobject<UMOInteractableComponent>(TEXT("InteractableComponent"));

	// Interaction collision sphere - blocks ECC_Visibility traces for interaction system
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetCapsuleComponent());
	InteractionSphere->SetSphereRadius(50.0f);
	InteractionSphere->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f)); // Centered on torso
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetGenerateOverlapEvents(false);

	// Navigation invoker - tells voxel world to generate navmesh around this character
	NavigationInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvoker"));
	NavigationInvoker->SetGenerationRadii(3000.f, 5000.f); // Generate within 30m, remove beyond 50m

	// Voxel collision invoker -- off until a human drives this pawn (NotifyControllerChanged). See the header comment.
	VoxelCollisionInvoker = CreateDefaultSubobject<UVoxelCollisionInvokerComponent>(TEXT("VoxelCollisionInvoker"));
	VoxelCollisionInvoker->SetupAttachment(RootComponent);
	VoxelCollisionInvoker->bEnabled = false;
	VoxelCollisionInvoker->Radius = 3000.f;           // 30 m of ground around a human-driven pawn
	VoxelCollisionInvoker->bWaitForVoxelWorld = true; // compute missing chunks inline instead of letting the pawn fall
	VoxelCollisionInvoker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VoxelCollisionInvoker->SetCanEverAffectNavigation(false);

	// Fine terrain around another player's pawn on this machine -- off until this pawn is one (UpdateRemotePlayerTerrainDetail). See the header comment.
	RemotePlayerTerrainDetail = CreateDefaultSubobject<UVoxelLODVolumeComponent>(TEXT("RemotePlayerTerrainDetail"));
	RemotePlayerTerrainDetail->SetupAttachment(RootComponent);
	RemotePlayerTerrainDetail->bEnabled = false;
	RemotePlayerTerrainDetail->MinLOD = 0;
	RemotePlayerTerrainDetail->Type = EVoxelLODInvokerType::Sphere;
	RemotePlayerTerrainDetail->SphereRadius = 3000.f;   // the collision invoker's 30 m
	RemotePlayerTerrainDetail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RemotePlayerTerrainDetail->SetCanEverAffectNavigation(false);
	// Its scene proxy draws the sphere (ShapeColor) wherever the component is visible: in game every character wore a 30 m pink wire sphere
	// (Oct 18 2026). The voxel world reads only bEnabled and the transform (FVoxelRenderInvokersManager), so hiding it changes nothing else.
	RemotePlayerTerrainDetail->SetHiddenInGame(true);
	RemotePlayerTerrainDetail->bDrawOnlyIfSelected = true;

	// Held item mesh components (attached to hand sockets)
	LeftHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandMesh"));
	LeftHandMesh->SetupAttachment(GetMesh(), LeftHandSocketName);
	LeftHandMesh->SetVisibility(false);
	LeftHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandMesh"));
	RightHandMesh->SetupAttachment(GetMesh(), RightHandSocketName);
	RightHandMesh->SetVisibility(false);
	RightHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Worn rigid gear (N-210): a helmet on the head, a quiver or pack on the back
	HeadWornMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadWornMesh"));
	HeadWornMesh->SetupAttachment(GetMesh(), HeadWornSocketName);
	HeadWornMesh->SetVisibility(false);
	HeadWornMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BackWornMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackWornMesh"));
	BackWornMesh->SetupAttachment(GetMesh(), BackWornSocketName);
	BackWornMesh->SetVisibility(false);
	BackWornMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default mesh position (mesh itself loaded in BeginPlay if DefaultMesh is set)
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	// Default movement speeds
	WalkSpeed = 400.0f;
	SprintSpeed = 700.0f;

	// NOTE: Mesh and animation are intentionally left unset for portability.
	// Set these in child Blueprints to avoid hard-coded asset paths.
}

void AMOCharacter::ApplySkinLook()
{
	static const FName RoughnessOffsetName(TEXT("Roughness Global Offset Post-Bake"));
	static const FName SpecularMultiplyName(TEXT("Specular Global Multiply Post-Bake"));
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(this);
	for (USkeletalMeshComponent* SkinMesh : Meshes)
	{
		for (int32 Slot = 0; Slot < SkinMesh->GetNumMaterials(); ++Slot)
		{
			// Only the slots whose material HAS the MetaHuman skin parameter (body, face skin LODs): teeth, eyes and hidden slots are left as they are.
			UMaterialInterface* Material = SkinMesh->GetMaterial(Slot);
			float Current = 0.0f;
			if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(RoughnessOffsetName), Current))
			{
				continue;
			}
			UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
			if (!Dynamic)
			{
				Dynamic = SkinMesh->CreateAndSetMaterialInstanceDynamic(Slot);
			}
			if (Dynamic)
			{
				Dynamic->SetScalarParameterValue(RoughnessOffsetName, SkinRoughnessOffset);
				Dynamic->SetScalarParameterValue(SpecularMultiplyName, SkinSpecularMultiply);
			}
		}
	}
}

void AMOCharacter::BeginPlay()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AMOCharacter_BeginPlay_Total);   // spawn-cost breakdown: the whole BeginPlay (Super = its components + Blueprint part)
	// Load and apply default mesh if not already set
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		if (DefaultMesh.IsNull())
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: No DefaultMesh configured. Set 'Default Mesh' in the Blueprint or leave skeletal mesh component empty."), *GetName());
		}
		else if (USkeletalMesh* LoadedMesh = DefaultMesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMesh(LoadedMesh);
		}
	}

	// Skin look: now, and once more after the MetaHuman component has built its runtime face material instances.
	ApplySkinLook();
	FTimerHandle SkinLookTimer;
	GetWorldTimerManager().SetTimer(SkinLookTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { ApplySkinLook(); }), 1.0f, false);

	// Load and apply default animation blueprint if not already set
	if (!GetMesh()->GetAnimInstance() && !GetMesh()->GetAnimClass())
	{
		if (!DefaultAnimBlueprint.IsNull())
		{
			if (UClass* AnimClass = DefaultAnimBlueprint.LoadSynchronous())
			{
				GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		}
	}

	// The gait speeds live on the movement component, which picks one per move from the gait the move carries (see UMOCharacterMovementComponent::SetWantedGait)
	if (UMOCharacterMovementComponent* MOMove = Cast<UMOCharacterMovementComponent>(GetCharacterMovement()))
	{
		MOMove->SetGaitSpeeds(WalkSpeed, JogSpeed, SprintSpeed);
	}
	else if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}

	// Apply FOV and sensitivity from game settings
	ApplyGameSettings();

	// Start movement physiology tracking
	StartMovementPhysiologyTracking();

	// Bind knowledge learning to recipe discovery
	if (KnowledgeComponent && RecipeDiscoveryComponent)
	{
		KnowledgeComponent->OnKnowledgeLearned.AddDynamic(this, &AMOCharacter::HandleKnowledgeLearned);
	}

	// Bind skill level up to recipe discovery
	if (SkillsComponent && RecipeDiscoveryComponent)
	{
		SkillsComponent->OnSkillLevelUp.AddDynamic(this, &AMOCharacter::HandleSkillLevelUp);
	}

	// Bind recipe discovery to notifications
	if (RecipeDiscoveryComponent)
	{
		RecipeDiscoveryComponent->OnRecipeDiscovered.AddDynamic(this, &AMOCharacter::HandleRecipeDiscovered);
	}

	// Bind equipment changes to held item visualization
	if (EquipmentComponent)
	{
		EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &AMOCharacter::HandleEquipmentChanged);

		// Update held and worn items for any already-equipped items (e.g., from save load)
		for (const EMOEquipmentSlot Slot : { EMOEquipmentSlot::LeftHand, EMOEquipmentSlot::RightHand, EMOEquipmentSlot::Head, EMOEquipmentSlot::Back })
		{
			UpdateHeldItemMesh(Slot, EquipmentComponent->GetEquippedItem(Slot));
		}
	}

	// Bind interactable component to recruitment system
	if (InteractableComponent && RecruitmentComponent)
	{
		InteractableComponent->OnHandleInteract.BindUObject(this, &AMOCharacter::HandleRecruitmentInteraction);
	}
	// A right-click on a pawn (another player, an unrecruited person) opens what can be done to it -- treat its wounds, light a torch from its
	// flame -- when there is anything (IMOWorldActionProvider below). A recruited survivor's right-click is its command menu first
	// (AMOPlayerController::HandleSecondaryAction), which lists the same options after its orders.
	if (InteractableComponent && !InteractableComponent->OnHandleSecondaryInteract.IsBound())
	{
		InteractableComponent->OnHandleSecondaryInteract.BindWeakLambda(this, [this](AController* InteractorController)
		{
			const APawn* Healer = InteractorController ? InteractorController->GetPawn() : nullptr;
			if (!Healer || Healer == this || bIsDead || !InteractorController->Implements<UMOUIContractInterface>())
			{
				return false;
			}
			TArray<FMOItemActionOption> Options;
			GetWorldActions(Healer, Options);
			if (Options.Num() == 0)
			{
				return false;
			}
			IMOUIContractInterface::Execute_RequestShowWorldActionMenu(InteractorController, this, GetActorLocation());
			return true;
		});
	}

	// Bind to death events from anatomy
	if (AnatomyComponent)
	{
		AnatomyComponent->OnInstantDeath.AddDynamic(this, &AMOCharacter::HandleInstantDeath);
	}
	if (RespirationComponent)
	{
		RespirationComponent->OnAsphyxiaDeath.AddDynamic(this, &AMOCharacter::HandleAsphyxiaDeath);
	}
	if (VitalsComponent)
	{
		VitalsComponent->OnSystemicDeath.AddDynamic(this, &AMOCharacter::HandleSystemicDeath);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AMOCharacter_BeginPlay_Super);   // components' BeginPlay + the Blueprint's BeginPlay event
		Super::BeginPlay();
	}
}

void AMOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMovementPhysiologyTracking();

	// Unbind from death events
	if (AnatomyComponent)
	{
		AnatomyComponent->OnInstantDeath.RemoveDynamic(this, &AMOCharacter::HandleInstantDeath);
	}
	if (RespirationComponent)
	{
		RespirationComponent->OnAsphyxiaDeath.RemoveDynamic(this, &AMOCharacter::HandleAsphyxiaDeath);
	}
	if (VitalsComponent)
	{
		VitalsComponent->OnSystemicDeath.RemoveDynamic(this, &AMOCharacter::HandleSystemicDeath);
	}

	Super::EndPlay(EndPlayReason);
}

void AMOCharacter::UnPossessed()
{
	Super::UnPossessed();

	// Fire LostControl on the event bus — any active player-driven actions
	// (inspection, build, harvest, crafting) decide how to react. Most pause;
	// inspection cancels. Doing this here means the unpossess path doesn't
	// need to know about any specific activity system.
	//
	// We don't gate this on authority because LostControl is a client-relevant
	// signal too: UI controllers running on the client own listeners that
	// need to tear down.
	BroadcastInterrupt(FMOInterruptContext(EMOInterruptReason::LostControl, this));

	// When player unpossesses this pawn, spawn survivor AI controller if this is a recruited survivor
	if (!HasAuthority())
	{
		return;
	}

	// Don't spawn during world teardown
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	// A dead body gets no mind back: EnterDeadState removes the survivor AI, and this handler used to spawn a new one into the corpse (Oct 19).
	if (bIsDead)
	{
		return;
	}

	// Check if this is a recruited survivor
	if (!RecruitmentComponent || !RecruitmentComponent->IsPossessable())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - not a possessable survivor, no AI controller needed"), *GetName());
		return;
	}

	// Only spawn AI controller if we don't already have one
	if (GetController() != nullptr)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - already has controller %s"), *GetName(), *GetController()->GetName());
		return;
	}

	// Use the pawn's AIControllerClass if it's a survivor controller, otherwise use default
	TSubclassOf<AController> ControllerClass = AIControllerClass;
	if (!ControllerClass || !ControllerClass->IsChildOf(AMOSurvivorController::StaticClass()))
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - using default AMOSurvivorController (AIControllerClass was %s)"),
			*GetName(), ControllerClass ? *ControllerClass->GetName() : TEXT("None"));
		ControllerClass = AMOSurvivorController::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAIController* NewController = World->SpawnActor<AAIController>(ControllerClass, GetActorLocation(), GetActorRotation(), SpawnParams);
	if (NewController)
	{
		NewController->Possess(this);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: UnPossessed - spawned and possessed by %s"),
			*GetName(), *NewController->GetName());
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: UnPossessed - failed to spawn AI controller"), *GetName());
	}
}

void AMOCharacter::GatherRuntimeAssets(TArray<FSoftObjectPath>& Out) const
{
	if (AIControllerClass && AIControllerClass->IsChildOf(AMOAIController::StaticClass()))
	{
		GetDefault<AMOAIController>(AIControllerClass.Get())->GatherRuntimeAssets(Out);
	}
	// What this class resolves with LoadSynchronous itself: the default mesh / anim blueprint (BeginPlay) and the crouch / swim poses (the first crouch or swim
	// stroke of a pawn otherwise loads them on the game thread mid-movement).
	const auto Add = [&Out](const FSoftObjectPath& Path)
	{
		if (Path.IsValid())
		{
			Out.AddUnique(Path);
		}
	};
	Add(DefaultMesh.ToSoftObjectPath());
	Add(DefaultAnimBlueprint.ToSoftObjectPath());
	for (const TSoftObjectPtr<UAnimSequence>* Anim : { &SwimTreadAnim, &SwimSlowAnim, &SwimCrawlAnim, &SwimUnderIdleAnim, &SwimUnderForwardAnim, &SwimUnderUpAnim,
		&SwimUnderDownAnim, &CrouchIdleAnim, &CrouchWalkAnim })
	{
		Add(Anim->ToSoftObjectPath());
	}
}

float AMOCharacter::GetMovementNoiseLoudness(float Speed) const
{
	// Piecewise through this pawn's own gaits (a creature's WalkSpeed / SprintSpeed come from its definition): the speed is what is heard.
	const float Walk = FMath::Max(WalkSpeed, 1.0f);
	const float Sprint = FMath::Max(SprintSpeed, Walk + 1.0f);
	const float Jog = FMath::Clamp(JogSpeed, Walk + 0.5f, Sprint - 0.5f);
	float Loudness;
	if (Speed <= Walk)
	{
		Loudness = WalkingNoiseLoudness * FMath::Max(Speed, 0.0f) / Walk;
	}
	else if (Speed <= Jog)
	{
		Loudness = FMath::Lerp(WalkingNoiseLoudness, JoggingNoiseLoudness, (Speed - Walk) / (Jog - Walk));
	}
	else
	{
		Loudness = FMath::Lerp(JoggingNoiseLoudness, SprintingNoiseLoudness, FMath::Min(1.0f, (Speed - Jog) / (Sprint - Jog)));
	}
	if (bIsCrouched)
	{
		Loudness *= CrouchedNoiseScale;
	}
	return FMath::Clamp(Loudness, 0.0f, 1.0f);
}

void AMOCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickWater(DeltaTime);

	// Check for falling through the world
	if (bEnableFallThroughSafety)
	{
		CheckFallThroughSafety(DeltaTime);
		CheckEmbeddedSafety(DeltaTime);
	}

	// Generate movement noise for AI perception
	if (bGeneratesMovementNoise)
	{
		TimeSinceLastNoise += DeltaTime;

		// Check if character is moving
		const float Speed = GetVelocity().Size();
		if (Speed > 10.f && TimeSinceLastNoise >= NoiseInterval)
		{
			TimeSinceLastNoise = 0.f;

			// Report noise event to AI perception system (server: AActor::MakeNoise does nothing on a client)
			// MaxRange of 0 means use the loudness to determine range
			MakeNoise(GetMovementNoiseLoudness(Speed), this, GetActorLocation());
		}
	}

	// Handle smooth camera shoulder transition
	if (bIsCameraTransitioning && FollowCamera)
	{
		CameraTransitionAlpha += DeltaTime / FMath::Max(CameraTransitionDuration, 0.01f);

		if (CameraTransitionAlpha >= 1.0f)
		{
			// Transition complete
			CameraTransitionAlpha = 1.0f;
			bIsCameraTransitioning = false;

			FVector FinalLocation = FollowCamera->GetRelativeLocation();
			FinalLocation.Y = CameraTargetY;
			FollowCamera->SetRelativeLocation(FinalLocation);
		}
		else
		{
			// Smooth interpolation using ease-out curve
			const float EasedAlpha = 1.0f - FMath::Pow(1.0f - CameraTransitionAlpha, 2.0f);
			const float NewY = FMath::Lerp(CameraStartY, CameraTargetY, EasedAlpha);

			FVector CurrentLocation = FollowCamera->GetRelativeLocation();
			CurrentLocation.Y = NewY;
			FollowCamera->SetRelativeLocation(CurrentLocation);
		}
	}

	// Detect movement transitions and fire the Movement-reason broadcast.
	// Cheap when nobody's listening — just one bool compare and an early exit.
	TickMovementInterruptDetection();
}

// ============================================================================
// INTERRUPT EVENT BUS
// ============================================================================

void AMOCharacter::RegisterInterruptListener(UObject* Listener)
{
	if (!IsValid(Listener))
	{
		return;
	}

	// Reject objects that don't implement the interface — otherwise we'd silently
	// store a listener that can never be notified.
	if (!Listener->Implements<UMOInterruptibleInterface>())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOCharacter] RegisterInterruptListener: '%s' does not implement IMOInterruptibleInterface"),
			*GetNameSafe(Listener));
		return;
	}

	// No-op if already present. Cheap to check; saves duplicate notifications.
	for (const TWeakObjectPtr<UObject>& Existing : InterruptListeners)
	{
		if (Existing.Get() == Listener)
		{
			return;
		}
	}

	InterruptListeners.Add(Listener);
}

void AMOCharacter::UnregisterInterruptListener(UObject* Listener)
{
	if (!Listener)
	{
		return;
	}

	InterruptListeners.RemoveAll([Listener](const TWeakObjectPtr<UObject>& Weak)
	{
		// Also opportunistically drop stale weak pointers while we're here.
		return !Weak.IsValid() || Weak.Get() == Listener;
	});
}

void AMOCharacter::BroadcastInterrupt(const FMOInterruptContext& Context)
{
	// Snapshot the list so listeners can safely call UnregisterInterruptListener
	// from inside their NotifyInterrupt handler without invalidating our iteration.
	TArray<TWeakObjectPtr<UObject>> Snapshot = InterruptListeners;
	for (const TWeakObjectPtr<UObject>& Weak : Snapshot)
	{
		UObject* Listener = Weak.Get();
		if (!IsValid(Listener))
		{
			continue;
		}
		if (Listener->Implements<UMOInterruptibleInterface>())
		{
			IMOInterruptibleInterface::Execute_NotifyInterrupt(Listener, Context);
		}
	}

	// Compact stale entries the broadcast revealed.
	InterruptListeners.RemoveAll([](const TWeakObjectPtr<UObject>& Weak)
	{
		return !Weak.IsValid();
	});
}

void AMOCharacter::TickMovementInterruptDetection()
{
	// Fast path: nothing listening, skip entirely.
	if (InterruptListeners.Num() == 0)
	{
		bWasConsideredMovingLastTick = false;
		return;
	}

	const float SpeedSq = GetVelocity().SizeSquared();
	const bool bIsMovingNow = SpeedSq > MovementInterruptVelocitySqThreshold;

	// Edge-triggered: fire once on the transition stationary->moving.
	// While the player keeps moving, we don't keep re-firing.
	if (bIsMovingNow && !bWasConsideredMovingLastTick)
	{
		// Use the convenience constructor: self-caused, full severity.
		BroadcastInterrupt(FMOInterruptContext(EMOInterruptReason::Movement, this));
	}

	bWasConsideredMovingLastTick = bIsMovingNow;
}

// ============================================================================
// IMOControllableInterface IMPLEMENTATION
// ============================================================================

void AMOCharacter::RequestMove_Implementation(FVector2D MovementInput)
{
	if (!CanMove_Implementation())
	{
		return;
	}

	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Swimming steers by the camera direction: look down and stroke forward to dive, look up to rise. A dead zone of +-12 degrees around the horizon keeps a swimmer who merely looks around
		// level (otherwise a slightly downward glance is a slow dive and the head goes under); beyond it the pitch ramps up to the full angle.
		FRotator SwimRotation = Rotation;
		if (IsSwimming())
		{
			constexpr float DeadZone = 12.0f;
			const float Pitch = FRotator::NormalizeAxis(Rotation.Pitch);
			const float Excess = FMath::Max(FMath::Abs(Pitch) - DeadZone, 0.0f);
			SwimRotation.Pitch = FMath::Sign(Pitch) * Excess * (90.0f / (90.0f - DeadZone));
		}
		const FVector ForwardDirection = IsSwimming() ? SwimRotation.Vector() : FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementInput.Y);
		AddMovementInput(RightDirection, MovementInput.X);
	}
}

void AMOCharacter::RequestLook_Implementation(FVector2D LookInput)
{
	if (!CanBeControlled_Implementation())
	{
		return;
	}

	if (Controller)
	{
		// Apply sensitivity
		float YawInput = LookInput.X * LookSensitivity;
		float PitchInput = LookInput.Y * LookSensitivity;

		// Check InvertY setting
		if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
		{
			if (Settings->bInvertYAxis)
			{
				PitchInput = -PitchInput;
			}
		}

		AddControllerYawInput(YawInput);
		AddControllerPitchInput(PitchInput);
	}
}

void AMOCharacter::RequestJumpStart_Implementation()
{
	if (IsSwimming())
	{
		bSwimUpHeld = true;   // rise while held (TickWater adds the input)
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();   // the jump key stands up first (a crouched pawn cannot jump)
		return;
	}
	if (CanJump_Implementation())
	{
		Jump();
	}
}

void AMOCharacter::RequestJumpEnd_Implementation()
{
	bSwimUpHeld = false;
	StopJumping();
}

void AMOCharacter::RequestSprintStart_Implementation()
{
	// Sprint is now handled via StartSprint() from hustle hold
	StartSprint();
}

void AMOCharacter::RequestSprintEnd_Implementation()
{
	// Sprint release is now handled via StopSprint()
	StopSprint();
}

void AMOCharacter::RequestToggleJog_Implementation()
{
	ToggleJog();
}

void AMOCharacter::RequestCrouchToggle_Implementation()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AMOCharacter::RequestInteract_Implementation()
{
	if (InteractorComponent)
	{
		InteractorComponent->TryInteract();
	}
}

void AMOCharacter::RequestSecondaryInteract_Implementation()
{
	if (InteractorComponent)
	{
		InteractorComponent->TrySecondaryInteract();
	}
}

void AMOCharacter::RequestPrimaryAction_Implementation()
{
	// If in terraforming mode, perform terraform action
	if (bTerraformingMode)
	{
		DoTerraform();
		return;
	}

	// Try to start a combat attack via CombatComponent
	if (UMOCombatComponent* CombatComp = FindComponentByClass<UMOCombatComponent>())
	{
		if (CombatComp->StartLightAttack())
		{
			return;
		}
	}

	// DEBUG (punch, disabled Oct 18): re-enable to trace swings
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestPrimaryAction - No combat component or attack failed"));
}

void AMOCharacter::RequestPrimaryActionRelease_Implementation()
{
	// Override in subclasses
	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestPrimaryActionRelease - Override in subclass"));
}

void AMOCharacter::RequestSecondaryAction_Implementation()
{
	// Override in subclasses for block/aim/alt-use
	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestSecondaryAction - Override in subclass"));
}

void AMOCharacter::RequestSecondaryActionRelease_Implementation()
{
	// Override in subclasses
	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] RequestSecondaryActionRelease - Override in subclass"));
}

void AMOCharacter::RequestTerraformToggle_Implementation()
{
	ToggleTerraformMode();
}

void AMOCharacter::RequestTerraformCycleTool_Implementation()
{
	CycleTerraformTool();
}

bool AMOCharacter::CanBeControlled_Implementation() const
{
	// Check if controllable, not dead, and not pending kill
	return bCanBeControlled && !bIsDead && !IsPendingKillPending();
}

bool AMOCharacter::CanMove_Implementation() const
{
	// Can move if controllable and has valid movement component
	return CanBeControlled_Implementation() && GetCharacterMovement() != nullptr;
}

bool AMOCharacter::CanJump_Implementation() const
{
	return CanBeControlled_Implementation() && ACharacter::CanJump();
}

bool AMOCharacter::CanSprint_Implementation() const
{
	if (!CanMove_Implementation() || bIsCrouched)
	{
		return false;
	}

	// Check stamina requirement
	if (SurvivalStatsComponent)
	{
		float CurrentStamina = SurvivalStatsComponent->GetStatCurrent(TEXT("Stamina"));
		if (CurrentStamina < MinStaminaToSprint)
		{
			return false;
		}
	}

	return true;
}

// ============================================================================
// DIRECT INPUT (for Blueprint/UI)
// ============================================================================

void AMOCharacter::DoMove(float Right, float Forward)
{
	RequestMove_Implementation(FVector2D(Right, Forward));
}

void AMOCharacter::DoLook(float Yaw, float Pitch)
{
	RequestLook_Implementation(FVector2D(Yaw, Pitch));
}

void AMOCharacter::DoJumpStart()
{
	RequestJumpStart_Implementation();
}

void AMOCharacter::DoJumpEnd()
{
	RequestJumpEnd_Implementation();
}

// ============================================================================
// GAME SETTINGS
// ============================================================================

void AMOCharacter::ApplyGameSettings()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		return;
	}

	// Apply FOV to camera
	if (FollowCamera)
	{
		FollowCamera->SetFieldOfView(Settings->FieldOfView);
		// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] Applied FOV: %.1f"), Settings->FieldOfView);
	}

	// Store sensitivity for use in look input
	LookSensitivity = Settings->CameraSensitivity;
}

void AMOCharacter::ToggleCameraShoulder()
{
	if (!FollowCamera)
	{
		return;
	}

	// The old logic computed the new start by lerping the previous start
	// to the previous target using the LINEAR alpha (mismatched with the
	// eased rendering) and then flipped the SIGN of that partial value.
	// Result: each rapid press flipped a smaller and smaller value, so
	// repeated presses converged the camera to Y=0. (80 → -80 → 80/3 → ...)
	//
	// Fix: don't compute the new target from the current partial position.
	// The two valid shoulder positions are +CameraShoulderOffset and
	// -CameraShoulderOffset. Snap the target to the opposite extreme of
	// where the camera is *committed* to be:
	//   - At rest: the current rendered Y is the commitment.
	//   - Mid-transition: the in-flight CameraTargetY is the commitment
	//     (the player asked to go there; pressing V again means "go back").
	// Start is always the currently rendered Y so the visual stays smooth.
	const float CurrentRenderedY = FollowCamera->GetRelativeLocation().Y;
	const float CommittedSide = bIsCameraTransitioning ? CameraTargetY : CurrentRenderedY;

	const float Offset = FMath::Max(FMath::Abs(CameraShoulderOffset), 1.0f);
	CameraTargetY = (CommittedSide >= 0.0f) ? -Offset : Offset;
	CameraStartY = CurrentRenderedY;

	CameraTransitionAlpha = 0.0f;
	bIsCameraTransitioning = true;

	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] Camera shoulder transitioning from %.1f to %s (Y=%.1f)"),
	// 	CameraStartY,
	// 	CameraTargetY > 0 ? TEXT("right") : TEXT("left"),
	// 	CameraTargetY);
}

// ============================================================================
// APPEARANCE
// ============================================================================

void AMOCharacter::SetCharacterMesh(USkeletalMesh* NewMesh)
{
	if (NewMesh && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(NewMesh);
	}
}

void AMOCharacter::SetAnimationBlueprint(TSubclassOf<UAnimInstance> NewAnimClass)
{
	if (NewAnimClass && GetMesh())
	{
		GetMesh()->SetAnimInstanceClass(NewAnimClass);
	}
}

// ============================================================================
// MOVEMENT MODE SYSTEM
// ============================================================================

void AMOCharacter::SetMovementMode(EMOMovementMode NewMode)
{
	if (CurrentMovementMode == NewMode)
	{
		return;
	}

	CurrentMovementMode = NewMode;
	bIsSprinting = (NewMode == EMOMovementMode::Sprinting);
	UpdateMovementSpeed();

	// DEBUG (movement, disabled Oct 18): re-enable to trace gait changes (the log line and the "Jogging: 450" on-screen message)
	// // Get actual speed for logging
	// float ActualSpeed = 0.0f;
	// if (GetCharacterMovement())
	// {
	// 	ActualSpeed = GetCharacterMovement()->GetMaxSpeed();   // the gait's speed (MaxWalkSpeed is the walk only)
	// }
	//
	// const FString ModeName = NewMode == EMOMovementMode::Walking ? TEXT("Walking") :
	// 						 NewMode == EMOMovementMode::Jogging ? TEXT("Jogging") : TEXT("Sprinting");
	//
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Movement mode -> %s (Speed: %.0f)"),
	// 	*GetName(), *ModeName, ActualSpeed);
	//
	// // On-screen debug message
	// #if !UE_BUILD_SHIPPING
	// if (GEngine)
	// {
	// 	const FColor ModeColor = NewMode == EMOMovementMode::Walking ? FColor::White :
	// 							 NewMode == EMOMovementMode::Jogging ? FColor::Yellow : FColor::Orange;
	// 	GEngine->AddOnScreenDebugMessage(-1, 2.0f, ModeColor,
	// 		FString::Printf(TEXT("%s: %.0f"), *ModeName, ActualSpeed));
	// }
	// #endif
}

void AMOCharacter::ToggleJog()
{
	if (!CanMove_Implementation())
	{
		return;
	}

	// If currently sprinting, don't toggle jog
	if (bIsSprinting)
	{
		// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Cannot toggle jog while sprinting"), *GetName());
		return;
	}

	bJogToggled = !bJogToggled;

	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Jog toggled %s"), *GetName(), bJogToggled ? TEXT("ON") : TEXT("OFF"));

	if (bJogToggled)
	{
		SetMovementMode(EMOMovementMode::Jogging);
	}
	else
	{
		SetMovementMode(EMOMovementMode::Walking);
	}
}

void AMOCharacter::StartSprint()
{
	if (!CanSprint_Implementation())
	{
		// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Cannot sprint (CanSprint returned false)"), *GetName());
		return;
	}

	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Sprint START (hold detected)"), *GetName());
	bIsSprinting = true;
	SetMovementMode(EMOMovementMode::Sprinting);
}

void AMOCharacter::StopSprint()
{
	if (!bIsSprinting)
	{
		return;
	}

	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Sprint STOP (returning to %s)"),
	// 	*GetName(), bJogToggled ? TEXT("Jogging") : TEXT("Walking"));

	bIsSprinting = false;

	// Return to previous mode (jog if toggled, walk otherwise)
	if (bJogToggled)
	{
		SetMovementMode(EMOMovementMode::Jogging);
	}
	else
	{
		SetMovementMode(EMOMovementMode::Walking);
	}
}

void AMOCharacter::UpdateMovementSpeed()
{
	// The gait is MOVEMENT INPUT: the movement component carries it in every saved move, so the server simulates a joiner at the speed the joiner predicts.
	// (It used to write MaxWalkSpeed on the pressing machine only: a joiner's jog / sprint was corrected back to a walk by the server.)
	if (UMOCharacterMovementComponent* MOMove = Cast<UMOCharacterMovementComponent>(GetCharacterMovement()))
	{
		MOMove->SetWantedGait(CurrentMovementMode == EMOMovementMode::Jogging, CurrentMovementMode == EMOMovementMode::Sprinting);
		return;
	}
	if (!GetCharacterMovement())
	{
		return;
	}
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Walking:
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		break;
	case EMOMovementMode::Jogging:
		GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
		break;
	case EMOMovementMode::Sprinting:
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		break;
	}
}

void AMOCharacter::ApplyGaitFromMovement(bool bJog, bool bSprint)
{
	const EMOMovementMode NewMode = bSprint ? EMOMovementMode::Sprinting : (bJog ? EMOMovementMode::Jogging : EMOMovementMode::Walking);
	if (NewMode == CurrentMovementMode)
	{
		return;
	}
	CurrentMovementMode = NewMode;
	bIsSprinting = bSprint;
	bJogToggled = bJog;
	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: gait from the moves -> %s"), *GetName(), bSprint ? TEXT("Sprinting") : (bJog ? TEXT("Jogging") : TEXT("Walking")));
}

float AMOCharacter::GetCurrentMET() const
{
	if (IsSwimming())
	{
		return SwimmingMET;
	}
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Jogging:
		return JoggingMET;
	case EMOMovementMode::Sprinting:
		return SprintingMET;
	case EMOMovementMode::Walking:
	default:
		return WalkingMET;
	}
}

float AMOCharacter::GetCurrentExertionLevel() const
{
	if (IsSwimming())
	{
		return SwimmingExertion;
	}
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Jogging:
		return JoggingExertion;
	case EMOMovementMode::Sprinting:
		return SprintingExertion;
	case EMOMovementMode::Walking:
	default:
		return WalkingExertion;
	}
}

float AMOCharacter::GetCurrentTempRiseRate() const
{
	switch (CurrentMovementMode)
	{
	case EMOMovementMode::Jogging:
		return JoggingTempRisePerSec;
	case EMOMovementMode::Sprinting:
		return SprintingTempRisePerSec;
	case EMOMovementMode::Walking:
	default:
		return WalkingTempRisePerSec;
	}
}

void AMOCharacter::StartMovementPhysiologyTracking()
{
	// Set up a timer to apply physiological effects every 0.5 seconds
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MovementPhysiologyTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				ApplyMovementPhysiologyEffects(0.5f);
			}),
			0.5f,
			true
		);
	}
}

void AMOCharacter::StopMovementPhysiologyTracking()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementPhysiologyTimerHandle);
	}
}

void AMOCharacter::ApplyMovementPhysiologyEffects(float DeltaTime)
{
	// Only apply effects if we're actually moving (a swimmer treading water is working: it never rests)
	if (!GetCharacterMovement() || (GetVelocity().Size2D() < 10.0f && !IsSwimming()))
	{
		// Not moving - set exertion to resting
		if (VitalsComponent)
		{
			// Gradually return to resting exertion (handled by vitals component)
			VitalsComponent->SetExertionLevel(0.0f);
		}
		return;
	}

	// Get body weight for calorie calculation
	float BodyWeightKg = 75.0f; // Default
	if (MetabolismComponent)
	{
		BodyWeightKg = MetabolismComponent->BodyComposition.TotalWeight;
	}

	// Calculate calories burned: MET × weight(kg) × 0.0175 × time(minutes)
	const float CurrentMET = GetCurrentMET();
	const float CaloriesPerMinute = CurrentMET * BodyWeightKg * 0.0175f;
	const float CaloriesBurned = CaloriesPerMinute * (DeltaTime / 60.0f);

	// Apply to metabolism
	if (MetabolismComponent)
	{
		MetabolismComponent->ApplyCalorieBurn(CaloriesBurned);

		// Also count as cardio training if jogging or sprinting
		if (CurrentMovementMode != EMOMovementMode::Walking)
		{
			float Intensity = (CurrentMovementMode == EMOMovementMode::Sprinting) ? 0.9f : 0.5f;
			MetabolismComponent->ApplyCardioTraining(Intensity, DeltaTime);
		}
	}

	// Apply to vitals (exertion, temperature)
	if (VitalsComponent)
	{
		// Set exertion level (SetExertionLevel is itself authority-gated)
		VitalsComponent->SetExertionLevel(GetCurrentExertionLevel());

		// (H13) Exercise heat generation. This adds heat ONLY — the cooling /
		// regulation toward 37°C lives in the vitals environmental pipeline
		// (UMOVitalsComponent::ApplyEnvironmentalTemperature, now driven every
		// EnvironmentalTempUpdateInterval via H12). The two compose without
		// fighting: exercise nudges temp up here, regulation pulls it back to
		// 37 there. Do NOT add cooling in this block — that would double-regulate
		// against the environmental path.
		//
		// Authority-gate the direct write: Vitals.BodyTemperature is
		// COND_OwnerOnly-replicated and owned by the server. Writing it on a
		// client (this timer runs for all roles) would briefly fight the
		// replicated value. Gentle rates (~0.01–0.08°C/min) keep this far below
		// the vitals 25–45°C clamp.
		if (HasAuthority())
		{
			float TempRise = GetCurrentTempRiseRate() * DeltaTime;
			VitalsComponent->Vitals.BodyTemperature = FMath::Min(VitalsComponent->Vitals.BodyTemperature + TempRise, 40.0f);
		}
	}

	// ============================================================================
	// STAMINA CONSUMPTION
	// ============================================================================
	if (SurvivalStatsComponent)
	{
		float StaminaCost = 0.0f;
		switch (CurrentMovementMode)
		{
		case EMOMovementMode::Jogging:
			StaminaCost = JoggingStaminaCostPerSecond * DeltaTime;
			break;
		case EMOMovementMode::Sprinting:
			StaminaCost = SprintingStaminaCostPerSecond * DeltaTime;
			break;
		default:
			break;
		}
		if (IsSwimming())
		{
			StaminaCost = SwimmingStaminaCostPerSecond * DeltaTime;
		}

		if (StaminaCost > 0.0f)
		{
			SurvivalStatsComponent->ModifyStat(TEXT("Stamina"), -StaminaCost);

			// Auto-stop sprinting if stamina depleted
			if (bIsSprinting && SurvivalStatsComponent->GetStatCurrent(TEXT("Stamina")) <= 0.0f)
			{
				StopSprint();
			}
		}
	}

	// ============================================================================
	// ATHLETICS SKILL XP
	// ============================================================================
	if (SkillsComponent && !AthleticsSkillId.IsNone())
	{
		float XPGained = IsSwimming() ? JoggingXPPerSecond * DeltaTime : 0.0f;
		switch (CurrentMovementMode)
		{
		case EMOMovementMode::Walking:
			XPGained = WalkingXPPerSecond * DeltaTime;
			break;
		case EMOMovementMode::Jogging:
			XPGained = JoggingXPPerSecond * DeltaTime;
			break;
		case EMOMovementMode::Sprinting:
			XPGained = SprintingXPPerSecond * DeltaTime;
			break;
		}

		if (XPGained > 0.0f)
		{
			SkillsComponent->AddExperience(AthleticsSkillId, XPGained);
		}
	}
}

// ============================================================================
// COMPONENT EVENT HANDLERS
// ============================================================================

void AMOCharacter::HandleKnowledgeLearned(FName KnowledgeId, FName FromItemId)
{
	if (RecipeDiscoveryComponent)
	{
		RecipeDiscoveryComponent->CheckDiscoveryFromKnowledge(KnowledgeId);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Knowledge '%s' learned - checking for recipe discoveries"), *KnowledgeId.ToString());
	}

	// Show notification via player controller's notification component
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UMONotificationComponent* NotificationComp = PC->FindComponentByClass<UMONotificationComponent>())
		{
			NotificationComp->ShowKnowledgeLearnedNotification(KnowledgeId);
		}
	}
}

void AMOCharacter::HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel)
{
	if (RecipeDiscoveryComponent)
	{
		RecipeDiscoveryComponent->CheckDiscoveryFromSkillLevel(SkillId, NewLevel);
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Skill '%s' leveled up to %d - checking for recipe discoveries"), *SkillId.ToString(), NewLevel);
	}
}

void AMOCharacter::HandleRecipeDiscovered(FName RecipeId, EMODiscoveryMethod Method)
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Recipe '%s' discovered via method %d"), *RecipeId.ToString(), (int32)Method);

	// Show notification via player controller's notification component
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UMONotificationComponent* NotificationComp = PC->FindComponentByClass<UMONotificationComponent>())
		{
			NotificationComp->ShowRecipeUnlockedNotification(RecipeId);
		}
	}
}

bool AMOCharacter::IsSwimming() const
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	return Move && Move->IsSwimming();
}

void AMOCharacter::HandleAsphyxiaDeath()
{
	DieOf(EMODeathCause::Drowned, EMOBodyPartType::Brain);   // drowning kills by hypoxia of the brain
}

void AMOCharacter::HandleSystemicDeath(EMOSystemicFailure Cause)
{
	switch (Cause)
	{
	case EMOSystemicFailure::Exsanguination:     DieOf(EMODeathCause::BledOut, EMOBodyPartType::Heart); break;
	case EMOSystemicFailure::CardiacArrest:
		// Deep hypothermia stops the heart (the rate falls ~10 / min per degree under 32 C): a death by cold, and the recap says so.
		DieOf(VitalsComponent && VitalsComponent->Vitals.BodyTemperature < 30.0f ? EMODeathCause::Hypothermia : EMODeathCause::CardiacArrest, EMOBodyPartType::Heart);
		break;
	case EMOSystemicFailure::RespiratoryFailure: DieOf(EMODeathCause::RespiratoryFailure, EMOBodyPartType::LungLeft); break;
	case EMOSystemicFailure::HeatStroke:         DieOf(EMODeathCause::HeatStroke, EMOBodyPartType::Brain); break;
	default: break;
	}
}

void AMOCharacter::DieOf(EMODeathCause Cause, EMOBodyPartType Part)
{
	if (bIsDead)
	{
		return;
	}
	DeathCause = Cause;
	DeathCausePart = Part;
	bDeathCauseSet = true;
	HandleInstantDeath(Part);   // virtual: AMOCreature adds its own teardown
}

void AMOCharacter::UpdateWaterSounds(float DeltaTime)
{
	const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(GetCharacterMovement());
	if (!Move || IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	const float Immersion = Move->GetImmersion();
	const float Previous = LastImmersionForSfx;
	LastImmersionForSfx = Immersion;
	WaterSfxTimer = FMath::Max(0.0f, WaterSfxTimer - DeltaTime);

	UMOAudioSubsystem* Audio = UMOAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}
	auto Splash = [&](const FVector& At)
	{
		const FName Id = FMath::RandBool() ? FName(TEXT("SFX.Footsteps.WaterSplash1")) : FName(TEXT("SFX.Footsteps.WaterSplash2"));
		const bool bPlayed = Audio->PlayOneShotAtLocation(this, Id, At);
		UE_LOG(LogMOFramework, Log, TEXT("[MOWater] splash %s %s (immersion %.2f, speed %.0f)"), *Id.ToString(), bPlayed ? TEXT("played") : TEXT("NOT PLAYED"), Immersion, GetVelocity().Size2D());
	};

	float Surface = 0.0f;
	if (!Move->GetWaterSurfaceZ(Surface))
	{
		return;
	}
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetSimpleCollisionHalfHeight());
	const FVector OnWater(GetActorLocation().X, GetActorLocation().Y, Surface);
	const float Speed2D = GetVelocity().Size2D();

	// The feet reach the water (a step in, a landing) and the water reaches the chest (the plunge into a swim): one splash each.
	if (Previous < 0.10f && Immersion >= 0.10f)
	{
		Splash(Feet);
		WaterSfxTimer = 0.4f;
	}
	else if (Previous < 0.70f && Immersion >= 0.70f)
	{
		Splash(OnWater);
		WaterSfxTimer = 0.6f;
	}
	else if (WaterSfxTimer <= 0.0f)
	{
		if (Move->IsSwimming() && !Move->IsHeadUnderwater() && Speed2D > 30.0f)
		{
			// A stroke: about every 0.9 s slow, 0.6 s at a crawl, at the hands in front of the swimmer.
			Splash(OnWater + GetActorForwardVector() * 45.0f);
			WaterSfxTimer = Speed2D > 0.7f * Move->GetMaxSpeed() ? 0.6f : 0.9f;
		}
		else if (!Move->IsSwimming() && Immersion > 0.08f && Speed2D > 40.0f && Move->IsMovingOnGround())
		{
			// A wading step: the cadence follows the speed (a walk is about two steps a second).
			Splash(Feet);
			WaterSfxTimer = FMath::Clamp(0.62f * 250.0f / Speed2D, 0.3f, 0.9f);
		}
	}
}

AMOCharacter::ESwimPose AMOCharacter::ChooseSwimPose() const
{
	const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(GetCharacterMovement());
	if (!Move)
	{
		return ESwimPose::None;
	}
	const FVector Velocity = GetVelocity();
	const float Horizontal = Velocity.Size2D();
	const bool bMoving = Horizontal > 15.0f || FMath::Abs(Velocity.Z) > 25.0f;
	if (Move->IsHeadUnderwater())
	{
		if (!bMoving)
		{
			return ESwimPose::UnderIdle;
		}
		if (FMath::Abs(Velocity.Z) > 25.0f && FMath::Abs(Velocity.Z) > 0.5f * Horizontal)
		{
			return Velocity.Z > 0.0f ? ESwimPose::UnderUp : ESwimPose::UnderDown;
		}
		return ESwimPose::UnderForward;
	}
	if (Horizontal < 15.0f)
	{
		return ESwimPose::Tread;
	}
	return Horizontal > 0.7f * Move->GetMaxSpeed() ? ESwimPose::Crawl : ESwimPose::Slow;
}

void AMOCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	StopCrouchPose(0.3f);
}

void AMOCharacter::StopCrouchPose(float BlendOutSeconds)
{
	if (CrouchMontage)
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			if (UAnimInstance* Anim = Body->GetAnimInstance())
			{
				Anim->Montage_Stop(BlendOutSeconds, CrouchMontage);
			}
		}
	}
	CrouchMontage = nullptr;
	CurrentCrouchPose = ECrouchPose::None;
	WantedCrouchPose = ECrouchPose::None;
	WantedCrouchPoseSeconds = 0.0f;
}

void AMOCharacter::UpdateCrouchAnimation(float DeltaTime, bool bSwimming)
{
	if (IsNetMode(NM_DedicatedServer))
	{
		return;   // cosmetic
	}
	USkeletalMeshComponent* Body = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	// The swim override replaces the animation blueprint (no DefaultSlot then); a dead or airborne pawn is not in a crouch pose.
	const bool bWantPose = Body && Move && bIsCrouched && !bSwimming && !bIsDead && Move->IsMovingOnGround() && CurrentSwimPose == ESwimPose::None;
	if (!bWantPose)
	{
		if (CurrentCrouchPose != ECrouchPose::None)
		{
			StopCrouchPose(0.25f);
		}
		return;
	}
	UAnimInstance* Anim = Body->GetAnimInstance();
	if (!Anim)
	{
		return;
	}

	// The animation instance can be replaced under us (an appearance change swaps the mesh and its blueprint): the montage is then gone while the state still says a pose is playing. Start over.
	if (CurrentCrouchPose != ECrouchPose::None && (!CrouchMontage || !Anim->Montage_IsPlaying(CrouchMontage)))
	{
		CrouchMontage = nullptr;
		CurrentCrouchPose = ECrouchPose::None;
		WantedCrouchPose = ECrouchPose::None;
		WantedCrouchPoseSeconds = 0.0f;
	}

	const float Speed2D = GetVelocity().Size2D();
	const ECrouchPose Wanted = Speed2D > 25.0f ? ECrouchPose::Walk : ECrouchPose::Idle;
	if (Wanted == CurrentCrouchPose)
	{
		WantedCrouchPose = Wanted;
		WantedCrouchPoseSeconds = 0.0f;
		if (CurrentCrouchPose == ECrouchPose::Walk && CrouchMontage)
		{
			Anim->Montage_SetPlayRate(CrouchMontage, FMath::Clamp(Speed2D / CrouchWalkAnimSpeedCm, 0.5f, 1.6f));
		}
		return;
	}

	// Debounce: the speed hovers around the threshold while starting and stopping.
	if (Wanted != WantedCrouchPose)
	{
		WantedCrouchPose = Wanted;
		WantedCrouchPoseSeconds = 0.0f;
	}
	WantedCrouchPoseSeconds += DeltaTime;
	if (CurrentCrouchPose != ECrouchPose::None && WantedCrouchPoseSeconds < 0.12f)
	{
		return;
	}

	UAnimSequence* Sequence = (Wanted == ECrouchPose::Walk ? CrouchWalkAnim : CrouchIdleAnim).LoadSynchronous();
	if (!Sequence)
	{
		return;   // the clips are not in this build: keep the standing pose
	}
	// A new montage in the same slot blends the previous one out: idle <-> walk cross-fades. 100000 loops = until stopped.
	CrouchMontage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.25f, 0.25f, 1.0f, 100000);
	CurrentCrouchPose = Wanted;
}

void AMOCharacter::UpdateSwimAnimation(float DeltaTime, bool bSwimming)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}

	// Swim poses play as a looping montage in the locomotion blueprint's slot, so entering, changing pose and leaving the water
	// cross-fade (bug #13: the old single-node PlayAnimation swap snapped the pose every time and re-initialised the blueprint on
	// the way out). A blueprint without the slot falls back to the single-node swap.
	static const FName SwimSlotName(TEXT("DefaultSlot"));
	constexpr float SwimBlendInSeconds = 0.35f;
	constexpr float SwimBlendOutSeconds = 0.45f;

	// Leaving the water (or dying): hand the body back to locomotion.
	if (!bSwimming)
	{
		if (CurrentSwimPose != ESwimPose::None)
		{
			CurrentSwimPose = ESwimPose::None;
			PendingSwimPose = ESwimPose::None;
			if (bSwimPoseViaSlot)
			{
				if (UAnimInstance* AnimInstance = Body->GetAnimInstance())
				{
					AnimInstance->StopSlotAnimation(SwimBlendOutSeconds, SwimSlotName);
				}
				bSwimPoseViaSlot = false;
			}
			else
			{
				Body->SetAnimationMode((EAnimationMode::Type)SavedAnimMode);
				if (SavedAnimClass)
				{
					Body->SetAnimInstanceClass(SavedAnimClass);
				}
				SwimMeshRaise = 0.0f;
				Body->SetRelativeLocation(SavedMeshRelativeLocation);
			}
		}
		// Ease the surface-swimmer lift back down with the blend instead of dropping the mesh 70 cm in one frame.
		if (SwimMeshRaise > 0.1f)
		{
			SwimMeshRaise = FMath::FInterpTo(SwimMeshRaise, 0.0f, DeltaTime, 6.0f);
			Body->SetRelativeLocation(SavedMeshRelativeLocation + FVector(0.0f, 0.0f, SwimMeshRaise));
		}
		else if (SwimMeshRaise > 0.0f)
		{
			SwimMeshRaise = 0.0f;
			Body->SetRelativeLocation(SavedMeshRelativeLocation);
		}
		return;
	}

	// Debounce: a pose must be wanted for a quarter second before it replaces the current one (the speed hovers around the thresholds).
	const ESwimPose Wanted = ChooseSwimPose();
	if (Wanted != CurrentSwimPose)
	{
		if (Wanted != PendingSwimPose)
		{
			PendingSwimPose = Wanted;
			PendingSwimPoseSeconds = 0.0f;
		}
		PendingSwimPoseSeconds += DeltaTime;
		if (CurrentSwimPose == ESwimPose::None || PendingSwimPoseSeconds >= 0.25f)
		{
			const TSoftObjectPtr<UAnimSequence>* Source = nullptr;
			switch (Wanted)
			{
			default: check(false);
			case ESwimPose::Tread: Source = &SwimTreadAnim; break;
			case ESwimPose::Slow: Source = &SwimSlowAnim; break;
			case ESwimPose::Crawl: Source = &SwimCrawlAnim; break;
			case ESwimPose::UnderIdle: Source = &SwimUnderIdleAnim; break;
			case ESwimPose::UnderForward: Source = &SwimUnderForwardAnim; break;
			case ESwimPose::UnderUp: Source = &SwimUnderUpAnim; break;
			case ESwimPose::UnderDown: Source = &SwimUnderDownAnim; break;
			case ESwimPose::None: break;
			}
			UAnimSequence* Anim = (Source && !Source->IsNull()) ? Source->LoadSynchronous() : nullptr;
			if (Anim)
			{
				UAnimInstance* AnimInstance = Body->GetAnimInstance();
				if (CurrentSwimPose == ESwimPose::None)
				{
					SavedAnimMode = (uint8)Body->GetAnimationMode();
					SavedAnimClass = Body->GetAnimClass();
					if (SwimMeshRaise <= 0.0f)
					{
						SavedMeshRelativeLocation = Body->GetRelativeLocation();   // not while still easing down from the last swim
					}
					// The slot node reports its weight every graph update whether or not a montage plays in it: > 0 = it exists and is live.
					bSwimPoseViaSlot = AnimInstance && Body->GetAnimationMode() == EAnimationMode::AnimationBlueprint
						&& AnimInstance->GetSlotNodeGlobalWeight(SwimSlotName) > 0.0f;
					UE_LOG(LogMOFramework, Log, TEXT("[MOWater] swim poses via %s"), bSwimPoseViaSlot ? TEXT("the DefaultSlot montage (blended)") : TEXT("single-node swap (no slot in the blueprint)"));
				}
				if (bSwimPoseViaSlot && AnimInstance)
				{
					// A new montage in the same slot cross-fades from the current pose. Dynamic montages cannot loop forever: 100k loops.
					AnimInstance->PlaySlotAnimationAsDynamicMontage(Anim, SwimSlotName, SwimBlendInSeconds, SwimBlendOutSeconds, 1.0f, 100000);
				}
				else
				{
					Body->PlayAnimation(Anim, /*bLooping=*/true);
				}
				CurrentSwimPose = Wanted;
			}
			PendingSwimPose = ESwimPose::None;
			PendingSwimPoseSeconds = 0.0f;
		}
	}
	else
	{
		PendingSwimPose = ESwimPose::None;
		PendingSwimPoseSeconds = 0.0f;
	}

	// A horizontal swimmer rides the surface: lift the mesh until the body line is at the water (the capsule's centre is ~60 cm below it while floating). Upright and underwater poses keep the capsule's frame.
	if (CurrentSwimPose != ESwimPose::None)
	{
		float TargetRaise = 0.0f;
		if (CurrentSwimPose == ESwimPose::Slow || CurrentSwimPose == ESwimPose::Crawl)
		{
			float Surface = 0.0f;
			const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(GetCharacterMovement());
			if (Move && Move->GetWaterSurfaceZ(Surface))
			{
				TargetRaise = FMath::Clamp(Surface - GetActorLocation().Z - 6.0f, 0.0f, 70.0f);
			}
		}
		SwimMeshRaise = FMath::FInterpTo(SwimMeshRaise, TargetRaise, DeltaTime, 6.0f);
		Body->SetRelativeLocation(SavedMeshRelativeLocation + FVector(0.0f, 0.0f, SwimMeshRaise));
	}
}

void AMOCharacter::TickWater(float DeltaTime)
{
	UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(GetCharacterMovement());
	if (!Move)
	{
		return;
	}
	const bool bSwimming = Move->IsSwimming();

	// Rise while Space is held: the local input; the movement component replicates the resulting move like any other.
	if (bSwimming && bSwimUpHeld && !bIsDead && IsLocallyControlled())
	{
		AddMovementInput(FVector::UpVector, 1.0f);
	}
	if (!bSwimming)
	{
		bSwimUpHeld = false;
	}

	if (HasAuthority())
	{
		// The airway is closed while the eyes are under water: the respiration component runs the breath-hold from here.
		if (RespirationComponent)
		{
			RespirationComponent->SetAirwayClosedBy(TEXT("Water"), !bIsDead && Move->IsHeadUnderwater());
		}
		// The body exchanges heat with the water (colder than the air, soaked clothes do not insulate): the vitals' environmental temperature pipeline.
		if (VitalsComponent)
		{
			VitalsComponent->SetWaterImmersion(Move->GetImmersion());
		}
		// Clothes soak through as the water rises (the existing wetness pipeline: cold, drying by fire, moodle).
		if (VitalsComponent && Move->GetImmersion() > 0.05f)
		{
			const float Target = FMath::Clamp(Move->GetImmersion() * 1.25f, 0.0f, 1.0f);   // chest deep = soaked
			if (VitalsComponent->GetWetnessLevel() < Target)
			{
				VitalsComponent->SetWetnessLevel(FMath::Min(Target, VitalsComponent->GetWetnessLevel() + DeltaTime * 0.6f));
			}
		}
	}

	// Effort: a hustle stroke is faster; with no stamina left the swimmer can barely keep afloat.
	float Scale = 1.0f;
	if (bSwimming)
	{
		if (bIsSprinting)
		{
			Scale = SwimHustleSpeedScale;
		}
		if (SurvivalStatsComponent && SurvivalStatsComponent->GetStatCurrent(TEXT("Stamina")) <= 0.0f)
		{
			Scale = ExhaustedSwimSpeedScale;
		}
	}
	Move->SwimSpeedScale = Scale;

	if (bSwimming && bIsCrouched)
	{
		UnCrouch();   // nobody swims crouched
	}
	UpdateSwimAnimation(DeltaTime, bSwimming && !bIsDead);
	UpdateCrouchAnimation(DeltaTime, bSwimming);
	if (!bIsDead)
	{
		UpdateWaterSounds(DeltaTime);
	}

	bWasSwimmingLastTick = bSwimming;
}

namespace MOCharacterDeath
{
	/** Who to name on the death recap: the anatomy's last attacker, for deaths an attacker causes (wounds); empty for the environment. */
	FText DescribeKiller(const AMOCharacter& Victim, EMODeathCause Cause)
	{
		if (Cause != EMODeathCause::BodyPartDestroyed && Cause != EMODeathCause::BledOut)
		{
			return FText::GetEmpty();   // drowned, heart / breathing failure: no one to name
		}
		const UMOAnatomyComponent* Anatomy = Victim.FindComponentByClass<UMOAnatomyComponent>();
		const AActor* Killer = Anatomy ? Anatomy->GetLastDamageInstigator() : nullptr;
		if (const AController* Controller = Cast<AController>(Killer))
		{
			Killer = Controller->GetPawn();
		}
		if (!Killer || Killer == &Victim)
		{
			return FText::GetEmpty();
		}
		if (const AMOCreature* Creature = Cast<AMOCreature>(Killer))
		{
			if (const FMOCreatureDefinitionRow* Row = Creature->GetCreatureDefinitionRow(); Row && !Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;   // the species ("Wolf")
			}
		}
		if (const UMOIdentityComponent* Identity = Killer->FindComponentByClass<UMOIdentityComponent>(); Identity && !Identity->DisplayName.IsEmpty())
		{
			return Identity->DisplayName;   // a survivor or player
		}
		return FText::GetEmpty();   // unknown: say nothing rather than an actor name
	}
}

FMOOnAnyPawnDied AMOCharacter::OnAnyPawnDied;

void AMOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOCharacter, DeathRecord);
}

void AMOCharacter::HandleInstantDeath(EMOBodyPartType CausePart)
{
	if (bIsDead)
	{
		return; // Already dead
	}

	if (!bDeathCauseSet)   // came straight from anatomy's OnInstantDeath: a destroyed body part
	{
		DeathCause = EMODeathCause::BodyPartDestroyed;
		DeathCausePart = CausePart;
		bDeathCauseSet = true;
	}

	if (HasAuthority())
	{
		// The record every client enters the dead state from (OnRep_DeathRecord). Written before EnterDeadState so the server's own
		// listeners (the host's recap) read the same killer.
		DeathRecord.bDead = true;
		DeathRecord.Cause = DeathCause;
		DeathRecord.Part = CausePart;
		DeathRecord.KillerName = MOCharacterDeath::DescribeKiller(*this, DeathCause);
		ForceNetUpdate();
	}

	EnterDeadState(CausePart);
}

void AMOCharacter::RestoreAsCorpse()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}
	DeathCause = EMODeathCause::BodyPartDestroyed;
	DeathCausePart = EMOBodyPartType::None;
	bDeathCauseSet = true;
	DeathRecord.bDead = true;
	DeathRecord.Cause = DeathCause;
	DeathRecord.Part = DeathCausePart;
	ForceNetUpdate();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s restored as a corpse (its save record is deceased)"), *GetName());
	EnterDeadState(EMOBodyPartType::None);
}

void AMOCharacter::OnRep_DeathRecord()
{
	if (!DeathRecord.bDead || bIsDead)
	{
		return;
	}
	DeathCause = DeathRecord.Cause;
	DeathCausePart = DeathRecord.Part;
	bDeathCauseSet = true;
	EnterDeadState(DeathRecord.Part);
}

void AMOCharacter::EnterDeadState(EMOBodyPartType CausePart)
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s DIED! Cause: %s (body part %d)%s%s [%s]"),
		*GetName(), *UEnum::GetValueAsString(DeathCause), static_cast<int32>(CausePart),
		DeathRecord.KillerName.IsEmpty() ? TEXT("") : TEXT(", killed by "), *DeathRecord.KillerName.ToString(),
		HasAuthority() ? TEXT("server") : TEXT("client"));

	// Broadcast the death event BEFORE the interrupt cascade tears down
	// state. Listeners (death recap UI, kill feed, spectator handoff) get a
	// clean snapshot — vitals/skills/inventory haven't been touched yet,
	// so the recap can pull final stats accurately.
	OnPawnDied.Broadcast(this, CausePart);
	OnAnyPawnDied.Broadcast(this, CausePart);

	// Permadeath: record the death in the save so this pawn doesn't resurrect on
	// reload (MarkPawnDeceased previously had zero callers — H28). Authority-only:
	// it mutates the persisted world save.
	if (HasAuthority() && IdentityComponent)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UMOPersistenceSubsystem* Persistence = GI->GetSubsystem<UMOPersistenceSubsystem>())
				{
					Persistence->MarkPawnDeceased(IdentityComponent->GetGuid());
				}
			}
		}
	}

	// Fire the Death interrupt BEFORE we tear down input/movement. Listeners
	// (active build, harvest, inspection, crafting) will see the Death reason
	// and run their terminal cleanup — refund materials, cancel widgets,
	// drop UI. This is the second example source on the event bus alongside
	// movement detection; the pattern is the same for every future source
	// (combat-start, knockdown, sleep, etc.). Server only, as it always ran: its listeners refund and cancel authoritative work.
	if (HasAuthority())
	{
		BroadcastInterrupt(FMOInterruptContext(EMOInterruptReason::Death, this));

		// A dead body has no mind: its AI (a survivor's) stops for good and its queued work is dropped. Until Oct 19 a dead survivor's controller kept
		// working its job queue -- failing jobs it could not move for (and, with the job-failure notices, telling everyone). A player keeps its controller.
		if (AAIController* Mind = Cast<AAIController>(GetController()))
		{
			if (UMOSurvivorJobQueueComponent* Jobs = FindComponentByClass<UMOSurvivorJobQueueComponent>())
			{
				Jobs->CancelAllJobs();
			}
			Mind->UnPossess();
			Mind->Destroy();
		}
	}

	// Disable input and movement
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->DisableMovement();
		MovementComp->StopMovementImmediately();
	}

	// Disable capsule collision (ragdoll mesh takes over collision)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Enable ragdoll physics on mesh with proper collision
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Stop all animations - both montages and the base animation blueprint
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.0f);
		}

		// Pause animation blueprint updates so it doesn't fight with physics
		MeshComp->bPauseAnims = true;

		// Set collision profile before enabling physics
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// Enable physics simulation on ALL bodies, not just root
		// SetSimulatePhysics(true) only enables root body - we need all bodies for ragdoll
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetAllBodiesPhysicsBlendWeight(1.0f);
		MeshComp->WakeAllRigidBodies();
	}

	// Schedule ragdoll freeze after delay (allows body to settle naturally)
	GetWorld()->GetTimerManager().SetTimer(
		RagdollFreezeTimerHandle,
		this,
		&AMOCharacter::FreezeRagdoll,
		RagdollFreezeDelay,
		false
	);

	// TODO: Broadcast death event for game mode to handle (respawn, spectate, etc.)
	// OnCharacterDeath.Broadcast(this, CausePart);
}

void AMOCharacter::FreezeRagdoll()
{
	if (bRagdollFrozen)
	{
		return; // Already frozen
	}

	bRagdollFrozen = true;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s ragdoll frozen - body is now static and interactable"),
		*GetName());

	// Ensure animations stay paused (prevents snapping back to animated pose)
	MeshComp->bPauseAnims = true;

	// Put all physics bodies to sleep - they'll stay in current position
	// but won't actively simulate (saves performance)
	MeshComp->PutAllRigidBodiesToSleep();

	// Set collision to QueryOnly for interaction traces
	// Keep physics enabled but sleeping so the pose is preserved
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

// ============================================================================
// IMOInventoryHolderInterface IMPLEMENTATION
// ============================================================================

UMOInventoryComponent* AMOCharacter::GetInventory_Implementation() const
{
	return InventoryComponent;
}

bool AMOCharacter::HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const
{
	return InventoryComponent ? InventoryComponent->HasItem(ItemDefinitionId, Quantity) : false;
}

int32 AMOCharacter::GetInventoryItemCount_Implementation(FName ItemDefinitionId) const
{
	return InventoryComponent ? InventoryComponent->GetItemCountByDefinitionId(ItemDefinitionId) : 0;
}

// ============================================================================
// IMOMaterialSourceInterface IMPLEMENTATION
// ============================================================================

bool AMOCharacter::CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const
{
	return InventoryComponent ? InventoryComponent->HasItem(MaterialId, Quantity) : false;
}

int32 AMOCharacter::GatherMaterial_Implementation(FName MaterialId, int32 Quantity)
{
	if (!InventoryComponent)
	{
		return 0;
	}

	int32 Gathered = 0;
	for (int32 i = 0; i < Quantity; ++i)
	{
		if (InventoryComponent->RemoveItemByDefinitionId(MaterialId, 1))
		{
			++Gathered;
		}
		else
		{
			break;
		}
	}
	return Gathered;
}

int32 AMOCharacter::GetMaterialSourcePriority_Implementation() const
{
	// Player inventory has highest priority (100)
	return 100;
}

// ============================================================================
// IMOIdentifiableInterface IMPLEMENTATION
// ============================================================================

UMOIdentityComponent* AMOCharacter::GetIdentityComponent_Implementation() const
{
	return IdentityComponent;
}

FGuid AMOCharacter::GetPersistentGuid_Implementation() const
{
	return IdentityComponent ? IdentityComponent->GetGuid() : FGuid();
}

bool AMOCharacter::HasValidIdentity_Implementation() const
{
	return IdentityComponent && IdentityComponent->GetGuid().IsValid();
}

// ============================================================================
// IMOCraftingCapableInterface IMPLEMENTATION
// ============================================================================

UMOCraftingQueueComponent* AMOCharacter::GetCraftingQueue_Implementation() const
{
	return CraftingQueueComponent;
}

UMORecipeDiscoveryComponent* AMOCharacter::GetRecipeDiscovery_Implementation() const
{
	return RecipeDiscoveryComponent;
}

void AMOCharacter::SetActiveCraftingStation_Implementation(AActor* StationActor)
{
	if (CraftingQueueComponent)
	{
		CraftingQueueComponent->SetActiveStation(StationActor);
	}
}

AActor* AMOCharacter::GetActiveCraftingStation_Implementation() const
{
	if (CraftingQueueComponent)
	{
		return CraftingQueueComponent->GetActiveStation();
	}
	return nullptr;
}

// ============================================================================
// IMOMedicalProviderInterface IMPLEMENTATION
// ============================================================================

UMOVitalsComponent* AMOCharacter::GetVitals_Implementation() const
{
	return VitalsComponent;
}

UMOAnatomyComponent* AMOCharacter::GetAnatomy_Implementation() const
{
	return AnatomyComponent;
}

UMOMentalStateComponent* AMOCharacter::GetMentalState_Implementation() const
{
	return MentalStateComponent;
}

UMOAdrenalineComponent* AMOCharacter::GetAdrenaline_Implementation() const
{
	return AdrenalineComponent;
}

UMOMetabolismComponent* AMOCharacter::GetMetabolism_Implementation() const
{
	return MetabolismComponent;
}

// ============================================================================
// TERRAFORMING
// ============================================================================

void AMOCharacter::ToggleTerraformMode()
{
	bTerraformingMode = !bTerraformingMode;

	// Terraform and building mode exclude each other (B leaves Terraform the same way).
	if (bTerraformingMode)
	{
		if (const AController* OwnerController = GetController())
		{
			if (UMOBuildingComponent* Building = OwnerController->FindComponentByClass<UMOBuildingComponent>())
			{
				Building->SetBuildingMode(false);
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: Terraform mode %s"),
		*GetName(),
		bTerraformingMode ? TEXT("ENABLED") : TEXT("DISABLED"));

	// Update UI via player controller
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>())
		{
			UIManager->RefreshGameplayMode();   // Terraform while the mode is on (the UI manager decides the HUD mode from state)
			if (bTerraformingMode)
			{

				// Show current tool hint PERSISTENTLY (duration < 0). The
				// indicator reflects an ongoing state — "you are in terraform
				// mode and the current tool is X" — so it must stay visible
				// the whole time the mode is active, not just for the 2-second
				// default. HideToolHint below clears it on mode exit.
				if (TerraformingComponent)
				{
					UIManager->ShowToolHint(TerraformingComponent->GetModeDisplayName(), -1.0f);
				}
			}
			else
			{
				UIManager->HideToolHint();
			}
		}
	}

	if (bTerraformingMode && TerraformingComponent)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] Current terraform tool: %s"),
			*TerraformingComponent->GetModeDisplayName().ToString());
	}
}

void AMOCharacter::CycleTerraformTool()
{
	if (TerraformingComponent)
	{
		TerraformingComponent->CycleMode();

		// Show new tool name persistently — same reasoning as in
		// ToggleTerraformMode: it's a mode indicator, not a momentary popup.
		// HideToolHint on terraform-mode exit will clear it.
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UMOUIManagerComponent* UIManager = PC->FindComponentByClass<UMOUIManagerComponent>())
			{
				UIManager->ShowToolHint(TerraformingComponent->GetModeDisplayName(), -1.0f);
			}
		}
	}
}

bool AMOCharacter::DoTerraform()
{
	if (!TerraformingComponent)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] DoTerraform: No TerraformingComponent"));
		return false;
	}

	if (!bTerraformingMode)
	{
		// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] DoTerraform: Not in terraform mode"));
		return false;
	}

	// BeginTerraform starts the 5-second progress timer (with movement-interrupt
	// support) and fires OnTerraformStarted, which UMOCharacterUIController
	// listens to in order to spawn the progress widget. Sculpt is applied by
	// the widget on timer completion, not here.
	return TerraformingComponent->BeginTerraform();
}

// ============================================================================
// HELD ITEM VISUALS
// ============================================================================

void AMOCharacter::HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem)
{
	// The hands (held) and Head / Back (worn rigid gear) have visuals; clothing slots do not yet (GetItemVisualTarget)
	UpdateHeldItemMesh(EquipSlot, EquippedItem);
}

bool AMOCharacter::HandleRecruitmentInteraction(AController* InteractorController)
{
	if (!RecruitmentComponent)
	{
		return false;
	}

	// Another PLAYER's character is not a stranger to recruit (co-op): nothing to do, nothing to change.
	if (IsPlayerControlled() && GetController() != InteractorController)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s is played by another player: no recruitment interaction"), *GetName());
		return false;
	}

	// Already recruited - no special interaction needed
	if (RecruitmentComponent->IsPossessable())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s is already recruited"), *GetName());
		return false;
	}

	// Get the interacting pawn
	APawn* InteractingPawn = InteractorController ? InteractorController->GetPawn() : nullptr;
	if (!InteractingPawn)
	{
		return false;
	}

	// Attempt recruitment interaction and TELL the player what came of it: every outcome, refusals included, reaches their screen (a silent refusal looked like a broken interaction).
	const FMORecruitmentResult Result = RecruitmentComponent->BeginInteractionDetailed(InteractingPawn);

	FText Name = NSLOCTEXT("MORecruitment", "TheSurvivor", "The survivor");
	if (const UMOIdentityComponent* Identity = FindComponentByClass<UMOIdentityComponent>())
	{
		if (!Identity->DisplayName.IsEmpty())
		{
			Name = Identity->DisplayName;
		}
	}

	FText Message;
	EMONotificationType Type = EMONotificationType::Info;
	float Duration = 5.0f;
	switch (Result.Outcome)
	{
	case EMORecruitmentOutcome::Recruited:
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s has been recruited!"), *GetName());
		Message = FText::Format(NSLOCTEXT("MORecruitment", "Recruited", "{0} joins you."), Name);
		Type = EMONotificationType::Success;
		break;
	case EMORecruitmentOutcome::QuestStarted:
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s has a quest: %s"), *GetName(), *RecruitmentComponent->GetQuestProgressText().ToString());
		const FText& Dialogue = RecruitmentComponent->ActiveQuest.QuestDialogue;
		Message = Dialogue.IsEmpty()
			? FText::Format(NSLOCTEXT("MORecruitment", "QuestStarted", "{0} needs help before joining you: {1}"), Name, RecruitmentComponent->GetQuestProgressText())
			: FText::Format(NSLOCTEXT("MORecruitment", "QuestStartedDialogue", "{0}: \"{1}\""), Name, Dialogue);
		Duration = 8.0f;
		break;
	}
	case EMORecruitmentOutcome::QuestPending:
		Message = FText::Format(NSLOCTEXT("MORecruitment", "QuestPending", "{0} is still waiting for you: {1}"), Name, RecruitmentComponent->GetQuestProgressText());
		Type = EMONotificationType::Warning;
		break;
	case EMORecruitmentOutcome::DeclinedStanding:
		Message = FText::Format(NSLOCTEXT("MORecruitment", "DeclinedStanding",
			"{0} does not know your settlement well enough to join yet (trust {1}% of the {2}% needed). Spend about {3} more hours of game time near them."),
			Name, FText::AsNumber(FMath::RoundToInt(Result.Standing * 100.0f)), FText::AsNumber(FMath::RoundToInt(Result.StandingNeeded * 100.0f)),
			FText::AsNumber(FMath::Max(1, FMath::RoundToInt(Result.HoursToPass))));
		Type = EMONotificationType::Warning;
		Duration = 8.0f;
		break;
	case EMORecruitmentOutcome::Unavailable:
		Message = FText::Format(NSLOCTEXT("MORecruitment", "Unavailable", "{0} is dead."), Name);
		break;
	default:
		break;   // an animal, or someone already with you: nothing to say
	}
	if (!Message.IsEmpty())
	{
		if (AMOPlayerController* PlayerController = Cast<AMOPlayerController>(InteractorController))
		{
			PlayerController->ClientShowNotification(Message, Duration, Type);
		}
	}

	return Result.Succeeded();
}

void AMOCharacter::UpdateHeldItemMesh(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem)
{
	// Determine which mesh component to use
	UStaticMeshComponent* TargetMesh = nullptr;
	FName SocketName = NAME_None;
	if (!GetItemVisualTarget(EquipSlot, TargetMesh, SocketName))
	{
		return; // A slot with no visual (clothing)
	}

	// If slot is empty, hide the mesh
	if (!EquippedItem.IsValid())
	{
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	// Look up item definition to get the mesh
	const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
	if (!Settings)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: No item database settings"));
		return;
	}

	UDataTable* ItemTable = Settings->ItemDefinitionsDataTable.LoadSynchronous();
	if (!ItemTable)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: Failed to load item DataTable"));
		return;
	}

	const FMOItemDefinitionRow* ItemDef = ItemTable->FindRow<FMOItemDefinitionRow>(EquippedItem.ItemDefinitionId, TEXT("UpdateHeldItemMesh"));
	if (!ItemDef)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: Item '%s' not found in DataTable"),
			*EquippedItem.ItemDefinitionId.ToString());
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	// Determine which mesh to use: HeldVisual.StaticMesh first, fall back to WorldVisual.StaticMesh
	TSoftObjectPtr<UStaticMesh> MeshToLoad = ItemDef->HeldVisual.StaticMesh;
	if (MeshToLoad.IsNull())
	{
		MeshToLoad = ItemDef->WorldVisual.StaticMesh;
	}

	if (MeshToLoad.IsNull())
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] UpdateHeldItemMesh: Item '%s' has no StaticMesh defined (checked HeldVisual and WorldVisual)"),
			*EquippedItem.ItemDefinitionId.ToString());
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	UStaticMesh* LoadedMesh = MeshToLoad.LoadSynchronous();
	if (!LoadedMesh)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] UpdateHeldItemMesh: Failed to load mesh for '%s'"),
			*EquippedItem.ItemDefinitionId.ToString());
		ClearHeldItemMesh(EquipSlot);
		return;
	}

	// Set the mesh and make it visible
	TargetMesh->SetStaticMesh(LoadedMesh);

	// Apply held transform for the appropriate hand; worn gear its offset from the slot's bone
	const bool bIsHand = EquipSlot == EMOEquipmentSlot::LeftHand || EquipSlot == EMOEquipmentSlot::RightHand;
	const bool bIsLeftHand = (EquipSlot == EMOEquipmentSlot::LeftHand);
	const FTransform Offset = bIsHand ? ItemDef->HeldVisual.GetTransformForHand(bIsLeftHand) : ItemDef->HeldVisual.WornTransform;

	// Apply material override: HeldVisual first, fall back to WorldVisual
	TSoftObjectPtr<UMaterialInterface> MaterialToLoad = ItemDef->HeldVisual.MaterialOverride;
	if (MaterialToLoad.IsNull())
	{
		MaterialToLoad = ItemDef->WorldVisual.MaterialOverride;
	}

	if (!MaterialToLoad.IsNull())
	{
		if (UMaterialInterface* Material = MaterialToLoad.LoadSynchronous())
		{
			TargetMesh->SetMaterial(0, Material);
		}
	}

	// Ensure it's attached to the correct socket, then its offset (attaching with SnapToTarget resets the relative transform)
	TargetMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	TargetMesh->SetRelativeTransform(Offset);

	TargetMesh->SetVisibility(true);
	if (EquipSlot == EMOEquipmentSlot::Head)
	{
		SetScalpHairHiddenForHelmet(true);
	}

	// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
	// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Equipped '%s' to %s hand - mesh visible"),
	// 	*GetName(),
	// 	*EquippedItem.ItemDefinitionId.ToString(),
	// 	EquipSlot == EMOEquipmentSlot::LeftHand ? TEXT("left") : TEXT("right"));
}

bool AMOCharacter::GetItemVisualTarget(EMOEquipmentSlot EquipSlot, UStaticMeshComponent*& OutMesh, FName& OutSocket) const
{
	switch (EquipSlot)
	{
	case EMOEquipmentSlot::LeftHand: OutMesh = LeftHandMesh; OutSocket = LeftHandSocketName; break;
	case EMOEquipmentSlot::RightHand: OutMesh = RightHandMesh; OutSocket = RightHandSocketName; break;
	case EMOEquipmentSlot::Head: OutMesh = HeadWornMesh; OutSocket = HeadWornSocketName; break;
	case EMOEquipmentSlot::Back: OutMesh = BackWornMesh; OutSocket = BackWornSocketName; break;
	default: return false;
	}
	return OutMesh != nullptr;
}

void AMOCharacter::SetScalpHairHiddenForHelmet(bool bHide)
{
	// The MetaHuman's grooms: the scalp hair is "Hair"; eyebrows, eyelashes, beard, mustache and fuzz have their own names and stay.
	TArray<UGroomComponent*> Grooms;
	GetComponents<UGroomComponent>(Grooms);
	for (UGroomComponent* Groom : Grooms)
	{
		if (IsValid(Groom) && Groom->GetName().StartsWith(TEXT("Hair")))
		{
			Groom->SetHiddenInGame(bHide);
		}
	}
}

void AMOCharacter::ClearHeldItemMesh(EMOEquipmentSlot EquipSlot)
{
	UStaticMeshComponent* TargetMesh = nullptr;
	FName SocketName = NAME_None;
	GetItemVisualTarget(EquipSlot, TargetMesh, SocketName);
	if (EquipSlot == EMOEquipmentSlot::Head)
	{
		SetScalpHairHiddenForHelmet(false);
	}

	if (TargetMesh)
	{
		TargetMesh->SetVisibility(false);
		TargetMesh->SetStaticMesh(nullptr);

		// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: Cleared %s hand mesh"),
		// 	*GetName(),
		// 	EquipSlot == EMOEquipmentSlot::LeftHand ? TEXT("left") : TEXT("right"));
	}
}

// ============================================================================
// FALL-THROUGH SAFETY
// ============================================================================

namespace
{
	/** TEST ONLY: turns the embedded rescue off so a harness can prove the "stuck in geometry" test really sees a stuck pawn. */
	TAutoConsoleVariable<bool> CVarDisableEmbeddedRescue(
		TEXT("MO.EmbeddedRescue.Disable"), false,
		TEXT("TEST ONLY: do not free a human-driven pawn that is embedded in solid geometry."), ECVF_Cheat);
}

void AMOCharacter::CheckEmbeddedSafety(float DeltaTime)
{
	// Server decides where a pawn is; only HUMAN-driven pawns (an AI colonist's own path-following copes, and testing every NPC would
	// cost collision queries for nothing). Not during a load: terrain is still regenerating and the pawns are held in place.
	if (!HasAuthority() || bSuppressFallThroughDuringLoad || !IsPlayerControlled() || CVarDisableEmbeddedRescue.GetValueOnGameThread())
	{
		EmbeddedSeconds = 0.f;
		return;
	}

	EmbeddedCheckTimer += DeltaTime;
	if (EmbeddedCheckTimer < 1.0f)
	{
		return;
	}
	const float Elapsed = EmbeddedCheckTimer;
	EmbeddedCheckTimer = 0.f;

	UWorld* World = GetWorld();
	const MOSpawnClearance::FCapsule Capsule = MOSpawnClearance::CapsuleOf(this);
	FString Blocker;
	if (!World || !MOSpawnClearance::IsCapsuleBlocked(World, GetActorLocation(), Capsule, this, /*bIgnorePawns=*/true, &Blocker))
	{
		EmbeddedSeconds = 0.f;
		return;
	}

	// Brief overlaps happen (a door swinging, a building being placed); only a pawn that STAYS embedded is stuck.
	EmbeddedSeconds += Elapsed;
	if (EmbeddedSeconds < 3.0f)
	{
		return;
	}
	EmbeddedSeconds = 0.f;

	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, Capsule.HalfHeight);
	FVector Ground;
	if (MOSpawnClearance::FindClearGround(World, Feet, Capsule, this, Ground, 2500.0f))
	{
		const FVector Target = Ground + FVector(0.f, 0.f, Capsule.HalfHeight + 20.0f);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s is stuck inside %s -- moving it from %s to clear ground at %s"),
			*GetName(), *Blocker, *GetActorLocation().ToCompactString(), *Target.ToCompactString());
		TeleportTo(Target, GetActorRotation(), /*bIsATest=*/false, /*bNoCheck=*/true);
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Falling); // land normally
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s is stuck inside %s and no clear ground was found within 25 m"), *GetName(), *Blocker);
	}
}

namespace
{
	/** TEST ONLY control: the rescue's old behaviour (first blocking hit, whatever it is) -- proves the buried-pawn test can fail. */
	TAutoConsoleVariable<bool> CVarRescueFirstHitOnly(
		TEXT("MO.Rescue.FirstHitOnly"), false,
		TEXT("TEST ONLY: the fall-through rescue uses a single first-hit trace (the original bug) instead of skipping non-voxel blockers."), ECVF_Cheat);
}

bool AMOCharacter::TraceTerrainLine(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (bSafetyTeleportOnlyVoxelTerrain && !CVarRescueFirstHitOnly.GetValueOnGameThread())
	{
		return MOSpawnClearance::TraceVoxelGround(World, Start.X, Start.Y, Start.Z, End.Z, this, OutHit);
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (!World->LineTraceSingleByChannel(OutHit, Start, End, ECC_WorldStatic, QueryParams))
	{
		return false;
	}
	// First-hit mode: with the voxel-only rule on, a hit on anything else is "no terrain" (the legacy, buggy answer); with it off, any hit is ground.
	return !bSafetyTeleportOnlyVoxelTerrain || (OutHit.GetActor() && OutHit.GetActor()->IsA<AVoxelWorld>());
}

bool AMOCharacter::FindSafeTerrainNearLocation(const FVector& NearLocation, FVector& OutSafeLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Helper lambda to check if a hit is valid voxel terrain with acceptable slope (the traces below go through TraceTerrainLine, which already
	// skips non-voxel blockers; the voxel test here stays as the single place that also states the slope rule)
	auto IsValidTerrainHit = [this](const FHitResult& Hit) -> bool
	{
		if (bSafetyTeleportOnlyVoxelTerrain)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || !HitActor->IsA<AVoxelWorld>())
			{
				return false;
			}
		}

		// Check slope - normal Z must be above threshold
		if (Hit.ImpactNormal.Z < SafetyMinSlopeNormalZ)
		{
			return false;
		}

		return true;
	};

	// Every spot this returns must be somewhere the pawn's CAPSULE fits. The traces below are thin lines: one that passes a few
	// centimetres from a tree trunk hits terrain, but the capsule overlaps the trunk, and the rescue used to put the pawn into the
	// tree where it stayed stuck (reported by Wes). Re-grounds on the voxel when it has to move.
	const MOSpawnClearance::FCapsule Capsule = MOSpawnClearance::CapsuleOf(this);
	auto ClearGroundFor = [&](const FHitResult& Hit, FVector& OutGround, float& OutNormalZ) -> bool
	{
		OutGround = Hit.Location;
		OutNormalZ = Hit.ImpactNormal.Z;
		if (MOSpawnClearance::IsStandingSpotClear(World, Hit.Location, Capsule, this))
		{
			return true;
		}
		FVector Moved;
		if (MOSpawnClearance::FindClearGround(World, Hit.Location, Capsule, this, Moved, 1500.0f))
		{
			OutGround = Moved;
			OutNormalZ = 1.0f; // FindClearGround only returns ground it vetted for slope
			return true;
		}
		return false;
	};

	const float RaycastStartZ = 50000.0f;

	// FIRST: try the immediate area directly above (handles "stuck below terrain"
	// — pawn is inside a voxel chunk and needs to be lifted out).
	{
		FHitResult UpHit;
		const FVector UpTraceStart = NearLocation;
		const FVector UpTraceEnd = NearLocation + FVector(0.f, 0.f, SafetyTerrainSearchDistance);

		if (TraceTerrainLine(UpTraceStart, UpTraceEnd, UpHit))
		{
			FVector Ground;
			float NormalZ;
			if (IsValidTerrainHit(UpHit) && ClearGroundFor(UpHit, Ground, NormalZ))
			{
				const float SlopeMultiplier = 1.0f + (1.0f - NormalZ) * 2.5f;
				OutSafeLocation = Ground + FVector(0.f, 0.f, SafetyTeleportHeight * SlopeMultiplier);
				return true;
			}
		}
	}

	// SECOND: trace straight DOWN from high above the pawn's X/Y — this is the
	// common case (pawn fell through, terrain is below). CRITICAL: this MUST
	// run before any lateral search. The saved position is authoritative —
	// if terrain exists directly below the pawn's X/Y, snap there. Don't
	// laterally relocate the pawn (that's how loads ended up 20m off the
	// saved spot — the lateral search picked a random "flatter" spot in a
	// 20m ring instead of just dropping the pawn straight down).
	{
		const FVector DownStart(NearLocation.X, NearLocation.Y, NearLocation.Z + RaycastStartZ);
		const FVector DownEnd(NearLocation.X, NearLocation.Y, NearLocation.Z - RaycastStartZ);

		FHitResult DownHit;
		if (TraceTerrainLine(DownStart, DownEnd, DownHit))
		{
			FVector Ground;
			float NormalZ;
			if (IsValidTerrainHit(DownHit) && ClearGroundFor(DownHit, Ground, NormalZ))
			{
				const float SlopeMultiplier = 1.0f + (1.0f - NormalZ) * 2.5f;
				OutSafeLocation = Ground + FVector(0.f, 0.f, SafetyTeleportHeight * SlopeMultiplier);
				return true;
			}
		}
	}

	// LAST RESORT: no terrain at the pawn's exact X/Y — only NOW try a small
	// lateral search. Default radius reduced to a sane value (was 2000cm = 20m
	// which caused saved-position-loss). Use smaller search radius so even the
	// worst case is a few meters, not a screen-clear teleport.
	const float LastResortRadius = FMath::Min(SafetyLateralSearchRadius, 500.0f); // cap at 5m

	FVector BestLocation = FVector::ZeroVector;
	float BestSlopeZ = -1.0f;
	bool bFoundValid = false;

	for (int32 Sample = 0; Sample < SafetyLateralSearchSamples; ++Sample)
	{
		const float Angle = (static_cast<float>(Sample) / SafetyLateralSearchSamples) * 2.0f * PI;
		const FVector SampleXY(
			NearLocation.X + FMath::Cos(Angle) * LastResortRadius,
			NearLocation.Y + FMath::Sin(Angle) * LastResortRadius,
			RaycastStartZ
		);
		const FVector TraceEnd = SampleXY - FVector(0.f, 0.f, RaycastStartZ * 2.f);

		FHitResult Hit;
		if (TraceTerrainLine(SampleXY, TraceEnd, Hit))
		{
			FVector Ground;
			float NormalZ;
			if (IsValidTerrainHit(Hit) && ClearGroundFor(Hit, Ground, NormalZ))
			{
				if (NormalZ > BestSlopeZ)
				{
					BestSlopeZ = NormalZ;
					const float SlopeMultiplier = 1.0f + (1.0f - NormalZ) * 2.5f;
					BestLocation = Ground + FVector(0.f, 0.f, SafetyTeleportHeight * SlopeMultiplier);
					bFoundValid = true;
				}
			}
		}
	}

	if (bFoundValid)
	{
		OutSafeLocation = BestLocation;
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOCharacter] %s: fell back to lateral terrain search (radius %.0fcm) — direct-down had no terrain at X/Y"),
			*GetName(), LastResortRadius);
		return true;
	}

	return false;
}

void AMOCharacter::NotifyControllerChanged()
{
	// The pawn's record: a player leaving it, or taking it back ("while you were away", N-091). Read before Super, which overwrites PreviousController.
	if (HasAuthority())
	{
		if (UMOCharacterHistoryComponent* History = FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			History->HandleControllerChange(PreviousController, Controller);
		}
	}
	Super::NotifyControllerChanged();

	if (VoxelCollisionInvoker)
	{
		const bool bHumanDriven = Controller && Controller->IsPlayerController();
		VoxelCollisionInvoker->bEnabled = bHumanDriven;
		// DEBUG (MOCharacter, disabled Oct 18): re-enable to trace verbose detail
		// UE_LOG(LogMOFramework, Verbose, TEXT("[MOCharacter] %s: voxel collision invoker %s (controller %s)"),
		// 	*GetName(), bHumanDriven ? TEXT("ON") : TEXT("off"), Controller ? *Controller->GetName() : TEXT("none"));
	}
	UpdateRemotePlayerTerrainDetail();
}

void AMOCharacter::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);
	UpdateRemotePlayerTerrainDetail();
}

void AMOCharacter::UpdateRemotePlayerTerrainDetail()
{
	if (!RemotePlayerTerrainDetail)
	{
		return;
	}
	const APlayerState* PS = GetPlayerState();
	const bool bRemotePlayer = PS && !PS->IsABot() && !IsLocallyControlled() && GetNetMode() != NM_Standalone;
	if (RemotePlayerTerrainDetail->bEnabled != bRemotePlayer)
	{
		RemotePlayerTerrainDetail->bEnabled = bRemotePlayer;
		UE_LOG(LogMOFramework, Log, TEXT("[MOCharacter] %s: fine terrain around this remote player %s"), *GetName(), bRemotePlayer ? TEXT("ON") : TEXT("off"));
	}
}

namespace
{
	/** How far under a remote pawn's capsule this machine looks for a floor before letting its proxy simulate gravity (N-160). */
	constexpr double RemotePawnFloorReachCm = 200.0;

	/** TEST ONLY control: let simulated proxies predict falls like the stock engine (the original bug). */
	TAutoConsoleVariable<bool> CVarRemotePawnSimGravity(
		TEXT("MO.RemotePawn.SimGravity"), false,
		TEXT("TEST ONLY: remote players' pawns on a client apply simulated gravity (stock engine behaviour; with no local floor they fall forever)."), ECVF_Cheat);
}

void AMOCharacter::OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation)
{
	Super::OnUpdateSimulatedPosition(OldLocation, OldRotation); // resets bSimGravityDisabled, then sets it only if the proxy is encroaching geometry
	if (GetLocalRole() != ROLE_SimulatedProxy || CVarRemotePawnSimGravity.GetValueOnGameThread() || bSimGravityDisabled)
	{
		return;
	}
	// A remote pawn with no floor under it ON THIS MACHINE (its terrain not built here yet) must not predict a fall: it would free-fall forever
	// (the host's pawn fell to z = -498,790 on clients). But disabling sim gravity on EVERY proxy (until Oct 19) made the engine's SimulateMovement
	// treat it as stuck and zero its velocity each tick: remote players read as standing still and were never extrapolated between updates (N-160).
	// So: no gravity only when nothing is under the pawn within reach.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!Move || !Move->UpdatedComponent || !Capsule || !World)
	{
		bSimGravityDisabled = true;
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MORemotePawnFloor), false, this);
	FCollisionResponseParams Response;
	Move->InitCollisionParams(Params, Response);
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.0, 0.0, Capsule->GetScaledCapsuleHalfHeight() + RemotePawnFloorReachCm);
	bSimGravityDisabled = !World->LineTraceTestByChannel(Start, End, Move->UpdatedComponent->GetCollisionObjectType(), Params, Response);
}

void AMOCharacter::SetLoadHold(bool bHold)
{
	bSuppressFallThroughDuringLoad = bHold;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	if (bHold)
	{
		if (Move->MovementMode != MOVE_None)
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_None);
			bMovementHeldByLoad = true;
		}
	}
	else if (bMovementHeldByLoad)
	{
		bMovementHeldByLoad = false;
		if (Move->MovementMode == MOVE_None)
		{
			Move->SetDefaultMovementMode();
		}
	}
}

void AMOCharacter::CheckFallThroughSafety(float DeltaTime)
{
	// A character's position is SERVER-authoritative (CharacterMovement replicates it; clients only predict). A client
	// has voxel collision only around its own camera, so it sees every other pawn -- and its own, until local terrain
	// streams in -- as "falling with no terrain below". This rescue then teleported the pawn to a fixed low height
	// (SafetyTeleportHeight), under the real terrain, every couple of seconds: other players' characters were yanked
	// underground on every client (measured: 237 teleports in ~6 minutes on one client, 0 on the host). Only the
	// authority, which owns the pawn's real position, may decide it has fallen out of the world.
	if (!HasAuthority())
	{
		ContinuousFallTime = 0.f;
		return;
	}

	// Suppressed during world load: voxel collision hasn't generated yet, so
	// the pawn LOOKS like it's falling through the world — but it's just
	// waiting for terrain. Triggering the "find safe terrain" rescue here
	// teleports the pawn ~20m off its saved position. Bail and keep the
	// fall timer at zero so we don't fire the moment suppression lifts.
	if (bSuppressFallThroughDuringLoad)
	{
		ContinuousFallTime = 0.f;
		return;
	}

	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if (!MovementComp)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float CurrentZ = CurrentLocation.Z;
	const float VerticalVelocity = GetVelocity().Z;

	if (MovementComp->IsMovingOnGround())
	{
		LastGroundedLocation = CurrentLocation; // the one place we KNOW the pawn was on solid ground
		bHasLastGrounded = true;
	}

	// Check if we're falling (in air with significant downward velocity)
	const bool bIsFalling = MovementComp->IsFalling() && VerticalVelocity < FallThroughVelocityThreshold;

	if (bIsFalling)
	{
		ContinuousFallTime += DeltaTime;

		// Only check for fall-through after exceeding time threshold
		if (ContinuousFallTime >= FallThroughTimeThreshold)
		{
			// Is there valid terrain below the pawn? (TraceTerrainLine skips non-voxel blockers: the first thing under a pawn is often a tree,
			// a prop or the PCG volume brush, and that must not read as "no terrain".)
			FHitResult DownHit;
			const FVector TraceStart = CurrentLocation;
			const FVector TraceEnd = CurrentLocation - FVector(0.f, 0.f, SafetyTerrainSearchDistance);
			const bool bFoundValidTerrainBelow = TraceTerrainLine(TraceStart, TraceEnd, DownHit);

			if (!bFoundValidTerrainBelow)
			{
				// No valid terrain below - search for safe location
				FVector SafeLocation;
				if (FindSafeTerrainNearLocation(CurrentLocation, SafeLocation))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: Fall-through detected! Found safe terrain, teleporting to %s"),
						*GetName(), *SafeLocation.ToString());
				}
				else
				{
					// No valid terrain found anywhere - say WHAT is in the way (first few times), then teleport to a default safe height.
					// This line is the evidence a bug report's log tail needs: a rescue that cannot find ground is invisible otherwise (the host's pawn
					// sat under the terrain for minutes and every client saw it as missing).
					if (RescueFailLogCount < 3)
					{
						++RescueFailLogCount;
						TArray<FHitResult> SkyHits;
						FCollisionQueryParams SkyParams;
						SkyParams.AddIgnoredActor(this);
						GetWorld()->LineTraceMultiByChannel(SkyHits, CurrentLocation + FVector(0.f, 0.f, 50000.f), CurrentLocation - FVector(0.f, 0.f, 50000.f), ECC_WorldStatic, SkyParams);
						FString Described;
						for (int32 i = 0; i < SkyHits.Num() && i < 6; ++i)
						{
							Described += FString::Printf(TEXT(" [%d] %s/%s z=%.0f nZ=%.2f"), i, *GetNameSafe(SkyHits[i].GetActor()), *GetNameSafe(SkyHits[i].GetComponent()),
								SkyHits[i].ImpactPoint.Z, SkyHits[i].ImpactNormal.Z);
						}
						UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: rescue found no ground at %s; blocking hits on the sky line (top first):%s"),
							*GetName(), *CurrentLocation.ToCompactString(), SkyHits.Num() ? *Described : TEXT(" none"));
					}
					// Back to where the pawn last stood on the ground when it has stood anywhere; the fixed height is only the last resort (it is a
					// constant, and on this map the ground is often above it -- a pawn put at Z=200 under 1500+ cm of terrain just falls again).
					SafeLocation = bHasLastGrounded
						? LastGroundedLocation + FVector(0.f, 0.f, SafetyTeleportHeight * 0.25f)
						: FVector(CurrentLocation.X, CurrentLocation.Y, SafetyTeleportHeight);
					UE_LOG(LogMOFramework, Warning, TEXT("[MOCharacter] %s: Fall-through detected! No valid terrain found, teleporting to %s Z=%.0f"),
						*GetName(), bHasLastGrounded ? TEXT("where it last stood") : TEXT("default height"), SafeLocation.Z);
				}

				// Teleport to safety
				SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);

				// Reset velocity to prevent continued falling
				MovementComp->StopMovementImmediately();

				// Reset fall tracking
				ContinuousFallTime = 0.f;

#if !UE_BUILD_SHIPPING
				// Log warning for debugging
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
						FString::Printf(TEXT("Fall-through safety activated! Teleported to %s"), *SafeLocation.ToString()));
				}
#endif
			}
			else
			{
				// There's valid terrain below - we're just falling normally
				// Reset timer only if we're getting close to it (within 1000 units)
				if (DownHit.Distance < 1000.f)
				{
					ContinuousFallTime = 0.f;
				}
			}
		}
	}
	else
	{
		// Not falling (or not falling fast enough) - reset timer
		ContinuousFallTime = 0.f;
	}

	LastZPosition = CurrentZ;
}

// ============================================================================
// TREATED BY ANOTHER PAWN (IMOWorldActionProvider, N-061 co-op, Oct 20 2026)
// ============================================================================

FText AMOCharacter::GetWorldActionTitle() const
{
	const FText Name = IdentityComponent && !IdentityComponent->DisplayName.IsEmpty() ? IdentityComponent->DisplayName : FText::FromString(GetName());
	return Name;
}

void AMOCharacter::GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const
{
	// Wounds replicate to every machine (UMOAnatomyComponent), and so does a carried flame: the menu is built where it shows, from the same
	// checks the server runs again
	if (!Pawn || Pawn == this || bIsDead)
	{
		return;
	}
	FMOFireActions::GetBearerActions(Pawn, this, Out);
	if (!AnatomyComponent)
	{
		return;
	}
	for (const FMOWound& Wound : AnatomyComponent->GetAllWounds())
	{
		TArray<FMOItemActionOption> Options;
		FMOMedicalTreatmentActions::GetOptions(const_cast<APawn*>(Pawn), const_cast<AMOCharacter*>(this), Wound, Options);
		for (FMOItemActionOption& Option : Options)
		{
			Option.Label = FText::Format(NSLOCTEXT("MOCharacter", "TreatOption", "{0} {1}: {2}"), UEnum::GetDisplayValueAsText(Wound.BodyPart),
				UEnum::GetDisplayValueAsText(Wound.WoundType), Option.Label);
			Option.Id = FName(*FString::Printf(TEXT("%s|%s"), *Wound.WoundId.ToString(EGuidFormats::Digits), *Option.Id.ToString()));
			Out.Add(MoveTemp(Option));
		}
	}
}

bool AMOCharacter::PerformWorldAction(APawn* Pawn, FName ActionId)
{
	if (ActionId == FMOFireActions::LightFromBearer)
	{
		return FMOFireActions::PerformBearerAction(Pawn, this, ActionId);
	}
	FString WoundText, TreatmentText;
	FGuid WoundId;
	if (!ActionId.ToString().Split(TEXT("|"), &WoundText, &TreatmentText) || !FGuid::Parse(WoundText, WoundId))
	{
		return false;
	}
	const FMOMedicalActionResult Result = FMOMedicalTreatmentActions::Begin(Pawn, this, WoundId, FName(*TreatmentText));
	if (!Result.IsAccepted())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTreatment] %s treating %s: %s refused: %s"), *GetNameSafe(Pawn), *GetName(), *TreatmentText, *Result.Describe().ToString());
	}
	return Result.IsAccepted();
}
