"""mo_ui - spec-driven Widget Blueprint toolset (runs inside the editor's Python).

WHY THIS EXISTS
---------------
Building UMG by hand (click, screenshot, analyze, click) is the slowest possible loop. Everything a designer
does in the Designer panel is reachable programmatically, so this module turns UI work into data: describe the
widget tree once (a "spec"), apply it, check it against the C++ contract, look at it. The host-side entry point
is `python Tools/ue.py ui <verb>`; full guide: Docs/UI_TOOLING.md.

VERBS (all return text; see cli())
    list [folder] [parent]   Widget Blueprints and their native parents
    contract <Class>         BindWidget requirements of a C++ widget class (parsed from the headers)
    scaffold <Class> <asset> starter spec generated from that contract
    check <asset>            does the WBP satisfy its parent's contract? (names, types, Is Variable)
    dump <asset>             the live tree as a spec-shaped dict (non-default properties only)
    build <spec file>        create/update Widget Blueprints from SPECS (idempotent, additive)
    compile <asset>          compile + save
    preview <asset>          add the widget to the running PIE viewport (preview_clear removes them)
    close_tabs               close every asset editor tab (the level viewport must be visible for PIE shots)

SPEC FORMAT
-----------
    {"asset":  "/MOFramework/UI/MainMenu_UI/WBP_JoinGamePanel",     # package path of the WBP
     "parent": "/Script/MOFramework.MOJoinGamePanel",               # C++ parent (used only on create)
     "root":   NODE,                                                # optional
     "attach": [{"parent": "FocusWindowSwitcher", "node": NODE}],   # optional: nodes under EXISTING widgets
     "class_defaults": {"entry_widget_class": "/Game/.../WBP_X"}}   # optional: CDO properties

    NODE = {"type":  "VerticalBox" | "MOButton" | "/Game/Path/WBP_X" | "/Script/Mod.Class",
            "name":  "RefreshButton",           # EXACT widget name (BindWidget names are contracts)
            "var":   True,                      # Is Variable (forced on for every named contract widget)
            "props": {"button_label": "Refresh"},   # any editor property of the widget
            "slot":  {"padding": 8, "size": {"size_rule": "Fill", "value": 1}},  # properties of its slot
            "children": [NODE, ...],
            # sugar: "text", "font_size", "color", "label", "width", "height"
            }
    Values: Margin from number / [h,v] / [l,t,r,b]; LinearColor from [r,g,b(,a)] or "#RRGGBB(AA)"; enums by
    friendly name ("Fill", "HitTestInvisible"); structs as dicts (only the keys you give change); class and
    asset references as package paths.

LIMITS (found by use): the editor Python API cannot remove or reparent a widget, so `build` is additive: it
creates what is missing and updates what exists, never deletes.
"""
import importlib
import json
import pprint
import re
import runpy

import unreal

import mo_ui_contract

EUL = unreal.EditorUtilityLibrary
MOU = unreal.MOWidgetEditorUtils
EAL = unreal.EditorAssetLibrary

contract = mo_ui_contract.contract

# Short names usable as a node "type": the project's reusable widget blueprints.
ALIASES = {
    "MOButton": "/MOFramework/UI/WBP_MOCommonButton",
    "MOListEntry": "/MOFramework/UI/CommonUI/WBP_MOListEntry",
    "MOScrollList": "/MOFramework/UI/CommonUI/WBP_MOScrollList",
}


class UIError(Exception):
    pass


# ----------------------------------------------------------------------------- helpers

def _snake(name):
    if "_" in name or name.islower():
        return name
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    return s[1:] if s.startswith("b_") and len(name) > 2 and name[1].isupper() else s


def _asset_name(path):
    return path.rsplit("/", 1)[-1].split(".")[0]


def resolve_class(spec, as_parent=False):
    """Class object from an alias, engine widget name, native class path, or BP widget asset path.

    `as_parent=True`: the class is the C++ PARENT of a Widget Blueprint about to be created, which is exactly what a native UserWidget class is for;
    only a native class used as a NODE in a tree is the bare-spacer trap below."""
    if spec.startswith("TODO:"):
        raise UIError(f"spec still contains a placeholder type '{spec}': create that widget blueprint "
                      f"(or point the node at an existing one) before building")
    spec = ALIASES.get(spec, spec)
    cls = None
    if spec.startswith("/Script/"):
        cls = unreal.load_class(None, spec)
    elif spec.startswith("/"):
        tail = spec.rsplit("/", 1)[-1]
        path = spec if "." in tail else f"{spec}.{tail}_C"
        cls = unreal.load_class(None, path)
    else:
        py = getattr(unreal, spec, None)
        cls = py.static_class() if py is not None else None
    if cls is None:
        raise UIError(f"cannot resolve widget class '{spec}'")
    cdo = unreal.get_default_object(cls)
    if not as_parent and mo_ui_contract.is_bare_native_userwidget(cls.get_name(), isinstance(cdo, unreal.UserWidget)):
        raise UIError(f"'{spec}' is a native UserWidget with no Widget Blueprint: it has no widget tree, so it renders "
                      f"as an empty spacer that real mouse clicks pass straight through (simulated clicks still "
                      f"'work', which hides it). Use a Widget Blueprint instead -- type 'MOButton' for buttons.")
    return cls


def _norm(s):
    return s.replace("_", "").replace(" ", "").lower()


def _enum_value(enum_cls, text):
    """Match 'H_ALIGN_FILL', 'Fill', 'HitTestInvisible', 'hit test invisible' ... to an enum member."""
    names = [n for n in dir(enum_cls) if n.isupper() or (n[:1].isupper() and "_" in n)]
    want = _norm(text)
    exact = [n for n in names if _norm(n) == want]
    if exact:
        return getattr(enum_cls, exact[0])
    tail = [n for n in names if _norm(n).endswith(want)]
    if len(tail) == 1:
        return getattr(enum_cls, tail[0])
    raise UIError(f"'{text}' is not a value of {enum_cls.__name__} (have: {', '.join(names)})")


def _color(v):
    if isinstance(v, str):
        h = v.lstrip("#")
        r, g, b = (int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
        a = int(h[6:8], 16) / 255.0 if len(h) >= 8 else 1.0
        return unreal.LinearColor(r, g, b, a)
    if len(v) == 3:
        return unreal.LinearColor(v[0], v[1], v[2], 1.0)
    return unreal.LinearColor(*v)


def _margin(v):
    if isinstance(v, (int, float)):
        return unreal.Margin(v, v, v, v)
    if len(v) == 2:
        return unreal.Margin(v[0], v[1], v[0], v[1])
    return unreal.Margin(*v)


def _coerce(name, cur, value):
    """Convert a spec value (json-ish) to what set_editor_property wants, guided by the current value."""
    if isinstance(value, dict):
        if cur is None:
            raise UIError(f"cannot build struct for '{name}' (no current value to copy)")
        for k, v in value.items():
            k = _snake(k)
            cur.set_editor_property(k, _coerce(k, cur.get_editor_property(k), v))
        return cur
    if isinstance(cur, unreal.LinearColor) and isinstance(value, (list, tuple, str)):
        return _color(value)
    if isinstance(cur, unreal.Margin) and isinstance(value, (int, float, list, tuple)):
        return _margin(value)
    if isinstance(cur, unreal.Vector2D) and isinstance(value, (list, tuple)):
        return unreal.Vector2D(*value)
    if isinstance(cur, unreal.EnumBase) and isinstance(value, str):
        return _enum_value(type(cur), value)
    if isinstance(value, str) and value.startswith("/"):
        if value.startswith("/Script/") or value.endswith("_C") or name.endswith("_class"):
            return resolve_class(value)
        asset = EAL.load_asset(value)
        if asset is None:
            raise UIError(f"asset not found for '{name}': {value}")
        return asset
    return value


_SIZEBOX_OVERRIDES = {"width_override", "height_override", "min_desired_width", "min_desired_height", "max_desired_width", "max_desired_height",
                      "min_aspect_ratio", "max_aspect_ratio"}


def set_props(obj, props, what=""):
    for key, value in props.items():
        name = _snake(key)
        try:
            cur = obj.get_editor_property(name)
        except Exception as exc:
            raise UIError(f"{what}: no property '{name}' on {obj.get_class().get_name()} ({str(exc)[:60]})")
        try:
            coerced = _coerce(name, cur, value)
            obj.set_editor_property(name, coerced)
            # A SizeBox override (width/height/min/max/aspect) takes effect only while its enable flag (bOverride_*) is on, and set_editor_property
            # writes the VALUE only. The flag is not reachable from Python; the setter UFUNCTION (SetHeightOverride ...) turns it on. Without this a
            # spec's `height: 100` is stored but ignored at run time (the bug report form's boxes collapsed to one line).
            if isinstance(obj, unreal.SizeBox) and name in _SIZEBOX_OVERRIDES:
                getattr(obj, "set_" + name)(coerced)
        except UIError:
            raise
        except Exception as exc:
            raise UIError(f"{what}: setting '{name}' = {value!r} failed: {str(exc)[:120]}")


def _to_jsonable(v, depth=0):
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    if isinstance(v, unreal.EnumBase):
        return str(v).split(".")[-1].split(":")[0].replace("<", "").strip()
    if isinstance(v, (unreal.Text, unreal.Name)):
        return str(v)
    if isinstance(v, unreal.LinearColor):
        return [round(v.r, 4), round(v.g, 4), round(v.b, 4), round(v.a, 4)]
    if isinstance(v, (unreal.Class, unreal.Object)):
        return v.get_path_name()
    if hasattr(v, "get_editor_property") and depth < 6:
        out = {}
        for n in dir(v):
            if n.startswith("_") or n[:1].isupper():
                continue
            try:
                member = v.get_editor_property(n)
            except Exception:
                continue
            out[n] = _to_jsonable(member, depth + 1)
        return out
    return str(v)


_SKIP_PROPS = {"slot", "parent", "cached_widget", "display_label", "navigation", "is_variable", "designer_flags",
               "widget_generated_class", "category", "pixel_snapping"}
_SAME = object()


def _json_diff(v, d):
    """Smallest json describing how v differs from default d (structs recurse); _SAME if equal."""
    if isinstance(v, dict) and isinstance(d, dict):
        out = {}
        for k, val in v.items():
            sub = _json_diff(val, d.get(k))
            if sub is not _SAME:
                out[k] = sub
        return out if out else _SAME
    if isinstance(v, str) and v.startswith("<Struct"):
        return _SAME  # unserializable leftovers are never layout
    return _SAME if json.dumps(v, sort_keys=True, default=str) == json.dumps(d, sort_keys=True, default=str) else v


def _nondefault(obj):
    """{prop: value} for properties that differ from the class default object."""
    cdo = unreal.get_default_object(obj.get_class())
    res = {}
    for n in dir(obj):
        if n.startswith("_") or n[:1].isupper() or n in _SKIP_PROPS:
            continue
        try:
            v = obj.get_editor_property(n)
            d = cdo.get_editor_property(n)
        except Exception:
            continue
        jv, jd = _to_jsonable(v), _to_jsonable(d)
        if isinstance(jv, str) and jv.startswith("<") and "delegate" in jv.lower():
            continue  # event dispatchers are not layout
        delta = _json_diff(jv, jd)
        if delta is not _SAME:
            res[n] = delta
    return res


# ----------------------------------------------------------------------------- registry queries

def _registry():
    return unreal.AssetRegistryHelpers.get_asset_registry()


def parent_of(asset):
    """Native parent class path of a Widget Blueprint (walks up through BP parents)."""
    ar = _registry()
    path = asset
    for _ in range(8):
        data = ar.get_asset_by_object_path(f"{path}.{_asset_name(path)}")
        tag = str(data.get_tag_value("ParentClass") or "")
        tag = tag.split("'")[-2] if "'" in tag else tag
        if not tag:
            return None
        if tag.startswith("/Script/"):
            return tag
        path = tag.rsplit(".", 1)[0]
    return None


def list_wbps(folder="", parent=""):
    assets = _registry().get_assets_by_class(unreal.TopLevelAssetPath("/Script/UMGEditor", "WidgetBlueprint"), True)
    rows = []
    for a in assets:
        p = str(a.package_name)
        if folder and not p.startswith(folder):
            continue
        pc = str(a.get_tag_value("ParentClass") or "")
        short = pc.split(".")[-1].strip("'\"")
        if parent and parent.lower() not in short.lower():
            continue
        rows.append(f"{p}   [{short}]")
    return "\n".join(sorted(rows) + [f"({len(rows)} widget blueprints)"])


# ----------------------------------------------------------------------------- check / dump

def check(asset):
    """Report whether the Widget Blueprint satisfies its parent's BindWidget contract."""
    wbp = EAL.load_asset(asset)
    if wbp is None:
        raise UIError(f"asset not found: {asset}")
    parent = parent_of(asset)
    lines = [f"{asset}  (parent {parent})"]
    if parent is None:
        return "\n".join(lines + ["  cannot determine parent class"])
    names = set(str(n) for n in MOU.get_all_widget_names(wbp))
    bad = 0
    for m in contract(parent):
        tag = "required" if m["required"] else "optional"
        if m["name"] not in names:
            if m["required"]:
                bad += 1
            lines.append(f"  {'MISSING' if m['required'] else 'absent '} {tag:8} {m['name']}: {m['type']}  [{m['declared_in']}]")
            continue
        w = EUL.find_source_widget_by_name(wbp, m["name"])
        native = m["type"][1:] if m["type"][:1] in "UA" else m["type"]
        issues = []
        py = getattr(unreal, native, None)
        if py is not None and not isinstance(w, py):
            issues.append(f"wrong type {type(w).__name__}, need {native}")
        if not MOU.get_widget_is_variable(w):
            issues.append("Is Variable is OFF")
        if issues:
            bad += 1
        lines.append(f"  {'BAD    ' if issues else 'ok     '} {tag:8} {m['name']}: {m['type']}" + (("  <- " + "; ".join(issues)) if issues else ""))
    # Any widget in the tree, contract member or not: a bare native UserWidget draws nothing and eats no clicks.
    for n in sorted(names):
        w = EUL.find_source_widget_by_name(wbp, n)
        if w is not None and mo_ui_contract.is_bare_native_userwidget(w.get_class().get_name(),
                                                                      isinstance(w, unreal.UserWidget)):
            bad += 1
            lines.append(f"  BAD    bare native UserWidget '{n}' ({w.get_class().get_name()}): no widget tree, so it is "
                         f"invisible to the mouse -- replace it with a Widget Blueprint (see `ui remove`)")
    lines.append("RESULT: " + ("contract satisfied" if bad == 0 else f"{bad} problem(s)"))
    return "\n".join(lines)


def dump(asset, with_props=True):
    wbp = EAL.load_asset(asset)
    if wbp is None:
        raise UIError(f"asset not found: {asset}")
    infos = list(MOU.get_all_widget_layout_info(wbp))
    nodes, root = {}, None
    for i in infos:
        w = EUL.find_source_widget_by_name(wbp, i.name)
        cls = w.get_class()
        cpath = cls.get_path_name()
        typ = cls.get_name() if cpath.startswith("/Script/") else cpath.rsplit(".", 1)[0]
        for alias, ap in ALIASES.items():
            if ap == typ:
                typ = alias
        node = {"type": typ, "name": str(i.name)}
        if i.is_variable:
            node["var"] = True
        if with_props:
            props = _nondefault(w)
            if props:
                node["props"] = props
            slot = w.get_editor_property("slot") if i.parent_name else None
            if slot is not None:
                sp = {k: v for k, v in _nondefault(slot).items() if k not in ("parent", "content")}
                if sp:
                    node["slot"] = sp
        node["children"] = []
        nodes[str(i.name)] = node
        if i.parent_name:
            nodes[str(i.parent_name)]["children"].append(node)
        else:
            root = node
    return {"asset": asset, "parent": parent_of(asset), "root": root}


# ----------------------------------------------------------------------------- build

def ensure_wbp(asset, parent):
    if EAL.does_asset_exist(asset):
        return EAL.load_asset(asset), False
    folder, name = asset.rsplit("/", 1)
    if not parent:
        raise UIError(f"{asset} does not exist and the spec has no 'parent'")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", resolve_class(parent, as_parent=True))
    wbp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.WidgetBlueprint, factory)
    if wbp is None:
        raise UIError(f"failed to create {asset}")
    return wbp, True


def _sugar(node, props):
    typ = node.get("type", "")
    if "text" in node:
        props["hint_text" if typ == "EditableTextBox" else "text"] = node["text"]
    if "label" in node:
        props["button_label"] = node["label"]
    if "font_size" in node:
        props.setdefault("font", {})["size"] = node["font_size"]
    if "color" in node:
        if typ == "TextBlock":
            props["color_and_opacity"] = {"specified_color": node["color"]}
        else:
            props["brush_color"] = node["color"]
    if "width" in node:
        props["width_override"] = node["width"]
    if "height" in node:
        props["height_override"] = node["height"]
    return props


def _apply(wbp, node, parent_name, report):
    name = node["name"]
    w = EUL.find_source_widget_by_name(wbp, name)
    if w is None:
        cls = resolve_class(node["type"])
        w = EUL.add_source_widget(wbp, cls, name, parent_name or "None")
        if w is None:
            raise UIError(f"add_source_widget failed for {name} ({node['type']}) under {parent_name}")
        report.append(f"+ {name}: {node['type']}" + (f" -> {parent_name}" if parent_name else " (root)"))
    else:
        report.append(f"= {name}: {w.get_class().get_name()} (exists)")
    props = _sugar(node, dict(node.get("props", {})))
    if props:
        set_props(w, props, what=name)
    if node.get("slot"):
        slot = w.get_editor_property("slot")
        if slot is None:
            raise UIError(f"{name} has no slot (is it the root?) but the spec sets slot properties")
        set_props(slot, node["slot"], what=f"{name}.slot")
    if "var" in node:
        MOU.set_widget_is_variable_by_name(wbp, name, bool(node["var"]))
    for child in node.get("children", []):
        _apply(wbp, child, name, report)


def _find_todo(node, path=""):
    if not isinstance(node, dict):
        return None
    if str(node.get("type", "")).startswith("TODO:"):
        return f"{path}/{node.get('name')}: {node['type']}"
    for c in node.get("children", []):
        r = _find_todo(c, f"{path}/{node.get('name')}")
        if r:
            return r
    return None


def build(spec, save=True):
    report = []
    asset = spec["asset"]
    todo = _find_todo(spec.get("root")) or next((t for a in spec.get("attach", []) for t in [_find_todo(a["node"])] if t), None)
    if todo:
        raise UIError(f"{asset}: placeholder type left in spec -> {todo}")
    wbp, created = ensure_wbp(asset, spec.get("parent"))
    report.append(("CREATED " if created else "UPDATING ") + asset)
    root = spec.get("root")
    if root:
        existing_root = str(MOU.get_root_widget_name(wbp))
        if existing_root and existing_root != root["name"]:
            raise UIError(f"{asset} already has root '{existing_root}', spec wants '{root['name']}' "
                          f"(the Python API cannot remove widgets; rename the spec root or clear the asset in the editor)")
        _apply(wbp, root, None, report)
    # "attach": add nodes under widgets that already exist, addressed by name (no need to restate the tree)
    for att in spec.get("attach", []):
        if EUL.find_source_widget_by_name(wbp, att["parent"]) is None:
            raise UIError(f"attach: parent widget '{att['parent']}' not found in {asset}")
        _apply(wbp, att["node"], att["parent"], report)
    # contract: every named contract widget must be a variable
    parent = parent_of(asset)
    if parent:
        for m in contract(parent):
            if EUL.find_source_widget_by_name(wbp, m["name"]) is not None:
                MOU.set_widget_is_variable_by_name(wbp, m["name"], True)
    unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    # class defaults (e.g. a list widget's EntryWidgetClass) live on the generated class's CDO
    if spec.get("class_defaults"):
        cdo = unreal.get_default_object(wbp.generated_class())
        set_props(cdo, spec["class_defaults"], what=f"{_asset_name(asset)} class defaults")
        report.append("~ class defaults: " + ", ".join(spec["class_defaults"]))
        unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    if save:
        EAL.save_loaded_asset(wbp)
    report.append(check(asset))
    return "\n".join(report)


def build_file(path):
    ns = runpy.run_path(path)
    specs = ns.get("SPECS")
    if not specs:
        raise UIError(f"{path} defines no SPECS")
    return "\n\n".join(build(s) for s in specs)


# ----------------------------------------------------------------------------- editor / PIE helpers

def _game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def preview(asset, z_order=50):
    """Create the widget in the running PIE world and add it to the viewport."""
    world = _game_world()
    if world is None:
        raise UIError("no PIE world: start PIE first (python Tools/ue.py ui menu)")
    cls = resolve_class(asset)
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    w = unreal.WidgetBlueprintLibrary.create(world, cls, pc)  # (was unreal.WidgetLibrary: no such class in 5.8)
    w.add_to_viewport(z_order)
    store = getattr(unreal, "_mo_ui_previews", None)
    if store is None:
        store = unreal._mo_ui_previews = []
    store.append(w)
    return f"previewing {asset} (z={z_order}); {len(store)} preview widget(s) live"


def preview_clear():
    store = getattr(unreal, "_mo_ui_previews", [])
    n = 0
    for w in store:
        try:
            w.remove_from_parent()
            n += 1
        except Exception:
            pass
    store.clear()
    return f"removed {n} preview widget(s)"


def remove(asset, widget):
    """Delete a widget and its subtree (MOWidgetEditorUtils.RemoveWidget), then compile + save."""
    wbp = EAL.load_asset(asset)
    if wbp is None:
        raise UIError(f"asset not found: {asset}")
    if not MOU.remove_widget(wbp, widget):
        raise UIError(f"widget '{widget}' not found in {asset}")
    unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    EAL.save_loaded_asset(wbp)
    return f"removed '{widget}' (and its children) from {asset}; compiled and saved"


def close_tabs():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        return "NOT closed: asset editors cannot be closed during PIE (end PIE first)"
    # The subsystem has no "close everything" verb, but an asset being edited is necessarily loaded, so
    # walk the loaded assets (cheap: registry lookup, no disk) and close any editor open on them.
    eas = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    skip = {"World", "MapBuildDataRegistry"}
    n = 0
    for d in unreal.AssetRegistryHelpers.get_asset_registry().get_all_assets():
        if d.is_asset_loaded() and str(d.asset_class_path.asset_name) not in skip:
            eas.close_all_editors_for_asset(d.get_asset())
            n += 1
    return f"closed asset editor tabs (checked {n} loaded assets)"


# ----------------------------------------------------------------------------- CLI

def cli(args_json):
    """Entry point for the host: returns text. args = {"verb": ..., ...}."""
    a = json.loads(args_json)
    verb = a["verb"]
    try:
        if verb == "check":
            return check(a["asset"])
        if verb == "contract":
            return "\n".join(f"{'REQ' if m['required'] else 'opt'} {m['name']}: {m['type']}  [{m['declared_in']}]"
                             for m in contract(a["parent"]))
        if verb == "scaffold":
            spec = mo_ui_contract.scaffold(a["parent"], a["asset"])
            # pformat, not json.dumps: the output must be a valid Python file (JSON's true/null are not)
            return "SPECS = [\n" + pprint.pformat(spec, width=110, sort_dicts=False) + "\n]"
        if verb == "list":
            return list_wbps(a.get("folder", ""), a.get("parent", ""))
        if verb == "dump":
            return json.dumps(dump(a["asset"], a.get("props", True)), indent=1)
        if verb == "build":
            return build_file(a["spec"])
        if verb == "remove":
            return remove(a["asset"], a["widget"])
        if verb == "compile":
            wbp = EAL.load_asset(a["asset"])
            unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
            EAL.save_loaded_asset(wbp)
            return "compiled and saved " + a["asset"]
        if verb == "preview":
            return preview(a["asset"], a.get("z", 50))
        if verb == "preview_clear":
            return preview_clear()
        if verb == "close_tabs":
            return close_tabs()
    except UIError as exc:
        return f"UI ERROR: {exc}"
    except KeyError as exc:
        return f"UI ERROR: {exc}"
    raise UIError(f"unknown verb {verb}")
