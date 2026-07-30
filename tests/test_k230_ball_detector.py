import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "k230_app"))

import config as k230_config
from ball_detector import ConfirmationTracker, circle_to_candidate, select_best_candidate


class CircleWithMagnitudeProperty:
    magnitude = 2300

    def x(self):
        return 480

    def y(self):
        return 270

    def r(self):
        return 18


class BallDetectorTests(unittest.TestCase):
    def test_runtime_defaults_prefer_fast_detection_path(self):
        self.assertEqual(k230_config.DETECTOR_BACKEND, "auto")
        self.assertEqual(k230_config.FRAME_WIDTH, 320)
        self.assertEqual(k230_config.FRAME_HEIGHT, 240)
        self.assertEqual(k230_config.CV_LITE_FRAME_WIDTH, 320)
        self.assertEqual(k230_config.CV_LITE_FRAME_HEIGHT, 240)
        self.assertEqual(k230_config.IMAGE_FRAME_WIDTH, 320)
        self.assertEqual(k230_config.IMAGE_FRAME_HEIGHT, 240)

    def test_rgb888_path_uses_official_ide_display_mode(self):
        main_source = (PROJECT_ROOT / "k230_app" / "main.py").read_text(encoding="utf-8")

        self.assertIn("Display.ST7701", main_source)
        self.assertIn("CV_LITE_DISPLAY_QUALITY", main_source)
        self.assertIn("candidates={}", main_source)

    def test_main_has_a_standalone_canmv_entrypoint_fallback(self):
        main_source = (PROJECT_ROOT / "k230_app" / "main.py").read_text(encoding="utf-8")

        self.assertIn("except ImportError:", main_source)
        self.assertIn("class _DefaultConfig", main_source)
        self.assertIn("class ConfirmationTracker", main_source)

    def test_circle_conversion_accepts_magnitude_property(self):
        result = circle_to_candidate(CircleWithMagnitudeProperty())

        self.assertEqual(result["score"], 2300)
        self.assertEqual(result["radius"], 18)

    def test_select_best_candidate_filters_radius_and_prefers_score(self):
        candidates = [
            {"x": 480, "y": 270, "radius": 18, "score": 2300},
            {"x": 120, "y": 80, "radius": 2, "score": 5000},
            {"x": 700, "y": 420, "radius": 24, "score": 1800},
        ]

        result = select_best_candidate(
            candidates,
            min_radius=4,
            max_radius=120,
        )

        self.assertEqual(result, {"x": 480, "y": 270, "radius": 18, "score": 2300})

    def test_empty_candidates_start_in_searching_state(self):
        tracker = ConfirmationTracker(window=5, hits=3)

        result = tracker.update(None)

        self.assertEqual(result["status"], "SEARCHING")
        self.assertIsNone(result["candidate"])

    def test_three_matching_frames_confirm_found(self):
        tracker = ConfirmationTracker(
            window=5,
            hits=3,
            center_tolerance=20,
            radius_tolerance=8,
        )
        candidate = {"x": 480, "y": 270, "radius": 18, "score": 2300}

        states = [tracker.update(candidate)["status"] for _ in range(3)]

        self.assertEqual(states, ["SEARCHING", "SEARCHING", "FOUND"])
        self.assertEqual(tracker.update(candidate)["candidate"], candidate)

    def test_single_outlier_does_not_confirm_target(self):
        tracker = ConfirmationTracker(window=5, hits=3)
        target = {"x": 480, "y": 270, "radius": 18, "score": 2300}
        outlier = {"x": 100, "y": 80, "radius": 20, "score": 2400}

        states = [
            tracker.update(target)["status"],
            tracker.update(outlier)["status"],
            tracker.update(target)["status"],
        ]

        self.assertEqual(states, ["SEARCHING", "SEARCHING", "SEARCHING"])

    def test_confirmed_target_is_lost_after_consecutive_misses(self):
        tracker = ConfirmationTracker(window=5, hits=3, lost_after=2)
        target = {"x": 480, "y": 270, "radius": 18, "score": 2300}

        for _ in range(3):
            tracker.update(target)

        self.assertEqual(tracker.update(None)["status"], "FOUND")
        lost = tracker.update(None)

        self.assertEqual(lost["status"], "LOST")
        self.assertIsNone(lost["candidate"])


if __name__ == "__main__":
    unittest.main()
