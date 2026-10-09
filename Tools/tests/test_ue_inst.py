"""Offline tests for Tools/ue_inst.py (multi-instance harness) -- the pure parts only; no editor, no processes."""
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ue_inst as ui  # noqa: E402


class LaunchArgsTests(unittest.TestCase):
    def test_standalone_game_with_own_log_and_flush(self):
        a = ui.build_launch_args("D:/P/MO57.uproject", "D:/log/game.log")
        self.assertEqual(a[0], "D:/P/MO57.uproject")
        for flag in ("-game", "-windowed", "-nosplash", "-nosound", "-forcelogflush", "-abslog=D:/log/game.log"):
            self.assertIn(flag, a)
        self.assertNotIn("-log", a)  # no console window cluttering the desktop

    def test_nosteam_only_when_asked(self):
        self.assertNotIn("-NoSteam", ui.build_launch_args("p", "l"))
        self.assertIn("-NoSteam", ui.build_launch_args("p", "l", nosteam=True))

    def test_position_resolution_and_extra_args(self):
        a = ui.build_launch_args("p", "l", pos=(680, 20), res=(800, 450), extra=["-ExecCmds=x"])
        self.assertIn("-WinX=680", a)
        self.assertIn("-WinY=20", a)
        self.assertIn("-ResX=800", a)
        self.assertIn("-ResY=450", a)
        self.assertEqual(a[-1], "-ExecCmds=x")


class NamingTests(unittest.TestCase):
    def test_instance_dirs_are_isolated_per_name(self):
        self.assertNotEqual(ui.inst_dir("host"), ui.inst_dir("client"))
        self.assertTrue(ui.game_log("host").endswith(os.path.join("inst_host", "game.log")))

    def test_bad_names_are_rejected(self):
        for bad in ("", "../x", "a b", "a/b", "a\\b", "x;y"):
            with self.assertRaises(ValueError, msg=bad):
                ui.inst_dir(bad)

    def test_env_isolates_bridge_and_log(self):
        env = ui.env_for("host")
        self.assertEqual(env["MO57_BRIDGE_DIR"], ui.inst_dir("host"))
        self.assertEqual(env["MO57_GAMELOG"], ui.game_log("host"))


class ParsingTests(unittest.TestCase):
    def test_state_line(self):
        line = "LogMOFramework: Warning: [MOQUERY] STATE netmode=ListenServer level=TestMap inGame=NO(menu?) pawn=none class=-"
        self.assertEqual(ui.STATE_RE.search(line).groups(), ("ListenServer", "TestMap", "NO(menu?)", "none"))

    def test_found_sessions_line(self):
        self.assertEqual(ui.FOUND_RE.search("[MOSession] FindSessions complete: 3 result(s)").group(1), "3")

    def test_in_world_on_override_map_ignores_the_pawn(self):
        st = ("ListenServer", "TestMap", "NO(menu?)", "none")
        self.assertTrue(ui.in_world(st, "ListenServer", "/Game/Penumbra/Maps/TestMap"))
        self.assertFalse(ui.in_world(st, "Client", "/Game/Penumbra/Maps/TestMap"))
        self.assertFalse(ui.in_world(("ListenServer", "LoadingLevel", "NO", "none"), "ListenServer",
                                     "/Game/Penumbra/Maps/TestMap"))

    def test_in_world_on_real_map_requires_the_pawn(self):
        self.assertFalse(ui.in_world(("ListenServer", "MOPCGScattering", "NO(menu?)", "none"), "ListenServer", ""))
        self.assertTrue(ui.in_world(("ListenServer", "MOPCGScattering", "YES", "BP_Pawn_0"), "ListenServer", ""))
        self.assertFalse(ui.in_world(None, "ListenServer", ""))

    def test_weather_bridge_lines_parse(self):
        follow = ("LogMOFramework: Warning: [MOWorldSync] client following the host's weather preset "
                  "/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/Rain.Rain")
        dispatch = ("LogMOFramework: Warning: [MOWeatherIntegration] SetWeatherPreset: PresetObject=Rain (class=UDS_Weather_Settings_C) "
                    "→ dispatching to provider BP_WeatherBridge_C_2")
        f, d = ui.re.search(ui.BRIDGE_FOLLOW_RE, follow), ui.re.search(ui.BRIDGE_DISPATCH_RE, dispatch)
        self.assertTrue(f.group(1).endswith("Rain.Rain"))
        self.assertEqual(d.group(1), "Rain")
        self.assertIsNone(ui.re.search(ui.BRIDGE_FOLLOW_RE, "[MOWorldSync] host published weather preset /x/Rain.Rain"))

    def test_net_driver_parsed_from_log(self):
        with tempfile.TemporaryDirectory() as td, mock.patch.object(ui, "TMP", td):
            os.makedirs(ui.inst_dir("t"))
            with open(ui.game_log("t"), "w", encoding="utf-8") as f:
                f.write("LogNet: Name:GameNetDriver Def:GameNetDriver SteamSocketsNetDriver_0 SteamSocketsNetDriver "
                        "listening on port 7777\n")
            self.assertEqual(ui.net_driver("t"), "SteamSocketsNetDriver")
            with open(ui.game_log("t"), "w", encoding="utf-8") as f:
                f.write("LogNet: Name:GameNetDriver Def:GameNetDriver IpNetDriver_0 IpNetDriver listening on port 7777\n")
            self.assertEqual(ui.net_driver("t"), "IpNetDriver")
            self.assertIsNone(ui.net_driver("nonexistent"))


class PackagedAnalysisTests(unittest.TestCase):
    HOST = ("LogMOFramework: Warning: [MOWorldSeed] host published world seed 42 (parameter 'Seed')\n"
            "LogMOFramework: [SpawnManager] Category 0 first spawn - setting initial staggered cooldown: 7.4s\n")
    CLIENT = ("LogMOFramework: Warning: [MOWorldSeed] client applied world seed 42: voxel runtime created\n"
              "LogMOFramework: AMOPlayerController: Possessed BP_Pawn_C_1\n"
              "LogMOFramework: Warning: [MOWorldSync] client clock was -44.6 game-s off the host's; snapping to 2026.06.01-08.00.44\n"
              "LogMOFramework: Warning: [MOWorldSync] client following the host's weather preset /Game/UDS/Rain.Rain\n")

    def verdicts(self, host=None, client=None, withheld=False):
        rows = ui.analyse_packaged_logs(self.HOST if host is None else host, self.CLIENT if client is None else client, withheld)
        return {label: ok for label, ok, _ in rows}

    def test_a_healthy_pair_passes_every_check(self):
        self.assertTrue(all(self.verdicts().values()), self.verdicts())

    def test_a_crash_in_either_log_fails(self):
        self.assertFalse(self.verdicts(host=self.HOST + "LogWindows: Error: Fatal error!\n")["host: no crash, ensure or assertion in its log"])
        self.assertFalse(self.verdicts(client=self.CLIENT + "=== Handled ensure: ===\n")["client: no crash, ensure or assertion in its log"])

    def test_a_seed_mismatch_fails(self):
        v = self.verdicts(client=self.CLIENT.replace("seed 42", "seed 43"))
        self.assertFalse(v["client: applied the SAME seed and built its voxel runtime"])

    def test_a_client_that_spawns_creatures_fails_and_a_silent_host_voids_the_check(self):
        spawn = "LogMOFramework: [SpawnManager] Fallback spawned Wolf_C at X=1\n"
        self.assertFalse(self.verdicts(client=self.CLIENT + spawn)["client: its spawn manager spawned nothing"])
        silent_host = "LogMOFramework: Warning: [MOWorldSeed] host published world seed 42\n"
        self.assertFalse(self.verdicts(host=silent_host)["CONTROL: the host's spawn manager ran (so a silent client means something)"])

    def test_missing_sync_fails_when_expected_and_passes_as_a_control_when_withheld(self):
        client = "\n".join(l for l in self.CLIENT.splitlines() if "MOWorldSync" not in l) + "\n"
        v = self.verdicts(client=client)
        self.assertFalse(v["client: adopted the host's game clock"])
        self.assertFalse(v["client: followed the host's weather preset through the bridge"])
        w = self.verdicts(client=client, withheld=True)
        self.assertTrue(all(ok for label, ok in w.items() if label.startswith("NEGATIVE CONTROL")), w)
        # ...and if the client DID sync although the host withheld, the control fails (the check can see it)
        self.assertFalse(all(ok for label, ok in self.verdicts(withheld=True).items() if label.startswith("NEGATIVE CONTROL")))


class ReportTests(unittest.TestCase):
    def test_report_ok_only_if_every_step_passed(self):
        r = ui.Report()
        with mock.patch("builtins.print"):
            r.step("a", True)
            self.assertTrue(r.ok)
            r.step("b", False, "why")
        self.assertFalse(r.ok)


class ConfigGuardTests(unittest.TestCase):
    """The net-driver config bug was silent (the game ran on IpNetDriver while lobbies advertised Steam addresses),
    so pin it: DefaultEngine.ini must clear the engine's default array and name a driver class that exists."""

    REPO = Path(__file__).resolve().parents[2]

    def _engine_ini(self):
        return (self.REPO / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8", errors="replace")

    def test_game_net_driver_override_clears_the_engine_default_first(self):
        ini = self._engine_ini()
        section = ini.split("[/Script/Engine.GameEngine]", 1)[1].split("\n[", 1)[0]
        lines = [l.strip() for l in section.splitlines() if l.strip() and not l.strip().startswith(";")]
        clear = lines.index("!NetDriverDefinitions=ClearArray")
        game = next(i for i, l in enumerate(lines) if l.startswith("+NetDriverDefinitions=(DefName=\"GameNetDriver\""))
        self.assertLess(clear, game, "ClearArray must precede the GameNetDriver entry or the engine's IpNetDriver wins")

    def test_steam_driver_class_is_the_5_8_one_and_its_plugin_is_enabled(self):
        active = [l for l in self._engine_ini().splitlines() if l.strip() and not l.strip().startswith(";")]
        self.assertFalse(any("/Script/OnlineSubsystemSteam.SteamNetDriver" in l for l in active),
                         "that class does not exist in UE 5.8 (the engine falls back to IpNetDriver silently)")
        self.assertTrue(any("/Script/SteamSockets.SteamSocketsNetDriver" in l for l in active))
        uproject = (self.REPO / "MO57.uproject").read_text(encoding="utf-8")
        self.assertIn('"Name": "SteamSockets"', uproject)


class CliParsingTests(unittest.TestCase):
    """`ue.py inst` once swallowed start's own options (--nosteam, --pos) into a REMAINDER positional, so "LAN"
    instances silently launched on Steam. Parse through the REAL parser built by ue.main()."""

    @classmethod
    def setUpClass(cls):
        import argparse
        import importlib.util
        spec = importlib.util.spec_from_file_location("ue_cli_under_test", Path(__file__).resolve().parents[1] / "ue.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        captured = {}

        def grab(self, *a, **k):
            captured["parser"] = self
            raise SystemExit(0)

        with mock.patch.object(sys, "argv", ["ue.py"]), mock.patch.object(argparse.ArgumentParser, "parse_args", grab):
            try:
                mod.main()
            except SystemExit:
                pass
        cls.parser = captured["parser"]

    def test_start_keeps_its_own_options(self):
        a = self.parser.parse_args(["inst", "start", "lanhost", "--nosteam", "--pos", "680,0", "--res", "800x450"])
        self.assertEqual((a.verb, a.name, a.nosteam, a.pos, a.res), ("start", "lanhost", True, "680,0", "800x450"))

    def test_start_defaults_to_steam_on(self):
        self.assertFalse(self.parser.parse_args(["inst", "start", "x"]).nosteam)

    def test_do_passes_everything_after_the_name_through(self):
        a = self.parser.parse_args(["inst", "do", "x", "run", "MO.Test.State", "--grep", "MOQUERY"])
        self.assertEqual((a.verb, a.name, a.rest), ("do", "x", ["run", "MO.Test.State", "--grep", "MOQUERY"]))

    def test_list_and_stop(self):
        self.assertEqual(self.parser.parse_args(["inst", "list"]).verb, "list")
        self.assertEqual(self.parser.parse_args(["inst", "stop", "x"]).name, "x")

    def test_nettest_modes(self):
        self.assertEqual(self.parser.parse_args(["nettest", "lan"]).mode, "lan")
        self.assertTrue(self.parser.parse_args(["nettest", "steam-host", "--keep"]).keep)

    def test_engine_console_does_not_own_tilde(self):
        # The Tilde console popup (AMOPlayerController::HandleOpenDevConsole) can only open if the engine's own console does not
        # consume the key first; the config used to re-add it ("-ConsoleKeys=Tilde" then "+ConsoleKeys=Tilde").
        ini = (Path(__file__).resolve().parents[2] / "Config" / "DefaultInput.ini").read_text(encoding="utf-8")
        added = [l.strip() for l in ini.splitlines() if l.strip().startswith("+ConsoleKeys=")]
        self.assertNotIn("+ConsoleKeys=Tilde", added)

    def test_nettest_game_extras(self):
        self.assertEqual(self.parser.parse_args(["nettest", "hostsave"]).mode, "hostsave")
        self.assertTrue(self.parser.parse_args(["nettest", "game", "--withhold-seed"]).withhold_seed)
        self.assertFalse(self.parser.parse_args(["nettest", "game"]).withhold_seed)

    def test_walk_analysis_aligns_by_time_and_measures_disagreement(self):
        line = "LogPython: Warning: [NETWALK] t=100.500 role=client x=10 y=20 z=1000.0 mode=MOVE_Falling speed=300"
        s = ui._walk_samples(line, "client")
        self.assertEqual(s, [dict(t=100.5, x=10, y=20, z=1000.0, mode="MOVE_Falling", speed=300)])
        self.assertEqual(ui._walk_samples(line, "server"), [])
        client = [dict(t=1.0, x=0, y=0, z=100.0, mode="MOVE_Walking", speed=300),
                  dict(t=2.0, x=600, y=0, z=100.0, mode="MOVE_Falling", speed=300)]
        server = [dict(t=1.05, x=0, y=0, z=100.0, mode="MOVE_Walking", speed=300),
                  dict(t=2.1, x=600, y=0, z=400.0, mode="MOVE_Walking", speed=300),
                  dict(t=9.0, x=0, y=0, z=0.0, mode="MOVE_Walking", speed=0)]  # too far in time to pair
        a = ui.analyse_walk(client, server)
        self.assertEqual(a["pairs"], 2)
        self.assertAlmostEqual(a["walked_cm"], 600.0)
        self.assertEqual(a["max_dz"], 300.0)
        self.assertAlmostEqual(a["client_falling"], 0.5)
        self.assertEqual(ui.analyse_walk(client, [])["pairs"], 0)

    def test_grid_comparison_separates_offset_from_noise(self):
        host = [100.0, 200.0, None, 400.0]
        client = [100.0, 330.0, 300.0, 530.0]
        a = ui.compare_grids(host, client)
        self.assertEqual((a["n"], a["points"]), (3, 4))      # the point the host lacks is not "different", it is missing
        self.assertAlmostEqual(a["mean_signed"], (0 + 130 + 130) / 3)
        self.assertEqual(a["max_abs"], 130.0)
        self.assertEqual(a["exact"], 1)
        self.assertEqual(ui.compare_grids([None], [None])["n"], 0)
        m = __import__("re").search(ui.GRID_RE, "[MOQUERY] VOXEL SurfaceGrid cx=1 cy=-2 half=1 step=200 hits=2/9 z=10;x;12;")
        self.assertEqual(m.group(7), "10;x;12;")

    def test_surface_probe_line_carries_what_it_hit(self):
        # the probe once "agreed" on z=30000 -- the trace origin -- on both machines; the line now says what was skipped
        import re
        m = re.search(ui.SURFACE_RE, "[MOQUERY] VOXEL SurfaceZ x=-576080 y=271474 hit=1 z=1024.5 skipped=2 firstOther=Sky/StaticMeshComponent")
        self.assertIsNotNone(m)
        self.assertEqual((m.group(3), m.group(4), m.group(5), m.group(6)), ("1", "1024.5", "2", "Sky/StaticMeshComponent"))


class BugReportAnalyserTests(unittest.TestCase):
    """analyse_bugreport_upload judges a received bug report. Every check has a case that must FAIL, or a check that is always green proves nothing."""

    TITLE = "PackagedBugReport accepted 1"

    def make(self, title=None, log="LogInit: Build: ++UE5\nLogMOFramework: ready\n", user_in="", extra_fields="", crash_type="BugReport", shot=True, category="Other"):
        import ue_crash_bundle as cb
        title = title or self.TITLE
        fields = {"Build.Version": "0.0.0.1", "Build.Commit": "abc1234", "System.OS": "Windows 11", "System.GPU": "RTX", "Session.NetMode": "ListenServer",
                  "Session.Map": "Gameplay", "Session.WorldSeed": "12345", "Clock.GameTime": "2026-01-01T08:00:00", "Player.Pawn": "BP_Human",
                  "Player.Location": "1, 2, 3", "Report.Category": category}
        rows = "".join(f'\t\t<Field name="{k}">{v}</Field>\n' for k, v in fields.items()) + extra_fields
        xml = ("<FGenericCrashContext>\n<RuntimeProperties>\n<CrashType>%s</CrashType>\n<ErrorMessage>%s</ErrorMessage>\n</RuntimeProperties>\n<GameData>\n%s</GameData>\n</FGenericCrashContext>\n"
               % (crash_type, title, rows))
        files = [("CrashContext.runtime-xml", xml.encode()), ("BugReport.txt", ("--- Attachments ---\n" + user_in).encode()), ("game.log", (log + user_in).encode())]
        if shot:
            files.append(("Screenshot.jpg", b"\xff\xd8" + b"\x01" * 20000 + b"\xff\xd9"))
        return cb.write_bundle("UECC-Windows-" + "B" * 32 + "_0000", files)

    META = {"query": {"AppID": "CrashReporter", "AppVersion": "5.8.1", "UploadType": "crashreports", "ReportKind": "bugreport", "UserID": "abc||"}}

    def judge(self, body, meta=None, **kw):
        kw.setdefault("title", self.TITLE)
        return ui.analyse_bugreport_upload(body, self.META if meta is None else meta, **kw)

    def failed(self, rows):
        return [label for label, ok, _ in rows if not ok]

    def test_a_good_report_passes_every_check(self):
        rows = self.judge(self.make(), user_name="penum", computer_name="PENUMPC")
        self.assertEqual(self.failed(rows), [])
        self.assertTrue(any("user" in label for label, _, _ in rows) and any("computer" in label for label, _, _ in rows))

    def test_a_crash_is_not_accepted_as_a_bug_report(self):
        self.assertIn("it is marked as a bug report, not a crash", self.failed(self.judge(self.make(crash_type="Crash"))))

    def test_the_title_must_arrive_intact(self):
        self.assertIn("the title the player typed arrived intact", self.failed(self.judge(self.make(title="something else"))))

    def test_missing_query_parameters_are_caught(self):
        meta = {"query": {"AppID": "CrashReporter", "UploadType": "crashreports"}}
        self.assertTrue(any("the query carries" in label for label in self.failed(self.judge(self.make(), meta=meta))))

    def test_a_missing_screenshot_is_caught_only_when_one_was_expected(self):
        body = self.make(shot=False)
        self.assertTrue(any("the bundle holds" in label for label in self.failed(self.judge(body))))
        self.assertEqual(self.failed(self.judge(body, expect_screenshot=False)), [])

    def test_the_user_name_in_the_log_is_caught_as_a_whole_word_only(self):
        leak = self.judge(self.make(user_in="Loading C:/Users/penum/AppData\n"), user_name="penum")
        self.assertTrue(any("user" in label for label in self.failed(leak)))
        fine = self.judge(self.make(user_in="project penumbra loads\n"), user_name="penum")  # the name inside another word is not a leak
        self.assertEqual(self.failed(fine), [])

    def test_the_computer_name_is_caught_case_insensitively(self):
        leak = self.judge(self.make(user_in="Computer: penumpc\n"), computer_name="PENUMPC")
        self.assertTrue(any("computer" in label for label in self.failed(leak)))

    def test_state_fields_are_required_and_the_world_ones_only_when_a_world_is_expected(self):
        import ue_crash_bundle as cb
        xml = "<FGenericCrashContext><RuntimeProperties><CrashType>BugReport</CrashType><ErrorMessage>%s</ErrorMessage></RuntimeProperties><GameData></GameData></FGenericCrashContext>" % self.TITLE
        bare = cb.write_bundle("d", [("CrashContext.runtime-xml", xml.encode()), ("BugReport.txt", b"--- Attachments ---"), ("game.log", b"LogInit")])
        self.assertIn("the game-state fields are filled in", self.failed(self.judge(bare, expect_screenshot=False)))

    def test_garbage_is_one_failed_row_not_an_exception(self):
        rows = self.judge(b"not a bundle")
        self.assertEqual(len(rows), 1)
        self.assertFalse(rows[0][1])

    def test_result_line_regex_reads_the_games_log(self):
        sent = "[2026.10.09-19.00.00:000][  1]LogMOFramework: Warning: [MOBugReport] SENT id=ab12cd34 http=200: Thank you - your report was sent. Its id is ab12cd34.\n"
        m = ui.BUGREPORT_RESULT_RE.search(sent)
        self.assertEqual((m.group(1), m.group(2), m.group(3)), ("SENT", "ab12cd34", "200"))
        failed = ("LogMOFramework: Warning: [MOBugReport] NOT SENT id=ab12cd34 http=500: The server answered with error 500. Your report was saved to D:\\x\\ab12cd34.uecrash"
                  " - you can attach that file on Discord.  [saved: D:\\x\\ab12cd34.uecrash]\n")
        m = ui.BUGREPORT_RESULT_RE.search(failed)
        self.assertEqual((m.group(1), m.group(3), m.group(5)), ("NOT SENT", "500", "D:\\x\\ab12cd34.uecrash"))
        self.assertIn("saved to", m.group(4))


class ChurnAnalyserTests(unittest.TestCase):
    """analyse_churn_logs / log helpers: each verdict has an input that must make it FAIL."""

    CLEAN = "[2026.10.09-19.00.00:000][  1]LogMOFramework: Warning: [MOCharacter] fine\n[2026.10.09-19.00.01:000][  2]LogNet: Error: some engine noise\n"

    def failed(self, rows):
        return [label for label, ok, _ in rows if not ok]

    def test_clean_logs_pass_and_engine_noise_is_not_ours(self):
        rows = ui.analyse_churn_logs(self.CLEAN, self.CLEAN)
        self.assertEqual(self.failed(rows), [])
        self.assertEqual(len(rows), 6)

    def test_a_rescue_on_either_machine_fails(self):
        bad = self.CLEAN + "[2026.10.09-19.00.02:000][ 3]LogMOFramework: Warning: [MOCharacter] BP_MOMetaHuman_Female_C_0: Fall-through detected! Found safe terrain, teleporting to X\n"
        self.assertEqual(self.failed(ui.analyse_churn_logs(bad, self.CLEAN)), ["host: no fall-through rescue of a player or colonist during the churn"])
        self.assertEqual(self.failed(ui.analyse_churn_logs(self.CLEAN, bad)), ["client: no fall-through rescue of a player or colonist during the churn"])

    def test_a_creature_rescue_is_not_a_player_rescue(self):
        deer = self.CLEAN + "[2026.10.09-19.00.02:000][ 3]LogMOFramework: Warning: [MOCharacter] BP_Deer_C_0: Fall-through detected! Found safe terrain, teleporting to X\n"
        rows = ui.analyse_churn_logs(deer, self.CLEAN)
        self.assertEqual(self.failed(rows), [])
        self.assertIn("1 creature rescue", [e for l, ok, e in rows if "rescue of a player" in l][0] + " ")

    def test_an_ensure_or_a_crash_fails(self):
        for line in ("LogOutputDevice: Error: Ensure condition failed: Component", "LogWindows: Error: Fatal error: [File:x] y", "Assertion failed: x"):
            self.assertIn("host: no crash, ensure or assertion during the churn", self.failed(ui.analyse_churn_logs(self.CLEAN + line + "\n", self.CLEAN)), line)

    def test_a_game_error_fails_and_repeats_are_one_line(self):
        err = "[2026.10.09-19.00.03:111][ 4]LogMOFramework: Error: [X] something broke\n"
        rows = ui.analyse_churn_logs(self.CLEAN + err * 3, self.CLEAN)
        self.assertEqual(self.failed(rows), ["host: the game's own code logged no Error-level line during the churn"])
        evidence = [e for label, ok, e in rows if "Error-level" in label and not ok][0]
        self.assertIn("(x3)", evidence)

    def test_timestamps_and_skew(self):
        self.assertEqual(ui.log_stamp_ms("[2026.10.09-19.47.54:265][153]LogX: y"), ((19 * 60 + 47) * 60 + 54) * 1000 + 265)
        self.assertIsNone(ui.log_stamp_ms("no stamp here"))

    def test_distinct_lines_ignore_the_prefix(self):
        import re
        out = ui.distinct_log_lines("[2026.10.09-19.00.00:001][  1]A: Error: x\n[2026.10.09-19.00.09:999][ 77]A: Error: x\nB: ok\n", re.compile("Error:"))
        self.assertEqual(out, {"A: Error: x": 2})


if __name__ == "__main__":
    unittest.main()
