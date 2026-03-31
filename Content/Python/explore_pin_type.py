# Explore EdGraphPinType - Figure out how to construct it properly
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/explore_pin_type.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def explore_edgraph_pin_type():
    """Explore the EdGraphPinType struct."""
    log("=" * 70)
    log("Exploring EdGraphPinType")
    log("=" * 70)

    # Check if EdGraphPinType exists
    if hasattr(unreal, 'EdGraphPinType'):
        pin_type_class = unreal.EdGraphPinType
        log(f"Found: unreal.EdGraphPinType")
        log(f"Type: {type(pin_type_class)}")

        # List all attributes
        log("")
        log("Attributes:")
        for attr in dir(pin_type_class):
            if not attr.startswith('_'):
                log(f"  {attr}")

        # Try to create an instance
        log("")
        log("Trying to create instance:")
        try:
            instance = unreal.EdGraphPinType()
            log(f"  Created: {instance}")
            log(f"  Type: {type(instance)}")

            # List instance attributes
            log("")
            log("Instance attributes:")
            for attr in dir(instance):
                if not attr.startswith('_'):
                    try:
                        val = getattr(instance, attr)
                        if not callable(val):
                            log(f"  {attr} = {val}")
                    except:
                        log(f"  {attr} (not readable)")
        except Exception as e:
            log_error(f"  Failed to create: {e}")
    else:
        log_warning("EdGraphPinType not found in unreal module")

    # Look for related types
    log("")
    log("Related types in unreal module:")
    for name in dir(unreal):
        lower = name.lower()
        if 'pin' in lower or 'graph' in lower and 'type' in lower:
            if not name.startswith('_'):
                log(f"  {name}")

def explore_blueprint_editor_library():
    """Explore BlueprintEditorLibrary in detail."""
    log("")
    log("=" * 70)
    log("Exploring BlueprintEditorLibrary.add_member_variable")
    log("=" * 70)

    bel = unreal.BlueprintEditorLibrary

    # Get help/docstring if available
    if hasattr(bel, 'add_member_variable'):
        func = bel.add_member_variable
        log(f"Function: {func}")

        if hasattr(func, '__doc__') and func.__doc__:
            log(f"Docstring: {func.__doc__}")

        # Try to get signature info
        log("")
        log("Trying to inspect function signature...")
        try:
            import inspect
            sig = inspect.signature(func)
            log(f"Signature: {sig}")
        except Exception as e:
            log(f"Could not inspect: {e}")

def try_create_pin_type_for_widget():
    """Try to create a pin type that references a widget class."""
    log("")
    log("=" * 70)
    log("Trying to create EdGraphPinType for widget reference")
    log("=" * 70)

    try:
        pin_type = unreal.EdGraphPinType()
        log(f"Created empty EdGraphPinType: {pin_type}")

        # Try to set properties
        log("")
        log("Trying to set pin_category to 'object':")
        try:
            pin_type.set_editor_property('pin_category', 'object')
            log("  Set pin_category = 'object'")
        except Exception as e:
            log_error(f"  Failed: {e}")

        # Try different property names
        props_to_try = [
            ('pin_category', 'object'),
            ('PinCategory', 'object'),
            ('category', 'object'),
            ('pin_sub_category', ''),
            ('pin_sub_category_object', None),
            ('container_type', 0),  # EPinContainerType::None
            ('is_reference', False),
            ('is_const', False),
            ('is_weak_pointer', False),
        ]

        log("")
        log("Trying to set various properties:")
        for prop_name, prop_value in props_to_try:
            try:
                pin_type.set_editor_property(prop_name, prop_value)
                log(f"  OK: {prop_name} = {prop_value}")
            except Exception as e:
                pass  # Silent fail

        # Try to get a widget class reference
        log("")
        log("Trying to get ProgressBar class for pin_sub_category_object:")
        try:
            pb_class = unreal.ProgressBar.static_class()
            log(f"  ProgressBar class: {pb_class}")

            pin_type.set_editor_property('pin_sub_category_object', pb_class)
            log("  Set pin_sub_category_object to ProgressBar class")
        except Exception as e:
            log_error(f"  Failed: {e}")

        # Now try to use this pin type
        log("")
        log("Attempting add_member_variable with constructed pin type:")
        wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/WBP_InspectionProgress")
        if wbp:
            try:
                result = unreal.BlueprintEditorLibrary.add_member_variable(
                    wbp,
                    "TestVariable",
                    pin_type
                )
                log(f"  Result: {result}")
            except Exception as e:
                log_error(f"  Failed: {e}")

    except Exception as e:
        log_error(f"Failed to create EdGraphPinType: {e}")

def try_make_variable_from_widget():
    """Try alternative approaches to make a widget a variable."""
    log("")
    log("=" * 70)
    log("Alternative approaches to make widget a variable")
    log("=" * 70)

    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/WBP_InspectionProgress")
    if not wbp:
        log_error("Failed to load WBP")
        return

    widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name("ProgressBar"))
    if not widget:
        log_error("Widget not found")
        return

    log(f"Found widget: {widget.get_name()}")

    # Try every possible method on the widget
    log("")
    log("All methods on widget that might help:")
    for attr in dir(widget):
        lower = attr.lower()
        if 'var' in lower or 'expose' in lower or 'bind' in lower or 'member' in lower:
            log(f"  {attr}")

    # Try to find methods on WidgetBlueprint
    log("")
    log("All methods on WidgetBlueprint that might help:")
    for attr in dir(wbp):
        lower = attr.lower()
        if 'var' in lower or 'widget' in lower or 'member' in lower or 'expose' in lower:
            log(f"  {attr}")

    # Check for any subsystems that might help
    log("")
    log("Checking for editor subsystems:")
    subsystem_types = [
        'WidgetBlueprintEditorSubsystem',
        'UMGEditorSubsystem',
        'BlueprintEditorSubsystem',
    ]
    for name in subsystem_types:
        if hasattr(unreal, name):
            log(f"  Found: {name}")

def run():
    log("=" * 70)
    log("EdGraphPinType and Blueprint Variable Explorer")
    log("=" * 70)

    explore_edgraph_pin_type()
    explore_blueprint_editor_library()
    try_create_pin_type_for_widget()
    try_make_variable_from_widget()

    log("")
    log("=" * 70)
    log("Exploration Complete")
    log("=" * 70)

if __name__ == "__main__":
    run()
