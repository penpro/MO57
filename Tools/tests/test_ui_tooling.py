"""Offline tests for the UI toolset (no editor needed).

Covers the pure-Python halves: the C++ contract parser (mo_ui_contract), the scaffold generator, the
`ue.py ui` path repair, and a static check that every spec in Content/Python/ui_specs satisfies the BindWidget
contract of its C++ parent -- the same rule `ue.py ui check` enforces on the live asset, caught before the editor.

Run: python -m unittest discover -s Tools/tests
"""
import importlib.util
import os
import runpy
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PY_DIR = REPO / "Content" / "Python"
sys.path.insert(0, str(PY_DIR))

import mo_ui_contract as uc  # noqa: E402  (path set up above)


def load_ue():
    spec = importlib.util.spec_from_file_location("mo57_ue_ui_test", REPO / "Tools" / "ue.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def names(members, required=None):
    return [m["name"] for m in members if required is None or m["required"] == required]


class ContractParserTests(unittest.TestCase):
    def test_join_panel_contract(self):
        c = uc.contract("MOJoinGamePanel")
        self.assertEqual(set(names(c, True)), {"SessionListWidget", "RefreshButton", "JoinButton"})
        self.assertEqual(set(names(c, False)), {"JoinStatusText", "BackButton"})
        by = {m["name"]: m["type"] for m in c}
        self.assertEqual(by["SessionListWidget"], "UMOSessionListWidget")
        self.assertEqual(by["RefreshButton"], "UMOCommonButton")

    def test_host_panel_contract(self):
        c = uc.contract("/Script/MOFramework.MOHostGamePanel")
        self.assertEqual(set(names(c, True)), {"SessionNameInputBox", "HostButton"})
        self.assertEqual({m["name"]: m["type"] for m in c}["SessionNameInputBox"], "UEditableTextBox")

    def test_contract_includes_inherited_members(self):
        c = uc.contract("UMOSessionListEntry")
        by = {m["name"]: m["declared_in"] for m in c}
        self.assertEqual(by["SessionNameText"], "UMOSessionListEntry")
        self.assertEqual(by["EntryButton"], "UMOListEntryBase")  # declared two levels up

    def test_required_members_listed_before_optional_in_scaffold_not_in_contract(self):
        # scaffold orders required first; contract keeps declaration order -- both must hold all members
        c = uc.contract("MOHostGamePanel")
        s = uc.scaffold("MOHostGamePanel", "/MOFramework/UI/X/WBP_T")
        flat = []

        def walk(n):
            flat.append(n["name"])
            for ch in n.get("children", []):
                walk(ch)
        walk(s["root"])
        for n in names(c):
            self.assertIn(n, flat)

    def test_unknown_class_raises(self):
        with self.assertRaises(KeyError):
            uc.contract("MONoSuchWidgetClass")

    def test_native_name_normalisation(self):
        for raw in ("MOJoinGamePanel", "UMOJoinGamePanel", "/Script/MOFramework.MOJoinGamePanel"):
            self.assertEqual(uc.native_name(raw), "UMOJoinGamePanel")

    def test_parser_reads_bindwidget_only_and_respects_optional(self):
        with tempfile.TemporaryDirectory() as td:
            Path(td, "W.h").write_text(textwrap.dedent("""
                class FOO_API UMyBase : public UUserWidget
                {
                    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Title;
                    UPROPERTY(EditAnywhere) TObjectPtr<UTextBlock> NotBound;
                };
                class FOO_API UMyPanel : public UMyBase
                {
                    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
                    TObjectPtr<UButton> Extra;
                };
            """), encoding="utf-8")
            c = uc.contract("UMyPanel", src_root=td)
            self.assertEqual([(m["name"], m["required"], m["declared_in"]) for m in c],
                             [("Extra", False, "UMyPanel"), ("Title", True, "UMyBase")])


class ScaffoldTests(unittest.TestCase):
    def test_label_derivation(self):
        self.assertEqual(uc._label("RefreshButton"), "Refresh")
        self.assertEqual(uc._label("HostGameButton"), "Host Game")

    def test_scaffold_groups_buttons_and_flags_unknown_widgets(self):
        s = uc.scaffold("MOJoinGamePanel", "/MOFramework/UI/MainMenu_UI/WBP_Join")
        self.assertEqual(s["parent"], "/Script/MOFramework.MOJoinGamePanel")
        content = s["root"]["children"][0]["children"][0]["children"]
        row = content[-1]
        self.assertEqual(row["name"], "ButtonRowSizeBox")
        row_names = [b["name"] for b in row["children"][0]["children"]]
        self.assertEqual(set(row_names), {"RefreshButton", "JoinButton", "BackButton"})
        todo = [n for n in content if n["type"].startswith("TODO:")]
        self.assertEqual([n["name"] for n in todo], ["SessionListWidget"])  # no ready-made blueprint for it


class PathRepairTests(unittest.TestCase):
    def test_git_bash_rewrite_is_undone(self):
        ue = load_ue()
        self.assertEqual(ue._unmsys("C:/Program Files/Git/MOFramework/UI/WBP_X"), "/MOFramework/UI/WBP_X")
        self.assertEqual(ue._unmsys("C:\\Program Files\\Git\\Game\\UI\\WBP_X"), "/Game/UI/WBP_X")

    def test_ordinary_paths_untouched(self):
        ue = load_ue()
        self.assertEqual(ue._unmsys("/MOFramework/UI/WBP_X"), "/MOFramework/UI/WBP_X")
        self.assertEqual(ue._unmsys("D:/UEProjects/MO57/spec.py"), "D:/UEProjects/MO57/spec.py")
        self.assertEqual(ue._unmsys(""), "")


class SpecContractTests(unittest.TestCase):
    """Every spec that builds a whole tree must contain every required BindWidget of its parent, by name and
    (where the node type is a native or aliased widget) by type."""

    NATIVE_BY_NODE_TYPE = {v: k for k, v in {**uc.ENGINE_WIDGETS, **uc.PROJECT_WIDGETS}.items()}
    COMPATIBLE = {"UPanelWidget": {"UVerticalBox", "UHorizontalBox", "UGridPanel", "UOverlay", "UCanvasPanel",
                                   "UUniformGridPanel", "UScrollBox", "UPanelWidget"}}

    @staticmethod
    def walk(node, out):
        out[node["name"]] = node["type"]
        for ch in node.get("children", []):
            SpecContractTests.walk(ch, out)

    def specs(self):
        for path in sorted((PY_DIR / "ui_specs").glob("*.py")):
            for spec in runpy.run_path(str(path))["SPECS"]:
                yield path.name, spec

    def test_every_rooted_spec_satisfies_its_parent_contract(self):
        checked = 0
        for fname, spec in self.specs():
            if "root" not in spec:
                continue
            tree = {}
            self.walk(spec["root"], tree)
            for m in uc.contract(spec["parent"]):
                if not m["required"] and m["name"] not in tree:
                    continue
                where = f"{fname}:{spec['asset'].rsplit('/', 1)[-1]}.{m['name']}"
                if m["required"]:
                    self.assertIn(m["name"], tree, f"{where} required but missing")
                node_type = tree[m["name"]]
                native = self.NATIVE_BY_NODE_TYPE.get(node_type)
                if native:  # path-typed nodes (Widget Blueprints) are resolved only inside the editor
                    ok = native == m["type"] or native in self.COMPATIBLE.get(m["type"], set())
                    self.assertTrue(ok, f"{where}: spec uses {node_type} ({native}), contract wants {m['type']}")
            checked += 1
        self.assertGreater(checked, 0, "no rooted specs found -- ui_specs moved?")

    def test_no_unresolved_todo_types_in_committed_specs(self):
        for fname, spec in self.specs():
            tree = {}
            if "root" in spec:
                self.walk(spec["root"], tree)
            for att in spec.get("attach", []):
                self.walk(att["node"], tree)
            for name, t in tree.items():
                self.assertFalse(t.startswith("TODO:"), f"{fname}: {name} still has placeholder type {t}")

    def test_widget_names_are_unique_within_each_spec(self):
        for fname, spec in self.specs():
            seen = []

            def walk(n):
                seen.append(n["name"])
                for ch in n.get("children", []):
                    walk(ch)
            if "root" in spec:
                walk(spec["root"])
            for att in spec.get("attach", []):
                walk(att["node"])
            dupes = {n for n in seen if seen.count(n) > 1}
            self.assertFalse(dupes, f"{fname}: duplicate widget names {dupes} (UMG requires unique names)")


if __name__ == "__main__":
    unittest.main()
