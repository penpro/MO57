"""Offline tests for Tools/ue_netdiag.py (nettest netdiag) -- the pure parts only: parsing the game's [MONet] / [MONetPos] lines, the
cross-machine picture delay, one phase's numbers and the clean-phase verdicts. No editor, no processes."""
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ue_netdiag as nd  # noqa: E402

# One line exactly as FMONetSample::ToLogLine writes it (MONetDiagnostics.cpp; the C++ test checks the same format).
HOST_LINE = ("[2026.10.17-22.00.00:000][  0]LogMOFramework: Warning: [MONet] role=host fps=58.2 frame=17.2/41.0ms conns=1 out=23456 in=4567 "
             "cap=100000 use=23% pkts=60/62 loss=0.0/1.5% rtt=12ms ping=8ms jit=1.2ms reps=58 satRepl=0% satFrames=0% chans=210 objs=1450 dorm=12 "
             "| moves=58 corr=2 err=4.5/9.0cm xy=1.0 z=4.4 | acks=0 recv=0 snaps=0 snap=0.0/0.0cm | remote=1 upd=58.0 gap=40ms "
             "| topOut=BP_Deer_C:8000,MOWorldItem:900 | topIn=-")
CLIENT_LINE = ("LogMOFramework: Warning: [MONet] role=client fps=40.0 frame=25.0/80.0ms conns=1 out=3000 in=23000 cap=100000 use=3% pkts=60/58 "
               "loss=0.0/0.0% rtt=14ms ping=8ms jit=2.0ms reps=0 satRepl=0% satFrames=0% chans=200 objs=200 dorm=0 | moves=0 corr=0 err=0.0/0.0cm "
               "xy=0.0 z=0.0 | acks=58 recv=2 snaps=2 snap=3.0/5.5cm | remote=1 upd=30.0 gap=90ms | topOut=- | topIn=BP_Deer_C:7900")


def pos(t, pid, local, x, y, v=400):
    return dict(t=t, id=pid, local=local, role=3 if local else 1, ax=x, ay=y, az=100.0, mx=x, my=y, mz=10.0, v=v, gait="-", mgait="W")


def trajectory(pid, local, delay=0.0, speed=400.0, seconds=10.0, hz=20.0, snap_at=None):
    """A pawn walking along +X at `speed`; the viewer's copy is `delay` seconds behind; optionally one sample `snap_at` is 150 cm off."""
    out = []
    n = int(seconds * hz)
    for i in range(n):
        t = 1000.0 + i / hz
        x = speed * (t - 1000.0 - delay)
        y = 0.0
        if snap_at is not None and i == snap_at:
            y = 150.0
        out.append(pos(t, pid, local, x, y, speed))
    return out


class ParseTests(unittest.TestCase):
    def test_host_line(self):
        [d] = nd.parse_monet(HOST_LINE)
        self.assertEqual(d["role"], "host")
        self.assertAlmostEqual(d["fps"], 58.2)
        self.assertEqual((d["frame"], d["frame2"]), (17.2, 41.0))
        self.assertEqual((d["out"], d["cap"], d["use"]), (23456, 100000, 23))
        self.assertEqual((d["pkts"], d["pkts2"]), (60, 62))
        self.assertEqual((d["loss"], d["loss2"]), (0.0, 1.5))
        self.assertEqual((d["err"], d["err2"], d["z"]), (4.5, 9.0, 4.4))
        self.assertEqual(d["gap"], 40)
        self.assertEqual(d["topOut"], {"BP_Deer_C": 8000, "MOWorldItem": 900})
        self.assertEqual(d["topIn"], {})

    def test_client_line_and_several(self):
        ds = nd.parse_monet(HOST_LINE + "\n" + CLIENT_LINE + "\nunrelated line\n")
        self.assertEqual([d["role"] for d in ds], ["host", "client"])
        self.assertEqual((ds[1]["recv"], ds[1]["snap2"]), (2, 5.5))

    def test_pos_line(self):
        line = ("[MONetPos] t=12345.6789 id=256 local=0 role=1 ax=-100.5 ay=20.0 az=300.1 mx=-101.0 my=21.0 mz=210.1 v=550 gait=S mgait=J")
        [p] = nd.parse_monetpos(line)
        self.assertEqual((p["id"], p["local"], p["role"]), (256, 0, 1))
        self.assertAlmostEqual(p["t"], 12345.6789)
        self.assertEqual((p["mx"], p["my"], p["v"]), (-101.0, 21.0, 550.0))
        self.assertEqual((p["gait"], p["mgait"]), ("S", "J"))


class VisualLagTests(unittest.TestCase):
    def test_finds_a_known_delay(self):
        owner = trajectory(1, 1)
        viewer = trajectory(1, 0, delay=0.12)
        r = nd.visual_lag(owner, viewer)
        self.assertAlmostEqual(r["delay_ms"], 120.0, delta=10.0)
        self.assertLess(r["p95"], 5.0)

    def test_a_snap_shows_in_the_max_not_the_median(self):
        owner = trajectory(1, 1)
        viewer = trajectory(1, 0, delay=0.05, snap_at=100)
        r = nd.visual_lag(owner, viewer)
        self.assertAlmostEqual(r["delay_ms"], 50.0, delta=10.0)
        self.assertLess(r["p50"], 5.0)
        self.assertGreater(r["max"], 140.0)

    def test_control_more_delay_is_seen(self):
        a = nd.visual_lag(trajectory(1, 1), trajectory(1, 0, delay=0.03))
        b = nd.visual_lag(trajectory(1, 1), trajectory(1, 0, delay=0.13))
        self.assertGreater(b["delay_ms"] - a["delay_ms"], 80.0)

    def test_no_overlap_is_nan(self):
        r = nd.visual_lag(trajectory(1, 1), [])
        self.assertEqual(r["n"], 0)
        self.assertTrue(math.isnan(r["delay_ms"]))


class PhaseTests(unittest.TestCase):
    def _phase(self, host_lines, client_lines):
        host_pos = trajectory(1, 1) + trajectory(2, 0, delay=0.06)
        client_pos = trajectory(2, 1) + trajectory(1, 0, delay=0.08)
        return nd.analyse_phase(nd.parse_monet("\n".join(host_lines)), nd.parse_monet("\n".join(client_lines)), host_pos, client_pos)

    def test_clean_numbers_pass(self):
        r = self._phase([HOST_LINE.replace("corr=2", "corr=0")] * 10, [CLIENT_LINE] * 10)
        self.assertEqual((r["moves"], r["corrections"], r["saturated_seconds"]), (580, 0, 0))
        self.assertAlmostEqual(r["client_view_of_host"]["delay_ms"], 80.0, delta=10.0)
        self.assertAlmostEqual(r["host_view_of_joiner"]["delay_ms"], 60.0, delta=10.0)
        self.assertEqual(r["top_out_host"][0], ("BP_Deer_C", 8000.0))
        failed = [label for label, ok, _ in nd.verdicts(r) if not ok]
        self.assertEqual(failed, [])

    def test_saturation_and_corrections_fail(self):
        sat = HOST_LINE.replace("satRepl=0%", "satRepl=40%").replace("corr=2", "corr=20")
        r = self._phase([sat] * 10, [CLIENT_LINE.replace("snap=3.0/5.5cm", "snap=40.0/120.0cm")] * 10)
        failed = {label for label, ok, _ in nd.verdicts(r) if not ok}
        self.assertIn("the host's channel to the joiner never saturates", failed)
        self.assertTrue(any("corrects" in f for f in failed))
        self.assertTrue(any("rubber band" in f for f in failed))

    def test_slow_remote_updates_fail(self):
        slow = CLIENT_LINE.replace("upd=30.0 gap=90ms", "upd=5.0 gap=400ms")
        r = self._phase([HOST_LINE] * 10, [slow] * 10)
        failed = {label for label, ok, _ in nd.verdicts(r) if not ok}
        self.assertTrue(any(f.startswith("the joiner gets the host's pawn") for f in failed))

    def test_no_samples_never_passes(self):
        r = nd.analyse_phase([], [], [], [])
        self.assertTrue(all(not ok for label, ok, _ in nd.verdicts(r) if "saturates" not in label and "rubber" not in label))


class TwoMachineLogTests(unittest.TestCase):
    """The log reader for a real (Steam) session: two PCs, lined up by the replicated server clock (st=)."""

    def test_pos_line_with_server_clock(self):
        [p] = nd.parse_monetpos("[MONetPos] t=5.0 st=812.250 id=7 local=1 role=2 ax=1 ay=2 az=3 mx=1 my=2 mz=3 v=450 gait=- mgait=J")
        self.assertEqual((p["st"], p["id"], p["local"]), (812.25, 7, 1))
        [old] = nd.parse_monetpos("[MONetPos] t=5.0 id=7 local=1 role=2 ax=1 ay=2 az=3 mx=1 my=2 mz=3 v=450 gait=- mgait=J")
        self.assertEqual(old["st"], -1.0)

    def test_server_clock_lines_up_two_pcs(self):
        # each PC has its own t (here 1000 s apart); st is shared: the delay must come out of st, not t
        owner = [dict(p, st=p["t"] - 1000.0 + 50.0) for p in trajectory(1, 1)]
        viewer = [dict(p, st=p["t"] - 1000.0 + 50.0, t=p["t"] + 1000.0) for p in trajectory(1, 0, delay=0.09)]
        r = nd.visual_lag(owner, viewer, clock="st")
        self.assertAlmostEqual(r["delay_ms"], 90.0, delta=10.0)
        self.assertEqual(nd.visual_lag(owner, viewer, clock="t")["n"], 0)   # control: the PCs' own clocks do not overlap

    def test_correlate_corrections_with_hitches(self):
        events = [
            (100.0, "joiner", "joiner frame took 480 ms"),
            (100.6, "host", "host corrected joiner pawn BP_X by 210 cm (dx 0 dy -210 dz 0)"),
            (100.7, "joiner", "joiner rubber-banded 212 cm (a server correction)"),   # the same event seen by the joiner: counted once
            (200.0, "host", "host corrected joiner pawn BP_X by 40 cm (dx 40 dy 0 dz 0)"),   # nothing before it
            (299.5, "host", "host channel to joiner0 saturated: 30 of 60 replication passes cut short, 99000 B/s of a 100000 cap"),
            (300.0, "joiner", "joiner rubber-banded 55 cm (a server correction)"),
        ]
        rows, counts = nd.correlate_events(events)
        self.assertEqual(len(rows), 3)
        self.assertEqual(counts, {"joiner hitch": 1, "no hitch, saturation or stall just before": 1, "saturated channel": 1})

    def test_whole_session_and_minutes(self):
        host = "\n".join(HOST_LINE.replace("role=host ", f"role=host st={60.0 + i:.2f} ") for i in range(90))
        joiner = "\n".join(CLIENT_LINE.replace("role=client ", f"role=client st={60.5 + i:.2f} ") for i in range(90))
        joiner += "\nLogMOFramework: Warning: [MONetEvent] st=100.00 joiner frame took 480 ms"
        host += "\nLogMOFramework: Warning: [MONetEvent] st=100.60 host corrected joiner pawn BP_X by 210 cm (dx 0 dy -210 dz 0)"
        r = nd.analyse_logs(host, joiner)
        self.assertEqual([lo for lo, _ in r["minutes"]], [60.0, 120.0])
        self.assertEqual(r["whole"]["seconds_host"], 90)
        self.assertEqual(r["causes"], {"joiner hitch": 1})
        self.assertFalse(r["has_trace"])
        text = "\n".join(nd.report_lines(r))
        self.assertIn("1 after a joiner hitch", text)
        self.assertNotIn("picture", text)   # no traces: the picture verdicts are skipped, not failed

    def test_wall_clock_wins_over_a_drifting_server_clock(self):
        # the server clock (world time) of the joiner drifted 6 s behind the host's in a hitchy session; the wall clock did not
        host = "\n".join(HOST_LINE.replace("role=host ", f"role=host st={i:.2f} utc={1760000000 + i:.3f} ") for i in range(30))
        joiner = "\n".join(CLIENT_LINE.replace("role=client ", f"role=client st={i - 6:.2f} utc={1760000000 + i:.3f} ") for i in range(30))
        joiner += "\nX: [MONetEvent] st=4.00 utc=1760000010.000 joiner frame took 480 ms (a player moving at 450 cm/s)"
        joiner += "\nX: [MONetEvent] st=4.50 utc=1760000010.500 joiner rubber-banded 212 cm (a server correction)"
        r = nd.analyse_logs(host, joiner)
        self.assertEqual(r["clock"], "utc")
        self.assertEqual(r["events"][0][0], 1760000010.0)
        self.assertEqual(r["causes"], {"joiner hitch": 1})
        self.assertIn("UTC", "\n".join(nd.report_lines(r)))

    def test_loading_seconds_are_not_play(self):
        loading = HOST_LINE.replace("role=host ", "role=host st=1.00 play=0 ").replace("satRepl=0%", "satRepl=90%")
        playing = HOST_LINE.replace("role=host ", "role=host st=2.00 play=1 ")
        r = nd.analyse_phase(nd.parse_monet(loading + "\n" + playing), [], [], [])
        self.assertEqual((r["seconds_host"], r["saturated_seconds"]), (1, 0))   # the join's first burst, behind the loading screen, is not a fault

    def test_standing_owner_gives_no_picture_samples(self):
        still = [dict(p, mx=0.0, my=0.0, v=0.0) for p in trajectory(1, 1)]
        self.assertEqual(nd.visual_lag(still, [dict(p, mx=0.0, my=0.0) for p in trajectory(1, 0)])["n"], 0)

    def test_one_log_only_says_which_is_missing(self):
        r = nd.analyse_logs(HOST_LINE, "")
        self.assertIn("joiner's log is missing", "\n".join(nd.report_lines(r)))


if __name__ == "__main__":
    unittest.main()
