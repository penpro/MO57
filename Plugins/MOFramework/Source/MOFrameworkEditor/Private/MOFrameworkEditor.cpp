/**
 * MOFrameworkEditor.cpp - Editor module implementation
 */

#include "MOFrameworkEditor.h"
#include "MOToolboxCommands.h"

#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "IPythonScriptPlugin.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMOFrameworkEditorModule"

// Python script paths
static const FString PythonScriptDir = TEXT("D:/UEProjects/MO57/Content/Python/");
static const FString AuditOutputDir = TEXT("D:/UEProjects/MO57/Content/Python/audit_output/");

namespace MOToolboxHelpers
{
	void RunPythonScript(const FString& ScriptName)
	{
		FString ScriptPath = PythonScriptDir + ScriptName;

		// Check if script exists
		if (!FPaths::FileExists(ScriptPath))
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("ScriptNotFound", "Python script not found:\n{0}"),
					FText::FromString(ScriptPath)));
			return;
		}

		// Get Python plugin
		IPythonScriptPlugin* PythonPlugin = FModuleManager::GetModulePtr<IPythonScriptPlugin>("PythonScriptPlugin");
		if (!PythonPlugin)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				LOCTEXT("PythonNotAvailable", "Python Script Plugin is not available."));
			return;
		}

		// Execute script
		UE_LOG(LogTemp, Log, TEXT("MOToolbox: Running Python script: %s"), *ScriptPath);

		TArray<FString> CommandArgs;
		PythonPlugin->ExecPythonCommand(*FString::Printf(TEXT("exec(open('%s').read())"), *ScriptPath.Replace(TEXT("\\"), TEXT("/"))));

		// Notify user
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("ScriptComplete", "Script completed!\n\nCheck output at:\n{0}"),
				FText::FromString(AuditOutputDir)));
	}

	void OpenAuditOutputFolder()
	{
		FString OutputPath = AuditOutputDir;
		OutputPath.ReplaceInline(TEXT("/"), TEXT("\\"));

		if (!FPaths::DirectoryExists(OutputPath))
		{
			IFileManager::Get().MakeDirectory(*OutputPath, true);
		}

		FPlatformProcess::ExploreFolder(*OutputPath);
	}
}

void FMOFrameworkEditorModule::StartupModule()
{
	// Register commands
	FMOToolboxCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	// Map commands to actions
	PluginCommands->MapAction(
		FMOToolboxCommands::Get().RunComprehensiveAudit,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("audit_ui_all.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().RunLayoutAudit,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("audit_ui_layouts.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().RunStyleAudit,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("audit_ui_styles.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().RunValidation,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("validate_ui_widgets.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().FixWidgetVariables,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("fix_widget_variables_final.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().RunBatchOperations,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("batch_ui_operations.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().SmartRenameWidgets,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("smart_rename_widgets.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().ApplyWidgetRenames,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("apply_widget_renames.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().FixAllIssues,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::RunPythonScript(TEXT("fix_all_ui_issues.py"));
		}));

	PluginCommands->MapAction(
		FMOToolboxCommands::Get().OpenAuditOutput,
		FExecuteAction::CreateLambda([]() {
			MOToolboxHelpers::OpenAuditOutputFolder();
		}));

	// Register menus after engine init
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMOFrameworkEditorModule::RegisterMenus));
}

void FMOFrameworkEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FMOToolboxCommands::Unregister();
}

void FMOFrameworkEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add to Level Editor toolbar
	RegisterToolbar();

	// Add to main menu bar
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
		FToolMenuSection& Section = Menu->FindOrAddSection("MOFramework");

		// Add MO Framework menu
		Section.AddSubMenu(
			"MOFramework",
			LOCTEXT("MOFrameworkMenu", "MO Framework"),
			LOCTEXT("MOFrameworkMenuTooltip", "MO Framework tools and utilities"),
			FNewToolMenuDelegate::CreateLambda([this](UToolMenu* SubMenu)
			{
				// Audit section
				{
					FToolMenuSection& AuditSection = SubMenu->AddSection("Audits", LOCTEXT("AuditsSection", "Audits"));

					AuditSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().RunComprehensiveAudit,
						PluginCommands);

					AuditSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().RunLayoutAudit,
						PluginCommands);

					AuditSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().RunStyleAudit,
						PluginCommands);

					AuditSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().RunValidation,
						PluginCommands);
				}

				// Fixes section
				{
					FToolMenuSection& FixSection = SubMenu->AddSection("Fixes", LOCTEXT("FixesSection", "Fixes"));

					FixSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().FixWidgetVariables,
						PluginCommands);

					FixSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().RunBatchOperations,
						PluginCommands);

					FixSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().SmartRenameWidgets,
						PluginCommands);

					FixSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().ApplyWidgetRenames,
						PluginCommands);

					FixSection.AddSeparator("FixAllSeparator");

					FixSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().FixAllIssues,
						PluginCommands);
				}

				// Utilities section
				{
					FToolMenuSection& UtilSection = SubMenu->AddSection("Utilities", LOCTEXT("UtilitiesSection", "Utilities"));

					UtilSection.AddMenuEntryWithCommandList(
						FMOToolboxCommands::Get().OpenAuditOutput,
						PluginCommands);
				}
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings")
		);
	}
}

void FMOFrameworkEditorModule::RegisterToolbar()
{
	// Add toolbar button
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");

	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("MOFramework");

	// Add combo button with dropdown
	Section.AddEntry(FToolMenuEntry::InitComboButton(
		"MOToolbox",
		FUIAction(),
		FOnGetContent::CreateLambda([this](){
			FMenuBuilder MenuBuilder(true, PluginCommands);

			MenuBuilder.BeginSection("Audits", LOCTEXT("AuditsSection", "Audits"));
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().RunComprehensiveAudit);
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().RunLayoutAudit);
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().RunStyleAudit);
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().RunValidation);
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("Fixes", LOCTEXT("FixesSection", "Fixes"));
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().FixWidgetVariables);
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().RunBatchOperations);
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().SmartRenameWidgets);
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().ApplyWidgetRenames);
			MenuBuilder.AddSeparator();
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().FixAllIssues);
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("Utilities", LOCTEXT("UtilitiesSection", "Utilities"));
			MenuBuilder.AddMenuEntry(FMOToolboxCommands::Get().OpenAuditOutput);
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}),
		LOCTEXT("MOToolboxLabel", "MO Toolbox"),
		LOCTEXT("MOToolboxTooltip", "MO Framework UI Tools\n\nAudit, validate, and fix Widget Blueprints"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings")
	));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMOFrameworkEditorModule, MOFrameworkEditor)
