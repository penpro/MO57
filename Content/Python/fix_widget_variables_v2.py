# Fix Widget Variables V2 - Use EdGraphPinType with proper construction
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/fix_widget_variables_v2.py"
#
# Based on research: EdGraphPinType needs PinCategory='object' and PinSubCategoryObject=Class

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def explore_blueprint_editor_library():
    """Find all available methods on BlueprintEditorLibrary."""
    log("=" * 70)
    log("BlueprintEditorLibrary Methods")
    log("=" * 70)

    bel = unreal.BlueprintEditorLibrary
    for attr in sorted(dir(bel)):
        if not attr.startswith('_'):
            log(f"  {attr}")

def try_construct_pin_type_with_kwargs():
    """Try to construct EdGraphPinType with keyword arguments."""
    log("")
    log("=" * 70)
    log("Trying EdGraphPinType with keyword arguments")
    log("=" * 70)

    # Get ProgressBar class
    pb_class = unreal.ProgressBar.static_class()
    log(f"ProgressBar class: {pb_class}")

    # Try constructing with kwargs like the old UnrealEnginePython
    approaches = [
        # Approach 1: kwargs with class object
        lambda: unreal.EdGraphPinType(
            pin_category='object',
            pin_sub_category_object=pb_class
        ),
        # Approach 2: PascalCase kwargs
        lambda: unreal.EdGraphPinType(
            PinCategory='object',
            PinSubCategoryObject=pb_class
        ),
        # Approach 3: Try tuple/positional
        lambda: unreal.EdGraphPinType('object', '', pb_class),
        # Approach 4: Use import_text
        lambda: create_via_import_text(pb_class),
    ]

    for i, approach in enumerate(approaches):
        try:
            log(f"")
            log(f"Approach {i+1}:")
            pin_type = approach()
            log(f"  Created: {pin_type}")

            # Try to read back properties
            try:
                log(f"  to_tuple(): {pin_type.to_tuple()}")
            except Exception as e:
                log(f"  to_tuple() failed: {e}")

            # Test if it works with add_member_variable
            return pin_type
        except Exception as e:
            log_error(f"  Failed: {e}")

    return None

def create_via_import_text(widget_class):
    """Try creating EdGraphPinType via import_text."""
    pin_type = unreal.EdGraphPinType()
    # Format: (PinCategory="object",PinSubCategory="",PinSubCategoryObject=/Script/UMG.ProgressBar,...)
    class_path = widget_class.get_path_name()
    text = f'(PinCategory="object",PinSubCategory="",PinSubCategoryObject={class_path},PinSubCategoryMemberReference=(),PinValueType=(),ContainerType=None,bIsReference=False,bIsConst=False,bIsWeakPointer=False,bIsUObjectWrapper=False,bSerializeAsSinglePrecisionFloat=False)'
    log(f"  Trying import_text with: {text[:80]}...")
    pin_type.import_text(text)
    return pin_type

def try_using_existing_variable():
    """Try to copy pin type from an existing blueprint variable."""
    log("")
    log("=" * 70)
    log("Trying to copy pin type from existing variable")
    log("=" * 70)

    # Load a blueprint that has widget variables we can inspect
    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/WBP_InspectionProgress")
    if not wbp:
        log_error("Failed to load blueprint")
        return None

    # Check if we can get variable types
    if hasattr(unreal.BlueprintEditorLibrary, 'get_member_variable_type'):
        try:
            # Try to get the type of an existing variable
            pin_type = unreal.BlueprintEditorLibrary.get_member_variable_type(wbp, "TestVariable")
            log(f"Got existing variable type: {pin_type}")
            return pin_type
        except Exception as e:
            log_error(f"get_member_variable_type failed: {e}")

    return None

def add_widget_variable(wbp, var_name: str, widget_class):
    """
    Add a member variable that references a widget class.

    Args:
        wbp: WidgetBlueprint
        var_name: Variable name (should match widget name for BindWidget)
        widget_class: The widget class (e.g., unreal.ProgressBar)

    Returns:
        True if successful
    """
    log(f"  Adding variable: {var_name}")

    # Try different approaches to create the pin type
    pin_type = None

    # Approach 1: Empty pin type (we know this "works" but may not create proper type)
    # pin_type = unreal.EdGraphPinType()

    # Approach 2: Try import_text
    try:
        pin_type = unreal.EdGraphPinType()
        class_obj = widget_class.static_class() if hasattr(widget_class, 'static_class') else widget_class
        class_path = class_obj.get_path_name()
        text = f'(PinCategory="object",PinSubCategoryObject={class_path})'
        pin_type.import_text(text)
        log(f"    Created pin type via import_text")
    except Exception as e:
        log_warning(f"    import_text failed: {e}")
        pin_type = unreal.EdGraphPinType()

    # Add the variable
    try:
        result = unreal.BlueprintEditorLibrary.add_member_variable(
            wbp,
            var_name,
            pin_type
        )
        if result:
            log(f"    SUCCESS: Variable added")
            return True
        else:
            log_warning(f"    add_member_variable returned False")
            return False
    except Exception as e:
        log_error(f"    add_member_variable failed: {e}")
        return False

def fix_widget_blueprint(wbp_path: str, widgets: dict):
    """
    Fix a Widget Blueprint by adding variables for each widget.

    Args:
        wbp_path: Asset path
        widgets: Dict of {name: class} for widgets that need variables

    Returns:
        True if any changes made
    """
    log("")
    log(f"Fixing: {wbp_path}")
    log("-" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log_error(f"  Failed to load")
        return False

    if not isinstance(wbp, unreal.WidgetBlueprint):
        log_error(f"  Not a WidgetBlueprint")
        return False

    changes = False
    for var_name, widget_class in widgets.items():
        # Check if widget exists
        widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(var_name))
        if widget:
            log(f"  Widget '{var_name}' exists")
            if add_widget_variable(wbp, var_name, widget_class):
                changes = True
        else:
            log_warning(f"  Widget '{var_name}' NOT FOUND - skipping")

    if changes:
        try:
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED")
        except Exception as e:
            log_error(f"  Save failed: {e}")

    return changes

def run():
    log("=" * 70)
    log("Fix Widget Variables V2")
    log("=" * 70)

    # First explore what's available
    explore_blueprint_editor_library()

    # Try to construct proper pin type
    pin_type = try_construct_pin_type_with_kwargs()

    # Try to copy from existing
    try_using_existing_variable()

    # Now try to fix the actual blueprints
    log("")
    log("=" * 70)
    log("Fixing Widget Blueprints")
    log("=" * 70)

    blueprints = {
        "/MOFramework/UI/WBP_InspectionProgress": {
            "ProgressBar": unreal.ProgressBar,
            "ActionNameText": unreal.TextBlock,
            "TimeRemainingText": unreal.TextBlock,
            "InstructionText": unreal.TextBlock,
        },
        "/MOFramework/UI/WBP_HarvestProgress": {
            "ProgressBar": unreal.ProgressBar,
            "ActionNameText": unreal.TextBlock,
            "TimeRemainingText": unreal.TextBlock,
            "InstructionText": unreal.TextBlock,
        },
    }

    for wbp_path, widgets in blueprints.items():
        try:
            fix_widget_blueprint(wbp_path, widgets)
        except Exception as e:
            log_error(f"Error fixing {wbp_path}: {e}")

    log("")
    log("=" * 70)
    log("Complete - Try compiling blueprints now")
    log("=" * 70)

if __name__ == "__main__":
    run()
