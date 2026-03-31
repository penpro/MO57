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

	// Inventory Tests
	FMOUITestResult Test_Inventory_Open();
	FMOUITestResult Test_Inventory_CloseEscape();
	FMOUITestResult Test_Inventory_CloseToggle();
	FMOUITestResult Test_Inventory_CloseTab();
	FMOUITestResult Test_Inventory_InputState();
	FMOUITestResult Test_Inventory_FocusAfterButtonClick();

	// Crafting Tests
	FMOUITestResult Test_Crafting_Open();
	FMOUITestResult Test_Crafting_CloseEscape();
	FMOUITestResult Test_Crafting_CloseToggle();
	FMOUITestResult Test_Crafting_InputState();

	// Building Tests
	FMOUITestResult Test_Building_Open();
	FMOUITestResult Test_Building_CloseEscape();
	FMOUITestResult Test_Building_CloseToggle();
	FMOUITestResult Test_Building_InputState();

	// Skills Tests
	FMOUITestResult Test_Skills_Open();
	FMOUITestResult Test_Skills_CloseEscape();
	FMOUITestResult Test_Skills_CategoryCycling();

	// Status Tests
	FMOUITestResult Test_Status_Open();
	FMOUITestResult Test_Status_CloseEscape();

	// InGame Menu Tests
	FMOUITestResult Test_InGame_Open();
	FMOUITestResult Test_InGame_CloseEscape();
	FMOUITestResult Test_InGame_InputBlocking();

	// Menu Switching Tests
	FMOUITestResult Test_MenuSwitch_InventoryToCrafting();
	FMOUITestResult Test_MenuSwitch_CraftingToBuilding();

	// Nested Menu Tests
	FMOUITestResult Test_Nested_ContextMenuEscapeClosesOnlyContext();

	// Focus Tests
	FMOUITestResult Test_Focus_RestoredAfterMenuClose();

	// Input State Tests
	FMOUITestResult Test_InputState_CursorHiddenWhenNoMenus();
	FMOUITestResult Test_InputState_MovementRestoredAfterAllMenusClosed();

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
