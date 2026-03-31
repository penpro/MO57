# Add Missing ActionNameText to WBP_MOProgressWidget
#
# Run in UE Editor: py "D:/UEProjects/MO57/Content/Python/add_missing_actionname.py"

print("=== SCRIPT LOADING ===")
import unreal

def log(msg):
    print(f"[MO57] {msg}")

def run():
    log("=" * 60)
    log("Add Missing ActionNameText Widget")
    log("=" * 60)

    wbp_path = "/MOFramework/UI/CommonUI/WBP_MOProgressWidget"

    # Load the blueprint
    wbp = unreal.EditorAssetLibrary.load_asset(wbp_path)
    if not wbp:
        log("ERROR: Failed to load blueprint")
        return

    log(f"Loaded: {wbp_path}")

    # First, let's find what widgets exist to identify a parent
    log("\nSearching for existing widgets to find a parent...")

    # Common container widget names to check
    container_names = [
        "RootCanvas", "CanvasPanel", "RootPanel", "MainCanvas",
        "ContentPanel", "Container", "Root", "Overlay", "MainOverlay",
        "VerticalBox", "HorizontalBox", "ContentBox"
    ]

    found_parent = None
    for name in container_names:
        widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(name))
        if widget:
            log(f"  Found potential parent: {name} ({type(widget).__name__})")
            found_parent = name
            break

    # Also check for existing widgets that we know exist
    existing = ["ProgressBar", "TimeRemainingText"]
    for name in existing:
        widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(name))
        if widget:
            log(f"  Found existing widget: {name} ({type(widget).__name__})")
            # Try to get its parent
            try:
                slot = widget.get_editor_property('Slot')
                if slot:
                    parent = slot.get_editor_property('Parent')
                    if parent:
                        parent_name = parent.get_name()
                        log(f"    Parent of {name}: {parent_name} ({type(parent).__name__})")
                        if not found_parent:
                            found_parent = parent_name
            except Exception as e:
                log(f"    Could not get parent: {e}")

    if not found_parent:
        log("\nERROR: Could not find a parent widget.")
        log("You may need to manually add ActionNameText in the Designer.")
        return

    log(f"\nUsing parent: {found_parent}")

    # Try to add ActionNameText as a child of the found parent
    log("\nAdding ActionNameText widget...")
    try:
        new_widget = unreal.EditorUtilityLibrary.add_source_widget(
            wbp,
            unreal.TextBlock,
            unreal.Name("ActionNameText"),
            unreal.Name(found_parent)
        )
        if new_widget:
            log(f"  SUCCESS: Created ActionNameText")

            # Mark it as a variable
            if hasattr(unreal, 'MOWidgetEditorUtils'):
                unreal.MOWidgetEditorUtils.set_widget_is_variable(new_widget, True)
                log(f"  Set as variable")

            # Save
            wbp.modify()
            unreal.EditorAssetLibrary.save_asset(wbp_path)
            log(f"  SAVED")
        else:
            log("  ERROR: add_source_widget returned None")
    except Exception as e:
        log(f"  ERROR: {e}")

    log("\n" + "=" * 60)
    log("Done - Open WBP_MOProgressWidget and compile to verify")
    log("=" * 60)

run()
