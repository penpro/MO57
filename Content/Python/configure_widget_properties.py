# Configure widget properties after creation
#
# Run: py "D:/UEProjects/MO57/Content/Python/configure_widget_properties.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def find_widget(wbp, widget_name: str):
    try:
        return unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    except:
        return None

def explore_widget_properties(widget, name):
    """Explore what properties we can set on a widget."""
    log(f"")
    log(f"Exploring {name}:")
    log(f"  Type: {type(widget)}")

    # Try to get settable properties
    if hasattr(widget, 'set_editor_property'):
        # Try common properties
        props_to_try = [
            'slot', 'Slot',
            'visibility', 'Visibility',
            'render_transform', 'RenderTransform',
            'is_variable', 'bIsVariable',
        ]

        for prop in props_to_try:
            try:
                val = widget.get_editor_property(prop)
                log(f"  {prop}: {val}")
            except:
                pass

    # Check for text-related methods on TextBlock
    if 'TextBlock' in str(type(widget)):
        try:
            text = widget.get_text()
            log(f"  Current text: {text}")
        except Exception as e:
            log(f"  get_text error: {e}")

        # Try to set text
        try:
            widget.set_text("Test Text")
            log(f"  set_text: SUCCESS")
        except Exception as e:
            log(f"  set_text error: {e}")

def configure_confirmation():
    """Try to configure WBP_MOConfirmation widgets."""
    log("=" * 60)
    log("Configuring WBP_MOConfirmation")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOConfirmation"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return

    # Find and explore widgets
    title_text = find_widget(wbp, "TitleText")
    if title_text:
        explore_widget_properties(title_text, "TitleText")
        # Try to set default text
        try:
            title_text.set_text("Confirm Action")
            log("  Set TitleText to 'Confirm Action'")
        except Exception as e:
            log(f"  Could not set text: {e}")

    message_text = find_widget(wbp, "MessageText")
    if message_text:
        explore_widget_properties(message_text, "MessageText")
        try:
            message_text.set_text("Are you sure?")
            log("  Set MessageText to 'Are you sure?'")
        except Exception as e:
            log(f"  Could not set text: {e}")

    # Check buttons
    confirm_btn = find_widget(wbp, "ConfirmButton")
    if confirm_btn:
        log(f"")
        log(f"ConfirmButton found: {confirm_btn}")
        log(f"  Type: {type(confirm_btn)}")

        # List available methods
        methods = [m for m in dir(confirm_btn) if not m.startswith('_')]
        log(f"  Methods (first 20): {methods[:20]}")

    # Save
    try:
        wbp.modify()
        unreal.EditorAssetLibrary.save_asset(path)
        log(f"Saved: {path}")
    except Exception as e:
        log(f"Save error: {e}")

def run():
    configure_confirmation()
    log("")
    log("Done exploring widget configuration options.")

if __name__ == "__main__":
    run()
