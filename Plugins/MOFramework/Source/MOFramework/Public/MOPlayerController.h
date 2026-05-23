#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MOUIContractInterface.h"
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
 *   IA_MOMove -> HandleMove() -> Pawn->ReceiveMoveInput()
 *   IA_MOLook -> HandleLook() -> Pawn->ReceiveLookInput()
 *   IA_MOJump -> HandleJump() -> Pawn->ReceiveJumpInput()
 *   IA_Hustle -> HandleHustle() -> Pawn->ReceiveSprintInput() / ToggleJog()
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
 *   IA_Journal -> HandleJournal() -> UIManager->ToggleQuestLog()
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

// UI Controllers
class UMOInventoryUIController;
class UMOCraftingUIController;
class UMOBuildingUIController;
class UMOCharacterUIController;
class UMOSystemMenuUIController;
class UMOQuestUIController;

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
class MOFRAMEWORK_API AMOPlayerController : public APlayerController,
	public IMOUIContractInterface
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
	// UI CONTROLLER COMPONENTS
	// ============================================================================

	/** Inventory UI controller - handles inventory menus, item context, looting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|UI Controllers")
	TObjectPtr<UMOInventoryUIController> InventoryUIController;

	/** Crafting UI controller - handles crafting menu, station context, harvest. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|UI Controllers")
	TObjectPtr<UMOCraftingUIController> CraftingUIController;

	/** Building UI controller - handles building menu, ghost context. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|UI Controllers")
	TObjectPtr<UMOBuildingUIController> BuildingUIController;

	/** Character UI controller - handles skills panel, status panel, inspection. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|UI Controllers")
	TObjectPtr<UMOCharacterUIController> CharacterUIController;

	/** System menu UI controller - handles in-game menu, possession menu, confirmations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|UI Controllers")
	TObjectPtr<UMOSystemMenuUIController> SystemMenuUIController;

	/** Quest UI controller - handles quest log panel, quest HUD tracker. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|UI Controllers")
	TObjectPtr<UMOQuestUIController> QuestUIController;

	// ============================================================================
	// INPUT MAPPING CONTEXTS
	// ============================================================================

	/** Mapping context for direct pawn control. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	TObjectPtr<UInputMappingContext> PawnControlContext;

	/** Mapping context for base building mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	TObjectPtr<UInputMappingContext> BaseBuildingContext;

	/** Mapping context for menu navigation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Contexts")
	TObjectPtr<UInputMappingContext> MenuContext;

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
	TObjectPtr<UInputAction> MoveAction;

	/** Look action (mouse/gamepad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TObjectPtr<UInputAction> LookAction;

	/** Jump action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TObjectPtr<UInputAction> JumpAction;

	/** Hustle action (tap=toggle jog, hold=sprint). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TObjectPtr<UInputAction> HustleAction;

	/** Time threshold for hold vs tap (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Input|Actions|Movement")
	float HustleHoldThreshold = 0.2f;

	/** Crouch action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Movement")
	TObjectPtr<UInputAction> CrouchAction;

	// ============================================================================
	// INPUT ACTIONS - ACTIONS
	// ============================================================================

	/** Interact action (E key). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Actions")
	TObjectPtr<UInputAction> InteractAction;

	/** Primary action (left mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Actions")
	TObjectPtr<UInputAction> PrimaryAction;

	/** Secondary action (right mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Actions")
	TObjectPtr<UInputAction> SecondaryAction;

	// ============================================================================
	// INPUT ACTIONS - UI/SYSTEM
	// ============================================================================

	/** Toggle inventory action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> InventoryAction;

	/** Toggle crafting menu action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> CraftAction;

	/** Toggle player status HUD action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> PlayerStatusAction;

	/** Toggle skills panel action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> SkillsAction;

	/** Toggle quest/journal panel action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> JournalAction;

	/** Toggle building menu / placement mode action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> BuildAction;

	/**
	 * Back / menu action (typically Tab).
	 *
	 * Behavior on press:
	 *   - If any menu is open → close the topmost active menu
	 *   - Else → open the in-game system menu
	 *
	 * NAMING NOTE: this is NOT pause. MO57 has a strict no-pause policy
	 * (see Docs/PAUSE_POLICY.md). The earlier "PauseAction" + "CloseMenuAction"
	 * pair was misleading and overlapping — consolidated to this single
	 * action that captures both "back out of UI" and "open the menu" in
	 * one input.
	 *
	 * Recommended IA asset name: IA_Back_Menu.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TObjectPtr<UInputAction> BackMenuAction;

	/** Possess nearest pawn action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|System")
	TObjectPtr<UInputAction> PossessAction;

	// ============================================================================
	// INPUT ACTIONS - BUILDING
	// ============================================================================

	/** Place building ghost (Left Mouse). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TObjectPtr<UInputAction> PlaceBuildingAction;

	/** Rotate building ghost clockwise (Q). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TObjectPtr<UInputAction> RotateBuildingCWAction;

	/** Rotate building ghost counter-clockwise (E). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TObjectPtr<UInputAction> RotateBuildingCCWAction;

	/**
	 * Flip building ghost 180° in place (mouse wheel up / down). Toggle —
	 * each press swaps between flipped and un-flipped. Rotates around the
	 * cube center so position stays put; Q/E still cycle the snap edge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TObjectPtr<UInputAction> FlipPlacementYawAction;

	/** Cancel building placement (Escape/RMB). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Building")
	TObjectPtr<UInputAction> CancelPlacementAction;

	/** Rotation increment per keypress in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Input|Actions|Building")
	float BuildingRotationIncrement = 15.0f;

	// ============================================================================
	// INPUT ACTIONS - TERRAFORMING
	// ============================================================================

	/** Toggle terraforming mode action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Terraforming")
	TObjectPtr<UInputAction> TerraformToggleAction;

	/** Cycle terraforming tool action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Terraforming")
	TObjectPtr<UInputAction> TerraformCycleToolAction;

	// ============================================================================
	// INPUT ACTIONS - CAMERA
	// ============================================================================

	/** Toggle camera shoulder side (swap between left and right shoulder view). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Camera")
	TObjectPtr<UInputAction> ViewAction;

	// ============================================================================
	// INPUT ACTIONS - TUTORIAL
	// ============================================================================

	/**
	 * Skip the current tutorial popup objective (default F1). Marks the
	 * active tutorial objective complete without firing its event — the
	 * chain advances and the next hint appears.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Tutorial")
	TObjectPtr<UInputAction> SkipTutorialItemAction;

	/**
	 * Globally disable all tutorial popup banners for the rest of the session
	 * (default F3). Quest progress still happens silently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|Tutorial")
	TObjectPtr<UInputAction> SkipAllTutorialsAction;

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
	// SPECTATOR / CAMERA CONTROL
	// ============================================================================

	/**
	 * Position the spectator camera above a world location, looking down.
	 * @param TargetLocation - The X,Y location to position above
	 * @param Height - Height above the target location
	 * @param PitchAngle - Pitch angle in degrees (negative = looking down)
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void SetSpectatorViewAboveLocation(FVector TargetLocation, float Height = 2000.0f, float PitchAngle = -60.0f);

	/**
	 * Get the GUID of the last possessed pawn (for save/load).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Possession")
	FGuid GetLastPossessedPawnGuid() const { return LastPossessedPawnGuid; }

	/**
	 * Set the last possessed pawn GUID (called by persistence on load).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Possession")
	void SetLastPossessedPawnGuid(const FGuid& InGuid) { LastPossessedPawnGuid = InGuid; }

	/**
	 * Enable or disable spectator movement controls.
	 * When disabled, camera remains fixed until a pawn is possessed.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void SetSpectatorInputEnabled(bool bEnabled);

	/**
	 * Check if spectator input is enabled.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spectator")
	bool IsSpectatorInputEnabled() const { return bSpectatorInputEnabled; }

	/**
	 * Check if there's a pending spectator view to apply.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Spectator")
	bool HasPendingSpectatorView() const { return bHasPendingSpectatorView; }

	/**
	 * Get the pending spectator view location and rotation.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void GetPendingSpectatorView(FVector& OutLocation, FRotator& OutRotation) const
	{
		OutLocation = PendingSpectatorLocation;
		OutRotation = PendingSpectatorRotation;
	}

	/**
	 * Clear the pending spectator view flag (called after applying).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void ClearPendingSpectatorView() { bHasPendingSpectatorView = false; }

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

	// ============================================================================
	// IMOUIContractInterface IMPLEMENTATION
	// ============================================================================

	virtual void RequestShowBuildWidget_Implementation(AActor* BuildingActor) override;
	virtual void RequestOpenContainerInventory_Implementation(AActor* ContainerActor) override;
	virtual void RequestOpenCraftingMenu_Implementation(AActor* StationActor) override;
	virtual void RequestShowGhostContextMenu_Implementation(AActor* GhostActor, FVector WorldPosition) override;
	virtual void RequestShowStationContextMenu_Implementation(AActor* StationActor, FVector WorldPosition) override;
	virtual void RequestStartCarcassButchering_Implementation(AActor* CarcassActor, FName PartId) override;

protected:
	// ============================================================================
	// OVERRIDES
	// ============================================================================

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void PlayerTick(float DeltaTime) override;

	/**
	 * POLICY: MO57 is pure real-time. SetPause is always refused and a
	 * warning is logged. See Docs/PAUSE_POLICY.md for the rationale.
	 *
	 * Override at PlayerController because every pause path in UE
	 * (UGameplayStatics::SetGamePaused, AGameMode::SetPause, BP nodes,
	 * console "pause" command) ultimately calls APlayerController::SetPause
	 * — this is the single chokepoint. Returning false means the world
	 * never enters paused state, regardless of caller.
	 */
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;

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

	/** Handle quest journal toggle. */
	void HandleJournal(const FInputActionValue& Value);

	/** Handle building menu toggle. */
	void HandleBuild(const FInputActionValue& Value);

	/**
	 * Handle Back/Menu input. Closes the topmost active menu if any UI is
	 * open; otherwise opens the in-game system menu. Bound to BackMenuAction
	 * (typically Tab). Does NOT pause — see Docs/PAUSE_POLICY.md.
	 */
	void HandleBackMenu(const FInputActionValue& Value);

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

	/** Handle in-place flip (mouse wheel) — toggles 180° yaw on the ghost. */
	void HandleFlipPlacementYaw(const FInputActionValue& Value);

	/** Handle cancel placement. */
	void HandleCancelPlacement(const FInputActionValue& Value);

	// ============================================================================
	// INPUT HANDLERS - TERRAFORMING
	// ============================================================================

	/** Handle terraforming mode toggle. */
	void HandleTerraformToggle(const FInputActionValue& Value);

	/** Handle terraforming tool cycle. */
	void HandleTerraformCycleTool(const FInputActionValue& Value);

	// ============================================================================
	// INPUT HANDLERS - CAMERA
	// ============================================================================

	/** Handle camera shoulder toggle. */
	void HandleView(const FInputActionValue& Value);

	// ============================================================================
	// INPUT HANDLERS - TUTORIAL
	// ============================================================================

	/** Skip the currently displayed tutorial popup item (F1). */
	void HandleSkipTutorialItem(const FInputActionValue& Value);

	/** Disable all tutorial popups for the rest of the session (F3). */
	void HandleSkipAllTutorials(const FInputActionValue& Value);

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

	/** GUID of the last possessed pawn (for save/load camera positioning). */
	FGuid LastPossessedPawnGuid;

	/** Whether spectator input (movement/look) is enabled when no pawn is possessed. */
	bool bSpectatorInputEnabled = false;

	/** Pending spectator view location (applied when spectator pawn spawns). */
	FVector PendingSpectatorLocation = FVector::ZeroVector;

	/** Pending spectator view rotation (applied when spectator pawn spawns). */
	FRotator PendingSpectatorRotation = FRotator::ZeroRotator;

	/** Whether we have a pending spectator view to apply. */
	bool bHasPendingSpectatorView = false;

	// ============================================================================
	// TUTORIAL EVENT BROADCAST
	// ============================================================================
	// Move/Look broadcast on every active tick. The quest subsystem matches
	// active objectives and discards unmatched events cheaply, so no throttle
	// or one-shot flag is needed (and one-shot flags caused stuck-popup bugs
	// when the player performed the action before the matching tutorial
	// quest activated).

	/** Fire a game event via the quest subsystem, if one exists. */
	void BroadcastTutorialEvent(FName EventName);
};
