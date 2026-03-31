/**
 * MOFrameworkEditor.h - Editor module for MO Framework toolbox
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMOFrameworkEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void RegisterToolbar();

	TSharedPtr<class FUICommandList> PluginCommands;
};
