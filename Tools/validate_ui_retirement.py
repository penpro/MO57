"""Read-only live-editor validation for the UI retirement candidate set."""

import unreal

from editor_toolset.toolsets.asset import AssetTools
from editor_toolset.toolsets.blueprint import BlueprintTools


RETIREMENT_ASSETS = (
    "/MOFramework/UI/WBP_RecipeEntry",
    "/MOFramework/UI/WBP_RecipeList",
    "/MOFramework/UI/WBP_RecipeDetail",
    "/MOFramework/UI/WBP_CraftingQueue",
    "/MOFramework/UI/CommonUI/WBP_MOListEntry",
    "/MOFramework/UI/CommonUI/WBP_MOScrollList",
    "/MOFramework/UI/CommonUI/WBP_MODetailPanel",
)

# The generic detail Blueprint is the only expected asset directly parented to
# a native class in the retirement set. It is itself a verified candidate.
ALLOWED_DIRECT_CHILDREN = {
    "MOBuildWidget": set(),
    "MOBuildingEntryWidget": set(),
    "MODetailPanelBase": {"/MOFramework/UI/CommonUI/WBP_MODetailPanel"},
}


def _asset_path(asset):
    return asset.get_path_name().split(".", 1)[0]


def main():
    failures = []

    for path in RETIREMENT_ASSETS:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            failures.append(f"{path}: asset is missing; retirement state is ambiguous")
            continue

        referencers = AssetTools.get_referencers(path)
        if referencers:
            failures.append(f"{path}: still referenced by {referencers}")
        else:
            unreal.log(f"[UIRetirement] PASS zero referencers: {path}")

    direct_children = {class_name: set() for class_name in ALLOWED_DIRECT_CHILDREN}
    for object_path in unreal.EditorAssetLibrary.list_assets(
        "/MOFramework/UI", recursive=True, include_folder=False
    ):
        asset = unreal.load_asset(object_path)
        if not isinstance(asset, unreal.Blueprint):
            continue

        parent = BlueprintTools.get_parent(asset)
        if not parent:
            continue

        parent_name = parent.get_name()
        if parent_name in direct_children:
            direct_children[parent_name].add(_asset_path(asset))

    for class_name, allowed_paths in ALLOWED_DIRECT_CHILDREN.items():
        found_paths = direct_children[class_name]
        unexpected = found_paths - allowed_paths
        missing = allowed_paths - found_paths
        if unexpected:
            failures.append(
                f"{class_name}: unexpected direct Blueprint children {sorted(unexpected)}"
            )
        if missing:
            failures.append(
                f"{class_name}: expected retirement assets not found as children {sorted(missing)}"
            )
        if not unexpected and not missing:
            unreal.log(
                f"[UIRetirement] PASS {class_name} direct children={sorted(found_paths)}"
            )

    if failures:
        for failure in failures:
            unreal.log_error(f"[UIRetirement] FAIL {failure}")
        raise RuntimeError(f"UI retirement validation failed ({len(failures)} failures)")

    unreal.log(
        f"[UIRetirement] COMPLETE assets={len(RETIREMENT_ASSETS)} "
        f"native_classes={len(ALLOWED_DIRECT_CHILDREN)}"
    )


main()
