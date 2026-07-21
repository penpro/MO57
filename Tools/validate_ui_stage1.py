"""Live editor validation for the UI consolidation Stage 1 contract."""

import unreal

from editor_toolset.toolsets.blueprint import BlueprintTools
from editor_toolset.toolsets.asset import AssetTools


BLUEPRINTS = {
    "/MOFramework/UI/CommonUI/WBP_MOListEntry": "MOListEntryBase",
    "/MOFramework/UI/CommonUI/WBP_MOScrollList": "MOScrollListBase",
    "/MOFramework/UI/CommonUI/WBP_MODetailPanel": "MODetailPanelBase",
    "/MOFramework/UI/CraftingSystem/WBP_CraftingRecipeEntry": "MORecipeEntryWidget",
    "/MOFramework/UI/CraftingSystem/WBP_CraftingRecipeList": "MORecipeListWidget",
    "/MOFramework/UI/CraftingSystem/WBP_CraftingRecipeDetail": "MORecipeDetailPanel",
    "/MOFramework/UI/CraftingSystem/WBP_CraftingQueue": "MOCraftingQueueWidget",
    "/MOFramework/UI/CraftingSystem/WBP_CraftQueueEntry": "MOCraftingQueueEntryWidget",
    "/MOFramework/UI/CraftingSystem/WBP_CraftingMenu": "MOCraftingMenu",
    "/MOFramework/UI/BuildSystem/WBP_BuildingRecipeEntry": "MOBuildingRecipeEntryWidget",
    "/MOFramework/UI/BuildSystem/WBP_BuildingRecipeList": "MOBuildingRecipeListWidget",
    "/MOFramework/UI/BuildSystem/WBP_BuildingRecipeDetail": "MOBuildingDetailPanel",
    "/MOFramework/UI/BuildSystem/WBP_BuildingQueue": "MOBuildingQueueWidget",
    "/MOFramework/UI/BuildSystem/WBP_BuildQueueEntry": "MOBuildingQueueEntryWidget",
    "/MOFramework/UI/BuildSystem/WBP_BuildingMenu": "MOBuildingMenu",
}

LEGACY_ASSET_CANDIDATES = (
    "/MOFramework/UI/WBP_RecipeEntry",
    "/MOFramework/UI/WBP_RecipeList",
    "/MOFramework/UI/WBP_RecipeDetail",
    "/MOFramework/UI/WBP_CraftingQueue",
    "/MOFramework/UI/CommonUI/WBP_MOListEntry",
    "/MOFramework/UI/CommonUI/WBP_MOScrollList",
    "/MOFramework/UI/CommonUI/WBP_MODetailPanel",
)


def main():
    failures = []
    for path, expected_parent in BLUEPRINTS.items():
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.Blueprint):
            failures.append(f"{path}: missing or not a Blueprint ({type(asset).__name__})")
            continue

        parent = BlueprintTools.get_parent(asset)
        parent_name = parent.get_name() if parent else "None"
        if parent_name != expected_parent:
            failures.append(f"{path}: parent {parent_name}, expected {expected_parent}")
            continue

        try:
            BlueprintTools.compile_blueprint(asset, warnings_as_errors=True)
        except Exception as exc:
            failures.append(f"{path}: compile failed: {exc}")
            continue

        unreal.log(f"[UIStage1] PASS {path} parent={parent_name}")

    for path in LEGACY_ASSET_CANDIDATES:
        try:
            referencers = AssetTools.get_referencers(path)
            unreal.log(f"[UIStage1] REFERENCERS {path} -> {referencers}")
        except Exception as exc:
            unreal.log_warning(f"[UIStage1] REFERENCERS unavailable {path}: {exc}")

    if failures:
        for failure in failures:
            unreal.log_error(f"[UIStage1] FAIL {failure}")
        raise RuntimeError(f"UI Stage 1 validation failed ({len(failures)} failures)")

    unreal.log(f"[UIStage1] COMPLETE {len(BLUEPRINTS)}/{len(BLUEPRINTS)} blueprints passed")


main()
