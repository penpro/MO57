# MO57 Widget Blueprint Binding Setup
#
# This script programmatically adds required widget bindings to Widget Blueprints.
# Run: py "D:/UEProjects/MO57/Content/Python/setup_widget_bindings.py"
#
# Uses EditorUtilityLibrary.add_source_widget() to add widgets to the design-time hierarchy.

import unreal

# ============================================================================
# HELPERS
# ============================================================================

def log(msg: str):
    unreal.log(f"[MO57] {msg}")

def log_warning(msg: str):
    unreal.log_warning(f"[MO57] {msg}")

def log_error(msg: str):
    unreal.log_error(f"[MO57] {msg}")

def add_widget(wbp, widget_class, widget_name: str, parent_name: str = ""):
    """
    Add a widget to a Widget Blueprint.

    Args:
        wbp: The WidgetBlueprint asset
        widget_class: The widget class (e.g., unreal.ProgressBar)
        widget_name: Name for the widget (becomes the variable name)
        parent_name: Name of parent widget, or "" for root

    Returns:
        The created widget, or None on failure
    """
    try:
        parent = unreal.Name(parent_name) if parent_name else unreal.Name("")
        widget = unreal.EditorUtilityLibrary.add_source_widget(
            wbp,
            widget_class,
            unreal.Name(widget_name),
            parent
        )
        if widget:
            log(f"  Added: {widget_name} ({widget_class.__name__})")
            return widget
        else:
            log_error(f"  Failed to add: {widget_name}")
            return None
    except Exception as e:
        log_error(f"  Error adding {widget_name}: {e}")
        return None

def find_widget(wbp, widget_name: str):
    """Find an existing widget by name."""
    try:
        return unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, unreal.Name(widget_name))
    except:
        return None

def save_wbp(wbp, path: str):
    """Save a Widget Blueprint."""
    try:
        wbp.modify()
        unreal.EditorAssetLibrary.save_asset(path)
        log(f"  Saved: {path}")
        return True
    except Exception as e:
        log_error(f"  Error saving: {e}")
        return False

# ============================================================================
# WIDGET SETUP FUNCTIONS
# ============================================================================

def setup_progress_widget():
    """
    Setup WBP_MOProgressWidget with required bindings.

    Required: ProgressBar
    Optional: ActionLabel (TextBlock), TimeRemainingText (TextBlock), CancelButton (MOCommonButton)
    """
    log("")
    log("=" * 60)
    log("Setting up WBP_MOProgressWidget")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOProgressWidget"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log_error(f"Failed to load: {path}")
        return False

    # Check if already has a root
    root = find_widget(wbp, "CanvasPanel")
    if not root:
        # Add CanvasPanel as root (empty parent = root)
        root = add_widget(wbp, unreal.CanvasPanel, "CanvasPanel", "")
        if not root:
            log_error("Failed to create root CanvasPanel")
            return False

    # Check if ProgressBar already exists
    existing_pb = find_widget(wbp, "ProgressBar")
    if existing_pb:
        log("  ProgressBar already exists, skipping")
    else:
        # Add ProgressBar as child of CanvasPanel
        add_widget(wbp, unreal.ProgressBar, "ProgressBar", "CanvasPanel")

    # Add optional widgets
    if not find_widget(wbp, "ActionLabel"):
        add_widget(wbp, unreal.TextBlock, "ActionLabel", "CanvasPanel")

    if not find_widget(wbp, "TimeRemainingText"):
        add_widget(wbp, unreal.TextBlock, "TimeRemainingText", "CanvasPanel")

    # Note: CancelButton should use MOCommonButton, but we can add a placeholder
    # The user will need to reparent it to the correct class in Blueprint

    save_wbp(wbp, path)
    return True

def setup_confirmation_widget():
    """
    Setup WBP_MOConfirmation with required bindings.

    Required: TitleText, MessageText, ConfirmButton, CancelButton
    """
    log("")
    log("=" * 60)
    log("Setting up WBP_MOConfirmation")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOConfirmation"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log_error(f"Failed to load: {path}")
        return False

    # Check if already has a root
    root = find_widget(wbp, "CanvasPanel")
    if not root:
        root = add_widget(wbp, unreal.CanvasPanel, "CanvasPanel", "")
        if not root:
            log_error("Failed to create root CanvasPanel")
            return False

    # Add VerticalBox for layout
    vbox = find_widget(wbp, "ContentBox")
    if not vbox:
        vbox = add_widget(wbp, unreal.VerticalBox, "ContentBox", "CanvasPanel")

    # Add text blocks
    if not find_widget(wbp, "TitleText"):
        add_widget(wbp, unreal.TextBlock, "TitleText", "ContentBox")

    if not find_widget(wbp, "MessageText"):
        add_widget(wbp, unreal.TextBlock, "MessageText", "ContentBox")

    # Add button container
    hbox = find_widget(wbp, "ButtonBox")
    if not hbox:
        hbox = add_widget(wbp, unreal.HorizontalBox, "ButtonBox", "ContentBox")

    # Add buttons (using regular Button - user will need to change to MOCommonButton)
    # Actually, let's try to load MOCommonButton class
    mo_button_class = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
    button_class = mo_button_class if mo_button_class else unreal.Button

    if mo_button_class:
        log(f"  Using MOCommonButton class")
    else:
        log_warning("  MOCommonButton not found, using Button (will need manual fix)")

    if not find_widget(wbp, "ConfirmButton"):
        add_widget(wbp, button_class, "ConfirmButton", "ButtonBox")

    if not find_widget(wbp, "CancelButton"):
        add_widget(wbp, button_class, "CancelButton", "ButtonBox")

    save_wbp(wbp, path)
    return True

def setup_scroll_list_widget():
    """Setup WBP_MOScrollList with optional bindings."""
    log("")
    log("=" * 60)
    log("Setting up WBP_MOScrollList")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOScrollList"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log_error(f"Failed to load: {path}")
        return False

    root = find_widget(wbp, "CanvasPanel")
    if not root:
        root = add_widget(wbp, unreal.CanvasPanel, "CanvasPanel", "")

    if not find_widget(wbp, "ContentScrollBox"):
        add_widget(wbp, unreal.ScrollBox, "ContentScrollBox", "CanvasPanel")

    save_wbp(wbp, path)
    return True

def setup_list_entry_widget():
    """Setup WBP_MOListEntry with optional bindings."""
    log("")
    log("=" * 60)
    log("Setting up WBP_MOListEntry")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MOListEntry"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log_error(f"Failed to load: {path}")
        return False

    root = find_widget(wbp, "BackgroundBorder")
    if not root:
        root = add_widget(wbp, unreal.Border, "BackgroundBorder", "")

    # Try to add MOCommonButton
    mo_button_class = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
    button_class = mo_button_class if mo_button_class else unreal.Button

    if not find_widget(wbp, "EntryButton"):
        add_widget(wbp, button_class, "EntryButton", "BackgroundBorder")

    save_wbp(wbp, path)
    return True

def setup_detail_panel_widget():
    """Setup WBP_MODetailPanel with optional bindings."""
    log("")
    log("=" * 60)
    log("Setting up WBP_MODetailPanel")
    log("=" * 60)

    path = "/MOFramework/UI/CommonUI/WBP_MODetailPanel"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log_error(f"Failed to load: {path}")
        return False

    root = find_widget(wbp, "CanvasPanel")
    if not root:
        root = add_widget(wbp, unreal.CanvasPanel, "CanvasPanel", "")

    vbox = find_widget(wbp, "ContentBox")
    if not vbox:
        vbox = add_widget(wbp, unreal.VerticalBox, "ContentBox", "CanvasPanel")

    if not find_widget(wbp, "IconImage"):
        add_widget(wbp, unreal.Image, "IconImage", "ContentBox")

    if not find_widget(wbp, "TitleText"):
        add_widget(wbp, unreal.TextBlock, "TitleText", "ContentBox")

    if not find_widget(wbp, "DescriptionText"):
        add_widget(wbp, unreal.TextBlock, "DescriptionText", "ContentBox")

    mo_button_class = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
    button_class = mo_button_class if mo_button_class else unreal.Button

    if not find_widget(wbp, "ActionButton"):
        add_widget(wbp, button_class, "ActionButton", "ContentBox")

    save_wbp(wbp, path)
    return True

def setup_colony_portrait_widget():
    """Setup WBP_MOColonyPortrait with optional bindings."""
    log("")
    log("=" * 60)
    log("Setting up WBP_MOColonyPortrait")
    log("=" * 60)

    path = "/MOFramework/UI/Colony/WBP_MOColonyPortrait"
    wbp = unreal.EditorAssetLibrary.load_asset(path)
    if not wbp:
        log_error(f"Failed to load: {path}")
        return False

    root = find_widget(wbp, "AlertBorder")
    if not root:
        root = add_widget(wbp, unreal.Border, "AlertBorder", "")

    if not find_widget(wbp, "PortraitImage"):
        add_widget(wbp, unreal.Image, "PortraitImage", "AlertBorder")

    if not find_widget(wbp, "ActivityIndicator"):
        add_widget(wbp, unreal.Image, "ActivityIndicator", "AlertBorder")

    if not find_widget(wbp, "NameLabel"):
        add_widget(wbp, unreal.TextBlock, "NameLabel", "AlertBorder")

    mo_button_class = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
    button_class = mo_button_class if mo_button_class else unreal.Button

    if not find_widget(wbp, "ClickButton"):
        add_widget(wbp, button_class, "ClickButton", "AlertBorder")

    save_wbp(wbp, path)
    return True

# ============================================================================
# MAIN
# ============================================================================

def run():
    """Setup all widget bindings."""
    log("=" * 60)
    log("MO57 Widget Blueprint Binding Setup")
    log("=" * 60)

    results = {
        "WBP_MOProgressWidget": setup_progress_widget(),
        "WBP_MOConfirmation": setup_confirmation_widget(),
        "WBP_MOScrollList": setup_scroll_list_widget(),
        "WBP_MOListEntry": setup_list_entry_widget(),
        "WBP_MODetailPanel": setup_detail_panel_widget(),
        "WBP_MOColonyPortrait": setup_colony_portrait_widget(),
    }

    log("")
    log("=" * 60)
    log("Summary")
    log("=" * 60)
    for name, success in results.items():
        status = "OK" if success else "FAILED"
        log(f"  {name}: {status}")

    log("")
    log("NOTE: You may need to:")
    log("  1. Open each WBP and adjust widget sizes/anchors")
    log("  2. Compile each Blueprint")
    log("  3. If buttons show as 'Button', replace with MOCommonButton")
    log("")
    log("Done!")

if __name__ == "__main__":
    run()
