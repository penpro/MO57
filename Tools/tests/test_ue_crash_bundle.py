"""Offline tests for Tools/ue_crash_bundle.py (CR1 reader/writer) and Tools/bugreport_receiver.py -- no editor, no game."""
import json
import os
import struct
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import zlib
from http.server import HTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import bugreport_receiver as rx  # noqa: E402
import crash_triage as ct  # noqa: E402
import ue_crash_bundle as cb  # noqa: E402

CONTEXT = (
    '<?xml version="1.0" encoding="UTF-8"?>\n<FGenericCrashContext>\n\t<RuntimeProperties>\n'
    "\t\t<CrashType>BugReport</CrashType>\n\t\t<ErrorMessage>a &lt; b &amp; c</ErrorMessage>\n\t\t<UserDescription>it broke</UserDescription>\n"
    "\t\t<MachineId>ABCDEF0123456789</MachineId>\n\t</RuntimeProperties>\n\t<GameData>\n"
    '\t\t<Field name="Build.Version">0.0.0.1</Field>\n\t\t<Field name="Report.Category">Gameplay</Field>\n'
    '\t\t<Field name="Report.Contact">me#1</Field>\n\t</GameData>\n</FGenericCrashContext>\n'
)


def sample_bundle():
    return cb.write_bundle("UECC-Windows-" + "A" * 32 + "_0000", [
        ("CrashContext.runtime-xml", CONTEXT.encode()),
        ("BugReport.txt", b"hello"),
        ("Screenshot.jpg", bytes(range(256)) * 4),
    ])


class RoundTripTests(unittest.TestCase):
    def test_round_trip_keeps_names_and_bytes(self):
        b = cb.parse_bundle(sample_bundle())
        self.assertEqual(b.directory, "UECC-Windows-" + "A" * 32 + "_0000")
        self.assertEqual(b.file_name, b.directory + ".uecrash")
        self.assertEqual(b.names(), ["CrashContext.runtime-xml", "BugReport.txt", "Screenshot.jpg"])
        self.assertEqual(b.get("Screenshot.jpg"), bytes(range(256)) * 4)
        self.assertIsNone(b.get("nope"))

    def test_header_layout_matches_the_crash_reporter(self):
        raw = zlib.decompress(sample_bundle())
        self.assertEqual(raw[:3], b"CR1")
        self.assertEqual(struct.unpack_from("<i", raw, 3)[0], 260)  # name fields are at least 260 wide
        # the size field sits after marker + two (4+260) name fields, i.e. at offset 531, and counts the whole stream
        self.assertEqual(struct.unpack_from("<i", raw, 531)[0], len(raw))
        self.assertEqual(struct.unpack_from("<i", raw, 535)[0], 3)

    def test_summary_of_a_bug_report(self):
        s = cb.summarise(cb.parse_bundle(sample_bundle()))
        self.assertEqual(s["kind"], "bugreport")
        self.assertEqual(s["title"], "a < b & c")  # entities are decoded
        self.assertEqual(s["category"], "Gameplay")
        self.assertEqual(s["contact"], "me#1")
        self.assertEqual(s["build"], "0.0.0.1")
        self.assertEqual(s["description"], "it broke")

    def test_a_crash_is_not_a_bug_report(self):
        crash = cb.write_bundle("d", [("CrashContext.runtime-xml", CONTEXT.replace("BugReport", "Crash").encode())])
        self.assertEqual(cb.summarise(cb.parse_bundle(crash))["kind"], "crash")
        self.assertFalse(cb.is_bug_report(cb.parse_bundle(crash)))


def crash_with(log: str = "", context: str = "") -> cb.Bundle:
    xml = "<FGenericCrashContext><RuntimeProperties>" + context + "</RuntimeProperties></FGenericCrashContext>"
    return cb.Bundle("d", "f", [("CrashContext.runtime-xml", xml.encode()), ("game.log", log.encode())])


class TriageTests(unittest.TestCase):
    """Which crashes are the agents' own, and what is worth reading in one (the Nov 5 2026 tarball: 19 uploads, all of them agents' test runs)."""

    def test_an_agents_instance_names_itself(self):
        line = r'LogInit: Command Line: -windowed -abslog=C:\Users\penum\AppData\Local\Temp\claude\inst_labcliff\game.log -game'
        self.assertEqual(ct.run_kind(crash_with(line)), "instance inst_labcliff")

    def test_an_editor_ue_py_started(self):
        self.assertEqual(ct.run_kind(crash_with("LogInit: Command Line: -ModelContextProtocolStartServer")), "editor")

    def test_a_test_commandlet(self):
        line = 'LogInit: Command Line: -unattended -nullrhi -ExecCmds="Automation RunTests MOFramework.WorldGen;Quit"'
        self.assertEqual(ct.run_kind(crash_with(line)), "tests")

    def test_anything_else_is_no_agent(self):
        self.assertEqual(ct.run_kind(crash_with("LogInit: Command Line: -windowed")), "")
        self.assertEqual(ct.run_kind(crash_with("no such line")), "")

    def test_named_frames_beat_unknown_ones(self):
        log = ("Ensure condition failed: OutResult.Normal.IsNormalized()\n"
               "[Callstack] 0x1 UnrealEditor-Engine.dll!UnknownFunction []\n"
               "[Callstack] 0x2 UnrealEditor-MOWorldLook.dll!MOVoxStamp::RockUnder() [D:\\x.cpp:522]\n")
        self.assertEqual(ct.crash_signature(crash_with(log))[4], ("MOVoxStamp::RockUnder()",))

    def test_a_fatal_crash_is_read_from_its_context(self):
        context = ("<ErrorMessage>Unhandled Exception: EXCEPTION_ACCESS_VIOLATION 0x0</ErrorMessage>"
                   "<CallStack>UnrealEditor_Engine\nUnrealEditor_MOFramework!RunBowShotTest() [D:\\y.cpp:441]\nkernel32</CallStack>")
        signature = ct.crash_signature(crash_with("nothing useful", context))
        self.assertIn("EXCEPTION_ACCESS_VIOLATION", signature[3])
        self.assertEqual(signature[4], ("RunBowShotTest() [D:\\y.cpp:441]",))


class MalformedInputTests(unittest.TestCase):
    """The endpoint is public: whatever arrives must fail cleanly, never crash the triage tool."""

    def test_not_zlib(self):
        with self.assertRaises(cb.BundleError):
            cb.parse_bundle(b"definitely not a bundle")

    def test_wrong_marker(self):
        with self.assertRaises(cb.BundleError):
            cb.parse_bundle(zlib.compress(b"XX1" + b"\0" * 600))

    def test_truncated_stream(self):
        raw = zlib.decompress(sample_bundle())
        with self.assertRaises(cb.BundleError):
            cb.parse_bundle(zlib.compress(raw[:700]))

    def test_size_field_that_lies(self):
        raw = bytearray(zlib.decompress(sample_bundle()))
        struct.pack_into("<i", raw, 531, len(raw) + 5)
        with self.assertRaises(cb.BundleError):
            cb.parse_bundle(zlib.compress(bytes(raw)))

    def test_absurd_file_count(self):
        raw = bytearray(zlib.decompress(sample_bundle()))
        struct.pack_into("<i", raw, 535, 2_000_000_000)
        with self.assertRaises(cb.BundleError):
            cb.parse_bundle(zlib.compress(bytes(raw)))

    def test_negative_name_length(self):
        raw = bytearray(zlib.decompress(sample_bundle()))
        struct.pack_into("<i", raw, 3, -1)
        with self.assertRaises(cb.BundleError):
            cb.parse_bundle(zlib.compress(bytes(raw)))

    def test_inflate_bomb_is_capped(self):
        old = cb.MAX_INFLATED
        cb.MAX_INFLATED = 1 << 20
        try:
            with self.assertRaises(cb.BundleError):
                cb.parse_bundle(zlib.compress(b"CR1" + b"\0" * (4 << 20)))
        finally:
            cb.MAX_INFLATED = old

    def test_hostile_xml_is_just_text(self):
        evil = '<?xml version="1.0"?><!DOCTYPE x [<!ENTITY a "&b;&b;"><!ENTITY b "&c;&c;">]><FGenericCrashContext><RuntimeProperties><ErrorMessage>&a;</ErrorMessage></RuntimeProperties></FGenericCrashContext>'
        b = cb.parse_bundle(cb.write_bundle("d", [("CrashContext.runtime-xml", evil.encode())]))
        self.assertEqual(cb.summarise(b)["title"], "&a;")  # not expanded: nothing parses entities


class ReceiverTests(unittest.TestCase):
    def _serve(self, status):
        self.tmp = tempfile.TemporaryDirectory()
        server = HTTPServer(("127.0.0.1", 0), rx.make_handler(self.tmp.name, status, 0.0))
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.shutdown)
        self.addCleanup(self.tmp.cleanup)
        return f"http://127.0.0.1:{server.server_address[1]}"

    def _post(self, url, body):
        req = urllib.request.Request(url, data=body, method="POST", headers={"Content-Type": "application/octet-stream"})
        return urllib.request.urlopen(req, timeout=5)

    def test_stores_body_and_query_like_the_site(self):
        base = self._serve(200)
        body = sample_bundle()
        r = self._post(base + "/datarouter/crashes/tok?AppID=CrashReporter&AppVersion=5.8.1-1%2B%2B%2BUE5&ReportKind=bugreport", body)
        self.assertEqual(r.status, 200)
        bins = list(Path(self.tmp.name).rglob("*.bin"))
        self.assertEqual(len(bins), 1)
        self.assertEqual(bins[0].read_bytes(), body)  # the body is stored untouched
        meta = json.loads(bins[0].with_suffix(".json").read_text())
        self.assertEqual(meta["query"]["ReportKind"], "bugreport")
        self.assertEqual(meta["query"]["AppVersion"], "5.8.1-1+++UE5")
        self.assertEqual(meta["contentLength"], len(body))
        self.assertEqual(cb.parse_bundle(bins[0].read_bytes()).directory, "UECC-Windows-" + "A" * 32 + "_0000")

    def test_failure_status_is_answered_but_the_post_is_still_recorded(self):
        base = self._serve(500)
        with self.assertRaises(urllib.error.HTTPError) as ctx:
            self._post(base + "/x?AppVersion=v", b"abc")
        self.assertEqual(ctx.exception.code, 500)
        self.assertEqual(len(list(Path(self.tmp.name).rglob("*.bin"))), 1)

    def test_version_cannot_escape_the_output_folder(self):
        base = self._serve(200)
        self._post(base + "/x?AppVersion=..%2F..%2Fescape", b"abc")
        out = Path(self.tmp.name).resolve()
        for p in out.rglob("*.bin"):
            self.assertTrue(str(p.resolve()).startswith(str(out)))

    def test_empty_post_is_rejected(self):
        base = self._serve(200)
        with self.assertRaises(urllib.error.HTTPError):
            self._post(base + "/x", b"")


if __name__ == "__main__":
    unittest.main()
