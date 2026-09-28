"""Tests for analyze_perf.py. Run: python -m unittest discover Tools/Perf/tests"""

import contextlib
import io
import json
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import analyze_perf  # noqa: E402

HEADER = "frame,time_s,phase,distance_m,frame_ms,game_ms,render_ms,gpu_ms,ram_mb"
META = "# route=Swing;map=L_PerfRoute_Swing;speed_mps=60;resolution=1920x1080;machine=DEVPC;gpu=NVIDIA GeForce RTX 3060;build=Development"


def make_csv(folder: Path, frames, name="PerfRoute_Swing_1.csv", warmup=(), meta=META) -> Path:
    """frames: list of (frame_ms, game_ms, render_ms, gpu_ms, ram_mb) for measured rows."""
    lines = [meta, HEADER]
    t, idx = 0.0, 0
    for frame_ms in warmup:
        lines.append(f"{idx},{t:.3f},warmup,0.0,{frame_ms},5,5,5,4000")
        idx += 1
        t += frame_ms / 1000
    for i, (frame_ms, game, render, gpu, ram) in enumerate(frames):
        lines.append(f"{idx},{t:.3f},route,{i * 1.0:.1f},{frame_ms},{game},{render},{gpu},{ram}")
        idx += 1
        t += frame_ms / 1000
    path = folder / name
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def good_frames(n=200, frame_ms=12.0):
    return [(frame_ms, 6.0, 6.5, 11.0, 6000)] * n


BUDGETS = json.loads(analyze_perf.DEFAULT_BUDGETS.read_text(encoding="utf-8"))


class AnalyzePerfTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def run_cli(self, *args):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return analyze_perf.main(list(args))

    def gate(self, result, prefix):
        return next(g for g in result.gates if g.name.startswith(prefix))

    def test_good_run_passes_every_gate(self):
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, good_frames())), BUDGETS)
        self.assertTrue(result.passed, [g for g in result.gates if g.status != "PASS"])
        self.assertAlmostEqual(result.metrics["avg_frame_ms"], 12.0)
        self.assertEqual(result.run.metadata["gpu"], "NVIDIA GeForce RTX 3060")

    def test_hitch_over_50ms_fails_and_is_located(self):
        frames = good_frames(199) + [(61.0, 20, 20, 20, 6000)]
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, frames)), BUDGETS)
        self.assertEqual(self.gate(result, "Hitches").status, "FAIL")
        self.assertEqual(len(result.hitches), 1)
        self.assertEqual(result.hitches[0]["distance_m"], 199.0)
        self.assertFalse(result.passed)

    def test_exactly_50ms_is_not_a_hitch(self):
        frames = good_frames(199) + [(50.0, 6, 6, 11, 6000)]
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, frames)), BUDGETS)
        self.assertEqual(result.hitches, [])

    def test_warmup_frames_are_excluded(self):
        path = make_csv(self.dir, good_frames(), warmup=[250.0, 120.0, 80.0])
        result = analyze_perf.evaluate(analyze_perf.load_run(path), BUDGETS)
        self.assertEqual(result.run.warmup_frames, 3)
        self.assertEqual(result.hitches, [])
        self.assertTrue(result.passed)

    def test_one_percent_low_uses_slowest_one_percent(self):
        # 300 frames -> slowest 3 frames: 30, 30, 30 ms -> 33.3 FPS, below the 45 FPS gate.
        frame_ms = [10.0] * 297 + [30.0] * 3
        self.assertAlmostEqual(analyze_perf.one_percent_low_fps(frame_ms), 1000 / 30)
        frames = [(f, 5, 5, 8, 5000) for f in frame_ms]
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, frames)), BUDGETS)
        self.assertEqual(self.gate(result, "1% low").status, "FAIL")
        self.assertEqual(self.gate(result, "Average frame").status, "PASS")

    def test_percentile_matches_linear_interpolation(self):
        self.assertAlmostEqual(analyze_perf.percentile([1, 2, 3, 4], 50), 2.5)
        self.assertAlmostEqual(analyze_perf.percentile([10, 20, 30, 40, 50], 95), 48.0)
        self.assertEqual(analyze_perf.percentile([7], 99), 7)

    def test_thread_budgets(self):
        frames = [(15.0, 9.0, 6.0, 12.0, 6000)] * 100
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, frames)), BUDGETS)
        self.assertEqual(self.gate(result, "Average game thread").status, "FAIL")
        self.assertEqual(self.gate(result, "Average render thread").status, "PASS")

    def test_unreported_gpu_time_is_not_applicable(self):
        frames = [(12.0, 6.0, 6.0, 0.0, 6000)] * 100
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, frames)), BUDGETS)
        self.assertEqual(self.gate(result, "Average GPU").status, "N/A")
        self.assertTrue(result.passed)

    def test_ram_budget(self):
        frames = good_frames(99) + [(12.0, 6, 6, 11, 13000)]
        result = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, frames)), BUDGETS)
        self.assertEqual(self.gate(result, "Peak process RAM").status, "FAIL")

    def test_regression_against_baseline(self):
        base = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, good_frames(frame_ms=12.0), "a.csv")), BUDGETS)
        baseline = analyze_perf.baseline_payload(base)

        slightly_slower = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, good_frames(frame_ms=12.4), "b.csv")), BUDGETS)
        slightly_slower.regressions = analyze_perf.compare_to_baseline(slightly_slower, baseline, 5.0)
        self.assertTrue(slightly_slower.passed)

        much_slower = analyze_perf.evaluate(analyze_perf.load_run(make_csv(self.dir, good_frames(frame_ms=13.5), "c.csv")), BUDGETS)
        much_slower.regressions = analyze_perf.compare_to_baseline(much_slower, baseline, 5.0)
        failed = {g.name for g in much_slower.regressions if g.status == "FAIL"}
        self.assertIn("avg_frame_ms", failed)
        self.assertIn("one_percent_low_fps", failed)
        self.assertFalse(much_slower.passed)

    def test_folder_input_uses_newest_csv(self):
        old = make_csv(self.dir, good_frames(frame_ms=20.0), "PerfRoute_Swing_old.csv")
        new = make_csv(self.dir, good_frames(frame_ms=11.0), "PerfRoute_Swing_new.csv")
        now = time.time()
        os.utime(old, (now - 100, now - 100))
        os.utime(new, (now, now))
        self.assertEqual(analyze_perf.resolve_csv(self.dir), new)

    def test_missing_column_is_an_input_error(self):
        path = self.dir / "bad.csv"
        path.write_text("frame,time_s,phase\n0,0,route\n", encoding="utf-8")
        with self.assertRaises(analyze_perf.InputError):
            analyze_perf.load_run(path)

    def test_cli_writes_baseline_then_detects_regression(self):
        budgets = self.dir / "budgets.json"
        budgets.write_text(json.dumps(BUDGETS), encoding="utf-8")
        baseline = self.dir / "baselines" / "Swing.json"
        report = self.dir / "report.md"

        first = make_csv(self.dir, good_frames(frame_ms=12.0), "first.csv")
        self.assertEqual(self.run_cli(str(first), "--budgets", str(budgets), "--baseline", str(baseline),
                                      "--write-baseline", str(baseline), "--report", str(report)), 0)
        self.assertTrue(baseline.is_file())
        self.assertIn("Verdict: PASS", report.read_text(encoding="utf-8"))

        second = make_csv(self.dir, good_frames(frame_ms=14.0), "second.csv")
        self.assertEqual(self.run_cli(str(second), "--budgets", str(budgets), "--baseline", str(baseline)), 1)

    def test_cli_bad_path_returns_2(self):
        self.assertEqual(self.run_cli(str(self.dir / "nope.csv")), 2)


if __name__ == "__main__":
    unittest.main()
