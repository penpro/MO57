# Test setting bIsVariable directly on widget templates
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/test_set_is_variable.py"

print("=== STARTING ===")
import unreal

def log(msg):
    print(f"[MO57] {msg}")

def test_widget_properties():
    """Test if we can access bIsVariable on widgets."""
    log("Loading WBP_InspectionProgress...")

    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/WBP_InspectionProgress")
    if not wbp:
        log("ERROR: Failed to load blueprint")
        return

    log(f"Blueprint type: {type(wbp)}")

    # Try to find the widget
    widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name("ProgressBar"))
    if not widget:
        log("ERROR: Widget not found")
        return

    log(f"Found widget: {widget.get_name()} ({type(widget).__name__})")

    # Try to read bIsVariable
    log("\n--- Trying to READ bIsVariable ---")

    # Attempt 1: get_editor_property with various names
    property_names = ['bIsVariable', 'b_is_variable', 'IsVariable', 'is_variable']
    for name in property_names:
        try:
            val = widget.get_editor_property(name)
            log(f"  get_editor_property('{name}') = {val}")
        except Exception as e:
            log(f"  get_editor_property('{name}') FAILED: {e}")

    # Attempt 2: Direct attribute access
    try:
        val = widget.b_is_variable
        log(f"  widget.b_is_variable = {val}")
    except Exception as e:
        log(f"  widget.b_is_variable FAILED: {e}")

    try:
        val = widget.bIsVariable
        log(f"  widget.bIsVariable = {val}")
    except Exception as e:
        log(f"  widget.bIsVariable FAILED: {e}")

    # List all properties that might be related
    log("\n--- All properties containing 'var' ---")
    for attr in dir(widget):
        if 'var' in attr.lower():
            log(f"  {attr}")

    log("\n--- Trying to SET bIsVariable ---")

    # Attempt to set it
    try:
        widget.set_editor_property('b_is_variable', True)
        log("  set_editor_property('b_is_variable', True) - SUCCESS!")
    except Exception as e:
        log(f"  set_editor_property('b_is_variable', True) FAILED: {e}")

    try:
        widget.set_editor_property('bIsVariable', True)
        log("  set_editor_property('bIsVariable', True) - SUCCESS!")
    except Exception as e:
        log(f"  set_editor_property('bIsVariable', True) FAILED: {e}")

    # Try accessing through UClass property system
    log("\n--- Trying UClass property access ---")
    try:
        cls = widget.get_class()
        log(f"Widget class: {cls.get_name()}")

        # Try to find the property
        for prop_name in ['bIsVariable', 'b_is_variable']:
            prop = cls.find_property_by_name(prop_name) if hasattr(cls, 'find_property_by_name') else None
            if prop:
                log(f"  Found property: {prop_name}")
    except Exception as e:
        log(f"  UClass access failed: {e}")

def run():
    log("=" * 60)
    log("Testing bIsVariable Access")
    log("=" * 60)

    try:
        test_widget_properties()
    except Exception as e:
        log(f"Test failed with exception: {e}")
        import traceback
        traceback.print_exc()

    log("\n" + "=" * 60)
    log("Test Complete")
    log("=" * 60)

run()
