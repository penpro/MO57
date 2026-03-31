# MO57 Generic Widget Blueprint Batch Generator
#
# This script creates Widget Blueprints from C++ parent classes.
# Run manually from the editor Python console:
#   py "Content/Python/create_generic_widgets.py"
#
# Or via Output Log:
#   Window -> Developer Tools -> Output Log
#   Type: py "Content/Python/create_generic_widgets.py"
#
# NOTE: Requires the editor to be open. Skips existing assets.

import unreal

# ============================================================================
# MANIFEST - Widget Blueprints to create
# ============================================================================

GENERIC_WIDGETS = [
    {
        "cpp_class": "/Script/MOFramework.MOScrollListBase",
        "output": "/MOFramework/UI/CommonUI/WBP_MOScrollList",
        "description": "Generic scrollable list widget"
    },
    {
        "cpp_class": "/Script/MOFramework.MOListEntryBase",
        "output": "/MOFramework/UI/CommonUI/WBP_MOListEntry",
        "description": "Generic list entry widget"
    },
    {
        "cpp_class": "/Script/MOFramework.MODetailPanelBase",
        "output": "/MOFramework/UI/CommonUI/WBP_MODetailPanel",
        "description": "Generic detail panel widget"
    },
    {
        "cpp_class": "/Script/MOFramework.MOProgressWidgetBase",
        "output": "/MOFramework/UI/CommonUI/WBP_MOProgressWidget",
        "description": "Generic progress widget"
    },
    {
        "cpp_class": "/Script/MOFramework.MOConfirmationBase",
        "output": "/MOFramework/UI/CommonUI/WBP_MOConfirmation",
        "description": "Generic confirmation dialog"
    },
    {
        "cpp_class": "/Script/MOFramework.MOColonyPortrait",
        "output": "/MOFramework/UI/Colony/WBP_MOColonyPortrait",
        "description": "Colony character portrait widget"
    },
]

# ============================================================================
# HELPERS
# ============================================================================

def log(msg: str):
    """Log to UE output with MO57 prefix."""
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    """Log warning to UE output."""
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    """Log error to UE output."""
    unreal.log_error(f"[MO57] {msg}")

def asset_exists(asset_path: str) -> bool:
    """Check if an asset exists at the given path."""
    return unreal.EditorAssetLibrary.does_asset_exist(asset_path)

def ensure_directory_exists(directory_path: str):
    """Ensure a content directory exists, creating it if necessary."""
    if not unreal.EditorAssetLibrary.does_directory_exist(directory_path):
        unreal.EditorAssetLibrary.make_directory(directory_path)
        log(f"Created directory: {directory_path}")

def create_widget_blueprint(cpp_class_path: str, output_path: str, description: str) -> bool:
    """
    Create a Widget Blueprint with the specified C++ parent class.

    Args:
        cpp_class_path: Full path to C++ class (e.g., "/Script/MOFramework.MOScrollListBase")
        output_path: Content path for the new asset (e.g., "/MOFramework/UI/CommonUI/WBP_MOScrollList")
        description: Human-readable description for logging

    Returns:
        True if created successfully, False otherwise
    """
    # Check if asset already exists
    if asset_exists(output_path):
        log(f"SKIP: {output_path} already exists")
        return False

    # Parse output path to get directory and asset name
    path_parts = output_path.rsplit("/", 1)
    if len(path_parts) != 2:
        log_error(f"Invalid output path: {output_path}")
        return False

    output_directory = path_parts[0]
    asset_name = path_parts[1]

    # Ensure directory exists
    ensure_directory_exists(output_directory)

    # Load the C++ parent class
    parent_class = unreal.load_class(None, cpp_class_path)
    if parent_class is None:
        log_error(f"Failed to load C++ class: {cpp_class_path}")
        return False

    # Get asset tools
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    # Create widget blueprint factory
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    # Create the asset
    # Note: The path needs to use the package path format
    package_path = output_directory

    try:
        new_asset = asset_tools.create_asset(
            asset_name=asset_name,
            package_path=package_path,
            asset_class=unreal.WidgetBlueprint,
            factory=factory
        )

        if new_asset is not None:
            log(f"CREATED: {output_path} ({description})")
            # Save the asset
            unreal.EditorAssetLibrary.save_asset(output_path, only_if_is_dirty=False)
            return True
        else:
            log_error(f"Failed to create asset: {output_path}")
            return False

    except Exception as e:
        log_error(f"Exception creating {output_path}: {str(e)}")
        return False

# ============================================================================
# MAIN
# ============================================================================

def run():
    """Execute the batch widget creation."""
    log("=" * 60)
    log("Starting Generic Widget Blueprint Batch Creation")
    log("=" * 60)

    created_count = 0
    skipped_count = 0
    error_count = 0

    for widget_def in GENERIC_WIDGETS:
        cpp_class = widget_def["cpp_class"]
        output = widget_def["output"]
        description = widget_def.get("description", "")

        if asset_exists(output):
            skipped_count += 1
            log(f"SKIP: {output} already exists")
        else:
            success = create_widget_blueprint(cpp_class, output, description)
            if success:
                created_count += 1
            else:
                error_count += 1

    log("=" * 60)
    log(f"Complete: {created_count} created, {skipped_count} skipped, {error_count} errors")
    log("=" * 60)

    return created_count, skipped_count, error_count

# Run when script is executed
if __name__ == "__main__":
    run()
