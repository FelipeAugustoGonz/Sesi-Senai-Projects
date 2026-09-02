#!/usr/bin/env python3
"""
EcoFlutuador POC - Main entry point.
Runs the autonomous navigation pipeline: Camera -> Detector -> Decision -> Serial.
Modes:
  --mock       : Simulated camera + serial (no hardware)
  --dry-run    : Real camera + detector + decision, NO serial commands sent
  --real       : Real camera + detector + decision + serial to ESP32
"""
import sys
import os
import argparse
import time
import logging
import signal
import json
import yaml
from typing import Optional

# Add project root to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.camera import Camera, CameraConfig, list_cameras
from core.detector import Detector, ModelConfig
from core.decision import DecisionEngine, DecisionConfig
from core.serial_link import SerialLink, SerialConfig
from core.state import DetectionState
from api.status_server import StatusServer
from mock.camera_mock import MockCamera, MockCameraConfig
from mock.serial_mock import MockSerialLink, MockSerialConfig


class Pipeline:
    """Main processing pipeline."""

    def __init__(self, config: dict, mode: str):
        self.config = config
        self.mode = mode
        self._logger = logging.getLogger(__name__)
        self._running = False
        self._camera = None
        self._detector = None
        self._decision = None
        self._serial = None
        self._status_server = None
        self._frame_count = 0
        self._start_time = 0
        self._last_fps_log = 0
        self._frames_since_log = 0
        self._target_fps = config.get("performance", {}).get("target_fps", 10)

    def setup(self) -> bool:
        """Initialize all components based on mode."""
        cam_cfg = self.config.get("camera", {})
        model_cfg = self.config.get("model", {})
        decision_cfg = self.config.get("decision", {})
        serial_cfg = self.config.get("serial", {})
        api_cfg = self.config.get("api", {})
        perf_cfg = self.config.get("performance", {})

        # Camera
        if self.mode == "mock":
            self._camera = MockCamera(MockCameraConfig(
                width=cam_cfg.get("width", 320),
                height=cam_cfg.get("height", 240),
                fps=cam_cfg.get("fps", 10)
            ))
        else:
            self._camera = Camera(CameraConfig(
                backend=cam_cfg.get("backend", "V4L2"),
                width=cam_cfg.get("width", 320),
                height=cam_cfg.get("height", 240),
                fps=cam_cfg.get("fps", 10),
                buffer_size=cam_cfg.get("buffer_size", 1),
                picamera2_format=cam_cfg.get("picamera2", {}).get("format", "RGB888"),
                picamera2_transform=cam_cfg.get("picamera2", {}).get("transform", {})
            ))

        if not self._camera.start():
            self._logger.error("Failed to start camera")
            return False

        # Detector (only for real/dry-run modes)
        if self.mode != "mock":
            try:
                self._detector = Detector(ModelConfig(
                    weights=model_cfg.get("weights", "models/frozen_inference_graph.pb"),
                    config=model_cfg.get("config", "models/ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt"),
                    classes=model_cfg.get("classes", "models/coco.names"),
                    input_size=tuple(model_cfg.get("input_size", [320, 320])),
                    conf_threshold=model_cfg.get("conf_threshold", 0.45),
                    nms_threshold=model_cfg.get("nms_threshold", 0.2),
                    target_classes=model_cfg.get("target_classes", ["bottle"])
                ))
            except FileNotFoundError as e:
                self._logger.error(str(e))
                self._camera.stop()
                return False
            except Exception as e:
                self._logger.error(f"Detector initialization failed: {e}")
                self._camera.stop()
                return False

        # Decision engine
        decision_cfg = self.config.get("decision", {})
        # frame_width is dynamic — passed from actual frame later
        self._decision = DecisionEngine(DecisionConfig(
            zones=decision_cfg.get("zones", 3),
            default_command=decision_cfg.get("default_command", "s"),
            send_only_on_change=decision_cfg.get("send_only_on_change", True)
        ))

        # Serial (only for real mode)
        if self.mode == "real":
            self._serial = SerialLink(SerialConfig(
                port=serial_cfg.get("port", "/dev/ttyUSB0"),
                baudrate=serial_cfg.get("baudrate", 115200),
                timeout=serial_cfg.get("timeout", 1.0),
                reconnect_delay=serial_cfg.get("reconnect_delay", 2.0),
                auto_detect=serial_cfg.get("auto_detect", True)
            ), on_response=self._on_serial_response)

            if not self._serial.start():
                self._logger.error("Failed to start serial communication")
                self._camera.stop()
                return False
            self._logger.info("Serial communication started (REAL MODE - commands will be sent to ESP32)")
        elif self.mode == "dry-run":
            self._logger.info("DRY-RUN MODE: Camera + Detection + Decision active, NO serial commands sent")
        else:  # mock
            self._serial = MockSerialLink(MockSerialConfig(response_delay=0.001))
            self._serial.start()
            self._logger.info("MOCK MODE: Simulated camera + serial")

        # Status API (optional)
        if api_cfg.get("enabled", True):
            self._status_server = StatusServer(
                host=api_cfg.get("host", "0.0.0.0"),
                port=api_cfg.get("port", 8080)
            )
            self._status_server.start()

        self._running = True
        self._start_time = time.time()
        self._last_fps_log = self._start_time
        return True

    def _on_serial_response(self, line: str) -> None:
        """Handle response from ESP32."""
        self._logger.debug(f"ESP32 response: {line}")

    def _process_frame(self, frame) -> Optional[DetectionState]:
        """Process a single frame: detect -> decide -> (send)."""
        inference_start = time.perf_counter()

        # Detection
        if self.mode == "mock":
            # In mock mode, create synthetic detection from frame analysis
            # For simplicity, we'll detect green pixels as "bottle"
            detections = self._mock_detect(frame)
        else:
            detections = self._detector.detect(frame)

        inference_ms = (time.perf_counter() - inference_start) * 1000

        # Decision — pass actual frame width for proportional zones
        frame_width = frame.shape[1]
        decision = self._decision.decide(detections, frame_width=frame_width)
        cmd_to_send = decision if decision else self._decision._last_command

        # Build state
        if detections:
            best = max(detections, key=lambda d: d.bbox[2] * d.bbox[3])
            state = DetectionState.from_detection(
                box=best.bbox,
                class_name=best.class_name,
                confidence=best.confidence,
                decision=cmd_to_send or "s",
                inference_ms=inference_ms,
                fps=self._current_fps(),
                frame_width=frame.shape[1],
                frame_height=frame.shape[0]
            )
        else:
            state = DetectionState.empty(
                timestamp=time.time(),
                fps=self._current_fps()
            )
            state.decision = cmd_to_send or "s"

        # Send command (only in real mode, only if decision changed)
        if self.mode == "real" and decision:
            self._serial.send_command(decision)
            self._logger.info(f"SENT TO ESP32: {decision}")

        return state

    def _mock_detect(self, frame) -> list:
        """Simple color-based detection for mock frames (green bottle)."""
        import cv2
        import numpy as np
        from core.detector import Detection

        # Convert to HSV and threshold for green
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        lower_green = np.array([35, 50, 50])
        upper_green = np.array([85, 255, 255])
        mask = cv2.inRange(hsv, lower_green, upper_green)

        # Find contours
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        detections = []
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area > 500:  # Minimum area
                x, y, w, h = cv2.boundingRect(cnt)
                detections.append(Detection([x, y, w, h], "bottle", 0.9))

        return detections

    def _current_fps(self) -> float:
        """Calculate current FPS."""
        elapsed = time.time() - self._start_time
        return self._frame_count / elapsed if elapsed > 0 else 0

    def _log_state(self, state: DetectionState) -> None:
        """Log state as JSON to stdout."""
        print(json.dumps(state.to_json()), flush=True)

    def _log_fps(self) -> None:
        """Log FPS periodically."""
        now = time.time()
        if now - self._last_fps_log >= 5.0:
            fps = self._frames_since_log / (now - self._last_fps_log)
            self._logger.info(f"FPS: {fps:.1f} | Total frames: {self._frame_count} | Inference: {getattr(self, '_last_inference_ms', 0):.1f}ms")
            self._last_fps_log = now
            self._frames_since_log = 0

    def run(self) -> int:
        """Main processing loop."""
        self._logger.info(f"Starting pipeline in {self.mode.upper()} mode")
        self._logger.info(f"Target FPS: {self._target_fps}")

        try:
            while self._running:
                loop_start = time.perf_counter()

                # Get frame
                frame = self._camera.get_frame()
                if frame is None:
                    time.sleep(0.001)
                    continue

                self._frame_count += 1
                self._frames_since_log += 1

                # Process frame
                state = self._process_frame(frame)

                # Log state
                self._log_state(state)

                # Update status API
                if self._status_server:
                    self._status_server.update_state(state)

                # FPS logging
                self._log_fps()

                # Maintain target FPS
                elapsed = time.perf_counter() - loop_start
                target_interval = 1.0 / self._target_fps
                sleep_time = target_interval - elapsed
                if sleep_time > 0:
                    time.sleep(sleep_time)

        except KeyboardInterrupt:
            self._logger.info("Interrupted by user (Ctrl+C)")
        except Exception as e:
            self._logger.error(f"Pipeline error: {e}", exc_info=True)
            return 1
        finally:
            self.shutdown()

        return 0

    def shutdown(self) -> None:
        """Clean shutdown of all components."""
        self._logger.info("Shutting down...")
        self._running = False

        if self._camera:
            self._camera.stop()
        if self._serial:
            self._serial.close()
        if self._status_server:
            self._status_server.stop()

        total_time = time.time() - self._start_time
        avg_fps = self._frame_count / total_time if total_time > 0 else 0
        self._logger.info(f"Pipeline stopped: {self._frame_count} frames in {total_time:.1f}s = {avg_fps:.1f} FPS avg")


def load_config(config_path: str) -> dict:
    """Load configuration from YAML file."""
    with open(config_path, 'r') as f:
        return yaml.safe_load(f)


def setup_logging(config: dict):
    """Configure logging from config."""
    log_cfg = config.get("logging", {})
    level = log_cfg.get("level", "INFO")
    json_format = log_cfg.get("format", "json") == "json"

    if json_format:
        fmt = '{"timestamp": "%(asctime)s", "level": "%(levelname)s", "logger": "%(name)s", "message": "%(message)s"}'
    else:
        fmt = '%(asctime)s | %(levelname)-8s | %(name)s | %(message)s'

    logging.basicConfig(
        level=getattr(logging, level.upper()),
        format=fmt,
        datefmt='%H:%M:%S'
    )


def signal_handler(signum, frame):
    """Handle shutdown signals."""
    raise KeyboardInterrupt()


def main():
    parser = argparse.ArgumentParser(description="EcoFlutuador POC - Autonomous Navigation")
    parser.add_argument("mode", choices=["mock", "dry-run", "real"],
                        help="Execution mode: mock (simulated), dry-run (camera+detect, no serial), real (full)")
    parser.add_argument("-c", "--config", default="config.yaml",
                        help="Path to config.yaml")
    parser.add_argument("--list-cameras", action="store_true",
                        help="List available cameras and exit")
    parser.add_argument("--log-level", default=None,
                        help="Override log level (DEBUG, INFO, WARNING, ERROR)")
    args = parser.parse_args()

    # List cameras
    if args.list_cameras:
        print("Available cameras:")
        for idx in list_cameras():
            print(f"  {idx}")
        # Also list Picamera2 modes if available
        try:
            from picamera2 import Picamera2
            p = Picamera2()
            modes = p.sensor_modes
            print(f"\nPicamera2 sensor modes ({len(modes)} available):")
            for m in modes:
                size = m.get('size', (0, 0))
                fmt = m.get('format', 'N/A')
                fps = m.get('fps', 'N/A')
                print(f"  {size[0]}x{size[1]} @ {fps}fps ({fmt})")
            p.close()
        except ImportError:
            pass
        return 0

    # Load config
    config_path = args.config
    if not os.path.exists(config_path):
        print(f"Config file not found: {config_path}")
        print("Copy config.yaml.example to config.yaml and adjust paths.")
        return 1

    config = load_config(config_path)

    # Override log level if provided
    if args.log_level:
        config.setdefault("logging", {})["level"] = args.log_level

    setup_logging(config)

    # Safety check for real mode
    if args.mode == "real":
        print("\n⚠️  REAL MODE: Commands WILL be sent to ESP32 via serial.")
        print("Ensure ESP32 is connected and firmware is running.")
        confirm = input("Continue? (yes/no): ").strip().lower()
        if confirm not in ("yes", "y"):
            print("Aborted.")
            return 0

    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # Run pipeline
    pipeline = Pipeline(config, args.mode)
    if not pipeline.setup():
        return 1

    return pipeline.run()


if __name__ == "__main__":
    sys.exit(main())