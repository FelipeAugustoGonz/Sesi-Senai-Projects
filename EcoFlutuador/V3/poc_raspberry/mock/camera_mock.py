"""
Mock camera for EcoFlutuador POC testing.
Generates synthetic frames with a moving "bottle" for testing decision logic.
"""
import cv2
import numpy as np
import time
import threading
import logging
from typing import Optional, Any, Tuple
from dataclasses import dataclass


@dataclass
class MockCameraConfig:
    width: int = 320
    height: int = 240
    fps: int = 10
    bottle_speed: float = 0.5  # zones per second
    bottle_size: Tuple[int, int] = (60, 100)  # w, h
    noise_level: float = 0.05


class MockCamera:
    """
    Simulates a camera with a moving bottle.
    Bottle moves left->center->right->center->left in a sinusoidal pattern.
    Generates frames compatible with the detector input format.
    """

    def __init__(self, config: MockCameraConfig):
        self.config = config
        self._frame: Optional[Any] = None
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._logger = logging.getLogger(__name__)
        self._start_time = time.time()
        self._zone_positions = [0.15, 0.5, 0.85]  # left, center, right as fraction of width

    def start(self) -> bool:
        """Start frame generation thread."""
        if self._running:
            return True

        self._running = True
        self._start_time = time.time()
        self._thread = threading.Thread(target=self._generate_loop, daemon=True)
        self._thread.start()
        self._logger.info(f"Mock camera started: {self.config.width}x{self.config.height} @ {self.config.fps} FPS")
        return True

    def _generate_loop(self) -> None:
        """Background thread: generate synthetic frames."""
        frame_interval = 1.0 / self.config.fps

        while self._running:
            loop_start = time.time()

            # Calculate bottle position (sinusoidal: left <-> center <-> right)
            elapsed = time.time() - self._start_time
            # Cycle through zones: 0=left, 1=center, 2=right, 1=center, 0=left...
            cycle = (elapsed * self.config.bottle_speed) % 4
            if cycle < 1:
                zone_idx = cycle  # 0 -> 1 (left to center)
            elif cycle < 2:
                zone_idx = 2 - (cycle - 1)  # 1 -> 2 (center to right)
            elif cycle < 3:
                zone_idx = 3 - (cycle - 2)  # 2 -> 1 (right to center)
            else:
                zone_idx = 4 - (cycle - 3)  # 1 -> 0 (center to left)

            # Interpolate position
            zone_fraction = zone_idx / 2.0  # 0.0, 0.5, 1.0, 0.5, 0.0...
            cx = int(zone_fraction * self.config.width)
            cy = self.config.height // 2
            bw, bh = self.config.bottle_size
            x = cx - bw // 2
            y = cy - bh // 2

            # Create frame: dark background with green "bottle" rectangle
            frame = np.zeros((self.config.height, self.config.width, 3), dtype=np.uint8)
            # Add some noise
            if self.config.noise_level > 0:
                noise = np.random.randint(0, int(255 * self.config.noise_level),
                                          (self.config.height, self.config.width, 3), dtype=np.uint8)
                frame = cv2.add(frame, noise)

            # Draw bottle (green rectangle)
            cv2.rectangle(frame, (x, y), (x + bw, y + bh), (0, 200, 0), -1)
            # Add label
            cv2.putText(frame, "BOTTLE", (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
            # Draw zone lines for debugging
            zw = self.config.width // 3
            cv2.line(frame, (zw, 0), (zw, self.config.height), (100, 100, 100), 1)
            cv2.line(frame, (2 * zw, 0), (2 * zw, self.config.height), (100, 100, 100), 1)
            # Draw zone labels
            cv2.putText(frame, "LEFT", (5, 15), cv2.FONT_HERSHEY_SIMPLEX, 0.3, (150, 150, 150), 1)
            cv2.putText(frame, "CENTER", (zw + 5, 15), cv2.FONT_HERSHEY_SIMPLEX, 0.3, (150, 150, 150), 1)
            cv2.putText(frame, "RIGHT", (2 * zw + 5, 15), cv2.FONT_HERSHEY_SIMPLEX, 0.3, (150, 150, 150), 1)

            with self._lock:
                self._frame = frame.copy()

            # Sleep to maintain FPS
            elapsed = time.time() - loop_start
            sleep_time = frame_interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    def get_frame(self) -> Optional[Any]:
        """Get latest generated frame."""
        with self._lock:
            return self._frame.copy() if self._frame is not None else None

    def stop(self) -> None:
        """Stop frame generation."""
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        with self._lock:
            self._frame = None
        self._logger.info("Mock camera stopped")

    def __enter__(self) -> "MockCamera":
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()