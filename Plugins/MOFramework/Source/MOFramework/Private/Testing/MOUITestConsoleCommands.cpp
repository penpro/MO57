/**
 * MOUITestConsoleCommands.cpp - Console command registration for UI testing
 *
 * Provides console commands that can be executed from PIE to run UI tests.
 * These are called by the editor toolbox when tests are triggered.
 */

#include "Testing/MOUITestSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// Console command to run all UI tests
static FAutoConsoleCommandWithWorldAndArgs GMORunAllUITestsCmd(
	TEXT("MO.UI.RunAllTests"),
	TEXT("Run all UI tests. Results logged and written to file."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunAllTests: No world context"));
			return;
		}

		UMOUITestSubsystem* TestSubsystem = World->GetSubsystem<UMOUITestSubsystem>();
		if (!TestSubsystem)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunAllTests: UITestSubsystem not found. Is this a game world?"));
			return;
		}

		FMOUITestSummary Summary = TestSubsystem->RunAllTests();
		UE_LOG(LogTemp, Log, TEXT("UI Test Suite Complete: %d/%d passed"), Summary.PassedTests, Summary.TotalTests);
	})
);

// Console command to run tests matching a pattern
static FAutoConsoleCommandWithWorldAndArgs GMORunUITestsCmd(
	TEXT("MO.UI.RunTests"),
	TEXT("Run UI tests matching a pattern (e.g., 'Inventory.*'). Usage: MO.UI.RunTests <pattern>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunTests: No world context"));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("MO.UI.RunTests: No pattern specified. Running all tests."));
		}

		UMOUITestSubsystem* TestSubsystem = World->GetSubsystem<UMOUITestSubsystem>();
		if (!TestSubsystem)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunTests: UITestSubsystem not found. Is this a game world?"));
			return;
		}

		FString Pattern = Args.Num() > 0 ? Args[0] : TEXT("*");
		FMOUITestSummary Summary = TestSubsystem->RunTestsMatching(Pattern);
		UE_LOG(LogTemp, Log, TEXT("UI Tests Complete (%s): %d/%d passed"), *Pattern, Summary.PassedTests, Summary.TotalTests);
	})
);

// Console command to list all available tests
static FAutoConsoleCommandWithWorldAndArgs GMOListUITestsCmd(
	TEXT("MO.UI.ListTests"),
	TEXT("List all available UI tests"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.ListTests: No world context"));
			return;
		}

		UMOUITestSubsystem* TestSubsystem = World->GetSubsystem<UMOUITestSubsystem>();
		if (!TestSubsystem)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.ListTests: UITestSubsystem not found"));
			return;
		}

		TArray<FString> TestNames = TestSubsystem->GetAllTestNames();
		UE_LOG(LogTemp, Log, TEXT("Available UI Tests (%d):"), TestNames.Num());
		for (const FString& Name : TestNames)
		{
			UE_LOG(LogTemp, Log, TEXT("  - %s"), *Name);
		}
	})
);

// Console command to run a single test
static FAutoConsoleCommandWithWorldAndArgs GMORunSingleUITestCmd(
	TEXT("MO.UI.RunTest"),
	TEXT("Run a single UI test by exact name. Usage: MO.UI.RunTest <TestName>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunTest: No world context"));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunTest: Test name required. Use MO.UI.ListTests to see available tests."));
			return;
		}

		UMOUITestSubsystem* TestSubsystem = World->GetSubsystem<UMOUITestSubsystem>();
		if (!TestSubsystem)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.RunTest: UITestSubsystem not found"));
			return;
		}

		FMOUITestResult Result = TestSubsystem->RunTest(Args[0]);
		if (Result.bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("[PASS] %s (%.1fms)"), *Result.TestName, Result.DurationMs);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[FAIL] %s: %s"), *Result.TestName, *Result.ErrorMessage);
		}
	})
);

// Console command to check UI state
static FAutoConsoleCommandWithWorldAndArgs GMOUIStateCmd(
	TEXT("MO.UI.State"),
	TEXT("Print current UI state (menus open, cursor, input)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.State: No world context"));
			return;
		}

		UMOUITestSubsystem* TestSubsystem = World->GetSubsystem<UMOUITestSubsystem>();
		if (!TestSubsystem)
		{
			UE_LOG(LogTemp, Error, TEXT("MO.UI.State: UITestSubsystem not found"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("=== UI State ==="));
		UE_LOG(LogTemp, Log, TEXT("Active Menus: %d"), TestSubsystem->GetActiveMenuCount());
		UE_LOG(LogTemp, Log, TEXT("Any Menu Open: %s"), TestSubsystem->IsAnyMenuOpen() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Log, TEXT("Cursor Visible: %s"), TestSubsystem->IsCursorVisible() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Log, TEXT("Move Input Ignored: %s"), TestSubsystem->IsMoveInputIgnored() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Log, TEXT("Look Input Ignored: %s"), TestSubsystem->IsLookInputIgnored() ? TEXT("Yes") : TEXT("No"));

		// Check individual menus
		UE_LOG(LogTemp, Log, TEXT("--- Menu States ---"));
		UE_LOG(LogTemp, Log, TEXT("Inventory: %s"), TestSubsystem->IsMenuOpen(TEXT("Inventory")) ? TEXT("Open") : TEXT("Closed"));
		UE_LOG(LogTemp, Log, TEXT("Crafting: %s"), TestSubsystem->IsMenuOpen(TEXT("Crafting")) ? TEXT("Open") : TEXT("Closed"));
		UE_LOG(LogTemp, Log, TEXT("Building: %s"), TestSubsystem->IsMenuOpen(TEXT("Building")) ? TEXT("Open") : TEXT("Closed"));
		UE_LOG(LogTemp, Log, TEXT("Skills: %s"), TestSubsystem->IsMenuOpen(TEXT("Skills")) ? TEXT("Open") : TEXT("Closed"));
		UE_LOG(LogTemp, Log, TEXT("Status: %s"), TestSubsystem->IsMenuOpen(TEXT("Status")) ? TEXT("Open") : TEXT("Closed"));
		UE_LOG(LogTemp, Log, TEXT("InGame: %s"), TestSubsystem->IsMenuOpen(TEXT("InGame")) ? TEXT("Open") : TEXT("Closed"));
	})
);
