#include "MOIconGenerator.h"
#include "MOFramework.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"

/**
 * Registers a right-click context menu action in the Content Browser.
 * When you right-click a static mesh, you'll see "Generate Item Icon" option.
 */
class FMOIconGeneratorActions
{
public:
	static void RegisterMenus()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (!ToolMenus)
		{
			return;
		}

		// Register for asset context menu
		UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu.StaticMesh");
		if (!Menu)
		{
			return;
		}

		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddMenuEntry(
			"GenerateItemIcon",
			FText::FromString("Generate Item Icon"),
			FText::FromString("Generate a 256x256 icon from this mesh's thumbnail"),
			FSlateIcon(),
			FToolMenuExecuteAction::CreateStatic(&FMOIconGeneratorActions::ExecuteGenerateIcon)
		);

		UE_LOG(LogMOFramework, Log, TEXT("[IconGen] Registered Content Browser context menu action"));
	}

	static void ExecuteGenerateIcon(const FToolMenuContext& Context)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Context menu action triggered!"));
		int32 Count = UMOIconGenerator::GenerateIconsForSelectedAssets(256);
		UE_LOG(LogMOFramework, Warning, TEXT("[IconGen] Generated %d icon(s) from context menu"), Count);
	}
};

// Register on module startup
struct FMOIconGeneratorActionsRegistrar
{
	FMOIconGeneratorActionsRegistrar()
	{
		// Delay registration until menus are ready
		if (!IsRunningCommandlet())
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([]()
			{
				FMOIconGeneratorActions::RegisterMenus();
			});
		}
	}
} GMOIconGeneratorActionsRegistrar;

#endif // WITH_EDITOR
