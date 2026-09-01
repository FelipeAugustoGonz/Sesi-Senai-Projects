#!/usr/bin/env python3
"""
Mock runner for EcoFlutuador POC.
Runs the full pipeline with simulated camera and serial.
Use for testing logic on notebook without hardware.
"""
import sys
import os
import argparse
import time
import logging
import signal
import json

# Add project root to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.camera import CameraConfig
from core.detector import ModelConfig
from core.decision import DecisionConfig
from core.serial_link import SerialConfig
from core.state import DetectionState
from mock.camera_mock import MockCamera, MockCameraConfig
from mock.serial_mock import MockSerialLink, MockSerialConfig


def setup_logging(level: str = "INFO", json_format: bool = True):
    """Configure logging."""
    log_format = '%(asctime)s | %(levelname)-8s | %(name)s | %(message)s'
    if json_format:
        log_format = '{"timestamp": "%(asctime)s", "level": "%(levelname)s", "logger": "%(name)s", "message": "%(message)s"}'

    logging.basicConfig(
        level=getattr(logging, level.upper()),
        format=log_format,
        datefmt='%H:%M:%S'
    )


def run_mock(num_frames: int = 100, target_fps: int = 10, verbose: bool = True):
    """
    Run mock pipeline.
    Returns True if successful.
    """
    logger = logging.getLogger("mock_runner")

    # Initialize components
    cam = MockCamera(MockCameraConfig(
        width=320, height=240, fps=target_fps
    ))
    serial = MockSerialLink(MockSerialConfig(response_delay=0.001))

    # For detector, we need model files - in mock mode we skip actual detection
    # and inject synthetic detections based on mock camera position
    # This is a simplified test that validates decision logic, not the detector

    if not cam.start():
        logger.error("Failed to start mock camera")
        return False

    if not serial.start():
        logger.error("Failed to start mock serial")
        cam.stop()
        return False

    logger.info(f"Starting mock run: {num_frames} frames @ {target_fps} FPS")

    frame_count = 0
    decisions_made = []
    start_time = time.time()
    last_fps_log = start_time
    frames_since_log = 0

    try:
        while frame_count < num_frames:
            loop_start = time.time()

            # Get frame from mock camera
            frame = cam.get_frame()
            if frame is None:
                time.sleep(0.01)
                continue

            frame_count += 1
            frames_since_log += 1

            # Simulate detection based on known bottle position in mock frame
            # The mock camera draws bottle at known position - we can calculate expected decision
            # For simplicity, we'll extract the "detection" from the frame by finding green pixels
            # But simpler: we know the mock camera's internal position logic
            # Let's just use the decision engine directly with synthetic detections

            # In a real test, we'd run the detector. Here we simulate the decision logic test.
            # We'll create a synthetic detection based on the mock camera's current bottle position.

            # Calculate expected position from mock camera's internal state
            elapsed = time.time() - cam._start_time
            cycle = (elapsed * cam.config.bottle_speed) % 4
            if cycle < 1:
                zone_idx = cycle
            elif cycle < 2:
                zone_idx = 2 - (cycle - 1)
            elif cycle < 3:
                zone_idx = 3 - (cycle - 2)
            else:
                zone_idx = 4 - (cycle - 3)

            zone_fraction = zone_idx / 2.0
            cx = int(zone_fraction * cam.config.width)
            bw, bh = cam.config.bottle_size
            x = cx - bw // 2
            y = cam.config.height // 2 - bh // 2

            # Create synthetic detection
            from core.detector import Detection
            detections = [Detection(bbox=[x, y, bw, bh], class_name="bottle", confidence=0.9)]

            # Decision
            from core.decision import DecisionEngine
            decision_engine = DecisionEngine(DecisionConfig(frame_width=320))
            decision = decision_engine.decide(detections)

            if decision:
                serial.send_command(decision)
                decisions_made.append(decision)

            # Create state for logging
            state = DetectionState.from_detection(
                box=[x, y, bw, bh],
                class_name="bottle",
                confidence=0.9,
                decision=decision or decision_engine._last_command,
                inference_ms=1.0,
                fps=target_fps,
                frame_width=320,
                frame_height=240
            )

            # Log state
            if verbose:
                print(json.dumps(state.to_json()))

            # FPS logging
            if time.time() - last_fps_log >= 5.0:
                actual_fps = frames_since_log / (time.time() - last_fps_log)
                logger.info(f"FPS: {actual_fps:.1f}")
                last_fps_log = time.time()
                frames_since_log = 0

            # Maintain target FPS
            elapsed = time.time() - loop_start
            sleep_time = (1.0 / target_fps) - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    finally:
        cam.stop()
        serial.close()

    total_time = time.time() - start_time
    avg_fps = frame_count / total_time if total_time > 0 else 0

    logger.info(f"Mock run complete: {frame_count} frames in {total_time:.1f}s = {avg_fps:.1f} FPS")
    logger.info(f"Decisions made: {decisions_made}")
    logger.info(f"Unique decisions: {set(decisions_made)}")

    # Validate decisions
    expected = {'a', 'w', 'd'}
    actual = set(decisions_made)
    if expected.issubset(actual):
        logger.info("SUCCESS: All zone decisions (left/center/right) were made")
        return True
    else:
        logger.warning(f"PARTIAL: Missing decisions. Expected {expected}, got {actual}")
        return False


def main():
    parser = argparse.ArgumentParser(description="EcoFlutuador POC Mock Runner")
    parser.add_argument("--frames", type=int, default=100, help="Number of frames to process")
    parser.add_argument("--fps", type=int, default=10, help="Target FPS")
    parser.add_argument("--log-level", default="INFO", help="Log level")
    parser.add_argument("--no-json", action="store_true", help="Disable JSON logging")
    parser.add_argument("--quiet", action="store_true", help="Suppress per-frame JSON output")
    args = parser.parse_args()

    setup_logging(args.log_level, json_format=not args.no_json)

    success = run_mock(
        num_frames=args.frames,
        target_fps=args.fps,
        verbose=not args.quiet
    )

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()