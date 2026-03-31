# MO57 Widget Blueprint Binding Setup
#
# Experimental script to programmatically add widget bindings to Widget Blueprints.
# Run from editor: py "D:/UEProjects/MO57/Content/Python/add_widget_bindings.py"
#
# This script attempts to add required widget children to WBPs and set them as variables.

import unreal

# ============================================================================
# HELPERS
# ============================================================================

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

# ============================================================================
# WIDGET BLUEPRINT MANIPULATION
# ============================================================================

def load_widget_blueprint(asset_path: str):
    """Load a Widget Blueprint asset."""
    return unreal.EditorAssetLibrary.load_asset(asset_path)

def explore_widget_blueprint(wbp):
    """Explore what's available on a Widget Blueprint for manipulation."""
    log("=" * 60)
    log(f"Exploring Widget Blueprint: {wbp.get_name()}")
    log("=" * 60)

    # Check for widget_tree
    if hasattr(wbp, 'widget_tree'):
        widget_tree = wbp.widget_tree()
        log(f"Has widget_tree: {widget_tree}")

        if widget_tree:
            # Explore widget tree methods
            log(f"  WidgetTree type: {type(widget_tree)}")

            # Try to get root widget
            if hasattr(widget_tree, 'root_widget'):
                root = widget_tree.root_widget
                log(f"  Root widget: {root}")

            # List available methods
            methods = [m for m in dir(widget_tree) if not m.startswith('_')]
            log(f"  Available methods: {methods[:20]}...")  # First 20
    else:
        log("No widget_tree attribute")

    # Check for other useful attributes
    useful_attrs = ['generated_class', 'parent_class', 'blueprint_description']
    for attr in useful_attrs:
        if hasattr(wbp, attr):
            val = getattr(wbp, attr)
            log(f"  {attr}: {val}")

    return wbp

def setup_progress_widget(asset_path: str = "/MOFramework/UI/CommonUI/WBP_MOProgressWidget"):
    """
    Add required ProgressBar widget to WBP_MOProgressWidget.
    """
    log("=" * 60)
    log("Setting up WBP_MOProgressWidget")
    log("=" * 60)

    # Load the widget blueprint
    wbp = load_widget_blueprint(asset_path)
    if not wbp:
        log_error(f"Failed to load: {asset_path}")
        return False

    log(f"Loaded: {wbp.get_name()}")
    log(f"Type: {type(wbp)}")

    # Get the widget tree
    widget_tree = None
    if hasattr(wbp, 'widget_tree'):
        widget_tree = wbp.widget_tree()
        log(f"Got widget_tree: {widget_tree}")
    elif hasattr(wbp, 'get_widget_tree'):
        widget_tree = wbp.get_widget_tree()
        log(f"Got widget_tree via get_widget_tree(): {widget_tree}")

    if not widget_tree:
        log_error("Could not access widget_tree")
        # Try alternative approach - explore the blueprint
        explore_widget_blueprint(wbp)
        return False

    # Try to construct a ProgressBar widget
    try:
        # Get the ProgressBar class
        progress_bar_class = unreal.find_class("ProgressBar")
        if not progress_bar_class:
            progress_bar_class = unreal.load_class(None, "/Script/UMG.ProgressBar")

        log(f"ProgressBar class: {progress_bar_class}")

        if progress_bar_class and hasattr(widget_tree, 'construct_widget'):
            # Construct the widget
            progress_bar = widget_tree.construct_widget(progress_bar_class, "ProgressBar")
            log(f"Constructed ProgressBar: {progress_bar}")

            if progress_bar:
                # Try to add to root
                root = widget_tree.root_widget if hasattr(widget_tree, 'root_widget') else None
                log(f"Root widget: {root}")

                # Mark as variable
                if hasattr(progress_bar, 'set_editor_property'):
                    # The variable name is set via the widget tree
                    pass

                log("ProgressBar created successfully!")

                # Save the asset
                unreal.EditorAssetLibrary.save_asset(asset_path)
                log(f"Saved: {asset_path}")
                return True
        else:
            log_warning("construct_widget not available or class not found")

    except Exception as e:
        log_error(f"Exception: {str(e)}")

    return False

def explore_umg_classes():
    """List available UMG widget classes."""
    log("=" * 60)
    log("Exploring UMG Widget Classes")
    log("=" * 60)

    # Common widget classes to check
    class_names = [
        "ProgressBar",
        "TextBlock",
        "Image",
        "Button",
        "Border",
        "CanvasPanel",
        "VerticalBox",
        "HorizontalBox",
        "ScrollBox",
    ]

    for name in class_names:
        try:
            cls = unreal.find_class(name)
            if cls:
                log(f"  {name}: {cls}")
            else:
                # Try with full path
                cls = unreal.load_class(None, f"/Script/UMG.{name}")
                if cls:
                    log(f"  {name} (via load_class): {cls}")
                else:
                    log_warning(f"  {name}: NOT FOUND")
        except Exception as e:
            log_error(f"  {name}: ERROR - {e}")

# ============================================================================
# MAIN
# ============================================================================

def run():
    """Main entry point."""
    log("=" * 60)
    log("Widget Binding Setup Script")
    log("=" * 60)

    # First, explore what classes are available
    explore_umg_classes()

    # Try to set up the progress widget
    log("")
    setup_progress_widget()

    log("")
    log("Script complete. Check output for results.")

if __name__ == "__main__":
    run()
