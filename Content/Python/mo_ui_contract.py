"""mo_ui_contract - the C++ side of the UI contract (pure Python: no unreal import, testable offline).

A Widget Blueprint is only valid if its widget tree contains the widgets its C++ parent class binds with
UPROPERTY(meta=(BindWidget)) / (BindWidgetOptional), with EXACTLY those names and compatible types. That
contract is declared in the headers, so we read it from there instead of trusting a hand-kept list.

    contract('/Script/MOFramework.MOJoinGamePanel')  -> [{name, type, required, declared_in}, ...]
    scaffold('/Script/MOFramework.MOJoinGamePanel', '/MOFramework/UI/X/WBP_JoinGamePanel')  -> spec skeleton
"""
import glob
import os
import re

SRC_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "Plugins", "MOFramework", "Source"))

_CLASS_RE = re.compile(r"^\s*class\s+(?:[A-Z0-9_]+_API\s+)?(U\w+)\s*(?:final\s*)?:\s*public\s+(\w+)", re.M)
_MEMBER_RE = re.compile(
    r"UPROPERTY\s*\((?P<spec>(?:[^()]|\((?:[^()]|\([^()]*\))*\))*)\)\s*"
    r"(?:TObjectPtr\s*<\s*(?P<t1>\w+)\s*>|(?P<t2>\w+)\s*\*)\s*(?P<name>\w+)\s*;", re.S)

_CACHE = {}


def headers(src_root=None):
    """class name -> {'base', 'members': [{name,type,required,declared_in}], 'file'} (cached per root)."""
    root = src_root or SRC_ROOT
    if root in _CACHE:
        return _CACHE[root]
    classes = {}
    for path in glob.glob(os.path.join(root, "**", "*.h"), recursive=True):
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        decls = [(m.start(), m.group(1), m.group(2)) for m in _CLASS_RE.finditer(text)]
        if not decls:
            continue
        for _, cname, base in decls:
            classes.setdefault(cname, {"base": base, "members": [], "file": path})
        for m in _MEMBER_RE.finditer(text):
            spec = m.group("spec")
            if "BindWidget" not in spec:
                continue
            owner = None
            for pos, cname, _ in decls:
                if pos < m.start():
                    owner = cname
            if owner:
                classes[owner]["members"].append({
                    "name": m.group("name"),
                    "type": (m.group("t1") or m.group("t2")),
                    "required": "BindWidgetOptional" not in spec,
                    "declared_in": owner,
                })
    _CACHE[root] = classes
    return classes


# Roots of the engine's UserWidget family. A class deriving from any of these is a UserWidget.
USERWIDGET_ROOTS = {"UUserWidget", "UCommonUserWidget", "UCommonButtonBase", "UCommonActivatableWidget"}


def is_bare_native_userwidget(class_name, derives_from_userwidget):
    """True for a native (C++) UserWidget class used directly as a widget.

    A UserWidget gets its Slate content from a widget TREE, and only a Widget Blueprint has one. The bare native class
    builds an empty, hit-test-invisible spacer: nothing draws and clicks pass through, yet every simulated check
    (SimulateClick, a bound delegate) still "works" -- so the bug only shows with a real mouse. Blueprint-generated
    classes end in `_C`; native ones do not.
    """
    return bool(derives_from_userwidget) and not class_name.endswith("_C")


def derives_from_userwidget(cname, src_root=None):
    """Does the native class `cname` (e.g. 'UMOCommonButton') derive from the engine's UserWidget family?"""
    classes = headers(src_root)
    seen = set()
    while cname and cname not in seen:
        if cname in USERWIDGET_ROOTS:
            return True
        seen.add(cname)
        cname = classes[cname]["base"] if cname in classes else None
    return False


def native_name(parent):
    """'/Script/MOFramework.MOJoinGamePanel' | 'MOJoinGamePanel' | 'UMOJoinGamePanel' -> 'UMOJoinGamePanel'."""
    cname = parent.split(".")[-1].strip("'\"")
    return cname if cname.startswith("U") and cname[1:2].isupper() else "U" + cname


def contract(parent, src_root=None):
    """BindWidget members (required first, then declaration order) for a native class and its bases."""
    cname = native_name(parent)
    classes = headers(src_root)
    if cname not in classes:
        raise KeyError(f"class {cname} not found under {src_root or SRC_ROOT}")
    members, seen, cur = [], set(), cname
    while cur in classes:
        for m in classes[cur]["members"]:
            if m["name"] not in seen:
                members.append(m)
                seen.add(m["name"])
        cur = classes[cur]["base"]
    return members


# Native widget types that are plain engine widgets (usable directly as a spec "type").
ENGINE_WIDGETS = {
    "UTextBlock": "TextBlock", "UEditableTextBox": "EditableTextBox", "UEditableText": "EditableText",
    "UScrollBox": "ScrollBox", "UBorder": "Border", "UImage": "Image", "UProgressBar": "ProgressBar",
    "UCheckBox": "CheckBox", "UComboBoxString": "ComboBoxString", "USpacer": "Spacer", "USlider": "Slider",
    "UWidgetSwitcher": "WidgetSwitcher", "UVerticalBox": "VerticalBox", "UHorizontalBox": "HorizontalBox",
    "UOverlay": "Overlay", "UCanvasPanel": "CanvasPanel", "USizeBox": "SizeBox", "UGridPanel": "GridPanel",
    "UUniformGridPanel": "UniformGridPanel", "UScaleBox": "ScaleBox", "URichTextBlock": "RichTextBlock",
    "USpinBox": "SpinBox", "UButton": "Button", "UListView": "ListView", "UTileView": "TileView",
    "UTreeView": "TreeView", "UPanelWidget": "VerticalBox",
}
# Project widgets that have a ready-made reusable blueprint.
PROJECT_WIDGETS = {"UMOCommonButton": "MOButton"}


def _label(name):
    """'RefreshButton' -> 'Refresh', 'HostGameButton' -> 'Host Game'."""
    base = re.sub(r"Button$", "", name)
    return re.sub(r"(?<=[a-z])(?=[A-Z])", " ", base) or name


def scaffold(parent, asset, panel_size=(800, 800), ui_folder=None):
    """A starter spec for `asset` derived from the parent class's contract.

    Layout: SizeBox > Border > VerticalBox, with one entry per contract member (required first). Buttons
    are grouped into one trailing row. Members whose type has no ready-made widget blueprint get a type of
    'TODO:/Game/.../WBP_<Class>' that must be replaced (the build refuses to run with it).
    """
    folder = ui_folder or asset.rsplit("/", 1)[0]
    members = sorted(contract(parent), key=lambda m: not m["required"])
    fill = {"size_rule": "Fill", "value": 1}
    items, buttons = [], []
    for m in members:
        t = m["type"]
        if t in PROJECT_WIDGETS:
            buttons.append({"type": PROJECT_WIDGETS[t], "name": m["name"], "label": _label(m["name"]),
                            "slot": {"padding": 6, "size": fill, "horizontal_alignment": "Fill",
                                     "vertical_alignment": "Fill"}})
        elif t in ENGINE_WIDGETS:
            node = {"type": ENGINE_WIDGETS[t], "name": m["name"]}
            if t == "UTextBlock":
                node.update(text=_label(m["name"]), font_size=16)
            if t in ("UScrollBox",):
                node["slot"] = {"size": fill}
            items.append(node)
        else:
            items.append({"type": f"TODO:{folder}/WBP_{t[1:]}", "name": m["name"], "slot": {"size": fill}})
    if buttons:
        items.append({"type": "SizeBox", "name": "ButtonRowSizeBox", "height": 72, "slot": {"padding": [0, 16, 0, 0]},
                      "children": [{"type": "HorizontalBox", "name": "ButtonRow", "children": buttons}]})
    w, h = panel_size
    return {
        "asset": asset,
        "parent": parent if parent.startswith("/Script/") else "/Script/MOFramework." + parent.lstrip("U"),
        "root": {"type": "SizeBox", "name": "RootSizeBox", "width": w, "height": h, "children": [
            {"type": "Border", "name": "PanelBorder", "color": [0, 0, 0, 0.2], "props": {"padding": 24}, "children": [
                {"type": "VerticalBox", "name": "ContentBox", "children": items}]}]},
    }
