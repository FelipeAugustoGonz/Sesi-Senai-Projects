"""
Unit tests for EcoFlutuador POC decision engine and serial protocol.
"""
import sys
import os
import unittest

# Add project root to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.decision import DecisionEngine, DecisionConfig
from core.detector import Detection
from core.serial_link import encode_command, encode_power, parse_response, SerialLink


class TestDecisionEngine(unittest.TestCase):
    """Test zone-based decision logic."""

    def setUp(self):
        self.config = DecisionConfig(frame_width=320, zones=3)
        self.engine = DecisionEngine(self.config)

    def _make_detection(self, x: int, y: int = 100, w: int = 60, h: int = 100, conf: float = 0.9) -> Detection:
        return Detection(bbox=[x, y, w, h], class_name="bottle", confidence=conf)

    def test_left_zone_returns_a(self):
        """Object in left third -> 'a' (turn left)."""
        # Left zone: 0-106px
        det = self._make_detection(x=50)  # center at ~80px
        result = self.engine.decide([det])
        self.assertEqual(result, 'a')

    def test_center_zone_returns_w(self):
        """Object in center third -> 'w' (forward)."""
        # Center zone: 106-212px
        det = self._make_detection(x=150)  # center at ~180px
        result = self.engine.decide([det])
        self.assertEqual(result, 'w')

    def test_right_zone_returns_d(self):
        """Object in right third -> 'd' (turn right)."""
        # Right zone: 212-320px
        det = self._make_detection(x=250)  # center at ~280px
        result = self.engine.decide([det])
        self.assertEqual(result, 'd')

    def test_no_detection_returns_stop(self):
        """No objects -> 's' (stop)."""
        result = self.engine.decide([])
        self.assertEqual(result, 's')

    def test_send_only_on_change(self):
        """Repeated same decision returns None (no change)."""
        det = self._make_detection(x=50)
        result1 = self.engine.decide([det])
        result2 = self.engine.decide([det])
        self.assertEqual(result1, 'a')
        self.assertIsNone(result2)  # No change

    def test_decision_changes_when_zone_changes(self):
        """Decision updates when object moves to new zone."""
        det_left = self._make_detection(x=50)
        det_center = self._make_detection(x=150)

        r1 = self.engine.decide([det_left])
        r2 = self.engine.decide([det_center])

        self.assertEqual(r1, 'a')
        self.assertEqual(r2, 'w')

    def test_multiple_detections_picks_largest(self):
        """Should pick detection with largest area."""
        det_small = self._make_detection(x=50, w=20, h=20)    # area=400, left zone
        det_large = self._make_detection(x=250, w=80, h=80)   # area=6400, right zone
        # Large is in right zone, should win
        result = self.engine.decide([det_small, det_large])
        self.assertEqual(result, 'd')

    def test_reset_clears_last_command(self):
        """Reset allows same command to be sent again."""
        det = self._make_detection(x=50)
        self.engine.decide([det])  # 'a'
        self.engine.reset()
        result = self.engine.decide([det])  # Should be 'a' again
        self.assertEqual(result, 'a')

    def test_custom_frame_width(self):
        """Zones scale with frame width."""
        config = DecisionConfig(frame_width=640, zones=3)
        engine = DecisionEngine(config)
        # Left zone: 0-213px
        det = self._make_detection(x=100)  # center at 130px -> left
        result = engine.decide([det])
        self.assertEqual(result, 'a')


class TestSerialProtocol(unittest.TestCase):
    """Test serial command encoding and response parsing."""

    def test_encode_movement_commands(self):
        """All valid movement commands encode correctly."""
        for cmd in ['w', 'a', 's', 'd', 'q', 'e']:
            encoded = encode_command(cmd)
            self.assertEqual(encoded, f"{cmd}\n".encode('utf-8'))

    def test_encode_invalid_command_raises(self):
        """Invalid command raises ValueError."""
        with self.assertRaises(ValueError):
            encode_command('x')
        with self.assertRaises(ValueError):
            encode_command('W')  # uppercase not allowed

    def test_encode_power_commands(self):
        """Power commands encode correctly."""
        self.assertEqual(encode_power(1, 0), b"P1000\n")
        self.assertEqual(encode_power(1, 45), b"P1045\n")
        self.assertEqual(encode_power(1, 100), b"P1100\n")
        self.assertEqual(encode_power(2, 50), b"P2050\n")

    def test_encode_power_clamps_values(self):
        """Power values clamped to 0-100."""
        self.assertEqual(encode_power(1, -10), b"P1000\n")
        self.assertEqual(encode_power(1, 150), b"P1100\n")

    def test_encode_power_invalid_motor_raises(self):
        """Invalid motor raises ValueError."""
        with self.assertRaises(ValueError):
            encode_power(0, 50)
        with self.assertRaises(ValueError):
            encode_power(3, 50)

    def test_parse_cmd_ok(self):
        """Parse CMD_OK response."""
        result = parse_response("CMD_OK:w")
        self.assertEqual(result, {"type": "cmd_ok", "command": "w"})

        result = parse_response("CMD_OK:a")
        self.assertEqual(result, {"type": "cmd_ok", "command": "a"})

    def test_parse_pwr_ok(self):
        """Parse PWR_OK response."""
        result = parse_response("PWR_OK:1:45")
        self.assertEqual(result, {"type": "pwr_ok", "motor": 1, "value": 45})

        result = parse_response("PWR_OK:2:100")
        self.assertEqual(result, {"type": "pwr_ok", "motor": 2, "value": 100})

    def test_parse_unknown_returns_none(self):
        """Unknown response returns None."""
        self.assertIsNone(parse_response("UNKNOWN"))
        self.assertIsNone(parse_response(""))
        self.assertIsNone(parse_response("GARBAGE"))

    def test_serial_link_valid_commands(self):
        """SerialLink.VALID_COMMANDS contains expected set."""
        self.assertEqual(SerialLink.VALID_COMMANDS, {'w', 'a', 's', 'd', 'q', 'e'})


class TestDetectionState(unittest.TestCase):
    """Test state serialization."""

    def test_empty_state(self):
        from core.state import DetectionState
        state = DetectionState.empty(timestamp=1234567890.0, fps=5.5)
        self.assertFalse(state.detected)
        self.assertEqual(state.decision, "s")
        self.assertEqual(state.fps, 5.5)
        self.assertEqual(state.timestamp, 1234567890.0)

        json_data = state.to_json()
        self.assertEqual(json_data["detected"], False)
        self.assertEqual(json_data["decision"], "s")

    def test_from_detection(self):
        from core.state import DetectionState
        state = DetectionState.from_detection(
            box=[100, 80, 60, 100],
            class_name="bottle",
            confidence=0.91,
            decision="w",
            inference_ms=180.5,
            fps=5.5,
            frame_width=320,
            frame_height=240
        )
        self.assertTrue(state.detected)
        self.assertEqual(state.object, "bottle")
        self.assertEqual(state.confidence, 0.91)
        self.assertEqual(state.center_x, round((100 + 30) / 320, 3))  # 0.406
        self.assertEqual(state.center_y, round((80 + 50) / 240, 3))   # 0.542
        self.assertEqual(state.bbox, [100, 80, 60, 100])
        self.assertEqual(state.decision, "w")
        self.assertEqual(state.inference_ms, 180.5)
        self.assertEqual(state.fps, 5.5)


if __name__ == "__main__":
    unittest.main(verbosity=2)