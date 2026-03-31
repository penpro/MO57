# Inspect Widget Blueprints - List all widgets and their properties
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/inspect_widget_blueprints.py"
#
# This script inspects Widget Blueprints and lists all child widgets,
# their types, and whether they are marked as variables (for BindWidget).

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def get_widget_children(widget):
    """Recursively get all children of a widget."""
    children = []

    # Try different methods to get children
    if hasattr(widget, 'get_all_children'):
        children = list(widget.get_all_children())
    elif hasattr(widget, 'get_children_count'):
        count = widget.get_children_count()
        for i in range(count):
            if hasattr(widget, 'get_child_at'):
                child = widget.get_child_at(i)
                if child:
                    children.append(child)

    return children

def inspect_widget_blueprint(wbp_path: str):
    """
    Inspect a Widget Blueprint and list all its widgets.

    Args:
        wbp_path: Asset path to the Widget Blueprint
    """
    log("")
    log("=" * 70)
    log(f"Inspecting: {wbp_path}")
    log("=" * 70)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"Failed to load: {wbp_path}")
        return

    log(f"Type: {type(wbp).__name__}")
    log(f"Name: {wbp.get_name()}")

    # Get parent class
    if hasattr(wbp, 'parent_class'):
        parent = wbp.parent_class
        log(f"Parent Class: {parent}")
    elif hasattr(wbp, 'get_editor_property'):
        try:
            parent = wbp.get_editor_property('parent_class')
            log(f"Parent Class: {parent}")
        except:
            pass

    # Try to get widget tree
    widget_tree = None
    if hasattr(wbp, 'widget_tree'):
        widget_tree = wbp.widget_tree()
    elif hasattr(wbp, 'get_widget_tree'):
        widget_tree = wbp.get_widget_tree()
    elif hasattr(wbp, 'get_editor_property'):
        try:
            widget_tree = wbp.get_editor_property('widget_tree')
        except:
            pass

    if widget_tree:
        log(f"Widget Tree: {widget_tree}")
        log(f"Widget Tree Type: {type(widget_tree).__name__}")

        # Get root widget
        root = None
        if hasattr(widget_tree, 'root_widget'):
            root = widget_tree.root_widget
        elif hasattr(widget_tree, 'get_root_widget'):
            root = widget_tree.get_root_widget()

        if root:
            log(f"Root Widget: {root.get_name()} ({type(root).__name__})")

        # Try to get all widgets
        all_widgets = []
        if hasattr(widget_tree, 'get_all_widgets'):
            all_widgets = list(widget_tree.get_all_widgets())
        elif hasattr(widget_tree, 'all_widgets'):
            all_widgets = list(widget_tree.all_widgets)

        if all_widgets:
            log(f"")
            log(f"All Widgets ({len(all_widgets)}):")
            log("-" * 70)
            for w in all_widgets:
                name = w.get_name() if hasattr(w, 'get_name') else str(w)
                wtype = type(w).__name__

                # Check if it's a variable
                is_variable = False
                if hasattr(w, 'b_is_variable'):
                    is_variable = w.b_is_variable
                elif hasattr(w, 'get_editor_property'):
                    try:
                        is_variable = w.get_editor_property('b_is_variable')
                    except:
                        pass

                var_status = "[VAR]" if is_variable else "[---]"
                log(f"  {var_status} {name}: {wtype}")
    else:
        log_warning("Could not access widget_tree")

    # Try using EditorUtilityLibrary to find specific widgets
    log("")
    log("Checking for expected bindings:")
    log("-" * 70)

    # Common binding names to check
    binding_names = [
        # Progress widgets
        "ProgressBar", "ActionNameText", "ItemNameText", "TimeRemainingText",
        "InstructionText", "CancelButton",
        # Confirmation
        "TitleText", "MessageText", "ConfirmButton",
        # Common
        "ContentScrollBox", "BackgroundBorder", "ContentBox",
        "IconImage", "DescriptionText", "ActionButton",
        # Buttons
        "CloseButton", "EntryButton", "ClickButton",
    ]

    for name in binding_names:
        widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(name))
        if widget:
            wtype = type(widget).__name__

            # Check IsVariable
            is_variable = False
            if hasattr(widget, 'b_is_variable'):
                is_variable = widget.b_is_variable
            elif hasattr(widget, 'get_editor_property'):
                try:
                    is_variable = widget.get_editor_property('b_is_variable')
                except:
                    pass

            var_status = "[VAR]" if is_variable else "[NOT VAR]"
            log(f"  FOUND: {name} ({wtype}) {var_status}")

def list_all_widget_methods(wbp_path: str):
    """List available methods on a widget blueprint for debugging."""
    log("")
    log("=" * 70)
    log(f"Available Methods/Properties")
    log("=" * 70)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        return

    # Widget Blueprint methods
    log("WidgetBlueprint methods (name-related):")
    for m in dir(wbp):
        if not m.startswith('_') and ('widget' in m.lower() or 'tree' in m.lower()):
            log(f"  {m}")

    # EditorUtilityLibrary methods
    log("")
    log("EditorUtilityLibrary methods (widget-related):")
    for m in dir(unreal.EditorUtilityLibrary):
        if not m.startswith('_') and 'widget' in m.lower():
            log(f"  {m}")

def run():
    """Inspect all relevant Widget Blueprints."""
    log("=" * 70)
    log("Widget Blueprint Inspector")
    log("=" * 70)

    # Widget blueprints to inspect
    widgets_to_check = [
        "/MOFramework/UI/WBP_InspectionProgress",
        "/MOFramework/UI/WBP_HarvestProgress",
        "/MOFramework/UI/CommonUI/WBP_MOProgressWidget",
        "/MOFramework/UI/CommonUI/WBP_MOConfirmation",
        "/MOFramework/UI/CommonUI/WBP_MOScrollList",
        "/MOFramework/UI/CommonUI/WBP_MOListEntry",
        "/MOFramework/UI/CommonUI/WBP_MODetailPanel",
        "/MOFramework/UI/WBP_InventoryMenu",
        "/MOFramework/UI/WBP_CraftingMenu",
        "/MOFramework/UI/WBP_SkillsPanel",
    ]

    for wbp_path in widgets_to_check:
        try:
            inspect_widget_blueprint(wbp_path)
        except Exception as e:
            log_error(f"Error inspecting {wbp_path}: {e}")

    # Show available methods for debugging
    list_all_widget_methods("/MOFramework/UI/WBP_InspectionProgress")

    log("")
    log("=" * 70)
    log("Inspection Complete")
    log("=" * 70)

if __name__ == "__main__":
    run()
