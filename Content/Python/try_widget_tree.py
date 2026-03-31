# Try to access WidgetTree with various methods
#
# Run: py "D:/UEProjects/MO57/Content/Python/try_widget_tree.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def try_access_widget_tree():
    """Try various ways to access the WidgetTree."""
    log("=" * 60)
    log("Attempting to access WidgetTree")
    log("=" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/CommonUI/WBP_MOProgressWidget")
    if not wbp:
        log("Failed to load WBP")
        return

    log(f"Loaded: {wbp.get_name()}")

    # Try various property names
    property_names = [
        "WidgetTree",
        "widget_tree",
        "widgetTree",
        "widgettree",
    ]

    for prop_name in property_names:
        try:
            val = wbp.get_editor_property(prop_name)
            log(f"get_editor_property('{prop_name}'): {val}")
            if val:
                explore_widget_tree(val)
                return val
        except Exception as e:
            log(f"get_editor_property('{prop_name}'): Failed - {type(e).__name__}")

    # Try as direct attribute
    for prop_name in property_names:
        if hasattr(wbp, prop_name):
            val = getattr(wbp, prop_name)
            log(f"getattr(wbp, '{prop_name}'): {val}")
            if val:
                explore_widget_tree(val)
                return val

    # Try calling as method
    for prop_name in property_names:
        if hasattr(wbp, prop_name):
            attr = getattr(wbp, prop_name)
            if callable(attr):
                try:
                    val = attr()
                    log(f"{prop_name}(): {val}")
                    if val:
                        explore_widget_tree(val)
                        return val
                except Exception as e:
                    log(f"{prop_name}(): Failed - {e}")

    # Check if WidgetTree is a class we can instantiate
    log("")
    log("Checking if WidgetTree class exists in unreal module:")
    if hasattr(unreal, 'WidgetTree'):
        log(f"  unreal.WidgetTree: {unreal.WidgetTree}")
    else:
        log("  unreal.WidgetTree not found")

    # List ALL properties on the WBP
    log("")
    log("All attributes on WidgetBlueprint (non-underscore):")
    attrs = [a for a in dir(wbp) if not a.startswith('_')]
    log(f"  {attrs}")

    return None

def explore_widget_tree(widget_tree):
    """Explore a WidgetTree object."""
    log("")
    log(f"Exploring WidgetTree: {widget_tree}")
    log(f"Type: {type(widget_tree)}")

    # Check for construct_widget method
    if hasattr(widget_tree, 'construct_widget'):
        log("  Has construct_widget method!")

        # Try to construct a ProgressBar
        try:
            progress_bar_class = unreal.load_class(None, "/Script/UMG.ProgressBar")
            log(f"  ProgressBar class: {progress_bar_class}")

            if progress_bar_class:
                pb = widget_tree.construct_widget(progress_bar_class, "ProgressBar")
                log(f"  Constructed: {pb}")
        except Exception as e:
            log(f"  construct_widget failed: {e}")

    # List methods
    methods = [m for m in dir(widget_tree) if not m.startswith('_') and callable(getattr(widget_tree, m, None))]
    log(f"  Methods: {methods}")

    # Check for root_widget
    if hasattr(widget_tree, 'root_widget'):
        log(f"  root_widget: {widget_tree.root_widget}")

def check_alternative_approaches():
    """Check other possible approaches."""
    log("")
    log("=" * 60)
    log("Alternative Approaches")
    log("=" * 60)

    # Check if we can use Blueprint function library
    if hasattr(unreal, 'WidgetBlueprintLibrary'):
        log("WidgetBlueprintLibrary exists!")
        wbl = unreal.WidgetBlueprintLibrary
        methods = [m for m in dir(wbl) if not m.startswith('_')]
        log(f"  Methods: {methods}")

    # Check EditorUtilityLibrary
    if hasattr(unreal, 'EditorUtilityLibrary'):
        log("EditorUtilityLibrary exists")
        eul = unreal.EditorUtilityLibrary
        methods = [m for m in dir(eul) if not m.startswith('_')]
        log(f"  Methods: {methods[:20]}")

    # Check if there's a way to run editor commands
    if hasattr(unreal, 'EditorLevelLibrary'):
        log("EditorLevelLibrary exists")

def run():
    widget_tree = try_access_widget_tree()

    if not widget_tree:
        log("")
        log("Could not access WidgetTree directly.")
        check_alternative_approaches()

    log("")
    log("Done.")

if __name__ == "__main__":
    run()
