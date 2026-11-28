import unittest
from lan_bridge.action_phase_audit import summarize_phase


def sample(tick, frame, seq=1):
    return f"LAN action phase: role=1 player=2 seq={seq} tick={tick} frame={frame} name=KatanaSlash.\n"


class PhaseTests(unittest.TestCase):
    def test_frame_is_not_tick(self):
        report = summarize_phase(sample(100, 0) + sample(106, 2))[0]
        self.assertEqual(report["frames_per_tick"], 0.3333)
        self.assertFalse(report["frame_reversed"])

    def test_repeated_actions_separated_by_sequence(self):
        self.assertEqual(len(summarize_phase(sample(100, 2) + sample(200, 0, 2))), 2)

    def test_duplicate_capture_rejected(self):
        with self.assertRaises(ValueError):
            summarize_phase(sample(100, 2) * 2)

    def test_reverse_frame_reported(self):
        self.assertTrue(summarize_phase(sample(100, 8) + sample(106, 2))[0]["frame_reversed"])

    def test_empty_has_no_measurement(self):
        self.assertEqual(summarize_phase(""), [])
