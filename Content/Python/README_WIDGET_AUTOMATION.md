# UE5 Widget Blueprint Python Automation

## Overview

This document covers findings from automating Widget Blueprint manipulation via UE's Python API.
The goal is to programmatically manage BindWidget bindings and widget properties.

**Last Updated:** 2026-03-29

---

## Key Findings

### What Works

| Operation | Function | Notes |
|-----------|----------|-------|
| Load Widget Blueprint | `unreal.EditorAssetLibrary.load_asset(path)` | Returns `WidgetBlueprint` object |
| Find widget by name | `unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, Name)` | Returns the widget instance |
| Add new widget | `unreal.EditorUtilityLibrary.add_source_widget(wbp, class, name, parent)` | Adds widget to hierarchy |
| Save asset | `unreal.EditorAssetLibrary.save_asset(path)` | Call `wbp.modify()` first |
| Get widget slot | `widget.get_editor_property('Slot')` | Returns CanvasPanelSlot, etc. |
| Get widget type | `type(widget).__name__` | e.g., "ProgressBar", "TextBlock" |

### What Doesn't Work (API Limitations)

| Operation | Attempted Approach | Result |
|-----------|-------------------|--------|
| Set IsVariable flag | `widget.set_editor_property('b_is_variable', True)` | **FAILS** - Property not exposed |
| Access widget tree | `wbp.widget_tree()` or `wbp.get_widget_tree()` | **FAILS** - Not exposed to Python |
| Rename widget | No direct API found | **NOT AVAILABLE** |
| Remove widget | `remove_source_widget` | **NOT TESTED** - may not exist |

### The "Is Variable" Problem

The `bIsVariable` flag that enables `BindWidget` meta binding is stored in the WidgetTree metadata,
NOT on the widget instance itself. UE's Python API does not expose this property.

**Workarounds:**
1. Use `add_source_widget()` which MAY automatically mark widgets as variables (needs testing)
2. Use `BlueprintEditorLibrary.add_member_variable()` to create matching variables (needs testing)
3. Manual fix in editor: Right-click widget → "Set as Variable"

---

## Available Scripts

### 1. inspect_widget_blueprints.py
**Purpose:** Inspect Widget Blueprints and list all widgets with their IsVariable status.

```bash
py "D:/UEProjects/MO57/Content/Python/inspect_widget_blueprints.py"
```

**Output:**
- Lists all widgets found via `find_source_widget_by_name`
- Shows `[VAR]` or `[NOT VAR]` status
- Checks for common binding names (ProgressBar, ActionNameText, etc.)

### 2. explore_widget_tree.py
**Purpose:** Deep exploration of WidgetBlueprint API to find available methods.

```bash
py "D:/UEProjects/MO57/Content/Python/explore_widget_tree.py"
```

**Output:**
- Lists all attributes on WidgetBlueprint
- Lists EditorUtilityLibrary methods
- Lists BlueprintEditorLibrary methods
- Explores widget slot properties

### 3. fix_widget_variables.py
**Purpose:** Attempt to set IsVariable=True on widgets (currently fails due to API limitation).

```bash
py "D:/UEProjects/MO57/Content/Python/fix_widget_variables.py"
```

**Status:** Does not work - `b_is_variable` property not exposed.

### 4. setup_widget_bindings.py
**Purpose:** Add missing widgets to Widget Blueprints using `add_source_widget`.

```bash
py "D:/UEProjects/MO57/Content/Python/setup_widget_bindings.py"
```

**Features:**
- Creates root CanvasPanel if missing
- Adds required widgets (ProgressBar, TextBlock, etc.)
- Checks for existing widgets before adding
- Saves changes automatically

### 5. recreate_widget_bindings.py
**Purpose:** Experimental - try adding member variables to link widgets.

```bash
py "D:/UEProjects/MO57/Content/Python/recreate_widget_bindings.py"
```

**Status:** Experimental - needs testing.

### 6. fix_inspection_widget.py
**Purpose:** Attempt to rename ItemNameText to ActionNameText.

```bash
py "D:/UEProjects/MO57/Content/Python/fix_inspection_widget.py"
```

**Status:** Rename not available in Python API.

### 7. fix_widget_bindings_complete.py (RECOMMENDED)
**Purpose:** Comprehensive script to create missing widgets and document manual fixes.

```bash
py "D:/UEProjects/MO57/Content/Python/fix_widget_bindings_complete.py"
```

**Features:**
- Creates MISSING widgets using `add_source_widget()`
- Documents EXISTING widgets that need manual "Is Variable" marking
- Focuses on required BindWidget bindings (not optional ones)
- Provides summary with next steps

**Output:**
- Lists widgets created (may auto-mark as variable)
- Lists widgets needing manual fix (exist but not variable)
- Instructions for manual fixes

### 8. fix_widget_variables_v3.py
**Purpose:** Add member variables using get_object_reference_type().

```bash
py "D:/UEProjects/MO57/Content/Python/fix_widget_variables_v3.py"
```

**Status:** Variables added successfully, but doesn't fix BindWidget (see Key Findings).

### 9. explore_pin_type.py
**Purpose:** Explore EdGraphPinType and BlueprintEditorLibrary APIs.

```bash
py "D:/UEProjects/MO57/Content/Python/explore_pin_type.py"
```

**Status:** Research complete - documented findings in Key Findings section.

---

## API Reference

### EditorUtilityLibrary Widget Methods

```python
# Find a widget by name in a Widget Blueprint
widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(
    wbp,                    # WidgetBlueprint
    unreal.Name("MyWidget") # Widget name
)

# Add a new widget to a Widget Blueprint
new_widget = unreal.EditorUtilityLibrary.add_source_widget(
    wbp,                      # WidgetBlueprint
    unreal.ProgressBar,       # Widget class
    unreal.Name("ProgressBar"), # Name for the widget
    unreal.Name("ParentName")   # Parent widget name, or "" for root
)

# Cast to WidgetBlueprint (if loaded as generic Object)
wbp = unreal.EditorUtilityLibrary.cast_to_widget_blueprint(obj)
```

### BlueprintEditorLibrary Methods

```python
# Add a member variable to a Blueprint
result = unreal.BlueprintEditorLibrary.add_member_variable(
    blueprint,    # Blueprint object
    "VarName",    # Variable name
    "VarType"     # Variable type (e.g., "ProgressBar")
)

# Other potentially useful methods:
# - remove_unused_variables
# - replace_variable_references
# - set_blueprint_variable_instance_editable
# - set_blueprint_variable_expose_on_spawn
```

### Widget Properties

```python
# Get widget name
name = widget.get_name()

# Get widget slot (layout info)
slot = widget.get_editor_property('Slot')

# Slot properties (for CanvasPanelSlot)
slot.get_anchors()
slot.get_offsets()
slot.get_alignment()
slot.get_size()
slot.get_position()
slot.get_z_order()

# Set slot properties
slot.set_anchors(anchors)
slot.set_size(size)
slot.set_position(position)
```

### Widget Classes

```python
# Standard UMG widgets
unreal.ProgressBar
unreal.TextBlock
unreal.Image
unreal.Button
unreal.Border
unreal.CanvasPanel
unreal.VerticalBox
unreal.HorizontalBox
unreal.ScrollBox
unreal.Overlay

# Load custom widget class
mo_button = unreal.load_class(None, "/Script/MOFramework.MOCommonButton")
```

---

## Common Patterns

### Loading and Saving

```python
# Load
wbp = unreal.EditorAssetLibrary.load_asset("/Game/UI/MyWidget")

# Modify (must call before save)
wbp.modify()

# Save
unreal.EditorAssetLibrary.save_asset("/Game/UI/MyWidget")
```

### Checking Widget Existence

```python
def widget_exists(wbp, name):
    widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(
        wbp, unreal.Name(name)
    )
    return widget is not None
```

### Adding Widget Only If Missing

```python
def ensure_widget(wbp, name, widget_class, parent=""):
    if not widget_exists(wbp, name):
        return unreal.EditorUtilityLibrary.add_source_widget(
            wbp,
            widget_class,
            unreal.Name(name),
            unreal.Name(parent)
        )
    return unreal.EditorUtilityLibrary.find_source_widget_by_name(
        wbp, unreal.Name(name)
    )
```

---

## Known Issues

### 1. IsVariable Flag Not Settable
**Problem:** Cannot programmatically check the "Is Variable" checkbox.
**Impact:** BindWidget bindings fail if widgets aren't marked as variables.
**Workaround:** Manual fix required, or create widgets fresh with add_source_widget (untested).

### 2. Widget Tree Not Accessible
**Problem:** `widget_tree()` method exists but returns None or isn't callable.
**Impact:** Cannot iterate all widgets or access tree metadata.
**Workaround:** Use `find_source_widget_by_name` with known widget names.

### 3. Widget Rename Not Available
**Problem:** No API to rename existing widgets.
**Impact:** If C++ binding name changes, Blueprint must be manually updated.
**Workaround:** Delete and recreate widget with new name (loses layout).

---

## Key Findings (2026-03-29)

### EdGraphPinType Construction

The `EdGraphPinType` struct cannot be constructed with keyword arguments in Python:
```python
# This FAILS:
pin_type = unreal.EdGraphPinType(pin_category='object')

# This WORKS - use the helper function:
pin_type = unreal.BlueprintEditorLibrary.get_object_reference_type(widget_class.static_class())
```

### Adding Member Variables

Member variables can be added successfully:
```python
pin_type = unreal.BlueprintEditorLibrary.get_object_reference_type(unreal.ProgressBar.static_class())
result = unreal.BlueprintEditorLibrary.add_member_variable(wbp, "ProgressBar", pin_type)
```

However, this does NOT automatically link to widgets or mark widgets as variables.
BindWidget requires the widget itself to be marked "Is Variable", not just a matching variable name.

### The bIsVariable Limitation

**CONFIRMED: Cannot set bIsVariable via Python.**

The `bIsVariable` flag is stored in the WidgetTree metadata (`WidgetTree->SetWidgetIsVariable()`),
which is an internal editor function not exposed to Python.

**Workarounds:**
1. Create widgets fresh with `add_source_widget()` - may auto-mark as variable (untested)
2. Manual fix in editor: Right-click widget → "Is Variable"
3. Future: Expose bIsVariable setter via custom C++ editor utility

---

## Future Improvements

1. ~~Test add_source_widget IsVariable behavior~~ - Needs testing in editor
2. **Expose bIsVariable setter** - Create C++ editor utility that Python can call
3. **Blueprint Nativization** - Store widget definitions in C++ DefaultSubobjects
4. **Editor Utility Widget** - Create a UI tool for batch widget variable marking

---

## File Locations

```
Content/Python/
├── README_WIDGET_AUTOMATION.md      # This file
├── fix_widget_bindings_complete.py  # RECOMMENDED - comprehensive fix script
├── inspect_widget_blueprints.py     # Inspection tool
├── explore_widget_tree.py           # API exploration
├── explore_pin_type.py              # EdGraphPinType exploration
├── fix_widget_variables.py          # IsVariable fix (doesn't work)
├── fix_widget_variables_v2.py       # Import text approach (doesn't work)
├── fix_widget_variables_v3.py       # get_object_reference_type (adds vars but not fix)
├── setup_widget_bindings.py         # Add missing widgets
├── recreate_widget_bindings.py      # Experimental member variable approach
├── fix_inspection_widget.py         # Rename attempt (doesn't work)
├── add_widget_bindings.py           # Earlier exploration script
├── create_generic_widgets.py        # Widget creation experiments
└── create_primary_layout.py         # Primary layout creation
```
