# Try EditorUtilityLibrary methods for widget manipulation
#
# Run: py "D:/UEProjects/MO57/Content/Python/try_editor_utility.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def explore_editor_utility_library():
    """Explore EditorUtilityLibrary methods."""
    log("=" * 60)
    log("Exploring EditorUtilityLibrary")
    log("=" * 60)

    eul = unreal.EditorUtilityLibrary

    # Get all methods with their signatures
    interesting_methods = [
        'add_source_widget',
        'find_source_widget_by_name',
        'cast_to_widget_blueprint',
        'convert_to_editor_utility_widget',
        'get_selected_assets',
        'get_selected_asset_data',
    ]

    for method_name in interesting_methods:
        if hasattr(eul, method_name):
            method = getattr(eul, method_name)
            log(f"")
            log(f"{method_name}:")
            log(f"  {method.__doc__ if method.__doc__ else 'No docstring'}")

def try_add_source_widget():
    """Try using add_source_widget."""
    log("")
    log("=" * 60)
    log("Trying add_source_widget")
    log("=" * 60)

    # Load the WBP
    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/CommonUI/WBP_MOProgressWidget")
    if not wbp:
        log("Failed to load WBP")
        return

    # Load ProgressBar class
    progress_bar_class = unreal.load_class(None, "/Script/UMG.ProgressBar")
    log(f"ProgressBar class: {progress_bar_class}")

    # Try to create an instance
    try:
        # Maybe we can create a widget and add it?
        # First check if we can create a widget at all
        if hasattr(unreal, 'ProgressBar'):
            log("unreal.ProgressBar exists")
            pb = unreal.ProgressBar()
            log(f"Created ProgressBar: {pb}")
        else:
            log("unreal.ProgressBar does not exist as constructable class")

    except Exception as e:
        log(f"Error creating ProgressBar: {e}")

    # Try add_source_widget
    try:
        eul = unreal.EditorUtilityLibrary
        # Check method signature by calling with no args
        result = eul.add_source_widget(wbp)
        log(f"add_source_widget result: {result}")
    except TypeError as e:
        # This will tell us the expected arguments
        log(f"add_source_widget signature error: {e}")
    except Exception as e:
        log(f"add_source_widget error: {e}")

def check_widget_classes():
    """Check what widget classes can be instantiated."""
    log("")
    log("=" * 60)
    log("Checking instantiable widget classes")
    log("=" * 60)

    widget_types = [
        'ProgressBar',
        'TextBlock',
        'Image',
        'Border',
        'Button',
        'CanvasPanel',
        'VerticalBox',
        'HorizontalBox',
        'ScrollBox',
        'Widget',
        'UserWidget',
        'PanelWidget',
    ]

    for wtype in widget_types:
        if hasattr(unreal, wtype):
            cls = getattr(unreal, wtype)
            log(f"  unreal.{wtype}: {cls}")
            # Try to instantiate
            try:
                instance = cls()
                log(f"    -> Instantiated: {instance}")
            except Exception as e:
                log(f"    -> Cannot instantiate: {type(e).__name__}")
        else:
            log(f"  unreal.{wtype}: NOT IN MODULE")

def run():
    explore_editor_utility_library()
    try_add_source_widget()
    check_widget_classes()
    log("")
    log("Done.")

if __name__ == "__main__":
    run()
