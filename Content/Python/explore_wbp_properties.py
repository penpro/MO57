# Explore Widget Blueprint internal properties
#
# Run: py "D:/UEProjects/MO57/Content/Python/explore_wbp_properties.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def explore_editor_properties():
    """Try to access internal properties via get_editor_property."""
    log("=" * 60)
    log("Exploring WBP Editor Properties")
    log("=" * 60)

    wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/CommonUI/WBP_MOProgressWidget")
    if not wbp:
        log("Failed to load WBP")
        return

    # Known UWidgetBlueprint properties from C++
    properties_to_try = [
        "widget_tree",
        "WidgetTree",
        "widgettree",
        "root_widget",
        "RootWidget",
        "bindings",
        "Bindings",
        "animations",
        "Animations",
        "palette_category",
        "PaletteCategory",
        "template_preview_size",
        "DesignTimeSize",
        "design_size_mode",
    ]

    log("Trying get_editor_property on WidgetBlueprint:")
    for prop in properties_to_try:
        try:
            val = wbp.get_editor_property(prop)
            log(f"  {prop}: {val} ({type(val)})")
        except Exception as e:
            pass  # Silent fail for non-existent properties

    # Check generated class properties
    log("")
    log("Checking generated_class properties:")
    gen_class = wbp.generated_class()
    if gen_class:
        gen_props = [
            "widget_tree",
            "WidgetTree",
            "template",
            "Template",
            "class_generated_by",
        ]
        for prop in gen_props:
            try:
                val = gen_class.get_editor_property(prop)
                log(f"  {prop}: {val} ({type(val)})")
            except:
                pass

    # Try to use set_editor_property to see what's writable
    log("")
    log("Checking for WidgetBlueprintFactory:")
    try:
        factory = unreal.WidgetBlueprintFactory()
        log(f"Factory created: {factory}")
        explore_object(factory, "WidgetBlueprintFactory")
    except Exception as e:
        log(f"Error creating factory: {e}")

def explore_object(obj, name):
    """Quick explore of an object."""
    log(f"{name}:")
    attrs = [a for a in dir(obj) if not a.startswith('_')]

    for attr in attrs[:30]:
        try:
            val = getattr(obj, attr)
            if not callable(val):
                log(f"  {attr}: {val}")
        except:
            pass

def check_blueprint_editor():
    """Check if we can access the blueprint editor."""
    log("")
    log("=" * 60)
    log("Checking Blueprint Editor Access")
    log("=" * 60)

    # Try to get editor utility subsystem
    try:
        eus = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        log(f"EditorUtilitySubsystem: {eus}")
    except Exception as e:
        log(f"EditorUtilitySubsystem: {e}")

    # Check for level editor subsystem (might have widget utilities)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        log(f"LevelEditorSubsystem: {les}")
    except Exception as e:
        log(f"LevelEditorSubsystem: {e}")

    # Check AssetEditorSubsystem
    try:
        aes = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        log(f"AssetEditorSubsystem: {aes}")
        if aes:
            # Try to open the widget blueprint
            wbp = unreal.EditorAssetLibrary.load_asset("/MOFramework/UI/CommonUI/WBP_MOProgressWidget")
            if wbp:
                log("Attempting to open asset in editor...")
                aes.open_editor_for_assets([wbp])
                log("Asset editor opened (if UI showed up)")
    except Exception as e:
        log(f"AssetEditorSubsystem: {e}")

def run():
    explore_editor_properties()
    check_blueprint_editor()
    log("")
    log("Done. If no widget_tree access found, Python API doesn't support direct widget tree editing.")

if __name__ == "__main__":
    run()
