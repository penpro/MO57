# MO57 Primary Game Layout Widget Blueprint Creator
#
# This script creates the WBP_MOPrimaryGameLayout Widget Blueprint.
# The 5 layer stacks must be added manually in the Widget Designer since
# Unreal's Python API has limited support for modifying widget hierarchies.
#
# Run manually from the editor:
#   py "Content/Python/create_primary_layout.py"
#
# After running this script, you must manually add the layer stacks in the
# Widget Designer. See the instructions printed after creation.

import unreal

# ============================================================================
# CONFIGURATION
# ============================================================================

CONFIG = {
    "cpp_class": "/Script/MOFramework.MOPrimaryGameLayout",
    "output": "/MOFramework/UI/CommonUI/WBP_MOPrimaryGameLayout",
    "description": "Primary game layout with UI layer stacks"
}

# Layer configuration for manual setup instructions
LAYERS = [
    {
        "name": "HUDLayer",
        "z_order": 0,
        "tag": "MO.UI.Layer.HUD",
        "purpose": "Always-visible HUD elements (reticle, mode indicator, colony bar)"
    },
    {
        "name": "GameLayer",
        "z_order": 50,
        "tag": "MO.UI.Layer.Game",
        "purpose": "Switchable gameplay menus (inventory, crafting, skills, building)"
    },
    {
        "name": "GameOverlayLayer",
        "z_order": 100,
        "tag": "MO.UI.Layer.GameOverlay",
        "purpose": "Context menus and progress widgets that overlay game menus"
    },
    {
        "name": "MenuLayer",
        "z_order": 150,
        "tag": "MO.UI.Layer.Menu",
        "purpose": "System menus (in-game menu, colony overview, possession)"
    },
    {
        "name": "ModalLayer",
        "z_order": 200,
        "tag": "MO.UI.Layer.Modal",
        "purpose": "Modal dialogs (confirmation, item context)"
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

def print_manual_setup_instructions():
    """Print instructions for manual layer stack setup."""
    log("")
    log("=" * 70)
    log("MANUAL SETUP REQUIRED")
    log("=" * 70)
    log("")
    log("Open WBP_MOPrimaryGameLayout in the Widget Designer and add:")
    log("")
    log("1. Add a Canvas Panel as the root widget")
    log("2. For each layer below, add a CommonActivatableWidgetStack as a child:")
    log("")

    for layer in LAYERS:
        log("-" * 60)
        log(f"  Widget Name: {layer['name']}")
        log(f"  Z-Order:     {layer['z_order']}")
        log(f"  Tag:         {layer['tag']}")
        log(f"  Purpose:     {layer['purpose']}")
        log("")

    log("-" * 60)
    log("")
    log("WIDGET SETTINGS:")
    log("  - Each layer should Fill the canvas (Anchors: stretch all)")
    log("  - Set Z-Order in the designer or via Details panel")
    log("  - The variable names MUST match exactly (HUDLayer, GameLayer, etc.)")
    log("  - Mark each widget as 'Is Variable' in Details panel")
    log("")
    log("VERIFICATION:")
    log("  After setup, compile the Blueprint. The C++ class will")
    log("  automatically register the layers via BindWidgetOptional.")
    log("")
    log("=" * 70)

# ============================================================================
# MAIN
# ============================================================================

def run():
    """Create the Primary Game Layout Widget Blueprint."""
    log("=" * 70)
    log("Creating Primary Game Layout Widget Blueprint")
    log("=" * 70)

    output_path = CONFIG["output"]
    cpp_class_path = CONFIG["cpp_class"]

    # Check if already exists
    if asset_exists(output_path):
        log(f"SKIP: {output_path} already exists")
        log("")
        log("To recreate, delete the existing asset first.")
        return False

    # Parse output path
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
        log_error("Make sure MOFramework is compiled and the class exists.")
        return False

    log(f"Loaded parent class: {cpp_class_path}")

    # Get asset tools
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    # Create widget blueprint factory
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    # Create the asset
    try:
        new_asset = asset_tools.create_asset(
            asset_name=asset_name,
            package_path=output_directory,
            asset_class=unreal.WidgetBlueprint,
            factory=factory
        )

        if new_asset is not None:
            log(f"CREATED: {output_path}")
            # Save the asset
            unreal.EditorAssetLibrary.save_asset(output_path, only_if_is_dirty=False)

            # Print manual setup instructions
            print_manual_setup_instructions()

            return True
        else:
            log_error(f"Failed to create asset: {output_path}")
            return False

    except Exception as e:
        log_error(f"Exception creating {output_path}: {str(e)}")
        return False

# Run when script is executed
if __name__ == "__main__":
    run()
