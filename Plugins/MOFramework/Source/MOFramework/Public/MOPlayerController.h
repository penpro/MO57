#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MOPlayerController.generated.h"

class APawn;
class UInputMappingContext;
class UInputAction;
class UMOUIManagerComponent;
class UMOPossessionComponent;
class UEnhancedInputLocalPlayerSubsystem;

/**
 * Input context types for mapping context management.
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
 * Responsibilities:
 * - Receives and processes all player input
 * - Manages input mapping contexts
 * - Delegates movement/action input to possessed pawn
 * - Hosts UI management and possession components
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

	/** Toggle player status HUD action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> PlayerStatusAction;

	/** Pause/menu action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|UI")
	TSoftObjectPtr<UInputAction> PauseAction;

	/** Possess nearest pawn action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input|Actions|System")
	TSoftObjectPtr<UInputAction> PossessAction;

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

	/** Handle player status toggle. */
	void HandlePlayerStatus(const FInputActionValue& Value);

	/** Handle pause/menu. */
	void HandlePause(const FInputActionValue& Value);

	/** Handle possess action. */
	void HandlePossess(const FInputActionValue& Value);

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
