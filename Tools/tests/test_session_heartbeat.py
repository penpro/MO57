"""Offline tests of Tools/session_heartbeat.py. The coop lines are the real ones of Nov 4 2026, the night two sessions fought over the build window.

    python -m unittest discover -s Tools/tests -p "test_session_heartbeat.py"
"""
from __future__ import annotations

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import session_heartbeat as hb  # noqa: E402

CROP = "CLAUDE (crop-plant side session)"
LAB = "CLAUDE (WorldLab side session)"
NOW = 1_800_000_000.0
MIN = 60.0


def entry(cid: str, author: str, status: str, refs: str = "") -> str:
    return f"### 2026-11-04 Pacific Standard Time — {author} — {cid}\nStatus: {status}\nReferences: {refs}\n\n"


CL355 = entry("CL-0355", CROP, 'CLAIM the editor and -game instances (~90 min), Wes Nov 4 ("Yes, do the grass"). No C++ build planned.', "CL-0348")
CL356 = entry("CL-0356", LAB, 'CLAIM (source files, now; the build window when CL-0355 releases): Wes, Nov 4: "Yes, option A" on fork F62. Crop session: no hurry, tell me when you release the editor.', "CL-0354, CL-0355")
CL357 = entry("CL-0357", CROP, "CL-0355 continues: a C++ build window (ue.py build, then ue.py auto --filter MOFramework.Rendering). The editor is closed.", "CL-0355")
CL358 = entry("CL-0358", CROP, "RELEASE CL-0355 and CL-0357: no editor, build or instance of mine runs; the build window is free (CL-0356 was waiting on it). For CL-0356: the full ue.py auto ran on the shared tree: 413 passed.", "CL-0355, CL-0356, CL-0357")
CL359 = entry("CL-0359", CROP, 'WAITING, not a claim: when CL-0356 releases the build window I take -game instances for ~40 min (Wes, Nov 4: "Yes, check crops").', "CL-0356, CL-0358")
CL360 = entry("CL-0360", CROP, "WITHDRAW CL-0359: the crop check is deferred, not cancelled; I hold no claim and am not queued.", "CL-0356, CL-0359")
CL361 = entry("CL-0361", LAB, "RELEASE CL-0356: generator v13 pushed; no process of mine runs.", "CL-0356")
# what the two sessions' claims would have been had crop queued before releasing: the deadlock
CROP_WAITS_HOLDING = entry("CL-0900", CROP, "WAITING: when CL-0356 releases I bench the crops.", "CL-0356")

CFG = hb.Config(sessions={"crop-plant": "dd9a61c9", "WorldLab": "c1ceb288"}, self_id="75fb3b25")


def editor(pid=100, cpu=500.0, started=NOW - 3600):
    return hb.Proc(pid, 1, "UnrealEditor.exe", started, cpu, f'"UnrealEditor.exe" "MO57.uproject" -game -abslog=inst/game.log')


def commandlet(pid=200, cpu=500.0, started=NOW - 600):
    return hb.Proc(pid, 1, "UnrealEditor-Cmd.exe", started, cpu, '"UnrealEditor-Cmd.exe" "MO57.uproject" -ExecCmds="Automation RunTests MOFramework.WorldGen;Quit"')


def loop(pid=300, target="CL-0356", started=NOW - 600):
    # crop's real watcher, as Git Bash shows it
    return hb.Proc(pid, 1, "bash.exe", started, 1.0, f'bash -c "while [ $(date +%s) -lt $end ]; do if grep -qE \'^Status: RELEASE[^:]*{target}\' claude-codex-coop.md; then break; fi; sleep 20; done"')


def check(coop, procs=(), crop_age=60.0, lab_age=60.0, state=None, now=NOW, cfg=CFG):
    entries = hb.parse_coop(coop, "2026-11-04", 3)
    ages = {"dd9a61c9-db97": crop_age, "c1ceb288-4d49": lab_age, "75fb3b25-39ba": 1.0}
    state = {} if state is None else state
    findings, summary = hb.detect(entries, list(procs), ages, state, now, cfg)
    return [f.kind for f in findings], findings, summary, state


class CoopParsing(unittest.TestCase):
    def test_headers_with_a_mangled_dash_still_parse(self):
        text = "### 2026-11-04 Pacific Standard Time �� CLAUDE (x) � CL-0001\nStatus: CLAIM it\n"
        self.assertEqual([e.id for e in hb.parse_coop(text, "2026-11-04")], ["CL-0001"])

    def test_old_entries_fall_out_of_the_window(self):
        text = entry("CL-0001", LAB, "CLAIM it").replace("2026-11-04", "2026-10-01") + entry("CL-0002", LAB, "CLAIM that")
        self.assertEqual([e.id for e in hb.parse_coop(text, "2026-11-04", 3)], ["CL-0002"])

    def test_a_release_closes_only_what_it_names_before_its_colon(self):
        # The false alarm of Nov 4: a loop read this line as CL-0356's release.
        self.assertEqual(hb.closed_ids("RELEASE CL-0355 and CL-0357: free (CL-0356 was waiting on it). For CL-0356: ..."), ["CL-0355", "CL-0357"])
        self.assertEqual(hb.closed_ids("RELEASE CL-0276 (and its notes CL-0277 / CL-0278): stopped"), ["CL-0276", "CL-0277", "CL-0278"])
        self.assertEqual(hb.closed_ids("RELEASE CL-0345 (croptest stopped; editor closed). Committed with CL-0346."), ["CL-0345"])

    def test_a_release_naming_nothing_closes_the_authors_own_claims(self):
        led = hb.Ledger(hb.parse_coop(CL355 + entry("CL-0002", LAB, "CLAIM other") + entry("CL-0003", CROP, "RELEASE (all stopped)"), "2026-11-04"))
        self.assertEqual([e.id for e in led.holds], ["CL-0002"])

    def test_what_counts_as_a_wait(self):
        self.assertEqual(hb.wait_targets("CLAIM (source files, now; the build window when CL-0355 releases): ..."), ["CL-0355"])
        self.assertEqual(hb.wait_targets("WAITING, not a claim: when CL-0356 releases the build window"), ["CL-0356"])
        self.assertEqual(hb.wait_targets("QUEUED for the UE_5.8 build window after CL-0231's RELEASE: a build"), ["CL-0231"])
        self.assertEqual(hb.wait_targets("CL-0322 waits (no process of mine runs). Queued after CL-0323's lab build and CL-0326's probes"), ["CL-0323", "CL-0326"])
        self.assertEqual(hb.wait_targets("RELEASE CL-0355: free (CL-0356 was waiting on it). For CL-0356: results"), [])
        self.assertEqual(hb.wait_targets("WITHDRAW CL-0359: when CL-0356 releases nobody is queued"), [])
        self.assertEqual(hb.wait_targets("CLAIM the editor: the bug first showed after CL-0300 shipped"), [])
        self.assertEqual(hb.wait_targets("PROGRESS: waiting on CL-0300 before the next step"), ["CL-0300"])


class ReplayOfNov4(unittest.TestCase):
    """The night's coop log, line by line."""

    def test_crop_holds_and_works_while_the_lab_waits(self):
        kinds, *_ = check(CL355 + CL356, [editor()], crop_age=60, lab_age=3600)
        self.assertEqual(kinds, [])

    def test_crop_goes_quiet_holding_the_editor(self):
        # An open editor is no sign of work: a session can leave one up and stop.
        kinds, findings, *_ = check(CL355 + CL356, [editor()], crop_age=45 * MIN, lab_age=45 * MIN)
        self.assertEqual(kinds, ["BLOCKED_ON_IDLE"])
        self.assertIn("CL-0355", findings[0].text)
        self.assertIn(CROP, findings[0].text)

    def test_after_the_release_the_lab_may_go(self):
        kinds, *_ = check(CL355 + CL356 + CL357 + CL358, [commandlet()], crop_age=45 * MIN, lab_age=45 * MIN, state={"cpu": {"200": {"started": NOW - 600, "cpu": 400.0, "ts": NOW - 120, "anchor_cpu": 400.0, "anchor_ts": NOW - 120}}})
        self.assertEqual(kinds, [])

    def test_if_the_lab_never_wakes(self):
        kinds, findings, *_ = check(CL355 + CL356 + CL357 + CL358, [], crop_age=1 * MIN, lab_age=30 * MIN)
        self.assertEqual(kinds, ["MISSED_WAKE"])
        self.assertIn("CL-0356", findings[0].text)

    def test_the_lab_at_work_and_crop_queued_is_no_stalemate(self):
        kinds, *_ = check(CL355 + CL356 + CL357 + CL358 + CL359, [loop()], crop_age=30 * MIN, lab_age=1 * MIN)
        self.assertEqual(kinds, [])

    def test_the_lab_stops_without_releasing_while_crop_is_queued(self):
        kinds, findings, *_ = check(CL355 + CL356 + CL357 + CL358 + CL359, [loop()], crop_age=30 * MIN, lab_age=30 * MIN)
        self.assertIn("BLOCKED_ON_IDLE", kinds)
        blocked = next(f for f in findings if f.kind == "BLOCKED_ON_IDLE")
        self.assertIn("CL-0356", blocked.text)
        self.assertIn("polling loop (pid 300)", blocked.text)

    def test_a_withdrawn_wait_is_over(self):
        led = hb.Ledger(hb.parse_coop(CL355 + CL356 + CL357 + CL358 + CL359 + CL360, "2026-11-04"))
        self.assertEqual([w.entry.id for w in led.waits], ["CL-0356"])
        kinds, *_ = check(CL355 + CL356 + CL357 + CL358 + CL359 + CL360, [], crop_age=30 * MIN, lab_age=1 * MIN)
        self.assertEqual(kinds, [])

    def test_all_released_nothing_left(self):
        led = hb.Ledger(hb.parse_coop(CL355 + CL356 + CL357 + CL358 + CL359 + CL360 + CL361, "2026-11-04"))
        self.assertEqual((led.holds, led.waits), ([], []))
        kinds, *_ = check(CL355 + CL356 + CL357 + CL358 + CL359 + CL360 + CL361, [], crop_age=500 * MIN, lab_age=500 * MIN)
        self.assertEqual(kinds, [])

    def test_had_crop_queued_before_releasing_it_was_a_deadlock(self):
        kinds, findings, *_ = check(CL355 + CL356 + CROP_WAITS_HOLDING, crop_age=1 * MIN, lab_age=1 * MIN)
        self.assertIn("CIRCULAR_WAIT", kinds)
        text = next(f for f in findings if f.kind == "CIRCULAR_WAIT").text
        self.assertIn("CL-0355", text)
        self.assertIn("CL-0356", text)


class Processes(unittest.TestCase):
    def test_a_running_build_means_someone_is_working(self):
        state = {"cpu": {"200": {"started": NOW - 600, "cpu": 400.0, "ts": NOW - 120, "anchor_cpu": 400.0, "anchor_ts": NOW - 120}}}
        kinds, *_ = check(CL355 + CL356, [commandlet()], crop_age=45 * MIN, lab_age=45 * MIN, state=state)
        self.assertEqual(kinds, [])

    def test_a_commandlet_doing_nothing_is_no_work(self):
        state = {"cpu": {"200": {"started": NOW - 600, "cpu": 500.0, "ts": NOW - 120, "anchor_cpu": 500.0, "anchor_ts": NOW - 120}}}
        kinds, *_ = check(CL355 + CL356, [commandlet(cpu=500.0)], crop_age=45 * MIN, lab_age=45 * MIN, state=state)
        self.assertEqual(kinds, ["BLOCKED_ON_IDLE"])

    def test_a_loop_on_a_released_claim_is_reported_once_it_has_lasted(self):
        coop = CL355 + CL356 + CL357 + CL358 + CL359 + CL360 + CL361
        kinds, _, _, state = check(coop, [loop(target="CL-0356")], crop_age=1 * MIN, lab_age=1 * MIN)
        self.assertEqual(kinds, [])
        kinds, *_ = check(coop, [loop(target="CL-0356")], crop_age=1 * MIN, lab_age=1 * MIN, state=state, now=NOW + 4 * MIN)
        self.assertEqual(kinds, ["WAITER_ON_RELEASED"])

    def test_a_loop_on_an_open_claim_is_fine_and_its_forked_twin_counts_once(self):
        twin = loop(pid=301)
        waiters = hb.find_waiters([loop(), twin], NOW, CFG)
        self.assertEqual(len(waiters), 1)
        kinds, *_ = check(CL355 + CL356 + CL357 + CL358 + CL359, [loop(), twin], crop_age=1 * MIN, lab_age=1 * MIN)
        self.assertEqual(kinds, [])

    def test_a_loop_and_the_wrapper_shell_it_runs_under_count_once(self):
        # Seen live: the tool's wrapper shell and the loop's own shell, different command lines, the same loop.
        wrapper = hb.Proc(5884, 1, "bash.exe", NOW - 600, 1.0, "bash -l -c 'eval \"until grep -q CL-0356 f; do sleep 10; done\" && pwd -P'")
        inner = hb.Proc(16696, 5884, "bash.exe", NOW - 600, 1.0, "bash -c 'until grep -q CL-0356 f; do sleep 10; done'")
        waiters = hb.find_waiters([inner, wrapper], NOW, CFG)
        self.assertEqual([p.pid for p, _ in waiters], [5884])

    def test_a_one_off_command_naming_a_claim_is_no_loop(self):
        young = loop(started=NOW - 5)
        self.assertEqual(hb.find_waiters([young], NOW, CFG), [])
        heartbeat = hb.Proc(9, 1, "bash.exe", NOW - 600, 1.0, "bash -c 'until python Tools/session_heartbeat.py --watch; do echo CL-0001; done'")
        self.assertEqual(hb.find_waiters([heartbeat], NOW, CFG), [])

    def test_loops_started_again_and_again(self):
        coop = CL355 + CL356 + CL357 + CL358 + CL359
        state = {}
        for i in range(3):
            kinds, *_ = check(coop, [loop(pid=300 + i, started=NOW - 300 + i * 60)], crop_age=1 * MIN, lab_age=1 * MIN, state=state)
        self.assertEqual(kinds, ["WAITER_FLAPPING"])

    def test_a_frozen_unreal_process(self):
        state = {"cpu": {"100": {"started": NOW - 3600, "cpu": 500.2, "ts": NOW - 120, "anchor_cpu": 500.0, "anchor_ts": NOW - 16 * MIN}}}
        kinds, *_ = check("", [editor(cpu=500.3)], state=state)
        self.assertEqual(kinds, ["HUNG_PROCESS"])
        state = {"cpu": {"100": {"started": NOW - 3600, "cpu": 400.0, "ts": NOW - 120, "anchor_cpu": 400.0, "anchor_ts": NOW - 16 * MIN}}}
        kinds, *_ = check("", [editor(cpu=500.3)], state=state)
        self.assertEqual(kinds, [])

    def test_a_pid_reused_starts_over(self):
        state = {"cpu": {"100": {"started": NOW - 9000, "cpu": 500.0, "ts": NOW - 120, "anchor_cpu": 500.0, "anchor_ts": NOW - 60 * MIN}}}
        kinds, *_ = check("", [editor(cpu=2.0, started=NOW - 30)], state=state)
        self.assertEqual(kinds, [])

    def test_a_crash_reporter_left_behind(self):
        reporter = hb.Proc(400, 1, "CrashReportClientEditor.exe", NOW - 900, 3.0, "CrashReportClientEditor.exe -RespawnedInstance -MONITOR=4242")
        kinds, _, _, state = check("", [reporter])
        self.assertEqual(kinds, [])
        kinds, *_ = check("", [reporter], state=state, now=NOW + 6 * MIN)
        self.assertEqual(kinds, ["LEFTOVER_REPORTER"])
        watched = hb.Proc(4242, 1, "UnrealEditor.exe", NOW - 900, 3.0, "UnrealEditor.exe -game")
        kinds, *_ = check("", [reporter, watched], state={}, now=NOW + 6 * MIN)
        self.assertEqual(kinds, [])


class Sessions(unittest.TestCase):
    def test_the_watchers_own_activity_proves_nothing(self):
        # Unmapped holder: every transcript but this session's counts, and this session is the only one moving.
        cfg = hb.Config(self_id="75fb3b25")
        kinds, *_ = check(CL355 + CL356, crop_age=45 * MIN, lab_age=45 * MIN, cfg=cfg)
        self.assertEqual(kinds, ["BLOCKED_ON_IDLE"])

    def test_with_no_map_any_other_session_at_work_holds_the_alarm(self):
        cfg = hb.Config(self_id="75fb3b25")
        kinds, *_ = check(CL355 + CL356, crop_age=45 * MIN, lab_age=1 * MIN, cfg=cfg)
        self.assertEqual(kinds, [])

    def test_a_mapped_holder_is_judged_by_its_own_transcript(self):
        kinds, *_ = check(CL355 + CL356, crop_age=45 * MIN, lab_age=1 * MIN)
        self.assertEqual(kinds, ["BLOCKED_ON_IDLE"])


class Reporting(unittest.TestCase):
    def test_snooze_then_again_and_a_cleared_finding_is_new(self):
        finding = hb.Finding("BLOCKED_ON_IDLE", "blocked:CL-0355", "t", "a")
        state = {}
        self.assertEqual(len(hb.fresh_findings([finding], state, NOW, 30)), 1)
        self.assertEqual(hb.fresh_findings([finding], state, NOW + 10 * MIN, 30), [])
        self.assertEqual(len(hb.fresh_findings([finding], state, NOW + 31 * MIN, 30)), 1)
        self.assertEqual(hb.fresh_findings([], state, NOW + 32 * MIN, 30), [])
        self.assertEqual(len(hb.fresh_findings([finding], state, NOW + 33 * MIN, 30)), 1)


if __name__ == "__main__":
    unittest.main()
