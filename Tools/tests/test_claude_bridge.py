"""The editor-side bridge (Content/Python/claude_bridge.py) without an editor: a stand-in `unreal` module, the Slate tick driven by hand."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock


BRIDGE_PATH = Path(__file__).resolve().parents[2] / "Content" / "Python" / "claude_bridge.py"


def load_bridge(bridge_dir):
    fake = types.SimpleNamespace(register_slate_post_tick_callback=lambda fn: object(),
                                 unregister_slate_post_tick_callback=lambda handle: None,
                                 log=lambda msg: None)
    spec = importlib.util.spec_from_file_location("mo57_claude_bridge_test", BRIDGE_PATH)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(sys.modules, {"unreal": fake}), mock.patch.dict(os.environ, {"MO57_BRIDGE_DIR": bridge_dir}):
        spec.loader.exec_module(module)
    return module


class BridgeNestingTests(unittest.TestCase):
    def test_a_command_queued_while_one_runs_waits_for_it(self):
        # Oct 26 2026: a command that opens a slow-task dialog pumps Slate, which ticks the bridge again; the next queued command ran
        # INSIDE the first (on a half-built MetaHuman character) and crashed the editor. Expected: first, end of first, second.
        with tempfile.TemporaryDirectory() as td:
            bridge = load_bridge(td)
            order = []

            def execute(line):
                order.append(line)
                if line == "first":
                    with open(bridge.CMD_PATH, "a", encoding="utf-8") as f:
                        f.write("second\n")
                    bridge._tick(1.0)   # the Slate pump inside a slow task
                    order.append("end of first")

            with open(bridge.CMD_PATH, "a", encoding="utf-8") as f:
                f.write("first\n")
            with mock.patch.object(bridge, "_execute", side_effect=execute):
                bridge._tick(1.0)
                bridge._tick(1.0)   # the next free tick
            self.assertEqual(order, ["first", "end of first", "second"])

    def test_a_failing_command_frees_the_bridge(self):
        with tempfile.TemporaryDirectory() as td:
            bridge = load_bridge(td)
            with open(bridge.CMD_PATH, "a", encoding="utf-8") as f:
                f.write("boom\n")
            with mock.patch.object(bridge, "_execute", side_effect=RuntimeError("boom")):
                bridge._tick(1.0)
            self.assertFalse(bridge._state["busy"])


if __name__ == "__main__":
    unittest.main()
