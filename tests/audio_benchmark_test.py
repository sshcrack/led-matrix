#!/usr/bin/env python3
"""Check scoring edge cases and run the report against generated PCM."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import audio_benchmark as benchmark  # noqa: E402

ANALYZER = Path(sys.argv.pop(1)).resolve()


class BenchmarkTest(unittest.TestCase):
    def test_matching_prefers_cardinality_then_nearest(self):
        self.assertEqual(benchmark.match_events([1, 1.08], [0.94, 1.02], 0.07),
                         [(1, 0.94), (1.08, 1.02)])
        result = benchmark.score_events([1], [0.95, 1.01, 1.2], 0.07)
        self.assertEqual((result["tp"], result["fp"], result["fn"]), (1, 2, 0))
        self.assertAlmostEqual(result["median_signed_ms"], 10)
        self.assertEqual(result["extra_s"], [0.95, 1.2])
        self.assertIsNone(benchmark.score_events([], [], 0.07)["f1"])

    def test_tempo_reports_octave_errors_and_recovery(self):
        frames = [{"time": time, "bpm": bpm, "confidence": 0.8}
                  for time, bpm in [(1, 60), (2, 120), (4, 120), (5, 150)]]
        result = benchmark.score_tempo(frames, [(0, 4, 120), (4, 7, 150)], 7, [
            {"start_s": 0, "end_s": 4, "kind": "acquire"},
            {"start_s": 4, "end_s": 7, "kind": "recover"},
        ])
        self.assertAlmostEqual(result["strict_accuracy"], 4 / 7)
        self.assertAlmostEqual(result["octave_tolerant_accuracy"], 5 / 7)
        self.assertEqual([entry["lock_delay_s"] for entry in result["locks"]], [2, 1])
        self.assertEqual([jump["time_s"] for jump in result["estimated_jumps"]], [2, 5])

    def test_report_against_analyzer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run([sys.executable, ROOT / "tests/fixtures/audio_benchmark/generate.py", root], check=True)
            results = root / "report"
            subprocess.run([sys.executable, ROOT / "scripts/audio_benchmark.py", "--analyzer", ANALYZER,
                            "--manifest", root / "manifest.json", "--corpus-root", root, "--output", results],
                           check=True)
            report = json.loads((results / "report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["mode"], "offline_sample_clock")
            self.assertEqual(len(report["tracks"]), 4)
            regular, stress = report["tracks"][:2]
            gap = "max_gap_ms"
            self.assertGreater(stress["analysis_cadence"][gap], regular["analysis_cadence"][gap])
            self.assertEqual(regular["analyzer"]["sample_rate"], 24000)
            self.assertEqual(regular["silence"][0]["false_beat_times_s"], [])
            self.assertEqual(regular["sha256"], stress["sha256"])
            for cadence in ("regular", "jitter_skip"):
                metadata, rows = benchmark.read_observations(
                    ANALYZER, root / "pulse-change.wav", cadence, root / "transient.csv")
                del metadata
                # The first kick gets a spectral response well before tempo lock.
                self.assertTrue(any(0.5 <= row["time"] < 0.65 and row["bass"] > 0.1
                                    and row["onset_strength"] > 0.1 and row["bpm"] == 0 for row in rows))


if __name__ == "__main__":
    unittest.main()
