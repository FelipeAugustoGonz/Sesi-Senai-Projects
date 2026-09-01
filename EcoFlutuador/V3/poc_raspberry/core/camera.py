"""
Camera module for EcoFlutuador POC.
Provides asynchronous frame capture with minimal latency.
"""
import cv2
import threading
import time
import logging
from typing import Optional, Tuple, Any
from dataclasses import dataclass


@dataclass
class CameraConfig:
    index: int = 0
    width: int = 320
    height: int = 240
    fps: int = 10
    backend: str = "V4L2"
    buffer_size: int = 1


class Camera:
    """
    Asynchronous camera capture thread.
    Maintains a single-frame buffer for minimal latency.
    """

    def __init__(self, config: CameraConfig):
        self.config = config
        self._cam: Optional[cv2.VideoCapture] = None
        self._frame: Optional[Any] = None
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._logger = logging.getLogger(__name__)
        self._backend_map = {
            "V4L2": cv2.CAP_V4L2,
            "ANY": cv2.CAP_ANY,
            "DSHOW": cv2.CAP_DSHOW,  # Windows
            "MSMF": cv2.CAP_MSMF,    # Windows
        }

    def start(self) -> bool:
        """Initialize camera and start capture thread."""
        if self._running:
            return True

        backend = self._backend_map.get(self.config.backend.upper(), cv2.CAP_ANY)
        self._cam = cv2.VideoCapture(self.config.index, backend)

        if not self._cam.isOpened():
            self._logger.error(f"Failed to open camera index {self.config.index} with backend {self.config.backend}")
            self._cam.release()
            self._cam = None
            return False

        # Configure camera properties
        self._cam.set(cv2.CAP_PROP_FRAME_WIDTH, self.config.width)
        self._cam.set(cv2.CAP_PROP_FRAME_HEIGHT, self.config.height)
        self._cam.set(cv2.CAP_PROP_FPS, self.config.fps)
        self._cam.set(cv2.CAP_PROP_BUFFERSIZE, self.config.buffer_size)

        # Verify actual settings
        actual_w = int(self._cam.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(self._cam.get(cv2.CAP_PROP_FRAME_HEIGHT))
        actual_fps = self._cam.get(cv2.CAP_PROP_FPS)
        self._logger.info(f"Camera opened: {actual_w}x{actual_h} @ {actual_fps:.1f} FPS (requested: {self.config.width}x{self.config.height} @ {self.config.fps})")

        self._running = True
        self._thread = threading.Thread(target=self._capture_loop, daemon=True)
        self._thread.start()
        return True

    def _capture_loop(self) -> None:
        """Background thread: continuously capture frames."""
        while self._running and self._cam is not None:
            ret, frame = self._cam.read()
            if ret and frame is not None:
                with self._lock:
                    self._frame = frame.copy()
            else:
                self._logger.warning("Camera read failed")
                time.sleep(0.01)

    def get_frame(self) -> Optional[Any]:
        """Get the latest frame (thread-safe copy)."""
        with self._lock:
            return self._frame.copy() if self._frame is not None else None

    def stop(self) -> None:
        """Stop capture thread and release camera."""
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        if self._cam is not None:
            self._cam.release()
            self._cam = None
        with self._lock:
            self._frame = None
        self._logger.info("Camera stopped")

    def __enter__(self) -> "Camera":
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()


def list_cameras(max_index: int = 10) -> list:
    """List available camera indices."""
    available = []
    for i in range(max_index):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            ret, _ = cap.read()
            if ret:
                available.append(i)
            cap.release()
    return available