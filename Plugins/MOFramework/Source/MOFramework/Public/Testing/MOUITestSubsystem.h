/**
 * =============================================================================
 * MOUITestSubsystem.h - Automated UI Testing Subsystem
 * =============================================================================
 *
 * PURPOSE:
 * Provides automated testing infrastructure for the UI system. Allows
 * programmatic opening/closing of menus, focus validation, input state
 * verification, and logging of test results.
 *
 * USAGE:
 * - Run tests via editor menu: MO Framework > UI Tests > Run All Tests
 * - Or call directly: UMOUITestSubsystem::Get(World)->RunAllTests()
 * - Results logged to: Content/Python/test_output/ui_test_results.txt
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "MOUITestSubsystem.generated.h"

class APlayerController;
class UMOUIManagerComponent;
class UMOMenuWidgetBase;
class UWidget;

/**
 * Result of a single test case
 */
USTRUCT(BlueprintType)
struct FMOUITestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TestName;

	UPROPERTY(BlueprintReadOnly)
	bool bPassed = false;

	UPROPERTY(BlueprintReadOnly)
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly)
	float DurationMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Logs;
};

/**
 * Summary of a test run
 */
USTRUCT(BlueprintType)
struct FMOUITestSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 TotalTests = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PassedTests = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 FailedTests = 0;

	UPROPERTY(BlueprintReadOnly)
	float TotalDurationMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FMOUITestResult> Results;
};

/**
 * Delegate for test completion
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOUITestComplete, const FMOUITestSummary&, Summary);

/**
 * Automated UI testing subsystem.
 *
 * Provides programmatic control over UI menus and validation of:
 * - Menu open/close behavior
 * - Toggle key functionality
 * - Escape/Tab close behavior
 * - Focus management
 * - Input state (cursor, movement blocking)
 * - Nested menu handling
 */
UCLASS()
class MOFRAMEWORK_API UMOUITestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem Interface

	/** Get the subsystem from a world context */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing", meta = (WorldContext = "WorldContextObject"))
	static UMOUITestSubsystem* Get(const UObject* WorldContextObject);

	// =========================================================================
	// TEST EXECUTION
	// =========================================================================

	/** Run all UI tests. Returns summary when complete. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	FMOUITestSummary RunAllTests();

	/** Run a specific test by name. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	FMOUITestResult RunTest(const FString& TestName);

	/** Run tests matching a pattern (e.g., "Inventory*"). */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	FMOUITestSummary RunTestsMatching(const FString& Pattern);

	/** Check if tests are currently running. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	bool IsRunningTests() const { return bIsRunningTests; }

	/** Broadcast when test run completes. */
	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Testing")
	FMOUITestComplete OnTestsComplete;

	// =========================================================================
	// TEST UTILITIES
	// =========================================================================

	/** Get list of all available test names. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	TArray<FString> GetAllTestNames() const;

	/** Write test results to file. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	void WriteResultsToFile(const FMOUITestSummary& Summary, const FString& FilePath);

	/** Get the last test summary. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	FMOUITestSummary GetLastTestSummary() const { return LastTestSummary; }

	// =========================================================================
	// STATE INSPECTION (for test validation)
	// =========================================================================

	/** Check if any menu is currently open. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	bool IsAnyMenuOpen() const;

	/** Get count of active menus. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	int32 GetActiveMenuCount() const;

	/** Check if cursor is currently visible. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	bool IsCursorVisible() const;

	/** Check if movement input is currently ignored. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	bool IsMoveInputIgnored() const;

	/** Check if look input is currently ignored. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	bool IsLookInputIgnored() const;

	/** Get the widget that currently has keyboard focus. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	UWidget* GetFocusedWidget() const;

	/** Check if a specific menu type is open. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	bool IsMenuOpen(const FString& MenuName) const;

	/** Get count of widgets in a specific layer. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	int32 GetLayerWidgetCount(FGameplayTag LayerTag) const;

	/** Check current input mode from CommonUI. */
	UFUNCTION(BlueprintPure, Category = "MO|UI|Testing")
	FString GetCurrentInputMode() const;

	/** Print full CommonUI diagnostic info. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	void PrintDiagnostics() const;

	// =========================================================================
	// MENU CONTROL (for test automation)
	// =========================================================================

	/** Open a menu by name (Inventory, Crafting, Building, Skills, Status, InGame, Possession). */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	bool OpenMenu(const FString& MenuName);

	/** Close all open menus. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	void CloseAllMenus();

	/** Simulate pressing a key. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	void SimulateKeyPress(FKey Key);

	/** Simulate pressing Escape. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	void SimulateEscape();

	/** Simulate pressing Tab. */
	UFUNCTION(BlueprintCallable, Category = "MO|UI|Testing")
	void SimulateTab();

	/** Wait for a condition with timeout. Returns true if condition was met. */
	bool WaitForCondition(TFunction<bool()> Condition, float TimeoutSeconds = 1.0f);

private:
	// =========================================================================
	// TEST IMPLEMENTATIONS
	// =========================================================================

	// =========================================================================
	// SETUP VALIDATION TESTS - CRITICAL: Must pass for other tests to work
	// =========================================================================
	FMOUITestResult Test_Setup_UISettingsConfigured();
	FMOUITestResult Test_Setup_LayoutCreated();
	FMOUITestResult Test_Setup_LayerStacksExist();
	FMOUITestResult Test_Setup_ActionRouterExists();
	FMOUITestResult Test_Setup_UIManagerExists();
	FMOUITestResult Test_Setup_ControllersExist();

	// =========================================================================
	// INVENTORY TESTS
	// =========================================================================
	FMOUITestResult Test_Inventory_Open();
	FMOUITestResult Test_Inventory_CloseEscape();
	FMOUITestResult Test_Inventory_CloseToggle();
	FMOUITestResult Test_Inventory_CloseTab();
	FMOUITestResult Test_Inventory_InputState();
	FMOUITestResult Test_Inventory_FocusAfterButtonClick();
	FMOUITestResult Test_Inventory_ReopenAfterClose();

	// =========================================================================
	// CRAFTING TESTS
	// =========================================================================
	FMOUITestResult Test_Crafting_Open();
	FMOUITestResult Test_Crafting_CloseEscape();
	FMOUITestResult Test_Crafting_CloseToggle();
	FMOUITestResult Test_Crafting_CloseTab();
	FMOUITestResult Test_Crafting_InputState();
	FMOUITestResult Test_Crafting_CategoryExists();

	// =========================================================================
	// BUILDING TESTS
	// =========================================================================
	FMOUITestResult Test_Building_Open();
	FMOUITestResult Test_Building_CloseEscape();
	FMOUITestResult Test_Building_CloseToggle();
	FMOUITestResult Test_Building_CloseTab();
	FMOUITestResult Test_Building_InputState();
	FMOUITestResult Test_Building_CategoryExists();

	// =========================================================================
	// SKILLS TESTS
	// =========================================================================
	FMOUITestResult Test_Skills_Open();
	FMOUITestResult Test_Skills_CloseEscape();
	FMOUITestResult Test_Skills_CloseTab();
	FMOUITestResult Test_Skills_CloseToggle();
	FMOUITestResult Test_Skills_CategoryCycling();
	FMOUITestResult Test_Skills_InputState();

	// =========================================================================
	// STATUS TESTS
	// =========================================================================
	FMOUITestResult Test_Status_Open();
	FMOUITestResult Test_Status_CloseEscape();
	FMOUITestResult Test_Status_CloseTab();
	FMOUITestResult Test_Status_CloseToggle();
	FMOUITestResult Test_Status_CategoryCycling();
	FMOUITestResult Test_Status_InputState();

	// =========================================================================
	// IN-GAME MENU TESTS
	// =========================================================================
	FMOUITestResult Test_InGame_Open();
	FMOUITestResult Test_InGame_CloseEscape();
	FMOUITestResult Test_InGame_InputBlocking();
	FMOUITestResult Test_InGame_BlocksOtherMenus();
	FMOUITestResult Test_InGame_ResumeButton();

	// =========================================================================
	// POSSESSION MENU TESTS
	// =========================================================================
	FMOUITestResult Test_Possession_Open();
	FMOUITestResult Test_Possession_CloseEscape();
	FMOUITestResult Test_Possession_InputState();

	// =========================================================================
	// CONTEXT MENU TESTS
	// =========================================================================
	FMOUITestResult Test_ContextMenu_ItemContextOpen();
	FMOUITestResult Test_ContextMenu_ItemContextCloseEscape();
	FMOUITestResult Test_ContextMenu_ClosesOnClickOutside();
	FMOUITestResult Test_ContextMenu_ParentMenuStaysOpen();

	// =========================================================================
	// CONFIRMATION DIALOG TESTS
	// =========================================================================
	FMOUITestResult Test_Confirmation_Open();
	FMOUITestResult Test_Confirmation_CloseEscape();
	FMOUITestResult Test_Confirmation_AcceptButton();
	FMOUITestResult Test_Confirmation_CancelButton();
	FMOUITestResult Test_Confirmation_ModalBlocking();

	// =========================================================================
	// MENU SWITCHING TESTS
	// =========================================================================
	FMOUITestResult Test_MenuSwitch_InventoryToCrafting();
	FMOUITestResult Test_MenuSwitch_CraftingToBuilding();
	FMOUITestResult Test_MenuSwitch_BuildingToSkills();
	FMOUITestResult Test_MenuSwitch_SkillsToStatus();
	FMOUITestResult Test_MenuSwitch_AllMenusCycle();
	FMOUITestResult Test_MenuSwitch_InGameBlocksSwitch();

	// =========================================================================
	// NESTED/STACKING TESTS
	// =========================================================================
	FMOUITestResult Test_Nested_ContextMenuEscapeClosesOnlyContext();
	FMOUITestResult Test_Nested_MultipleModalsStack();
	FMOUITestResult Test_Nested_ConfirmationOverMenu();
	FMOUITestResult Test_Nested_CorrectCloseOrder();

	// =========================================================================
	// FOCUS TESTS
	// =========================================================================
	FMOUITestResult Test_Focus_RestoredAfterMenuClose();
	FMOUITestResult Test_Focus_MenuReceivesFocusOnOpen();
	FMOUITestResult Test_Focus_ModalReceivesFocusOverMenu();
	FMOUITestResult Test_Focus_ReturnToGameAfterAllClosed();

	// =========================================================================
	// INPUT STATE TESTS
	// =========================================================================
	FMOUITestResult Test_InputState_CursorHiddenWhenNoMenus();
	FMOUITestResult Test_InputState_CursorVisibleWhenMenuOpen();
	FMOUITestResult Test_InputState_MovementBlockedWhenMenuOpen();
	FMOUITestResult Test_InputState_MovementRestoredAfterAllMenusClosed();
	FMOUITestResult Test_InputState_LookBlockedWhenMenuOpen();
	FMOUITestResult Test_InputState_GameInputBlockedByInGameMenu();

	// =========================================================================
	// HUD TESTS
	// =========================================================================
	FMOUITestResult Test_HUD_ReticleVisibleInGame();
	FMOUITestResult Test_HUD_ReticleHiddenWhenMenuOpen();
	FMOUITestResult Test_HUD_NotificationDisplays();
	FMOUITestResult Test_HUD_NotificationAutoDismisses();

	// =========================================================================
	// BUILD MODE TESTS
	// =========================================================================
	FMOUITestResult Test_BuildMode_WidgetAppearsOnEnter();
	FMOUITestResult Test_BuildMode_WidgetHidesOnExit();
	FMOUITestResult Test_BuildMode_EscapeExitsBuildMode();

	// =========================================================================
	// TOGGLE KEY TESTS (comprehensive)
	// =========================================================================
	FMOUITestResult Test_ToggleKey_InventoryOpensAndCloses();
	FMOUITestResult Test_ToggleKey_CraftingOpensAndCloses();
	FMOUITestResult Test_ToggleKey_BuildingOpensAndCloses();
	FMOUITestResult Test_ToggleKey_SkillsOpensAndCloses();
	FMOUITestResult Test_ToggleKey_StatusOpensAndCloses();
	FMOUITestResult Test_ToggleKey_WorksAfterButtonClick();

	// =========================================================================
	// STRESS/EDGE CASE TESTS
	// =========================================================================
	FMOUITestResult Test_Stress_RapidOpenClose();
	FMOUITestResult Test_Stress_RapidMenuSwitch();
	FMOUITestResult Test_Edge_OpenSameMenuTwice();
	FMOUITestResult Test_Edge_CloseAlreadyClosedMenu();
	FMOUITestResult Test_Edge_EscapeWithNoMenuOpen();

	// =========================================================================
	// COMMONUI-SPECIFIC TESTS
	// =========================================================================
	FMOUITestResult Test_CommonUI_LayerStackPush();
	FMOUITestResult Test_CommonUI_LayerStackPop();
	FMOUITestResult Test_CommonUI_MenuInputModeAll();
	FMOUITestResult Test_CommonUI_ModalInputModeMenu();
	FMOUITestResult Test_CommonUI_BackActionHandler();
	FMOUITestResult Test_CommonUI_ModalBlocksInput();
	FMOUITestResult Test_CommonUI_ToggleKeyPassthrough();
	FMOUITestResult Test_CommonUI_FocusRestoration();
	FMOUITestResult Test_CommonUI_WidgetActivation();
	FMOUITestResult Test_CommonUI_WidgetDeactivation();

	// =========================================================================
	// HELPERS
	// =========================================================================

	/** Get the UI manager component. */
	UMOUIManagerComponent* GetUIManager() const;

	/** Get the player controller. */
	APlayerController* GetPlayerController() const;

	/** Register all test cases. */
	void RegisterTests();

	/** Log a test message. */
	void LogTest(const FString& Message);

	/** Create a test result. */
	FMOUITestResult MakeResult(const FString& TestName, bool bPassed, const FString& ErrorMessage = TEXT(""));

	// =========================================================================
	// STATE
	// =========================================================================

	/** Map of test name to test function. */
	TMap<FString, TFunction<FMOUITestResult()>> TestRegistry;

	/** Whether tests are currently running. */
	bool bIsRunningTests = false;

	/** Last test summary. */
	FMOUITestSummary LastTestSummary;

	/** Current test logs (accumulated during test). */
	TArray<FString> CurrentTestLogs;

	/** Cached player controller. */
	mutable TWeakObjectPtr<APlayerController> CachedPlayerController;
};
