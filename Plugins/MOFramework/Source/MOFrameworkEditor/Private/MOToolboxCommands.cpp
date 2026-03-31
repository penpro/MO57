/**
 * MOToolboxCommands.cpp - Command registration for MO Framework toolbox
 */

#include "MOToolboxCommands.h"

#define LOCTEXT_NAMESPACE "FMOToolboxCommands"

void FMOToolboxCommands::RegisterCommands()
{
	// Audit commands
	UI_COMMAND(RunComprehensiveAudit,
		"Comprehensive Audit",
		"Run all UI audits and generate comprehensive report",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(RunLayoutAudit,
		"Layout Audit",
		"Analyze widget dimensions, anchors, and positioning",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(RunStyleAudit,
		"Style Audit",
		"Extract colors, fonts, and style patterns",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(RunValidation,
		"Validate Widgets",
		"Check for common issues in Widget Blueprints",
		EUserInterfaceActionType::Button,
		FInputChord());

	// Fix commands
	UI_COMMAND(FixWidgetVariables,
		"Fix Widget Variables",
		"Mark common widgets as variables for BindWidget",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(RunBatchOperations,
		"Batch Operations",
		"Run batch widget operations (rename, mark variables)",
		EUserInterfaceActionType::Button,
		FInputChord());

	// Rename commands
	UI_COMMAND(SmartRenameWidgets,
		"Suggest Renames",
		"Analyze widgets and suggest better names for generic widgets",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(ApplyWidgetRenames,
		"Apply Renames (Preview)",
		"Preview and apply widget renames (edit script to enable)",
		EUserInterfaceActionType::Button,
		FInputChord());

	// Utility commands
	UI_COMMAND(OpenAuditOutput,
		"Open Audit Output",
		"Open the audit output folder in Explorer",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(OpenToolboxPanel,
		"MO Toolbox",
		"Open the MO Framework Toolbox panel",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(FixAllIssues,
		"Fix All Issues",
		"Run all automatic fixes (variables, renames)",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
