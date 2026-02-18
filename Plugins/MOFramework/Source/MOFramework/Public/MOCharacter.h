#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "MOControllableInterface.h"
#include "MOInventoryHolderInterface.h"
#include "MOMaterialSourceInterface.h"
#include "MOIdentifiableInterface.h"
#include "MOCraftingCapableInterface.h"
#include "MOMedicalProviderInterface.h"
#include "MOCraftingTypes.h"
#include "MOCharacter.generated.h"

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
class UMOSurvivalStatsComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class UMOVitalsComponent;
class UMOMetabolismComponent;
class UMOMentalStateComponent;
class UMOAnatomyComponent;
class UMOAdrenalineComponent;
class UMOCraftingQueueComponent;
class UMORecipeDiscoveryComponent;
class UMOTerraformingComponent;
class UMOEquipmentComponent;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMesh;
class UAnimInstance;
class UStaticMeshComponent;
class UNavigationInvokerComponent;
struct FMOEquippedItem;
enum class EMOEquipmentSlot : uint8;

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
	public IMOMedicalProviderInterface
{
	GENERATED_BODY()

public:
	AMOCharacter();

	virtual void Tick(float DeltaTime) override;

	// ============================================================================
	// VOXEL WORLD COMPATIBILITY
	// ============================================================================
	// Required for base replication to work properly with Voxel Plugin.
	// Voxel actor components are generated at runtime & not replicated.
	// Without this override, character may teleport when voxel world updates.

	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName, const bool bNotifyActor) override
	{
		if (NewBase)
		{
			// LoadClass to not depend on the voxel module directly
			static UClass* const VoxelWorldClass = LoadClass<UObject>(nullptr, TEXT("/Script/Voxel.VoxelWorld"));

			const AActor* BaseOwner = NewBase->GetOwner();
			if (ensure(VoxelWorldClass) &&
				BaseOwner &&
				BaseOwner->IsA(VoxelWorldClass))
			{
				NewBase = Cast<UPrimitiveComponent>(BaseOwner->GetRootComponent());
				ensure(NewBase);
			}
		}

		Super::SetBase(NewBase, BoneName, bNotifyActor);
	}

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

	UFUNCTION(BlueprintPure, Category="MO")
	UMOEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	// ============================================================================
	// APPEARANCE
	// ============================================================================

	/** Set character skeletal mesh at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetCharacterMesh(USkeletalMesh* NewMesh);

	/** Set animation blueprint at runtime. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetAnimationBlueprint(TSubclassOf<UAnimInstance> NewAnimClass);

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

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** Camera boom positioning the camera behind the character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

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

	/** Crafting queue component (timed crafting). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOCraftingQueueComponent> CraftingQueueComponent;

	/** Recipe discovery component (unlockable recipes). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMORecipeDiscoveryComponent> RecipeDiscoveryComponent;

	/** Terraforming component for terrain modification. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOTerraformingComponent> TerraformingComponent;

	/** Equipment component for body/hand slots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOEquipmentComponent> EquipmentComponent;

	/** Navigation invoker - tells voxel world to generate navmesh around this character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

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

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Default skeletal mesh (soft reference to avoid load order issues). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	TSoftObjectPtr<USkeletalMesh> DefaultMesh;

	/** Default animation blueprint (soft reference). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	TSoftClassPtr<UAnimInstance> DefaultAnimBlueprint;

	/** Walking speed (default movement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Movement")
	float WalkSpeed = 400.0f;

	/** Jogging speed (toggle with hustle). */
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

	/** Time since last noise was generated. */
	float TimeSinceLastNoise = 0.f;

	// ============================================================================
	// FALL-THROUGH SAFETY
	// ============================================================================

	/** Whether fall-through safety is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Safety")
	bool bEnableFallThroughSafety = true;

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

private:
	/** How long we've been continuously falling. */
	float ContinuousFallTime = 0.f;

	/** Last Z position to detect if we're truly falling (not just jumping). */
	float LastZPosition = 0.f;

	/** Check for and handle falling through the world. */
	void CheckFallThroughSafety(float DeltaTime);

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

	/** Update the held item mesh for a specific hand slot. */
	void UpdateHeldItemMesh(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem);

	/** Clear the held item mesh for a specific hand slot. */
	void ClearHeldItemMesh(EMOEquipmentSlot EquipSlot);
};
