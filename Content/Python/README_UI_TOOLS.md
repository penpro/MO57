# MO57 UI Automation Tools

Python scripts for auditing, validating, and batch-modifying Widget Blueprints.

**Requires:** MOWidgetEditorUtils C++ utility (compile MOFramework first)

## Quick Start

```
# Run comprehensive audit (recommended first step)
py "D:/UEProjects/MO57/Content/Python/audit_ui_all.py"

# Output goes to: Content/Python/audit_output/
```

## Available Scripts

### Auditing

| Script | Purpose | Output |
|--------|---------|--------|
| `audit_ui_all.py` | **Master audit** - runs all audits | `comprehensive_report.txt` |
| `audit_ui_widgets.py` | Find common widget names across blueprints | `widget_audit.txt` |
| `audit_ui_layouts.py` | Analyze dimensions, anchors, positioning | `layout_audit.txt` |
| `audit_ui_styles.py` | Extract colors, fonts, style patterns | `style_audit.txt` |
| `validate_ui_widgets.py` | Find issues and suggest fixes | `validation_report.txt` |

### Operations

| Script | Purpose |
|--------|---------|
| `fix_all_ui_issues.py` | **One-click fix** - runs all fixes in sequence |
| `batch_ui_operations.py` | Mark variables, rename widgets by pattern |
| `fix_widget_variables_final.py` | Fix specific known variable issues |
| `smart_rename_widgets.py` | Analyze and suggest better widget names |
| `apply_widget_renames.py` | Apply rename suggestions (preview mode by default) |

## C++ API (MOWidgetEditorUtils)

Available from Python as `unreal.MOWidgetEditorUtils`:

### Query Functions
```python
# Get all widget names
names = unreal.MOWidgetEditorUtils.get_all_widget_names(wbp)

# Get widgets by type
buttons = unreal.MOWidgetEditorUtils.get_widgets_by_type(wbp, "Button")

# Check if widget exists
exists = unreal.MOWidgetEditorUtils.does_widget_exist(wbp, "MyWidget")

# Get root widget info
root_name = unreal.MOWidgetEditorUtils.get_root_widget_name(wbp)
root_type = unreal.MOWidgetEditorUtils.get_root_widget_type(wbp)

# Get detailed layout info for all widgets
layouts = unreal.MOWidgetEditorUtils.get_all_widget_layout_info(wbp)
```

### Modification Functions
```python
# Mark widget as variable
unreal.MOWidgetEditorUtils.set_widget_is_variable(widget, True)
unreal.MOWidgetEditorUtils.set_widget_is_variable_by_name(wbp, "MyWidget", True)

# Batch mark variables
name_array = unreal.Array(unreal.Name)
name_array.append(unreal.Name("TitleText"))
unreal.MOWidgetEditorUtils.batch_set_widgets_as_variables(wbp, name_array, True)

# Mark by pattern (e.g., all widgets containing "Button")
count = unreal.MOWidgetEditorUtils.batch_set_variables_by_pattern(wbp, "Button", True)

# Set anchors (preset names: TopLeft, Center, BottomRight, FullScreen, etc.)
unreal.MOWidgetEditorUtils.set_widget_anchors_preset(wbp, "MyWidget", "Center")

# Set size (for canvas panel widgets)
unreal.MOWidgetEditorUtils.set_widget_size(wbp, "MyWidget", 400, 300)

# Set position
unreal.MOWidgetEditorUtils.set_widget_position(wbp, "MyWidget", 100, 50)

# Set alignment (0-1 range)
unreal.MOWidgetEditorUtils.set_widget_alignment(wbp, "MyWidget", 0.5, 0.5)

# Set Z-order
unreal.MOWidgetEditorUtils.set_widget_z_order(wbp, "MyWidget", 10)

# Rename widget
unreal.MOWidgetEditorUtils.rename_widget(wbp, "OldName", "NewName")

# Batch rename by prefix
count = unreal.MOWidgetEditorUtils.batch_rename_by_prefix(wbp, "txt", "Text")
```

### Validation & Style
```python
# Validate blueprint
issues = unreal.MOWidgetEditorUtils.validate_widget_blueprint(wbp)
for issue in issues:
    print(f"{issue.severity}: {issue.widget_name} - {issue.message}")

# Extract styles
styles = unreal.MOWidgetEditorUtils.extract_widget_styles(wbp)
for style in styles:
    if style.b_has_text_style:
        print(f"{style.widget_name}: {style.font_size}pt, {style.text_color}")
```

## Anchor Presets

| Preset | Anchors |
|--------|---------|
| TopLeft | (0,0) - (0,0) |
| TopCenter | (0.5,0) - (0.5,0) |
| TopRight | (1,0) - (1,0) |
| CenterLeft | (0,0.5) - (0,0.5) |
| Center | (0.5,0.5) - (0.5,0.5) |
| CenterRight | (1,0.5) - (1,0.5) |
| BottomLeft | (0,1) - (0,1) |
| BottomCenter | (0.5,1) - (0.5,1) |
| BottomRight | (1,1) - (1,1) |
| TopStretch | (0,0) - (1,0) |
| BottomStretch | (0,1) - (1,1) |
| LeftStretch | (0,0) - (0,1) |
| RightStretch | (1,0) - (1,1) |
| FullScreen | (0,0) - (1,1) |

## Workflow

1. **Compile** MOFramework with new MOWidgetEditorUtils
2. **Run comprehensive audit**: `py "D:/UEProjects/MO57/Content/Python/audit_ui_all.py"`
3. **Review** `audit_output/comprehensive_report.txt`
4. **Fix issues**: `py "D:/UEProjects/MO57/Content/Python/batch_ui_operations.py"`
5. **Create base classes** based on common widget patterns
6. **Migrate** existing blueprints to use base classes
