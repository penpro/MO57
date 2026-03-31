# Configure all widget properties after creation
#
# Run: py "D:/UEProjects/MO57/Content/Python/configure_all_widgets.py"

import unreal

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def find_widget(wbp, widget_name: str):
    try:
        return unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    except:
        return None

def set_text_safe(widget, text: str):
    """Set text on a TextBlock widget."""
    if widget and hasattr(widget, 'set_text'):
        try:
            widget.set_text(text)
            return True
        except:
            pass
    return False

def configure_slot_fill(widget, horizontal_fill=True, vertical_fill=False):
    """Configure slot fill behavior for box layouts."""
    if not widget:
        return False
    try:
        slot = widget.get_editor_property('slot')
        if slot:
            # Check what type of slot it is
            slot_type = type(slot).__name__
            log(f"    Slot type: {slot_type}")

            if 'VerticalBoxSlot' in slot_type or 'HorizontalBoxSlot' in slot_type:
                # Try to set size rules
                if hasattr(slot, 'set_editor_property'):
                    # SizeRule: 0=Auto, 1=Fill
                    try:
                        if horizontal_fill:
                            slot.set_editor_property('size', unreal.SlateChildSize(1.0))  # Fill
                        return True
                    except Exception as e:
                        log(f"    Could not set size: {e}")
            return True
    except Exception as e:
        log(f"    Slot error: {e}")
    return False

def explore_button_text(button):
    """Try to find how to set button label text."""
    if not button:
        return

    log(f"  Exploring button text options:")

    # Check for common text properties
    text_props = ['text', 'Text', 'label', 'Label', 'button_text', 'ButtonText']
    for prop in text_props:
        try:
            val = button.get_editor_property(prop)
            log(f"    {prop}: {val}")
        except:
            pass

    # Check for child widgets (buttons often have a TextBlock child)
    if hasattr(button, 'get_all_children'):
        try:
            children = button.get_all_children()
            log(f"    Children: {children}")
        except:
            pass

def save_wbp(wbp, path: str):
    try:
        wbp.modify()
        unreal.EditorAssetLibrary.save_asset(path)
        log(f"  Saved: {path}")
        return True
    except Exception as e:
        log(f"  Error saving: {e}")
        return False

# ============================================================================
# WIDGET CONFIGURATIONS
# ============================================================================

def configure_confirmation():
    """Configure WBP_MOConfirmation widgets."""
    log("")
    log("=" * 60)
    log("Configuring WBP_MOConfirmation")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOConfirmation"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return False

    # Set text defaults
    title = find_widget(wbp, "TitleText")
    if title:
        set_text_safe(title, "Confirm Action")
        log("  TitleText: 'Confirm Action'")

    message = find_widget(wbp, "MessageText")
    if message:
        set_text_safe(message, "Are you sure you want to proceed?")
        log("  MessageText: 'Are you sure you want to proceed?'")

    # Explore button text setting
    confirm_btn = find_widget(wbp, "ConfirmButton")
    if confirm_btn:
        explore_button_text(confirm_btn)

    save_wbp(wbp, path)
    return True

def configure_progress_widget():
    """Configure WBP_MOProgressWidget widgets."""
    log("")
    log("=" * 60)
    log("Configuring WBP_MOProgressWidget")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOProgressWidget"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return False

    # Set text defaults
    action_label = find_widget(wbp, "ActionLabel")
    if action_label:
        set_text_safe(action_label, "Action in Progress...")
        log("  ActionLabel: 'Action in Progress...'")

    time_text = find_widget(wbp, "TimeRemainingText")
    if time_text:
        set_text_safe(time_text, "00:00")
        log("  TimeRemainingText: '00:00'")

    # Configure progress bar
    progress_bar = find_widget(wbp, "ProgressBar")
    if progress_bar:
        log(f"  ProgressBar found: {progress_bar}")
        # Try to set default percent
        try:
            progress_bar.set_percent(0.5)
            log("  ProgressBar percent set to 0.5")
        except Exception as e:
            log(f"  Could not set percent: {e}")

    save_wbp(wbp, path)
    return True

def configure_detail_panel():
    """Configure WBP_MODetailPanel widgets."""
    log("")
    log("=" * 60)
    log("Configuring WBP_MODetailPanel")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MODetailPanel"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return False

    title = find_widget(wbp, "TitleText")
    if title:
        set_text_safe(title, "Item Name")
        log("  TitleText: 'Item Name'")

    desc = find_widget(wbp, "DescriptionText")
    if desc:
        set_text_safe(desc, "Item description goes here.")
        log("  DescriptionText: 'Item description goes here.'")

    save_wbp(wbp, path)
    return True

def configure_list_entry():
    """Configure WBP_MOListEntry widgets."""
    log("")
    log("=" * 60)
    log("Configuring WBP_MOListEntry")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOListEntry"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return False

    # This widget is simpler - mainly a button with optional content
    entry_btn = find_widget(wbp, "EntryButton")
    if entry_btn:
        log(f"  EntryButton found: {entry_btn}")
        explore_button_text(entry_btn)

    save_wbp(wbp, path)
    return True

def configure_colony_portrait():
    """Configure WBP_MOColonyPortrait widgets."""
    log("")
    log("=" * 60)
    log("Configuring WBP_MOColonyPortrait")
    log("=" * 60)

    path = "/MOFramework/UI/Colony/WBP_MOColonyPortrait"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return False

    name_label = find_widget(wbp, "NameLabel")
    if name_label:
        set_text_safe(name_label, "Colonist Name")
        log("  NameLabel: 'Colonist Name'")

    save_wbp(wbp, path)
    return True

def configure_scroll_list():
    """Configure WBP_MOScrollList widgets."""
    log("")
    log("=" * 60)
    log("Configuring WBP_MOScrollList")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOScrollList"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log(f"Failed to load: {path}")
        return False

    scroll_box = find_widget(wbp, "ContentScrollBox")
    if scroll_box:
        log(f"  ContentScrollBox found: {scroll_box}")
        log(f"    Type: {type(scroll_box)}")

    save_wbp(wbp, path)
    return True

# ============================================================================
# MAIN
# ============================================================================

def run():
    log("=" * 60)
    log("Configuring All Widget Properties")
    log("=" * 60)

    results = {
        "WBP_MOConfirmation": configure_confirmation(),
        "WBP_MOProgressWidget": configure_progress_widget(),
        "WBP_MODetailPanel": configure_detail_panel(),
        "WBP_MOListEntry": configure_list_entry(),
        "WBP_MOColonyPortrait": configure_colony_portrait(),
        "WBP_MOScrollList": configure_scroll_list(),
    }

    log("")
    log("=" * 60)
    log("Summary")
    log("=" * 60)
    for name, success in results.items():
        status = "OK" if success else "FAILED"
        log(f"  {name}: {status}")

    log("")
    log("NOTES:")
    log("  - TextBlock text can be set via set_text()")
    log("  - ProgressBar percent can be set via set_percent()")
    log("  - Button labels may need manual setup in Designer")
    log("  - Widget sizes/anchors need manual setup in Designer")
    log("")
    log("Done!")

if __name__ == "__main__":
    run()
