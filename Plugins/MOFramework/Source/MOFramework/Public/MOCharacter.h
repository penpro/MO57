/**
 * =============================================================================
 * MOCharacter.h - Base Character with Framework Components
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE THIS HEADER when issues arise or patterns change
 *
 * PURPOSE:
 * Base character class with all framework components attached. Implements
 * multiple interfaces for controllability, inventory, crafting, medical,
 * identity, and material sourcing. Both player and AI pawns inherit from this.
 *
 * KEY RESPONSIBILITIES:
 * 1. Create and own all standard framework components
 * 2. Implement IMOControllableInterface for controller delegation
 * 3. Handle movement modes (walk/jog/sprint) with physiology effects
 * 4. Manage held item visuals based on equipment
 * 5. Handle terraforming mode and tool cycling
 *
 * COMPONENT OWNERSHIP (created in constructor):
 * - IdentityComponent: GUID-based persistence identity
 * - InventoryComponent: Item storage
 * - InteractorComponent: Interaction traces
 * - SurvivalStatsComponent: Basic survival needs
 * - SkillsComponent: Skill levels and XP
 * - KnowledgeComponent: Learned knowledge
 * - VitalsComponent: Blood, O2, temp, etc.
 * - MetabolismComponent: Digestion, nutrition
 * - MentalStateComponent: Morale, stress
 * - AnatomyComponent: Body parts, wounds
 * - AdrenalineComponent: Combat stress
 * - CraftingQueueComponent: Timed crafting
 * - RecipeDiscoveryComponent: Recipe unlocks
 * - EquipmentComponent: Body/hand slots
 * - RecruitmentComponent: NPC recruitment state
 * - JobQueueComponent: Survivor task queue
 * - TerraformingComponent: Terrain modification
 *
 * INTERFACES IMPLEMENTED:
 * - IMOControllableInterface: Controller input delegation
 * - IMOInventoryHolderInterface: Inventory access
 * - IMOMaterialSourceInterface: Crafting material gathering
 * - IMOIdentifiableInterface: GUID identity
 * - IMOCraftingCapableInterface: Crafting queue access
 * - IMOMedicalProviderInterface: Medical component access
 *
 * CRITICAL PATTERNS:
 * 1. Movement Modes:
 *    Walking (default) -> ToggleJog() -> Jogging
 *    Hold Hustle -> StartSprint() -> Sprinting
 *    Release -> StopSprint() -> Back to Jog/Walk
 *
 * 2. Physiology Integration:
 *    Movement mode -> MET value -> Metabolism calorie burn
 *    Movement mode -> Exertion -> Vitals stamina drain
 *    Movement mode -> Temp rise -> Body temperature
 *
 * 3. Equipment Visuals:
 *    EquipmentComponent.OnEquipmentChanged -> HandleEquipmentChanged()
 *    -> UpdateHeldItemMesh() -> Attach mesh to hand socket
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] VOXEL COMPATIBILITY: SetBase() override required for Voxel Plugin.
 *   Without it, character teleports when voxel world updates. Loads VoxelWorld
 *   class dynamically to avoid hard module dependency.
 *
 * [2024-02] COMPONENT INIT ORDER: Components created in constructor must follow
 *   this order (dependencies flow downward):
 *   1. IdentityComponent (no deps - provides GUID for all others)
 *   2. InventoryComponent (no deps)
 *   3. InteractorComponent (no deps)
 *   4. SkillsComponent (no deps)
 *   5. KnowledgeComponent (no deps)
 *   6. VitalsComponent (no deps)
 *   7. MetabolismComponent (reads from VitalsComponent in Tick)
 *   8. AnatomyComponent (writes to VitalsComponent)
 *   9. MentalStateComponent (reads VitalsComponent)
 *   10. AdrenalineComponent (reads MentalStateComponent)
 *   11. EquipmentComponent (reads InventoryComponent)
 *   If reordering, verify component Tick dependencies in .cpp.
 *
 * [2024-02] DELEGATE BINDING: In BeginPlay(), delegates are bound for:
 *   - KnowledgeComponent->OnKnowledgeLearned -> HandleKnowledgeLearned()
 *   - SkillsComponent->OnSkillLevelUp -> HandleSkillLevelUp()
 *   - RecipeDiscoveryComponent->OnRecipeDiscovered -> HandleRecipeDiscovered()
 *   - EquipmentComponent->OnEquipmentChanged -> HandleEquipmentChanged()
 *   SYMPTOM if duplicate binding: Handler fires multiple times per event,
 *   causing duplicate recipe notifications, double XP awards, etc.
 *   FIX: Use RemoveAll(this) before AddDynamic in BeginPlay.
 *
 * [2024-02] NETWORK AUTHORITY: IMOControllableInterface methods (RequestMove,
 *   RequestJump, etc.) are called on the owning client only. DoMove/DoLook are
 *   direct input methods also called client-side.
 *   SYMPTOM if called on server for remote pawns: Character movement desyncs,
 *   pawn rubber-bands back to last valid position, or input is ignored entirely.
 *   Movement replication handled by CharacterMovementComponent automatically.
 *   RULE: Only call input methods from owning client's PlayerController.
 *
 * [2024-02] FALL-THROUGH SAFETY: Character can fall through voxel terrain.
 *   Safety teleport activates after FallThroughTimeThreshold (2s default) of
 *   continuous falling below FallThroughVelocityThreshold (-500 default).
 *   FindSafeTerrainNearLocation() searches for valid voxel surface.
 *
 * [2024-02] SPRINT STAMINA: CanSprint_Implementation() checks stamina via
 *   VitalsComponent->GetCurrentStamina(). If below MinStaminaToSprint (10.0),
 *   sprint fails to start. StopSprint() called automatically when stamina
 *   depletes during sprint (checked in ApplyMovementPhysiologyEffects).
 *
 * [2024-02] HELD ITEM VISUALS: LeftHandMesh/RightHandMesh are StaticMeshComponents
 *   attached to sockets (hand_l/hand_r). HandleEquipmentChanged() updates these
 *   via UpdateHeldItemMesh(). If mesh doesn't appear, verify socket names match
 *   skeleton and item definition has valid StaticMesh set. Since Oct 25 2026
 *   (N-210) worn rigid gear too: HeadWornMesh / BackWornMesh on the head and
 *   spine_05 bones at the item's HeldVisual.WornTransform (GetItemVisualTarget).
 *
 * [2024-02] SOFT OBJECT LOADING: DefaultMesh and DefaultAnimBlueprint are
 *   TSoftObjectPtr/TSoftClassPtr. Call LoadSynchronous() before use. BeginPlay
 *   loads these if set.
 *   SYMPTOM if asset path invalid: Character appears as gray capsule (no mesh)
 *   or T-poses (no anim). Check Output Log for "Failed to load" warnings.
 *   DEBUG: Log DefaultMesh.GetAssetName() to verify path before loading.
 *   Null asset path (not set) is valid - uses inherited mesh from Blueprint.
 *
 * [2024-02] INTERFACE NULL SAFETY: All interface methods assume components exist.
 *   GetInventory_Implementation() returns InventoryComponent directly (no null check).
 *   SYMPTOM if component null: Hard crash on interface call.
 *   Components are created in constructor, so null only occurs if subclass
 *   removes component or constructor fails. Check ensure() in constructor.
 *
 * [2024-02] BEGINPLAY TIMING: Components are available in BeginPlay but may not
 *   have completed their own BeginPlay yet. Cross-component queries in BeginPlay
 *   may return stale/default data. Use timers or delegate binding for init that
 *   depends on other components being fully initialized.
 *
 * RELATED FILES:
 * - MOPlayerController.h - Controls this character
 * - MOControllableInterface.h - Input delegation interface
 * - All medical components (Vitals, Anatomy, etc.)
 * - MOEquipmentComponent.h - Equipment slots
 *
 * TESTING CHECKLIST:
 * [ ] Movement modes switch correctly
 * [ ] Physiology effects apply during movement
 * [ ] Equipment changes update held item meshes
 * [ ] Fall-through safety teleports character
 * [ ] All interface methods work correctly
 * [ ] Component delegates wire up in BeginPlay
 *
 * LAST UPDATED: 2026-02-24 - Initial audit header
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "MOTraversal.h"
#include "MOControllableInterface.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOIdentifiableInterface.h"
#include "MOCraftingCapableInterface.h"
#include "MOMedicalProviderInterface.h"
#include "MOWorldActionProvider.h"
#include "MOCraftingTypes.h"
#include "MOBodyPartTypes.h"
#include "MOInterruptTypes.h"
#include "MOVitalsComponent.h"   // EMOSystemicFailure (HandleSystemicDeath)
#include "MOBodySex.h"
#include "MOCharacter.generated.h"

/**
 * Broadcast when this character transitions to dead state (bIsDead becomes
 * true via HandleInstantDeath). Listeners — death recap UI, kill notifications,
 * spectator-cam handoff — subscribe here.
 *
 * Fires AFTER bIsDead is set but BEFORE input/movement/ragdoll teardown, so
 * handlers see a "freshly dead" snapshot of the character's state. The
 * delegate is single-fire per pawn (HandleInstantDeath early-outs if already
 * dead), so subscribers don't need to guard against duplicate dispatches.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMOOnPawnDied, AMOCharacter*, DeadCharacter, EMOBodyPartType, CausePart);

/** Every character's death, on every machine where it enters the dead state (AMOCharacter::OnAnyPawnDied). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMOOnAnyPawnDied, AMOCharacter* /*DeadCharacter*/, EMOBodyPartType /*CausePart*/);

/**
 * What killed a character, told apart at the source (the body part alone cannot: a drowning read "Brain destroyed", a bleed-out "None destroyed").
 * Set by AMOCharacter::DieOf before the one death path runs; read with GetDeathCause (the death recap does).
 */
UENUM(BlueprintType)
enum class EMODeathCause : uint8
{
	/** A vital body part was destroyed (anatomy). */
	BodyPartDestroyed,
	/** The airway was closed until the body failed (drowning). */
	Drowned,
	/** Circulatory arrest from blood loss. */
	BledOut,
	/** The heart stopped. */
	CardiacArrest,
	/** Breathing / oxygenation failed until the heart followed. */
	RespiratoryFailure,
	/** The core overheated for too long (EMOSystemicFailure::HeatStroke). */
	HeatStroke,
	/** The heart stopped with the core in deep hypothermia (under 30 C): froze to death. */
	Hypothermia,
};

/**
 * A death as every machine sees it: the server writes it once (AMOCharacter::HandleInstantDeath) and each client enters the dead state from
 * it (OnRep_DeathRecord -> EnterDeadState). Before Oct 18 the whole death ran on the server only: a joiner who died got no death recap
 * and stayed upright on every screen (N-064).
 */
USTRUCT()
struct FMODeathRecord
{
	GENERATED_BODY()

	UPROPERTY()
	bool bDead = false;

	UPROPERTY()
	EMODeathCause Cause = EMODeathCause::BodyPartDestroyed;

	UPROPERTY()
	EMOBodyPartType Part = EMOBodyPartType::None;

	/** Who dealt the last wound before the death ("Wolf", a survivor's name); empty = no attacker (drowning, illness, a fall). N-169. */
	UPROPERTY()
	FText KillerName;
};

/**
 * Movement mode affecting speed and physiological effects.
 */
UENUM(BlueprintType)
enum class EMOMovementMode : uint8
{
	Walking,	// Default walking speed
	Jogging,	// Faster sustained movement (toggle)
	Sprinting	// Maximum speed (hold)
};

class UMOIdentityComponent;
class UMOInventoryComponent;
class UMOInteractorComponent;
class UMOInteractableComponent;
class UMOSurvivalStatsComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
class UMOAnatomyComponent;
class UMOAdrenalineComponent;
class UMORespirationComponent;
class UMOFootstepComponent;
class UAnimSequence;
class UAnimMontage;
class UAnimInstance;
class UIKRetargeter;
class USkeletalMesh;
class UMOCharacterMovementComponent;
class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
class UMOTerraformingComponent;
class UMOEquipmentComponent;
class UMORecruitmentComponent;
class UMOSurvivorJobQueueComponent;
class UMOPersonalityComponent;
class UMOCharacterHistoryComponent;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMesh;
class UAnimInstance;
class UStaticMeshComponent;
class USphereComponent;
class UNavigationInvokerComponent;
class UVoxelCollisionInvokerComponent;
class UVoxelLODVolumeComponent;
struct FMOEquippedItem;
enum class EMOEquipmentSlot : uint8;

/** One of GASP's traversal sequences for one of our moves (GASP phase 2, Docs/Animation_GASP.md): played on the locomotion driver, its rise --
 *  the body leaving the ground to standing up on top, measured from its root curve by Content/Python/setup_gasp_survivor.py -- fitted to the
 *  move's own duration. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOTraversalAnim
{
	GENERATED_BODY()

	/** A sequence on the driver's skeleton (GASP's UEFN mannequin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Animation")
	TSoftObjectPtr<UAnimSequence> Sequence;

	/** Where the rise starts and ends in it (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Animation")
	float RiseStartSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Animation")
	float RiseEndSeconds = 1.0f;
};

/** The server's word to the other machines that a move started (they see only the movement mode, not the move): its kind (EMOTraversal) and
 *  duration. Serial makes every move a change. */
USTRUCT()
struct MOFRAMEWORK_API FMOTraversalCue
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Kind = 0;

	UPROPERTY()
	uint8 Serial = 0;

	UPROPERTY()
	float Duration = 0.0f;
};

/** What the climb in hand costs this body now (AMOCharacter::GetClimbEffort; F51). */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOClimbEffort
{
	GENERATED_BODY()

	/** The face's angle from level ground (FMOTraversal::FaceAngleDeg): 90 sheer, less a slab, more an overhang. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Traversal")
	float FaceAngleDeg = 90.0f;

	/** The face's share of a sheer face's cost (ClimbingCostByFaceAngle). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Traversal")
	float CostShare = 1.0f;

	/** What the climb spends: ClimbingStaminaCostPerSecond x CostShare x CostScale. */
	UPROPERTY(BlueprintReadOnly, Category="MO|Traversal")
	float StaminaPerSecond = 0.0f;

	/** ClimbingExertionByFaceAngle: drives the vitals and stamina recovery (none from 55 up). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Traversal")
	float Exertion = 70.0f;

	/** The climber's own scales (FMOTraversal::ClimbAbility: the Climbing level, the muscle share). */
	UPROPERTY(BlueprintReadOnly, Category="MO|Traversal")
	float CostScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|Traversal")
	float SpeedScale = 1.0f;
};

/**
 * Base character class for the MO Framework.
 * Implements IMOControllableInterface to receive input from AMOPlayerController.
 *
 * All player-controlled and AI-controlled pawns should inherit from this class.
 */
UCLASS()
class MOFRAMEWORK_API AMOCharacter : public ACharacter,
	public IMOControllableInterface,
	public IMOInventoryHolderInterface,
	public IMOMaterialSourceInterface,
	public IMOIdentifiableInterface,
	public IMOCraftingCapableInterface,
	public IMOMedicalProviderInterface,
	public IMOWorldActionProvider
{
	GENERATED_BODY()

public:
	/** Takes the object initializer so every pawn class swaps in UMOCharacterMovementComponent (water: wading, swimming, buoyancy). */
	AMOCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	/**
	 * Soft assets this pawn class loads at RUNTIME (behavior trees, tables resolved on possession / death), appended to Out. Called on the class
	 * default object by whoever spawns the class later (UMOSpawnManagerSubsystem), so they are loaded asynchronously behind the loading screen
	 * instead of with LoadSynchronous on the game thread at the spawn (a 60 ms frame per first creature of a kind, Oct 13 2026). The base adds its
	 * AI controller class's own (AMOAIController::GatherRuntimeAssets); a subclass adds what it resolves itself.
	 */
	virtual void GatherRuntimeAssets(TArray<FSoftObjectPath>& Out) const;

	// ============================================================================
	// VOXEL WORLD COMPATIBILITY
	// ============================================================================
	// Required for base replication to work with the Voxel Plugin: voxel actor
	// collision components are generated at runtime & not replicated, so when the
	// voxel world rebuilds we must re-base the character to the voxel world's ROOT
	// component instead of the transient collision sub-component. Without it the
	// character teleports when the voxel world updates, and on dev-phy 5.8 the
	// initial spawn lands wrong (lands in water).
	//
	// UE 5.8 changed ACharacter::SetBase's signature from UPrimitiveComponent* to
	// FMovementBaseInterfaceData*. We MUST override the new signature on 5.8 — an
	// override of only the old 5.7 signature is silently never called by the 5.8
	// engine (it dispatches to the new overload), which is exactly what was
	// breaking voxel basing. Engine-version gated (not VOXEL_ENGINE_VERSION) so
	// MOCharacter stays decoupled from the Voxel module; the VoxelWorld class is
	// resolved by path via LoadClass. Mirrors AVoxelCharacter::SetBase.
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	virtual void SetBase(FMovementBaseInterfaceData* MovementBaseInterfaceData, const FName BoneName, const bool bNotifyActor) override
	{
		FMovementBaseInterfaceData RedirectedData;
		if (MovementBaseInterfaceData && MovementBaseInterfaceData->IsValid())
		{
			static UClass* const VoxelWorldClass = LoadClass<UObject>(nullptr, TEXT("/Script/Voxel.VoxelWorld"));

			UObject* BaseOwner = MovementBaseInterfaceData->GetMovementBaseObjectOwner();
			if (ensure(VoxelWorldClass) && BaseOwner && BaseOwner->IsA(VoxelWorldClass))
			{
				const AActor* BaseActor = Cast<AActor>(BaseOwner);
				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(BaseActor->GetRootComponent()))
				{
					RedirectedData = FMovementBaseInterfaceData(Root);
					MovementBaseInterfaceData = &RedirectedData;
				}
			}
		}

		Super::SetBase(MovementBaseInterfaceData, BoneName, bNotifyActor);
	}
#else
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName, const bool bNotifyActor) override
	{
		if (NewBase)
		{
			static UClass* const VoxelWorldClass = LoadClass<UObject>(nullptr, TEXT("/Script/Voxel.VoxelWorld"));

			const AActor* BaseOwner = NewBase->GetOwner();
			if (ensure(VoxelWorldClass) && BaseOwner && BaseOwner->IsA(VoxelWorldClass))
			{
				NewBase = Cast<UPrimitiveComponent>(BaseOwner->GetRootComponent());
				ensure(NewBase);
			}
		}

		Super::SetBase(NewBase, BoneName, bNotifyActor);
	}
#endif

	// ============================================================================
	// COMPONENT ACCESSORS
	// ============================================================================
	// Note: GetIdentityComponent() is provided by IMOIdentifiableInterface
	// Note: GetInventory() is provided by IMOInventoryHolderInterface

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInteractorComponent* GetInteractorComponent() const { return InteractorComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOSurvivalStatsComponent* GetSurvivalStatsComponent() const { return SurvivalStatsComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOSkillsComponent* GetSkillsComponent() const { return SkillsComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOKnowledgeComponent* GetKnowledgeComponent() const { return KnowledgeComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOVitalsComponent* GetVitalsComponent() const { return VitalsComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOMetabolismComponent* GetMetabolismComponent() const { return MetabolismComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOMentalStateComponent* GetMentalStateComponent() const { return MentalStateComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOAnatomyComponent* GetAnatomyComponent() const { return AnatomyComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOAdrenalineComponent* GetAdrenalineComponent() const { return AdrenalineComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMORespirationComponent* GetRespirationComponent() const { return RespirationComponent; }

	/** True while the movement component is in swimming mode (water above the chest, see UMOCharacterMovementComponent). */
	UFUNCTION(BlueprintPure, Category="MO|Water")
	bool IsSwimming() const;

	/** Climbing a cliff, a rock or a tree (UMOCharacterMovementComponent, MOTraversal.h). */
	UFUNCTION(BlueprintPure, Category="MO|Movement")
	bool IsClimbing() const;

	UFUNCTION(BlueprintPure, Category="MO")
	UMOCraftingQueueComponent* GetCraftingQueueComponent() const { return CraftingQueueComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMORecipeDiscoveryComponent* GetRecipeDiscoveryComponent() const { return RecipeDiscoveryComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOTerraformingComponent* GetTerraformingComponent() const { return TerraformingComponent; }

	UFUNCTION(BlueprintPure, Category="MO|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category="MO|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Apply game settings (FOV, sensitivity) to this character's camera. */
	UFUNCTION(BlueprintCallable, Category="MO|Camera")
	void ApplyGameSettings();

	/** Toggle which shoulder the camera looks over (swaps Y offset between positive and negative). */
	UFUNCTION(BlueprintCallable, Category="MO|Camera")
	void ToggleCameraShoulder();

	UFUNCTION(BlueprintPure, Category="MO")
	UMOEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMORecruitmentComponent* GetRecruitmentComponent() const { return RecruitmentComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOSurvivorJobQueueComponent* GetJobQueueComponent() const { return JobQueueComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOPersonalityComponent* GetPersonalityComponent() const { return PersonalityComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOCharacterHistoryComponent* GetHistoryComponent() const { return HistoryComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInteractableComponent* GetInteractableComponent() const { return InteractableComponent; }

	//~ IMOWorldActionProvider: what another pawn can do to this one (a right-click on it): treat its wounds (N-061 co-op, Oct 20; one option per
	//~ wound and treatment for it, FMOMedicalTreatmentActions::GetOptions, the server's own checks; self-treatment is Status > Wounds) and
	//~ light a torch from the one it carries burning (FMOFireActions::GetBearerActions, N-056).
	virtual FText GetWorldActionTitle() const override;
	virtual void GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const override;
	virtual bool PerformWorldAction(APawn* Pawn, FName ActionId) override;

	// ============================================================================
	// APPEARANCE
	// ============================================================================

	/** Set character skeletal mesh at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetCharacterMesh(USkeletalMesh* NewMesh);

	/** Set animation blueprint at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetAnimationBlueprint(TSubclassOf<UAnimInstance> NewAnimClass);

	/** The body's sex (MOBodySex.h): the class default here; a MetaHuman survivor on a base says its base's. Pronouns: FMOPronouns. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance")
	virtual EMOBodySex GetBodySex() const { return DefaultBodySex; }

	/** The sex of this class's own body (the one a pawn without a base shows). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="MO|Appearance")
	EMOBodySex DefaultBodySex = EMOBodySex::Unspecified;

	// ============================================================================
	// IMOControllableInterface IMPLEMENTATION
	// ============================================================================

	virtual void RequestMove_Implementation(FVector2D MovementInput) override;
	virtual void RequestLook_Implementation(FVector2D LookInput) override;
	virtual void RequestJumpStart_Implementation() override;
	virtual void RequestJumpEnd_Implementation() override;
	virtual void RequestSprintStart_Implementation() override;
	virtual void RequestSprintEnd_Implementation() override;
	virtual void RequestToggleJog_Implementation() override;
	virtual void RequestCrouchToggle_Implementation() override;
	virtual void RequestInteract_Implementation() override;
	virtual void RequestSecondaryInteract_Implementation() override;
	virtual void RequestPrimaryAction_Implementation() override;
	virtual void RequestPrimaryActionRelease_Implementation() override;
	virtual void RequestSecondaryAction_Implementation() override;
	virtual void RequestSecondaryActionRelease_Implementation() override;
	virtual void RequestTerraformToggle_Implementation() override;
	virtual void RequestTerraformCycleTool_Implementation() override;
	virtual bool CanBeControlled_Implementation() const override;
	virtual bool CanMove_Implementation() const override;
	virtual bool CanJump_Implementation() const override;
	virtual bool CanSprint_Implementation() const override;

	/** SERVER: the gait this character's moves carry (UMOCharacterMovementComponent::UpdateFromCompressedFlags), mirrored into the gait state (exertion,
	 *  metabolism, sprint stamina) -- a joiner's gait used to exist on the joiner's machine only. Does not write back to the movement component. */
	void ApplyGaitFromMovement(bool bJog, bool bSprint);

	// ============================================================================
	// IMOInventoryHolderInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOInventoryComponent* GetInventory_Implementation() const override;
	virtual bool HasInventoryItem_Implementation(FName ItemDefinitionId, int32 Quantity) const override;
	virtual int32 GetInventoryItemCount_Implementation(FName ItemDefinitionId) const override;

	// ============================================================================
	// IMOMaterialSourceInterface IMPLEMENTATION
	// ============================================================================

	virtual bool CanProvideMaterial_Implementation(FName MaterialId, int32 Quantity) const override;
	virtual int32 GatherMaterial_Implementation(FName MaterialId, int32 Quantity) override;
	virtual int32 GetMaterialSourcePriority_Implementation() const override;

	// ============================================================================
	// IMOIdentifiableInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOIdentityComponent* GetIdentityComponent_Implementation() const override;
	virtual FGuid GetPersistentGuid_Implementation() const override;
	virtual bool HasValidIdentity_Implementation() const override;

	// ============================================================================
	// IMOCraftingCapableInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOCraftingQueueComponent* GetCraftingQueue_Implementation() const override;
	virtual UMORecipeDiscoveryComponent* GetRecipeDiscovery_Implementation() const override;
	virtual void SetActiveCraftingStation_Implementation(AActor* StationActor) override;
	virtual AActor* GetActiveCraftingStation_Implementation() const override;

	// ============================================================================
	// IMOMedicalProviderInterface IMPLEMENTATION
	// ============================================================================

	virtual UMOVitalsComponent* GetVitals_Implementation() const override;
	virtual UMOAnatomyComponent* GetAnatomy_Implementation() const override;
	virtual UMOMentalStateComponent* GetMentalState_Implementation() const override;
	virtual UMOAdrenalineComponent* GetAdrenaline_Implementation() const override;
	virtual UMOMetabolismComponent* GetMetabolism_Implementation() const override;

	// ============================================================================
	// DIRECT INPUT (for Blueprint/UI virtual joysticks)
	// ============================================================================

	/** Direct movement input (for UI virtual joysticks, etc.). */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoMove(float Right, float Forward);

	/** Direct look input (for UI virtual joysticks, etc.). */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Direct jump start (for UI buttons, etc.). */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoJumpStart();

	/** Direct jump end. */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoJumpEnd();

	// ============================================================================
	// STATE QUERIES
	// ============================================================================

	/** Check if currently sprinting. */
	UFUNCTION(BlueprintPure, Category="MO|State")
	bool IsSprinting() const { return bIsSprinting; }

	/** Check if character is dead. */
	UFUNCTION(BlueprintPure, Category="MO|State")
	bool IsDead() const { return bIsDead; }

	/**
	 * The survivor is still in the character creator: not in the world yet, so no creature takes it for quarry (Wes, Oct 27 2026: "we need to
	 * make sure we're not getting mauled by bears during character creation"). Server state (the creatures' AI runs there); the creator panel
	 * sets it while it is open over the host's own pawn.
	 */
	UFUNCTION(BlueprintPure, Category="MO|State")
	bool IsInCharacterCreator() const { return bInCharacterCreator; }
	void SetInCharacterCreator(bool bInCreator);

	/** What killed this character (meaningful once IsDead). */
	UFUNCTION(BlueprintPure, Category="MO|State")
	EMODeathCause GetDeathCause() const { return DeathCause; }

	/** The body part tied to the death (the destroyed part; Brain / Heart / LungLeft for the systemic causes). */
	EMOBodyPartType GetDeathCausePart() const { return DeathCausePart; }

	/** Who killed this character ("Wolf", a survivor's name); empty when nothing did (drowning, illness). Replicated with the death. */
	FText GetDeathKillerName() const { return DeathRecord.KillerName; }

	/** Server, load path (N-155): this pawn's record is deceased -- lie here as the body, with its gear, through the one EnterDeadState. */
	void RestoreAsCorpse();

	/**
	 * Every death, broadcast right after the dead character's own OnPawnDied, on the server and on every client. A listener that cares about
	 * ONE pawn (the local player's death recap) filters here instead of binding to that pawn: a joiner's per-pawn binding was dropped by a
	 * possession settle (unpossess + re-possess with no pawn-changed callback on the client) and its recap never opened (Oct 18).
	 */
	static FMOOnAnyPawnDied OnAnyPawnDied;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** THE death path with its cause: records it, then runs HandleInstantDeath (virtual: creatures add their own teardown). Single-fire. */
	void DieOf(EMODeathCause Cause, EMOBodyPartType Part);

	/**
	 * Broadcast once on death — see FMOOnPawnDied above. Single-fire per
	 * pawn (HandleInstantDeath bails if bIsDead is already true).
	 */
	UPROPERTY(BlueprintAssignable, Category="MO|State")
	FMOOnPawnDied OnPawnDied;

	// ============================================================================
	// INTERRUPT EVENT BUS
	// ============================================================================
	//
	// Centralized broadcast for "something just happened to this character that
	// should interrupt ongoing activities." Sources call BroadcastInterrupt
	// with an FMOInterruptContext; listeners implement IMOInterruptibleInterface
	// and register/unregister with this character.
	//
	// Sources currently emitting interrupts on AMOCharacter:
	//   - Movement detection (this class, in Tick — emits EMOInterruptReason::Movement)
	//
	// Future sources should call BroadcastInterrupt from wherever the relevant
	// event already happens. Examples:
	//   - AnatomyComponent on a serious wound       -> Reason::Damage
	//   - AnatomyComponent on unconsciousness/death -> Reason::Unconscious / Death
	//   - CombatComponent when combat starts        -> Reason::EnteredCombat
	//   - Possession change                         -> Reason::LostControl
	//
	// Listeners decide policy per-reason (pause vs. cancel vs. ignore).

	/**
	 * Add a listener for all interrupt events on this character. The listener
	 * must implement IMOInterruptibleInterface. Safe to call with an already-
	 * registered listener (no-op).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Interrupts")
	void RegisterInterruptListener(UObject* Listener);

	/** Stop notifying the given listener. Safe to call with an unregistered listener. */
	UFUNCTION(BlueprintCallable, Category="MO|Interrupts")
	void UnregisterInterruptListener(UObject* Listener);

	/**
	 * Fire an interrupt event to every registered listener. Anyone — internal
	 * tick code, components on this character, world systems acting on this
	 * character — can call this. The broadcaster doesn't decide what
	 * "interrupt" means for any given listener; it just notifies.
	 *
	 * Listeners receive the context by const-ref and may unregister themselves
	 * from within the callback (the broadcast iterates a snapshot).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Interrupts")
	void BroadcastInterrupt(const FMOInterruptContext& Context);

	/**
	 * Velocity² above which the character is considered to be "moving" for
	 * the automatic Movement-reason broadcast. Default 25 ((5 cm/s)²) is
	 * conservative — physics jitter while standing still won't trip it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interrupts", meta=(ClampMin="0"))
	float MovementInterruptVelocitySqThreshold = 25.0f;

	// ============================================================================
	// TERRAFORMING
	// ============================================================================

	/** Check if terraforming mode is active. */
	UFUNCTION(BlueprintPure, Category="MO|Terraforming")
	bool IsTerraformingMode() const { return bTerraformingMode; }

	/** Toggle terraforming mode on/off. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void ToggleTerraformMode();

	/** Cycle to next terraforming tool. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	void CycleTerraformTool();

	/** Perform terraform action at current aim location. */
	UFUNCTION(BlueprintCallable, Category="MO|Terraforming")
	bool DoTerraform();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;

	/**
	 * Runs on the server (PossessedBy/UnPossessed) AND on the owning client (OnRep_Controller): the one place that decides
	 * whether a human is driving this pawn, which is when it needs guaranteed voxel ground (VoxelCollisionInvoker).
	 */
	virtual void NotifyControllerChanged() override;

	/** Runs on the server AND on clients (APawn::SetPlayerState, OnRep_PlayerState): a remote player's pawn on a joiner has no controller there. */
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;

	/**
	 * A CLIENT draws every other player's pawn as a simulated proxy: it takes the server's position and PREDICTS between updates, and with no floor under the proxy the
	 * engine predicts a fall ("No floor, must fall"). Voxel terrain exists on a machine only around ITS OWN human-driven pawns (VoxelCollisionInvoker), and not at all before the
	 * client's runtime has been created from the host's seed -- so the host's pawn, standing still (no further position updates to correct it), fell through the client's world at
	 * terminal velocity forever: z=-498790 after a minute, invisible ("the client can't see the host's pawn"). Gravity is therefore never simulated on a proxy: its height is the
	 * server's. (MO.RemotePawn.SimGravity 1 restores the engine behaviour, for the test control.)
	 */
	virtual void OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation) override;

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Camera boom positioning the camera behind the character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Y offset for camera shoulder view (positive = right shoulder, negative = left shoulder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Camera")
	float CameraShoulderOffset = 80.0f;

	/** Duration in seconds for camera shoulder transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Camera")
	float CameraTransitionDuration = 0.5f;

private:
	/** Whether camera is currently transitioning between shoulders. */
	bool bIsCameraTransitioning = false;

	/** Target Y position for camera transition. */
	float CameraTargetY = 0.0f;

	/** Current transition progress (0 to 1). */
	float CameraTransitionAlpha = 0.0f;

	/** Starting Y position for camera transition. */
	float CameraStartY = 0.0f;

public:
	/** Identity component for persistence. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	/** Inventory component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	/** Interactor component for interaction system. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInteractorComponent> InteractorComponent;

	/** Survival stats component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOSurvivalStatsComponent> SurvivalStatsComponent;

	/** Skills component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOSkillsComponent> SkillsComponent;

	/** Knowledge component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	/** Vitals component (blood, oxygen, body temperature). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOVitalsComponent> VitalsComponent;

	/** Metabolism component (digestion, nutrition). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOMetabolismComponent> MetabolismComponent;

	/** Mental state component (morale, stress). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOMentalStateComponent> MentalStateComponent;

	/** Anatomy component (body parts, wounds). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOAnatomyComponent> AnatomyComponent;

	/** Adrenaline component (combat stress response). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOAdrenalineComponent> AdrenalineComponent;

	/** Breath-hold physiology: oxygen reserve, drowning (UMORespirationComponent). The head under water closes the airway. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMORespirationComponent> RespirationComponent;

	/** Surface-aware footsteps (foot plants; a skeleton without the foot bones, a creature, steps by distance when BoneFreeStepLengthCm is set; no-op on a dedicated server). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOFootstepComponent> FootstepComponent;

	/** Crafting queue component (timed crafting). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOCraftingQueueComponent> CraftingQueueComponent;

	/** Recipe discovery component (unlockable recipes). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMORecipeDiscoveryComponent> RecipeDiscoveryComponent;

	/** Terraforming component for terrain modification. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOTerraformingComponent> TerraformingComponent;

	/** The pawn's timed body action (drink at a lake, fill, boil ...): server-timed, interruptible, its progress replicated to the owner. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<class UMOTimedActionComponent> TimedActionComponent;

	/** A flame carried in the hand (a lit torch or lamp): its light, its burning down, putting it out. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<class UMOCarriedLightComponent> CarriedLightComponent;

	/** How light-adapted the viewer's eyes are near a flame (local, cosmetic): the camera's exposure floor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<class UMOEyeAdaptationComponent> EyeAdaptationComponent;

	/** Equipment component for body/hand slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOEquipmentComponent> EquipmentComponent;

	/** Recruitment component for survivor recruitment state machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMORecruitmentComponent> RecruitmentComponent;

	/** Job queue component for survivor task management. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOSurvivorJobQueueComponent> JobQueueComponent;

	/** Personality axes (colony V1) — zeroed until the colony tick rolls them. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOPersonalityComponent> PersonalityComponent;

	/** Life-event history log (colony V1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOCharacterHistoryComponent> HistoryComponent;

	/** Interactable component for other pawns to interact with this character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInteractableComponent> InteractableComponent;

	/** Collision sphere for interaction traces (blocks ECC_Visibility). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Navigation invoker - tells voxel world to generate navmesh around this character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

	/**
	 * Voxel COLLISION invoker. Ground collision only exists where the Voxel world has an invoker, and the default invoker
	 * is each machine's own camera. So (a) on the server a remote player's pawn away from the host's camera had NO ground
	 * and fell, over and over, through the fall-through rescue (20 rescues in 32 s measured at 150 m), and (b) on a joining
	 * client the ground under the pawn kept changing for ~7-9 s after "ready" (up to 3.7 m). With bWaitForVoxelWorld the
	 * plugin computes the chunks under the pawn inline, which is its documented way to keep players from falling through.
	 * Enabled only while a human controls the pawn (see NotifyControllerChanged) so NPCs don't each cost collision chunks.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Voxel")
	TObjectPtr<UVoxelCollisionInvokerComponent> VoxelCollisionInvoker;

	/**
	 * Fine terrain around ANOTHER player's pawn on this machine (the joiner's pawn on the host, the host's pawn on a joiner). Pawns stand on the
	 * voxel world's render-chunk (visibility) collision, whose detail follows THIS machine's camera: 1 km from the host's camera a joiner stood on
	 * coarse terrain 69 cm above the true ground, and the host corrected it about 10 times a second, forever, while the joiner (standing at its own
	 * camera) had the right floor (Docs/Coop_And_Networking.md section 7). This forces the finest detail (MinLOD 0) within 30 m of each remote
	 * player, so both machines stand it on the same ground. Off for the local pawn (the camera covers it) and for NPCs (no player state).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Voxel")
	TObjectPtr<UVoxelLODVolumeComponent> RemotePlayerTerrainDetail;

	/** Turns RemotePlayerTerrainDetail on exactly while this pawn is another (human) player's pawn on this machine. */
	void UpdateRemotePlayerTerrainDetail();

	// ============================================================================
	// HELD ITEM VISUALS
	// ============================================================================

	/** Mesh component for left hand held item. Dynamically spawned based on equipped item. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|HeldItems")
	TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	/** Mesh component for right hand held item. Dynamically spawned based on equipped item. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|HeldItems")
	TObjectPtr<UStaticMeshComponent> RightHandMesh;

	/** Socket name on the skeletal mesh for left hand attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HeldItems")
	FName LeftHandSocketName = TEXT("hand_l");

	/** Socket name on the skeletal mesh for right hand attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HeldItems")
	FName RightHandSocketName = TEXT("hand_r");

	/**
	 * Worn rigid gear on the body (N-210: a helmet equipped into Head showed nothing; only the hands had visuals). The item's mesh hangs from
	 * the slot's bone at its HeldVisual.WornTransform. Clothing (Chest / Legs / Feet / Hands) needs skinned meshes: a separate art track.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|HeldItems")
	TObjectPtr<UStaticMeshComponent> HeadWornMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|HeldItems")
	TObjectPtr<UStaticMeshComponent> BackWornMesh;

	/** The bone a Head item (a helmet) hangs from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HeldItems")
	FName HeadWornSocketName = TEXT("head");

	/** The bone a Back item (a quiver, a pack) hangs from: the upper spine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|HeldItems")
	FName BackWornSocketName = TEXT("spine_05");

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Default skeletal mesh (soft reference to avoid load order issues). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	TSoftObjectPtr<USkeletalMesh> DefaultMesh;

	/** Default animation blueprint (soft reference). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	TSoftClassPtr<UAnimInstance> DefaultAnimBlueprint;

	/**
	 * MetaHuman skin (and the clothing baked into the body texture) is tuned for studio light: under a low sun on a bright beach the arms and shoulders
	 * read oily and the cloth like satin (playtest 2026-10-14 #7 / #12, "character VERY shiny"). Added to the skin materials' post-bake roughness and
	 * multiplied into their post-bake specular on every skeletal mesh of the character (ApplySkinLook). 0 / 1 = the MetaHuman as shipped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance", meta=(ClampMin="0.0", ClampMax="0.5"))
	float SkinRoughnessOffset = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SkinSpecularMultiply = 0.7f;

	/** Push SkinRoughnessOffset / SkinSpecularMultiply into every material of every skeletal mesh that has the MetaHuman post-bake parameters. */
	virtual void ApplySkinLook();

	/** Walking speed (default movement; the survivor's is GASP's walk, 200: AMOMetaHumanCharacter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement")
	float WalkSpeed = 400.0f;

	/** Jogging speed (toggle with hustle; the survivor's is GASP's run, 500). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement")
	float JogSpeed = 550.0f;

	/** Sprinting speed (hold hustle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement")
	float SprintSpeed = 700.0f;

	// ============================================================================
	// MOVEMENT PHYSIOLOGY CONFIG
	// ============================================================================

	/** MET value for walking (used for calorie calculation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float WalkingMET = 3.5f;

	/** MET value for jogging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float JoggingMET = 7.0f;

	/** MET value for sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float SprintingMET = 18.0f;

	/** Exertion level for walking (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology", meta=(ClampMin="0", ClampMax="100"))
	float WalkingExertion = 15.0f;

	/** Exertion level for jogging (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology", meta=(ClampMin="0", ClampMax="100"))
	float JoggingExertion = 50.0f;

	/** Exertion level for sprinting (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology", meta=(ClampMin="0", ClampMax="100"))
	float SprintingExertion = 90.0f;

	/** MET value for swimming (a steady crawl, 6-8 METs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float SwimmingMET = 7.0f;

	/** Exertion level for swimming (0-100): raises heart rate and burns the oxygen reserve while the head is under. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology", meta=(ClampMin="0", ClampMax="100"))
	float SwimmingExertion = 60.0f;

	/** Stamina cost per second while swimming (a little under a jog: sustained, and you cannot rest in deep water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float SwimmingStaminaCostPerSecond = 1.6f;

	/** Swim speed multiplier while the hustle input is held (a harder stroke). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="1", ClampMax="2"))
	float SwimHustleSpeedScale = 1.35f;

	/** MET value for climbing (rock scrambling and tree climbing run about 7-9 METs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float ClimbingMET = 8.0f;

	/** Exertion while climbing (0-100) by the face's angle from level ground (F51, Wes Oct 29: "make it angle dependent"): a slab leaning back is mostly
	 *  feet (45 deg 30, 60 deg 40: under the 55 where stamina stops coming back, StaminaRecoveryByExertion -- a slab rests as it climbs), 75 deg 55, a
	 *  sheer face 70, an overhang hangs off the arms (110 deg 85, 125 deg on 90). Until Oct 29 75 on every face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	FRuntimeFloatCurve ClimbingExertionByFaceAngle;

	/** Stamina a second on a SHEER face (90 deg) for an untrained climber of average build: a full bar lasts ~100 s, ~26 m of rock (F51; until Oct 29 3/s
	 *  on every face, which with F52 B made a bar ~8 m). ClimbingCostByFaceAngle scales it by the face, FMOTraversal::ClimbAbility by the climber. Out of
	 *  stamina the hands give out and the climber falls (decided on the server). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float ClimbingStaminaCostPerSecond = 1.0f;

	/** A face's share of ClimbingStaminaCostPerSecond by its angle from level ground: a slab 45 deg 0.2, 60 deg 0.3, 75 deg 0.6, sheer 1, overhanging
	 *  100 deg 1.3, 110 deg 2, 125 deg 3, 140 deg on 4 (a roof is hung from the arms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	FRuntimeFloatCurve ClimbingCostByFaceAngle;

	/** The skill climbing trains and draws on (the DT_Skills row): technique, FMOTraversal::ClimbAbility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Skills")
	FName ClimbingSkillId = TEXT("Climbing");

	/** Climbing XP a second on a sheer face; the face's cost share scales it (harder rock teaches more). Server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Skills")
	float ClimbingXPPerSecond = 0.5f;

	/** What the climb in hand costs this body now: the face's angle, this climber's skill and muscle share (F51). Meaningful while climbing. */
	UFUNCTION(BlueprintPure, Category="MO|Traversal")
	FMOClimbEffort GetClimbEffort() const;

	/** This climber's FMOTraversal::ClimbAbility: the Climbing level and the muscle share of the body (UMOMetabolismComponent's body composition). */
	FMOTraversal::FClimbAbility GetClimbAbility() const;

	/** Swim speed multiplier when the stamina is gone (an exhausted swimmer can barely keep afloat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water", meta=(ClampMin="0.1", ClampMax="1"))
	float ExhaustedSwimSpeedScale = 0.35f;

	/** Temperature increase per second while walking (°C). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float WalkingTempRisePerSec = 0.00017f;  // ~0.01°C/min

	/** Temperature increase per second while jogging (°C). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float JoggingTempRisePerSec = 0.0005f;   // ~0.03°C/min

	/** Temperature increase per second while sprinting (°C). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Physiology")
	float SprintingTempRisePerSec = 0.00133f; // ~0.08°C/min

	// ============================================================================
	// MOVEMENT SKILL & STAMINA CONFIG
	// ============================================================================

	/** Skill ID for athletics/fitness (gains XP from movement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Skills")
	FName AthleticsSkillId = TEXT("athletics");

	/** XP gained per second while walking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Skills")
	float WalkingXPPerSecond = 0.1f;

	/** XP gained per second while jogging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Skills")
	float JoggingXPPerSecond = 0.5f;

	/** XP gained per second while sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Skills")
	float SprintingXPPerSecond = 1.0f;

	/** Stamina cost per second while jogging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Stamina")
	float JoggingStaminaCostPerSecond = 2.0f;

	/** Stamina cost per second while sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Stamina")
	float SprintingStaminaCostPerSecond = 8.0f;

	/** Minimum stamina required to start sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Stamina")
	float MinStaminaToSprint = 10.0f;

	/** How much of the stamina's at-rest recovery (UMOSurvivalStatsComponent Stamina.RegenRate, 10/s) the body gets while it works, by
	 *  exertion (0-100, GetCurrentExertionLevel; 0 = standing still): recovery follows the work (DECISIONS F52 B, Wes Oct 29). Rest 1, a
	 *  walk (15) half, a jog (50) a quarter -- 2.5/s against its 2/s cost, so a jog holds up and slowly gets a sprint's breath back -- and
	 *  nothing from a little above a jog (55) up: a swim (60), a climb (75) and a sprint (90) only spend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Stamina")
	FRuntimeFloatCurve StaminaRecoveryByExertion;

	/** StaminaRecoveryByExertion at this exertion, clamped to 0-1. */
	UFUNCTION(BlueprintPure, Category="MO|Movement|Stamina")
	float GetStaminaRecoveryFraction(float Exertion) const;

	/** Look sensitivity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Input", meta=(ClampMin="0.1", ClampMax="10.0"))
	float LookSensitivity = 1.0f;

	// ============================================================================
	// STATE
	// ============================================================================

	/** Current movement mode. */
	UPROPERTY(BlueprintReadOnly, Category="MO|State")
	EMOMovementMode CurrentMovementMode = EMOMovementMode::Walking;

	/** Whether jog mode is toggled on. */
	UPROPERTY(BlueprintReadOnly, Category="MO|State")
	bool bJogToggled = false;

	/** Whether the character is currently sprinting (holding hustle). */
	UPROPERTY(BlueprintReadOnly, Category="MO|State")
	bool bIsSprinting = false;

	/** Whether the character is dead. */
	UPROPERTY(BlueprintReadOnly, Category="MO|State")
	bool bIsDead = false;

	/** IsInCharacterCreator (server state, not replicated: only the server's creatures read it). */
	bool bInCharacterCreator = false;

	/** What killed it (DieOf); a death that arrives without a cause (anatomy's OnInstantDeath) is a destroyed body part. */
	EMODeathCause DeathCause = EMODeathCause::BodyPartDestroyed;
	EMOBodyPartType DeathCausePart = EMOBodyPartType::None;
	bool bDeathCauseSet = false;

	/** Whether the ragdoll has been frozen (physics disabled, pose preserved). */
	UPROPERTY(BlueprintReadOnly, Category="MO|State")
	bool bRagdollFrozen = false;

	/** Whether the character can currently be controlled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|State")
	bool bCanBeControlled = true;

	/** Whether terraforming mode is active. */
	UPROPERTY(BlueprintReadOnly, Category="MO|State")
	bool bTerraformingMode = false;

	// ============================================================================
	// AI PERCEPTION NOISE
	// ============================================================================

	/** Whether this character generates noise for AI perception. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AIPerception")
	bool bGeneratesMovementNoise = true;

	/** Interval between noise events while moving (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AIPerception", meta=(ClampMin="0.1"))
	float NoiseInterval = 0.3f;

	/** Noise loudness while walking (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AIPerception", meta=(ClampMin="0", ClampMax="1"))
	float WalkingNoiseLoudness = 0.3f;

	/** Noise loudness while jogging (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AIPerception", meta=(ClampMin="0", ClampMax="1"))
	float JoggingNoiseLoudness = 0.6f;

	/** Noise loudness while sprinting (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AIPerception", meta=(ClampMin="0", ClampMax="1"))
	float SprintingNoiseLoudness = 1.0f;

	/** Crouched footfalls scale the loudness by this (sneaking: a crouched walk is heard at a quarter of a walk's distance or less). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|AIPerception", meta=(ClampMin="0", ClampMax="1"))
	float CrouchedNoiseScale = 0.5f;

	/** Time since last noise was generated. */
	float TimeSinceLastNoise = 0.f;

	/**
	 * How loud this pawn's footfalls are at Speed (cm/s), 0-1 (a listener hears it within HearingRange x loudness): its ACTUAL speed
	 * through its own gaits -- below the walk quieter, Walking at WalkSpeed, Jogging at JogSpeed, Sprinting at SprintSpeed -- so an AI
	 * pawn (a running wolf) is as loud as its run, not stuck at the walk value its player-only gait state left it; crouched x CrouchedNoiseScale.
	 */
	float GetMovementNoiseLoudness(float Speed) const;

	// ============================================================================
	// FALL-THROUGH SAFETY
	// ============================================================================

	/** Whether fall-through safety is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	bool bEnableFallThroughSafety = true;

	/**
	 * Suppress fall-through safety teleport. Set true while the world is
	 * being loaded — during the gap between persistence-spawn and voxel-ready,
	 * the pawn appears to be "falling" only because voxel collision hasn't
	 * generated yet. The fall-through rescue would teleport the pawn 20m
	 * away from its saved position, corrupting the load.
	 *
	 * MOGameMode sets this true in WaitForVoxelAndRegroundPawns and clears
	 * it after RegroundAllPawns finishes.
	 */
	UPROPERTY(BlueprintReadWrite, Category="MO|Safety")
	bool bSuppressFallThroughDuringLoad = false;

	/**
	 * The load window, as ONE mechanism: suppress the fall-through rescue AND hold the character in place (no gravity).
	 * Suppressing the rescue alone was not enough -- loaded pawns are respawned at their saved spots while the voxel
	 * terrain is still regenerating, so they FELL ~70 m before RegroundAllPawns ran, and its +/-30 m search around the
	 * fallen position found no ground (hosting a saved world buried the colonists and teleported the host's pawn km away).
	 * GameMode sets this in WaitForVoxelAndRegroundPawns and releases it in FinishLoadHandoff.
	 */
	void SetLoadHold(bool bHold);

private:
	/** True only if SetLoadHold(true) is what put the movement into MOVE_None (so release never wakes a dead/ragdolled pawn). */
	bool bMovementHeldByLoad = false;

public:

	/** How long to fall before triggering safety teleport (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	float FallThroughTimeThreshold = 2.0f;

	/** Minimum Z velocity to be considered "falling through". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	float FallThroughVelocityThreshold = -500.0f;

	/** How far below to search for terrain when teleporting to safety. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	float SafetyTerrainSearchDistance = 50000.0f;

	/** Height above found terrain to teleport to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	float SafetyTeleportHeight = 200.0f;

	/** Whether to only accept voxel terrain for safety teleport (rejects trees/meshes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	bool bSafetyTeleportOnlyVoxelTerrain = true;

	/**
	 * Minimum Z component of surface normal for safety teleport (0-1).
	 * If terrain is steeper than this, search laterally for flatter ground.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SafetyMinSlopeNormalZ = 0.6f;

	/** How far to search laterally for flat ground when terrain is too steep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	float SafetyLateralSearchRadius = 2000.0f;

	/** Number of samples to take when searching laterally for flat ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	int32 SafetyLateralSearchSamples = 16;

private:
	/** How long we've been continuously falling. */
	float ContinuousFallTime = 0.f;

	/** Last Z position to detect if we're truly falling (not just jumping). */
	float LastZPosition = 0.f;

	/** How many times this pawn's failed rescue has logged what blocks the sky-to-ground line (capped: the rescue repeats every few seconds). */
	int32 RescueFailLogCount = 0;

	/** Where this pawn last stood on the ground (authority). The fall-through rescue's fallback when no terrain can be found: back here, not to a fixed height. */
	FVector LastGroundedLocation = FVector::ZeroVector;
	bool bHasLastGrounded = false;

	/** Check for and handle falling through the world. */
	void CheckFallThroughSafety(float DeltaTime);

	/**
	 * Server: a HUMAN-driven pawn whose capsule overlaps something solid that is not the terrain (a tree trunk, a rock, a
	 * building) for a few seconds is STUCK -- it could be spawned, teleported or have something generated into it, and
	 * character movement cannot always push it out. Move it to the nearest clear ground. Prevention is in the spawn paths
	 * (MOSpawnClearance.h); this catches whatever still ends up embedded.
	 */
	void CheckEmbeddedSafety(float DeltaTime);

	float EmbeddedCheckTimer = 0.f;
	float EmbeddedSeconds = 0.f;

	// ============================================================================
	// INTERRUPT EVENT BUS INTERNALS
	// ============================================================================

	/** All registered IMOInterruptibleInterface listeners. */
	TArray<TWeakObjectPtr<UObject>> InterruptListeners;

	/** Was the character above the movement threshold last tick? */
	bool bWasConsideredMovingLastTick = false;

	/**
	 * Detect a stationary->moving transition and emit a Movement-reason
	 * broadcast. Called from Tick. Does nothing if no listeners are registered,
	 * so the cost while idle is just one velocity read and a bool compare.
	 */
	void TickMovementInterruptDetection();

	/**
	 * Find a safe location near the given position by searching for flat voxel terrain.
	 * @param NearLocation The location to search around
	 * @param OutSafeLocation The found safe location (if successful)
	 * @return True if a safe location was found
	 */
	bool FindSafeTerrainNearLocation(const FVector& NearLocation, FVector& OutSafeLocation) const;

	/**
	 * THE ground trace of the fall-through rescue: the first terrain hit along the vertical line Start -> End (same X/Y; sorted from Start).
	 * With bSafetyTeleportOnlyVoxelTerrain it takes the first VOXEL hit and skips everything else that blocks WorldStatic -- the gameplay map's
	 * PCGVolume brush, trees, props. A first-hit trace returned that brush instead of the terrain, so every "is there terrain here?" answer was
	 * NO and a pawn buried under the surface was teleported to the fixed height (below the surface) again and again, forever -- the host's pawn
	 * was invisible to every client (Wes: "the client can't see the host's pawn"). All four rescue traces go through here.
	 */
	bool TraceTerrainLine(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	// ============================================================================
	// MOVEMENT MODE API
	// ============================================================================

	/** Get current movement mode. */
	UFUNCTION(BlueprintPure, Category="MO|Movement")
	EMOMovementMode GetMovementMode() const { return CurrentMovementMode; }

	/** Check if currently jogging. */
	UFUNCTION(BlueprintPure, Category="MO|Movement")
	bool IsJogging() const { return CurrentMovementMode == EMOMovementMode::Jogging; }

	/** Set movement mode directly. */
	UFUNCTION(BlueprintCallable, Category="MO|Movement")
	void SetMovementMode(EMOMovementMode NewMode);

	/** Toggle jog on/off. */
	UFUNCTION(BlueprintCallable, Category="MO|Movement")
	void ToggleJog();

	/** Start sprinting (from hold). */
	UFUNCTION(BlueprintCallable, Category="MO|Movement")
	void StartSprint();

	/** Stop sprinting (release hold). */
	UFUNCTION(BlueprintCallable, Category="MO|Movement")
	void StopSprint();

	/** Water each tick: swim input, the airway (head under water closes it), wetness, the swim speed from effort and stamina. */
	void TickWater(float DeltaTime);

	/** The respiration component reports the body failed with the airway closed: drowning. */
	UFUNCTION()
	void HandleAsphyxiaDeath();

	/** The vitals component reports a whole-body failure became fatal (bleed-out, cardiac arrest, respiratory failure). */
	UFUNCTION()
	void HandleSystemicDeath(EMOSystemicFailure Cause);

	/** Space is held while swimming: rise (a local input flag; the movement component replicates the resulting move). */
	bool bSwimUpHeld = false;
	bool bWasSwimmingLastTick = false;

	// ---- swim animation ----
	// While swimming the body mesh plays swim poses in single-node mode (the locomotion animation blueprint has no swim states); leaving the water restores the blueprint. The animations are the
	// Oceanology mannequin set retargeted to the MetaHuman skeleton (Content/Characters/Swim/, RTG_OceanologyToMetaHuman). Soft paths: a pawn without them keeps its normal locomotion.

	enum class ESwimPose : uint8 { None, Tread, Slow, Crawl, UnderIdle, UnderForward, UnderUp, UnderDown };

	/** Upright, treading water: head and shoulders out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimTreadAnim;

	/** A slow stroke on the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimSlowAnim;

	/** A hard crawl on the surface (the hustle stroke). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimCrawlAnim;

	/** Holding still under water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimUnderIdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimUnderForwardAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimUnderUpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Water|Animation", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> SwimUnderDownAnim;

	// ---- locomotion: Epic's Game Animation Sample (GASP phase 1, Oct 27 2026; Docs/Animation_GASP.md) ----
	// Walking, running, sprinting, crouching, jumping and landing come from GASP's motion-matching character: a hidden UEFN mannequin (the
	// locomotion driver, attached to the body) runs GASP's SandboxCharacter_CMC_ABP, which asks this pawn for its movement through GASP's pawn
	// interface (BPI_SandboxCharacter_Pawn::Get_PropertiesForAnimation, implemented in the survivor Blueprint from the getters below). The body runs
	// UMOBodyRetargetAnimInstance: the driver's pose retargeted onto the MetaHuman, then the DefaultSlot our montages use. The three assets are soft:
	// a pawn without them, or a clone without the imported GASP content (Tools/import_gasp.py), keeps the body's own animation blueprint.

public:
	/** The hidden mannequin that runs the locomotion; null while this pawn animates with the body's own blueprint. */
	USkeletalMeshComponent* GetLocomotionDriver() const { return LocomotionDriver; }

	/** The retarget from the driver's skeleton onto the body (loaded with the driver). */
	UIKRetargeter* GetBodyRetargeter() const { return LoadedBodyRetargeter; }

	/** The gait the animation shows, GASP's E_Gait order: 0 walk, 1 run (our jog), 2 sprint. The owner and the server read the gait the moves
	 *  carry; another machine reads it from the replicated speed. */
	int32 GetAnimGait() const;

	UFUNCTION(BlueprintPure, Category="MO|Animation")
	bool IsAnimGaitSprint() const { return GetAnimGait() == 2; }

	UFUNCTION(BlueprintPure, Category="MO|Animation")
	bool IsAnimGaitWalk() const { return GetAnimGait() == 0; }

	/** Line the body up with the capsule for a while when she stands (an attack turning her to its target): GASP otherwise holds a standing
	 *  body's facing (its root offset), so the swing would go the old way. Moving, GASP's own turning animations follow the capsule. */
	void HoldBodyToCapsule(float Seconds);

	/** Landed within the last 0.3 s (GASP's landing window). Every machine sees the landing in the replicated movement. */
	UFUNCTION(BlueprintPure, Category="MO|Animation")
	bool HasJustLanded() const;

	/** The velocity the last landing came down with. */
	UFUNCTION(BlueprintPure, Category="MO|Animation")
	FVector GetLandVelocity() const { return AnimLandVelocity; }

	/** The floor under the feet (the movement's last floor hit): its normal, and where it was struck. */
	UFUNCTION(BlueprintPure, Category="MO|Animation")
	FVector GetAnimGroundNormal() const;

	UFUNCTION(BlueprintPure, Category="MO|Animation")
	FVector GetAnimGroundLocation() const;

private:
	/** The driver's mesh: GASP's UEFN mannequin (SKM_UEFN_Mannequin). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Locomotion", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<USkeletalMesh> LocomotionDriverMesh;

	/** The driver's animation blueprint: GASP's SandboxCharacter_CMC_ABP. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Locomotion", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<UAnimInstance> LocomotionDriverAnimClass;

	/** The retarget from the mannequin onto the body: GASP's RTG_UEFN_to_Metahuman_nrw. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Locomotion", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UIKRetargeter> BodyRetargeter;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> LocomotionDriver;

	UPROPERTY(Transient)
	TObjectPtr<UIKRetargeter> LoadedBodyRetargeter;

	/** The driver's pose while the body plays a montage (an attack, a swim stroke): a mannequin idle (GASP's M_Neutral_Stand_Idle_Loop). GASP
	 *  lines the body up with the capsule only while a montage plays in its own slot (its root offset otherwise holds the body's facing while
	 *  she stands: an attack that turns her to the target would swing the old way), and our action montages play on the body, not the driver --
	 *  so the driver's slot plays this whenever the body's does (UpdateDriverHold). Its pose shows only under the montage's blends. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Locomotion", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> LocomotionDriverHoldAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> DriverHoldMontage;

	/** HoldBodyToCapsule's end (world seconds). */
	double HoldBodyUntilSeconds = -1.0;

	// ---- climbing and the moves onto things: GASP phase 2 (Wes, Oct 29: "can we use the hanging/grabbing animation for climbing instead of
	// them just walking up the cliff") -- the driver's motion matching knows no climbing (our custom modes count as on the ground), so it walked
	// up the face. GASP's own traversal sequences play on the driver instead, in its slot; the retarget carries them to the body. ----

	/** A hop up (waist high): GASP's mantle. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Traversal", meta=(AllowPrivateAccess="true"))
	FMOTraversalAnim HopUpAnim;

	/** A pull-up (to arm's reach): GASP's climb-start -- the hands catch the edge, the feet walk the wall, up and over. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Traversal", meta=(AllowPrivateAccess="true"))
	FMOTraversalAnim PullUpAnim;

	/** A vault over a thin obstacle: GASP's vault. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Traversal", meta=(AllowPrivateAccess="true"))
	FMOTraversalAnim VaultAnim;

	/** On a face: Titan Climbing's climbing set (free on Fab; Wes, Oct 29: "yeah i just want the animations"), retargeted onto the MetaHuman
	 *  (/MOFramework/Characters/Climbing, the crouch set's retargeter, pelvis repaired: Content/Python/repair_crouch_pelvis.py) and looped in
	 *  the BODY's slot like the swim strokes: idle, or the climb up / down / left / right the climber is making along the face. GASP has no
	 *  free-climbing cycle; its moves onto things (above) still play on the driver. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> ClimbIdleAnim;

	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> ClimbUpAnim;

	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> ClimbDownAnim;

	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> ClimbLeftAnim;

	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> ClimbRightAnim;

	/** How fast the up / down clips climb at rate 1 (cm/s along the face), for their play rate: measured Oct 29 from a hand while its contact
	 *  curve holds it on the rock (it slides down the body at the climbing speed): up 68-72, down 60-70. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	float ClimbClipVerticalSpeed = 70.0f;

	/** The same across the face (the side clips carry no contact curves to measure by: an estimate for Wes to judge by eye). */
	UPROPERTY(EditDefaultsOnly, Category="MO|Animation|Climbing", meta=(AllowPrivateAccess="true"))
	float ClimbClipSideSpeed = 35.0f;

	/** A move's kind and duration, for the machines that do not run it (see FMOTraversalCue). Server-written, not to the owner. */
	UPROPERTY(ReplicatedUsing=OnRep_TraversalCue)
	FMOTraversalCue TraversalCue;

	UFUNCTION()
	void OnRep_TraversalCue();

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> TraversalMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ClimbMontage;

	/** The clip ClimbMontage plays (0 idle, 1 up, 2 down, 3 left, 4 right); the one wanted next, and for how long it has been wanted. */
	int32 ClimbClip = -1;
	int32 PendingClimbClip = -1;
	float PendingClimbClipSeconds = 0.0f;

	bool bTraversalAnimStarted = false;

	/** The moves onto things, on the driver (every tick, every machine: cosmetic, from what each machine sees). */
	void UpdateTraversalAnimation(float DeltaTime);

	/** The climbing set on the body while on a face (every tick, every machine: the climbing mode and the velocity replicate). */
	void UpdateClimbAnimation(float DeltaTime);

	/** A move's sequence on the driver, its rise fitted to the move's duration. */
	void PlayTraversalAnim(uint8 Kind, float Duration);

	/** The move under way on this machine (the owner, the server): its kind and duration. */
	bool GetMoveTraversal(EMOTraversal& OutKind, float& OutDuration) const;

	/** Mirror the body's montages onto the driver's slot (every tick, every machine: cosmetic, from the montages each machine plays). */
	void UpdateDriverHold();

	/** Make the driver and hand the body to the retarget (BeginPlay; once). */
	void SetUpLocomotionDriver();

	/** Watch the movement for a landing (every tick, every machine). */
	void UpdateLandingForAnimation();

	bool bWasFallingForAnim = false;
	FVector AnimFallVelocity = FVector::ZeroVector;
	FVector AnimLandVelocity = FVector::ZeroVector;
	double AnimLandedAtSeconds = -1000.0;

	// ---- crouch ----
	// ACharacter::Crouch() is a silent no-op unless NavAgentProps.bCanCrouch is set (it never was: CanEverCrouch() was false). The capsule shrinks and the walk slows; the pose comes from the locomotion
	// animation blueprint's DefaultSlot as a looping dynamic montage (the blueprint has no crouch states, and a slot blends in and out where the swim override snaps). The clips are the engine's Manny
	// crouch set retargeted to the MetaHuman skeleton (Content/Characters/Crouch/). Soft paths: a pawn without them crouches with its standing pose (shorter capsule, slower).

public:
	/** Crouched capsule half height (cm; the survivor stands at 92.5: 185 cm, Oct 28). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Movement|Crouch", meta=(ClampMin="30.0"))
	float CrouchedHalfHeightCm = 60.0f;

	/** Walk speed while crouched (cm/s; the survivor's is GASP's crouch, 225). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Movement|Crouch", meta=(ClampMin="0.0"))
	float CrouchedWalkSpeed = 200.0f;

	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

private:
	/** Holding still, crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Crouch", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> CrouchIdleAnim;

	/** Walking forward, crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Crouch", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UAnimSequence> CrouchWalkAnim;

	/** The ground speed (cm/s) the crouch-walk clip was authored for: the play rate follows speed / this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement|Crouch", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float CrouchWalkAnimSpeedCm = 294.0f;

	enum class ECrouchPose : uint8 { None, Idle, Walk };

	/** Choose the crouch pose from the movement and play it on the animation blueprint's DefaultSlot. Cosmetic: runs on every machine from replicated movement. */
	void UpdateCrouchAnimation(float DeltaTime, bool bSwimming);
	void StopCrouchPose(float BlendOutSeconds);

	ECrouchPose CurrentCrouchPose = ECrouchPose::None;
	ECrouchPose WantedCrouchPose = ECrouchPose::None;
	float WantedCrouchPoseSeconds = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CrouchMontage;

	/** Water sounds for the body in water: the splash of feet entering, wading steps, swim strokes. Local presentation on every machine (it follows replicated movement). */
	void UpdateWaterSounds(float DeltaTime);

	float WaterSfxTimer = 0.0f;
	float LastImmersionForSfx = 0.0f;

	/** Choose the swim pose from the movement and the water, and apply it (single-node playback on the body mesh). Runs on every machine: it is cosmetic and driven by replicated movement. */
	void UpdateSwimAnimation(float DeltaTime, bool bSwimming);

	/** The pose a swimmer in this state should hold. */
	ESwimPose ChooseSwimPose() const;

	ESwimPose CurrentSwimPose = ESwimPose::None;
	ESwimPose PendingSwimPose = ESwimPose::None;
	float PendingSwimPoseSeconds = 0.0f;

	/** The animation setup to restore when the swimmer leaves the water. */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> SavedAnimClass;
	uint8 SavedAnimMode = 0;   // EAnimationMode::Type (0 = AnimationBlueprint), kept as a byte so the header needs no animation includes
	/** The swim poses play as a slot montage in the locomotion blueprint (blended) rather than as a single-node swap. */
	bool bSwimPoseViaSlot = false;
	FVector SavedMeshRelativeLocation = FVector::ZeroVector;
	float SwimMeshRaise = 0.0f;

protected:
	/** Apply physiological effects of current movement mode. */
	void ApplyMovementPhysiologyEffects(float DeltaTime);

	/** Update movement speed based on current mode. */
	void UpdateMovementSpeed();

	/** Get current MET value for calorie calculations. */
	float GetCurrentMET() const;

	/** Get current exertion level for vitals. */
	float GetCurrentExertionLevel() const;

	/** Get current temperature rise rate. */
	float GetCurrentTempRiseRate() const;

	/** Timer handle for movement physiology tick. */
	FTimerHandle MovementPhysiologyTimerHandle;

	/** A climb's first physiology tick has logged what it costs this climber (once per climb). */
	bool bClimbEffortLogged = false;

	/** Start physiology tracking. */
	void StartMovementPhysiologyTracking();

	/** Stop physiology tracking. */
	void StopMovementPhysiologyTracking();

	// ============================================================================
	// COMPONENT EVENT HANDLERS
	// ============================================================================

	/** Called when knowledge is learned - triggers recipe discovery. */
	UFUNCTION()
	void HandleKnowledgeLearned(FName KnowledgeId, FName FromItemId);

	/** Called when a skill levels up - triggers recipe discovery. */
	UFUNCTION()
	void HandleSkillLevelUp(FName SkillId, int32 OldLevel, int32 NewLevel);

	/** Called when a recipe is discovered - shows notification. */
	UFUNCTION()
	void HandleRecipeDiscovered(FName RecipeId, EMODiscoveryMethod Method);

	/** Called when equipment changes - updates held item visuals. */
	UFUNCTION()
	void HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem);

	/** Handle interaction from another pawn for recruitment. */
	bool HandleRecruitmentInteraction(AController* InteractorController);

	/** Handle instant death from anatomy (vital organ destroyed or blood loss). Server: records the death, then EnterDeadState. */
	UFUNCTION()
	virtual void HandleInstantDeath(EMOBodyPartType CausePart);

	/** The dead state, run on the server AND on every client (from the replicated record): OnPawnDied (the death recap), input, movement,
	 *  collision, ragdoll. The server-only parts (the save's deceased record, the Death interrupt) stay in HandleInstantDeath's authority path. */
	void EnterDeadState(EMOBodyPartType CausePart);

	/** Replicated once at death; clients enter the dead state from it (N-064). */
	UPROPERTY(ReplicatedUsing=OnRep_DeathRecord)
	FMODeathRecord DeathRecord;

	UFUNCTION()
	void OnRep_DeathRecord();

	// ============================================================================
	// DEATH AND RAGDOLL
	// ============================================================================

	/** Delay in seconds before ragdoll is frozen after death. Allows body to settle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Death")
	float RagdollFreezeDelay = 2.0f;

	/** Timer handle for delayed ragdoll freeze. */
	FTimerHandle RagdollFreezeTimerHandle;

	/**
	 * Freeze the ragdoll physics, preserving the current pose.
	 * Sets mesh to QueryOnly collision (interactable but not physics-simulated).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Death")
	void FreezeRagdoll();

	/** Update the item mesh of a slot with a visual: the hands (held) and Head / Back (worn rigid gear). */
	void UpdateHeldItemMesh(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem);

	/** Clear the item mesh of a slot with a visual. */
	void ClearHeldItemMesh(EMOEquipmentSlot EquipSlot);

	/** The mesh component and bone a slot's item shows on (false: the slot has no visual, e.g. clothing). */
	bool GetItemVisualTarget(EMOEquipmentSlot EquipSlot, UStaticMeshComponent*& OutMesh, FName& OutSocket) const;

	/** The scalp hair hides under a worn helmet (it would poke through it); eyebrows, lashes and beard stay. */
	void SetScalpHairHiddenForHelmet(bool bHide);
};
