import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock


UE_PATH = Path(__file__).resolve().parents[1] / "ue.py"


def load_ue(env=None):
    spec = importlib.util.spec_from_file_location("mo57_ue_test", UE_PATH)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(os.environ, env or {}, clear=False):
        spec.loader.exec_module(module)
    return module


class ConfigurationTests(unittest.TestCase):
    def test_root_defaults_to_repository_containing_script(self):
        ue = load_ue()
        self.assertEqual(Path(ue.ROOT), UE_PATH.parents[1])
        self.assertEqual(Path(ue.UPROJECT), UE_PATH.parents[1] / "MO57.uproject")

    def test_environment_overrides_are_honored(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / "Custom.uproject").write_text(
                json.dumps({"EngineAssociation": "9.9"}), encoding="utf-8")
            bridge = root / "bridge"
            engine = root / "engine"
            ue = load_ue({
                "MO57_ROOT": str(root),
                "MO57_UPROJECT": str(root / "Custom.uproject"),
                "UE_ENGINE_ROOT": str(engine),
                "MO57_BRIDGE_DIR": str(bridge),
                "MO57_MCP_URL": "http://127.0.0.1:9000/mcp",
            })
            self.assertEqual(Path(ue.ROOT), root)
            self.assertEqual(Path(ue.UPROJECT), root / "Custom.uproject")
            self.assertEqual(Path(ue.ENGINE_ROOT), engine)
            self.assertEqual(Path(ue.TMP), bridge)
            self.assertEqual(ue.MCP_URL, "http://127.0.0.1:9000/mcp")

    def test_mcp_url_reads_repository_configuration(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / "MO57.uproject").write_text(
                json.dumps({"EngineAssociation": "5.8"}), encoding="utf-8")
            (root / ".mcp.json").write_text(json.dumps({
                "mcpServers": {"unreal-mcp": {"url": "http://127.0.0.1:8123/custom"}}
            }), encoding="utf-8")
            with mock.patch.dict(os.environ, {"MO57_ROOT": str(root)}, clear=True):
                ue = load_ue()
            self.assertEqual(ue.MCP_URL, "http://127.0.0.1:8123/custom")

    def test_session_files_are_namespaced_by_project_and_endpoint(self):
        with tempfile.TemporaryDirectory() as td:
            common = {"MO57_BRIDGE_DIR": td}
            first = load_ue({**common, "MO57_MCP_URL": "http://127.0.0.1:8000/mcp"})
            second = load_ue({**common, "MO57_MCP_URL": "http://127.0.0.1:8001/mcp"})
            self.assertNotEqual(first.SID_FILE, second.SID_FILE)
            self.assertNotEqual(first.MCP_BODY_FILE, second.MCP_BODY_FILE)


class MCPParsingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ue = load_ue()

    def test_plain_json_result(self):
        raw = json.dumps({"jsonrpc": "2.0", "id": 1, "result": {"tools": ["a"]}})
        self.assertEqual(self.ue._mcp_parse(raw), {"tools": ["a"]})

    def test_sse_uses_final_result_event(self):
        raw = ("event: message\n"
               "data: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/progress\"}\n\n"
               "event: message\n"
               "data: {\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{\"value\":7}}\n\n")
        self.assertEqual(self.ue._mcp_parse(raw), {"value": 7})

    def test_sse_joins_multiline_data_within_event(self):
        raw = ('event: message\n'
               'data: {"jsonrpc":"2.0","id":2,\n'
               'data: "result":{"value":8}}\n\n')
        self.assertEqual(self.ue._mcp_parse(raw), {"value": 8})

    def test_jsonrpc_error_is_preserved(self):
        raw = json.dumps({"jsonrpc": "2.0", "id": 2, "error": {
            "code": -32602, "message": "Invalid params", "data": {"field": "name"}
        }})
        error = self.ue._mcp_parse(raw)
        self.assertIsInstance(error, self.ue.MCPError)
        self.assertEqual(error["code"], -32602)
        self.assertEqual(error["data"], {"field": "name"})
        self.assertFalse(self.ue._is_stale_session(error))

    def test_unknown_session_is_retryable(self):
        raw = json.dumps({"jsonrpc": "2.0", "id": 2, "error": {
            "code": -32600,
            "message": "Unknown session id 'old' for 'tools/call'; client should reinitialize"
        }})
        self.assertTrue(self.ue._is_stale_session(self.ue._mcp_parse(raw)))

    def test_tool_error_is_preserved(self):
        raw = json.dumps({"jsonrpc": "2.0", "id": 2, "result": {
            "isError": True, "content": [{"type": "text", "text": "asset not found"}]
        }})
        error = self.ue._mcp_parse(raw)
        self.assertIsInstance(error, self.ue.MCPError)
        self.assertEqual(error["kind"], "tool")
        self.assertIn("asset not found", str(error))

    def test_nested_return_value_is_decoded(self):
        wrapped = json.dumps({"returnValue": json.dumps({"rows": 3})})
        raw = json.dumps({"jsonrpc": "2.0", "id": 2, "result": {
            "content": [{"type": "text", "text": wrapped}]
        }})
        self.assertEqual(self.ue._mcp_parse(raw), {"rows": 3})

    def test_malformed_payload_is_protocol_error(self):
        error = self.ue._mcp_parse("event: message\ndata: not-json\n\n")
        self.assertIsInstance(error, self.ue.MCPError)
        self.assertEqual(error["kind"], "protocol")

    def test_non_session_error_does_not_reconnect(self):
        with tempfile.TemporaryDirectory() as td:
            sid_file = Path(td) / "sid.txt"
            sid_file.write_text("active", encoding="utf-8")
            response = json.dumps({"jsonrpc": "2.0", "id": 2, "error": {
                "code": -32602, "message": "Invalid params"
            }})
            with mock.patch.object(self.ue, "SID_FILE", str(sid_file)), \
                    mock.patch.object(self.ue, "_curl", return_value=response), \
                    mock.patch.object(self.ue, "mcp_connect") as connect:
                result = self.ue.mcp_call("tools", "bad", {})
            self.assertIsInstance(result, self.ue.MCPError)
            connect.assert_not_called()


class DataTableSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ue = load_ue()

    def test_deep_merge_preserves_omitted_nested_fields(self):
        current = {"nutrition": {"calories": 100, "water": 20}, "weight": 1.0}
        patch = {"nutrition": {"water": 35}}
        self.assertEqual(self.ue._deep_merge(current, patch), {
            "nutrition": {"calories": 100, "water": 35}, "weight": 1.0
        })

    def test_deep_merge_does_not_mutate_source(self):
        current = {"nested": {"keep": True}}
        self.ue._deep_merge(current, {"nested": {"added": 1}})
        self.assertEqual(current, {"nested": {"keep": True}})


if __name__ == "__main__":
    unittest.main()
