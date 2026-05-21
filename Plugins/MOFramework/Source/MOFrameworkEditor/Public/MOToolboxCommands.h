/**
 * MOToolboxCommands.h - Editor toolbar commands for MO Framework
 */

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/**
 * Toolbar commands for the MO Framework toolbox.
 */
class FMOToolboxCommands : public TCommands<FMOToolboxCommands>
{
public:
	FMOToolboxCommands()
		: TCommands<FMOToolboxCommands>(
			TEXT("MOToolbox"),
			NSLOCTEXT("Contexts", "MOToolbox", "MO Framework Toolbox"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands interface
	virtual void RegisterCommands() override;

	// Audit commands
	TSharedPtr<FUICommandInfo> RunComprehensiveAudit;
	TSharedPtr<FUICommandInfo> RunLayoutAudit;
	TSharedPtr<FUICommandInfo> RunStyleAudit;
	TSharedPtr<FUICommandInfo> RunValidation;

	// Fix commands
	TSharedPtr<FUICommandInfo> FixWidgetVariables;
	TSharedPtr<FUICommandInfo> RunBatchOperations;

	// Rename commands
	TSharedPtr<FUICommandInfo> SmartRenameWidgets;
	TSharedPtr<FUICommandInfo> ApplyWidgetRenames;

	// Utility commands
	TSharedPtr<FUICommandInfo> OpenAuditOutput;
	TSharedPtr<FUICommandInfo> OpenToolboxPanel;
	TSharedPtr<FUICommandInfo> FixAllIssues;

	// UI Testing commands
	TSharedPtr<FUICommandInfo> RunAllUITests;
	TSharedPtr<FUICommandInfo> RunInventoryTests;
	TSharedPtr<FUICommandInfo> RunCraftingTests;
	TSharedPtr<FUICommandInfo> RunBuildingTests;
	TSharedPtr<FUICommandInfo> RunInputStateTests;
	TSharedPtr<FUICommandInfo> OpenTestResults;

	// CommonUI-specific testing
	TSharedPtr<FUICommandInfo> RunSetupValidation;
	TSharedPtr<FUICommandInfo> RunCommonUITests;
	TSharedPtr<FUICommandInfo> RunDiagnostics;
};
