#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MOPlayerController.generated.h"

/**
 * =============================================================================
 * MOPlayerController - Central Input and Component Hub
 * =============================================================================
 *
 * PURPOSE:
 * The MOPlayerController is the central hub for all player-facing systems.
 * It owns the key framework components and handles all player input, delegating
 * to the appropriate subsystems and possessed pawns.
 *
 * -----------------------------------------------------------------------------
 * COMPONENT OWNERSHIP
 * -----------------------------------------------------------------------------
 *
 * This controller creates and owns these components as default subobjects:
 *
 *   UMOUIManagerComponent (UIManagerComponent)
 *     - Manages all UI widgets (menus, panels, dialogs)
 *     - Handles input mode switching for UI
 *     - See MOUIManagerComponent.h for full documentation
 *
 *   UMOPossessionComponent (PossessionComponent)
 *     - Manages pawn possession and switching
 *     - Tracks available pawns for possession
 *     - Handles possess/unpossess logic
 *
 *   UMONotificationComponent (NotificationComponent)
 *     - Displays notification messages to player
 *     - Manages notification queue and display duration
 *
 *   UMOBuildingComponent (BuildingComponent)
 *     - Manages building placement mode
 *     - Handles ghost preview positioning
 *     - Validates placement and confirms building
 *
 * -----------------------------------------------------------------------------
 * INPUT SYSTEM ARCHITECTURE
 * -----------------------------------------------------------------------------
 *
 * Uses UE5 Enhanced Input System with multiple mapping contexts:
 *
 *   EMOInputContext::PawnControl
 *     - Active during normal gameplay
 *     - Movement (WASD), Look (Mouse), Jump, Sprint, Crouch
 *     - Primary/Secondary actions
 *     - UI toggles (Tab, I, B, etc.)
 *
 *   EMOInputContext::BaseBuilding
 *     - Active during building placement mode
 *     - Ghost rotation (Q/E for yaw)
 *     - Placement confirm (Left Click)
 *     - Cancel (Right Click, Escape)
 *
 *   EMOInputContext::Menu
 *     - Active when UI menus are open
 *     - Navigation and selection
 *
 * CONTEXT SWITCHING:
 *   SetInputContext(EMOInputContext, bRemoveOthers)
 *     - Adds the specified context
 *     - If bRemoveOthers=true, removes all other contexts first
 *     - Handles priority ordering
 *
 * -----------------------------------------------------------------------------
 * INPUT HANDLING FLOW
 * -----------------------------------------------------------------------------
 *
 * 1. Enhanced Input System binds actions to handler functions
 * 2. Handler functions (HandleMove, HandleLook, etc.) are called
 * 3. For pawn-related input:
 *    a. Get possessed pawn
 *    b. Check if pawn implements IMOControllableInterface
 *    c. Call interface method (ReceiveMoveInput, ReceiveLookInput, etc.)
 * 4. For UI input:
 *    a. Call UIManagerComponent->Toggle*Menu()
 * 5. For building input:
 *    a. If in placement mode: BuildingComponent handles
 *    b. Otherwise: UIManagerComponent->ToggleBuildingMenu()
 *
 * -----------------------------------------------------------------------------
 * INPUT ACTION BINDINGS
 * -----------------------------------------------------------------------------
 *
 * Movement/Camera:
 *   IA_Move -> HandleMove() -> Pawn->ReceiveMoveInput()
 *   IA_Look -> HandleLook() -> Pawn->ReceiveLookInput()
 *   IA_Jump -> HandleJump() -> Pawn->ReceiveJumpInput()
 *   IA_Sprint -> HandleSprint() -> Pawn->ReceiveSprintInput()
 *   IA_Crouch -> HandleCrouch() -> Pawn->ReceiveCrouchInput()
 *
 * Actions:
 *   IA_PrimaryAction -> HandlePrimaryAction()
 *     -> If building mode: BuildingComponent->HandlePlacementPrimaryAction()
 *     -> Else: Pawn->ReceivePrimaryActionInput()
 *   IA_SecondaryAction -> HandleSecondaryAction()
 *     -> If building mode: BuildingComponent->HandlePlacementSecondaryAction()
 *     -> Else: Pawn->ReceiveSecondaryActionInput()
 *   IA_Interact -> HandleInteract() -> Pawn->ReceiveInteractInput()
 *
 * UI:
 *   IA_InGameMenu -> HandleInGameMenu() -> UIManager->ToggleInGameMenu()
 *   IA_Inventory -> HandleInventory() -> UIManager->ToggleInventoryMenu()
 *   IA_Crafting -> HandleCrafting() -> UIManager->ToggleCraftingMenu()
 *   IA_Skills -> HandleSkills() -> UIManager->ToggleSkillsPanel()
 *   IA_Build -> HandleBuild() -> UIManager->ToggleBuildingMenu()
 *   IA_Possess -> HandlePossess() -> UIManager->TogglePossessionMenu()
 *
 * Building:
 *   IA_RotateBuildingCW -> HandleRotateBuildingCW()
 *     -> BuildingComponent->RotateGhostZ(+increment)
 *   IA_RotateBuildingCCW -> HandleRotateBuildingCCW()
 *     -> BuildingComponent->RotateGhostZ(-increment)
 *
 * =============================================================================
 */

class APawn;
class UInputMappingContext;
class UInputAction;
class UMOUIManagerComponent;
class UMOPossessionComponent;
class UMONotificationComponent;
class UMOBuildingComponent;
class UEnhancedInputLocalPlayerSubsystem;

/**
 * Input context types for mapping context management.
 * Used with SetInputContext() to switch between input modes.
 */
UENUM(BlueprintType)
enum class EMOInputContext : uint8
{
	None = 0,
	PawnControl,		// Direct pawn control (movement, look, actions)
	BaseBuilding,		// Building placement and construction
	Menu				// Menu navigation
};

/**
 * Base player controller for the MO Framework.
 * Centralizes all player input handling and delegates to possessed pawns via IMOControllableInterface.
 *
 * See file header for full architecture documentation.
 */
UCLASS()
class MOFRAMEWORK_API AMOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMOPlayerController();

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	/** UI management component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Components")
	TObjectPtr<UMOUIManagerComponent> UIManagerComponent;

	/** Pawn possession component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Components")
	TObjectPtr<UMOPossessionComponent> PossessionComponent;

	/** Notification display component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Components")
	TObjectPtr<UMONotificationComponent> NotificationComponent;

	/** Building placement component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Components")
	TObjectPtr<UMOBuildingComponent> BuildingComponent;

	// ============================================================================
	// INPUT MAPPING CONTEXTS
	// ============================================================================

	/** Mapping context for direct pawn control. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	TSoftObjectPtr<UInputMappingContext> PawnControlContext;

	/** Mapping context for base building mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	TSoftObjectPtr<UInputMappingContext> BaseBuildingContext;

	/** Mapping context for menu navigation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	TSoftObjectPtr<UInputMappingContext> MenuContext;

	/** Priority for pawn control context (higher = takes precedence). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	int32 PawnControlContextPriority = 0;

	/** Priority for base building context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	int32 BaseBuildingContextPriority = 1;

	/** Priority for menu context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	int32 MenuContextPriority = 100;

	// ============================================================================
	// INPUT ACTIONS - MOVEMENT
	// ============================================================================

	/** Move action (WASD). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TSoftObjectPtr<UInputAction> MoveAction;

	/** Look action (mouse/gamepad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TSoftObjectPtr<UInputAction> LookAction;

	/** Jump action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TSoftObjectPtr<UInputAction> JumpAction;

	/** Hustle action (tap=toggle jog, hold=sprint). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TSoftObjectPtr<UInputAction> HustleAction;

	/** Time threshold for hold vs tap (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Input|Actions|Movement")
	float HustleHoldThreshold = 0.2f;

	/** Crouch action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TSoftObjectPtr<UInputAction> CrouchAction;

	// ============================================================================
	// INPUT ACTIONS - ACTIONS
	// ============================================================================

	/** Interact action (E key). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Actions")
	TSoftObjectPtr<UInputAction> InteractAction;

	/** Primary action (left mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Actions")
	TSoftObjectPtr<UInputAction> PrimaryAction;

	/** Secondary action (right mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Actions")
	TSoftObjectPtr<UInputAction> SecondaryAction;

	// ============================================================================
	// INPUT ACTIONS - UI/SYSTEM
	// ============================================================================

	/** Toggle inventory action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> InventoryAction;

	/** Toggle crafting menu action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> CraftAction;

	/** Toggle player status HUD action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> PlayerStatusAction;

	/** Toggle skills panel action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> SkillsAction;

	/** Toggle building menu / placement mode action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> BuildAction;

	/** Pause/menu action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> PauseAction;

	/** Possess nearest pawn action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|System")
	TSoftObjectPtr<UInputAction> PossessAction;

	// ============================================================================
	// INPUT ACTIONS - BUILDING
	// ============================================================================

	/** Place building ghost (Left Mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TSoftObjectPtr<UInputAction> PlaceBuildingAction;

	/** Rotate building ghost clockwise (Q). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TSoftObjectPtr<UInputAction> RotateBuildingCWAction;

	/** Rotate building ghost counter-clockwise (E). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TSoftObjectPtr<UInputAction> RotateBuildingCCWAction;

	/** Cancel building placement (Escape/RMB). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TSoftObjectPtr<UInputAction> CancelPlacementAction;

	/** Rotation increment per keypress in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Input|Actions|Building")
	float BuildingRotationIncrement = 15.0f;

	// ============================================================================
	// DEBUG
	// ============================================================================

	/** Enable debug mode for development commands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Debug")
	bool bDebugModeEnabled = true;

	/** Pawn class to spawn with debug spawn command (0 key). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Debug")
	TSubclassOf<APawn> DebugSpawnPawnClass;

	/** Distance to spawn debug pawn in front of camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Debug")
	float DebugSpawnDistance = 300.0f;

	// ============================================================================
	// CONTEXT MANAGEMENT
	// ============================================================================

	/**
	 * Switch to a different input context.
	 * @param NewContext - The context to switch to
	 * @param bRemoveOthers - If true, removes other non-system contexts
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	void SetInputContext(EMOInputContext NewContext, bool bRemoveOthers = true);

	/**
	 * Add an input context without removing others.
	 * @param Context - The context to add
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	void AddInputContext(EMOInputContext Context);

	/**
	 * Remove an input context.
	 * @param Context - The context to remove
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	void RemoveInputContext(EMOInputContext Context);

	/**
	 * Check if a context is currently active.
	 * @param Context - The context to check
	 * @return True if the context is active
	 */
	UFUNCTION(BlueprintPure, Category="MO|Input")
	bool IsInputContextActive(EMOInputContext Context) const;

	/**
	 * Get the current primary input context.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Input")
	EMOInputContext GetCurrentInputContext() const { return CurrentInputContext; }

	// ============================================================================
	// COMPONENT ACCESSORS
	// ============================================================================

	/** Get the UI Manager component. */
	UFUNCTION(BlueprintPure, Category="MO|Components")
	UMOUIManagerComponent* GetUIManager() const { return UIManagerComponent; }

	/** Get the Possession component. */
	UFUNCTION(BlueprintPure, Category="MO|Components")
	UMOPossessionComponent* GetPossessionComponent() const { return PossessionComponent; }

	/** Get the Notification component. */
	UFUNCTION(BlueprintPure, Category="MO|Components")
	UMONotificationComponent* GetNotificationComponent() const { return NotificationComponent; }

	/** Get the Building component. */
	UFUNCTION(BlueprintPure, Category="MO|Components")
	UMOBuildingComponent* GetBuildingComponent() const { return BuildingComponent; }

protected:
	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// ============================================================================
	// INPUT HANDLERS - MOVEMENT
	// ============================================================================

	/** Handle move input. */
	void HandleMove(const FInputActionValue& Value);

	/** Handle look input. */
	void HandleLook(const FInputActionValue& Value);

	/** Handle jump pressed. */
	void HandleJumpStart(const FInputActionValue& Value);

	/** Handle jump released. */
	void HandleJumpEnd(const FInputActionValue& Value);

	/** Handle hustle pressed (start tracking for tap/hold). */
	void HandleHustleStart(const FInputActionValue& Value);

	/** Handle hustle triggered (ongoing hold). */
	void HandleHustleTriggered(const FInputActionValue& Value);

	/** Handle hustle released (determine tap vs hold). */
	void HandleHustleEnd(const FInputActionValue& Value);

	/** Handle crouch. */
	void HandleCrouch(const FInputActionValue& Value);

	// ============================================================================
	// INPUT HANDLERS - ACTIONS
	// ============================================================================

	/** Handle interact. */
	void HandleInteract(const FInputActionValue& Value);

	/** Handle primary action pressed. */
	void HandlePrimaryAction(const FInputActionValue& Value);

	/** Handle primary action released. */
	void HandlePrimaryActionRelease(const FInputActionValue& Value);

	/** Handle secondary action pressed. */
	void HandleSecondaryAction(const FInputActionValue& Value);

	/** Handle secondary action released. */
	void HandleSecondaryActionRelease(const FInputActionValue& Value);

	// ============================================================================
	// INPUT HANDLERS - UI/SYSTEM
	// ============================================================================

	/** Handle inventory toggle. */
	void HandleInventory(const FInputActionValue& Value);

	/** Handle crafting menu toggle. */
	void HandleCraft(const FInputActionValue& Value);

	/** Handle player status toggle. */
	void HandlePlayerStatus(const FInputActionValue& Value);

	/** Handle skills panel toggle. */
	void HandleSkills(const FInputActionValue& Value);

	/** Handle building menu toggle. */
	void HandleBuild(const FInputActionValue& Value);

	/** Handle pause/menu. */
	void HandlePause(const FInputActionValue& Value);

	/** Handle possess action. */
	void HandlePossess(const FInputActionValue& Value);

	// ============================================================================
	// INPUT HANDLERS - BUILDING
	// ============================================================================

	/** Handle place building (left click). */
	void HandlePlaceBuilding(const FInputActionValue& Value);

	/** Handle rotate building clockwise. */
	void HandleRotateBuildingCW(const FInputActionValue& Value);

	/** Handle rotate building counter-clockwise. */
	void HandleRotateBuildingCCW(const FInputActionValue& Value);

	/** Handle cancel placement. */
	void HandleCancelPlacement(const FInputActionValue& Value);

	// ============================================================================
	// DEBUG INPUT HANDLERS
	// ============================================================================

	/** Debug: Spawn a pawn and possess it (0 key). */
	void HandleDebugSpawnPawn();

	/** Debug: Toggle debug mode (F1 key). */
	void HandleDebugToggle();

	/** Setup debug key bindings (hard-coded, no input actions needed). */
	void SetupDebugInputBindings();

	// ============================================================================
	// INTERNAL
	// ============================================================================

	/** Get the enhanced input subsystem. */
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	/** Add a mapping context to the input subsystem. */
	void AddMappingContext(UInputMappingContext* Context, int32 Priority);

	/** Remove a mapping context from the input subsystem. */
	void RemoveMappingContext(UInputMappingContext* Context);

	/** Setup default input context on begin play. */
	void SetupDefaultInputContext();

private:
	/** Currently active primary input context. */
	EMOInputContext CurrentInputContext = EMOInputContext::PawnControl;

	/** Cached weak reference to possessed pawn (for interface calls). */
	TWeakObjectPtr<APawn> CachedControllablePawn;

	/** Time when hustle button was pressed (for tap/hold detection). */
	float HustlePressTime = 0.0f;

	/** Whether we've already started sprinting from holding hustle. */
	bool bHustleHoldTriggered = false;
};
