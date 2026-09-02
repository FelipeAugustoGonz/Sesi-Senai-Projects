"""
Camera module for EcoFlutuador POC.
Provides asynchronous frame capture with minimal latency.
Supports multiple backends: V4L2 (USB), Picamera2 (RPi CSI), ANY (auto).
"""
import cv2
import threading
import time
import logging
import numpy as np
from typing import Optional, Tuple, Any, Dict, List
from dataclasses import dataclass


@dataclass
class CameraConfig:
    backend: str = "V4L2"  # V4L2, PICAMERA2, ANY, DSHOW, MSMF
    width: int = 320
    height: int = 240
    fps: int = 10
    buffer_size: int = 1
    # Picamera2 specific
    picamera2_format: str = "RGB888"
    picamera2_transform: Dict = None


class Camera:
    """
    Asynchronous camera capture thread.
    Maintains a single-frame buffer for minimal latency.
    Supports multiple backends including Picamera2 for RPi CSI cameras.
    """

    def __init__(self, config: CameraConfig):
        self.config = config
        self._cam: Any = None
        self._frame: Optional[Any] = None
        self._lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._logger = logging.getLogger(__name__)
        self._actual_width = config.width
        self._actual_height = config.height
        self._actual_fps = config.fps
        self._backend = config.backend.upper()
        
        # Backend map for OpenCV
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

        if self._backend == "PICAMERA2":
            return self._start_picamera2()
        else:
            return self._start_opencv()

    def _start_picamera2(self) -> bool:
        """Start camera using Picamera2 (RPi CSI)."""
        try:
            from picamera2 import Picamera2
            from libcamera import Transform
        except ImportError as e:
            self._logger.error(f"Picamera2 not available: {e}. Install with: pip install picamera2")
            return False

        self._cam = Picamera2()
        
        # Get available sensor modes to validate resolution
        sensor_modes = self._cam.sensor_modes
        self._logger.info(f"Available sensor modes: {sensor_modes}")
        
        # Find matching mode or closest
        requested_width = self.config.width
        requested_height = self.config.height
        requested_fps = self.config.fps
        
        best_mode = None
        best_score = float('inf')
        
        for mode in sensor_modes:
            # mode format: {'size': (width, height), 'format': '...', 'fps': ..., 'crop_limits': ...}
            mode_w, mode_h = mode.get('size', (0, 0))
            mode_fps = mode.get('fps', 0)
            
            # Score based on resolution and fps match
            score = abs(mode_w - requested_width) + abs(mode_h - requested_height) + abs(mode_fps - requested_fps) * 10
            if score < best_score:
                best_score = score
                best_mode = mode
        
        if best_mode is None:
            self._logger.error("No suitable sensor mode found")
            return False
            
        mode_w, mode_h = best_mode['size']
        self._logger.info(f"Selected sensor mode: {mode_w}x{mode_h} @ {best_mode.get('fps', 'N/A')} fps")
        
        # Create video configuration
        video_config = self._cam.create_video_configuration(
            main={"size": (mode_w, mode_h), "format": self.config.picamera2_format},
            controls={"FrameDurationLimits": (int(1_000_000 / requested_fps), int(1_000_000 / requested_fps))},
            transform=Transform(**self.config.picamera2_transform) if self.config.picamera2_transform else None
        )
        
        self._cam.configure(video_config)
        self._cam.start()
        
        # Get actual configuration
        actual_config = self._cam.camera_configuration()["main"]
        self._actual_width = actual_config["size"][0]
        self._actual_height = actual_config["size"][1]
        
        self._logger.info(f"Picamera2 started: {self._actual_width}x{self._actual_height} @ {requested_fps} FPS (requested: {requested_width}x{requested_height})")
        
        self._running = True
        self._thread = threading.Thread(target=self._capture_loop_picamera2, daemon=True)
        self._thread.start()
        return True

    def _start_opencv(self) -> bool:
        """Start camera using OpenCV VideoCapture (USB/V4L2)."""
        backend = self._backend_map.get(self._backend, cv2.CAP_ANY)
        
        # Try common indices if index not specified or 0
        indices_to_try = [0, 1, 2, 3] if self.config.backend.upper() != "PICAMERA2" else [0]
        
        for idx in indices_to_try:
            self._cam = cv2.VideoCapture(idx, backend)
            if self._cam.isOpened():
                ret, _ = self._cam.read()
                if ret:
                    break
            self._cam.release()
            self._cam = None
        else:
            self._logger.error(f"Failed to open any camera with backend {self._backend}")
            return False

        # Configure camera properties
        self._cam.set(cv2.CAP_PROP_FRAME_WIDTH, self.config.width)
        self._cam.set(cv2.CAP_PROP_FRAME_HEIGHT, self.config.height)
        self._cam.set(cv2.CAP_PROP_FPS, self.config.fps)
        self._cam.set(cv2.CAP_PROP_BUFFERSIZE, self.config.buffer_size)

        # Verify actual settings
        self._actual_width = int(self._cam.get(cv2.CAP_PROP_FRAME_WIDTH))
        self._actual_height = int(self._cam.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self._actual_fps = self._cam.get(cv2.CAP_PROP_FPS)
        self._logger.info(f"Camera opened: {self._actual_width}x{self._actual_height} @ {self._actual_fps:.1f} FPS (requested: {self.config.width}x{self.config.height} @ {self.config.fps})")

        self._running = True
        self._thread = threading.Thread(target=self._capture_loop, daemon=True)
        self._thread.start()
        return True

    def _capture_loop(self) -> None:
        """Background thread: continuously capture frames (OpenCV)."""
        while self._running and self._cam is not None:
            ret, frame = self._cam.read()
            if ret and frame is not None:
                with self._lock:
                    self._frame = frame.copy()
            else:
                self._logger.warning("Camera read failed")
                time.sleep(0.01)

    def _capture_loop_picamera2(self) -> None:
        """Background thread: continuously capture frames (Picamera2)."""
        while self._running and self._cam is not None:
            try:
                # Picamera2 returns RGB, convert to BGR for OpenCV compatibility
                frame = self._cam.capture_array()
                if frame is not None:
                    # Picamera2 returns RGB, OpenCV expects BGR
                    if frame.shape[2] == 3:
                        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                    elif frame.shape[2] == 4:
                        frame = cv2.cvtColor(frame, cv2.COLOR_RGBA2BGR)
                    
                    with self._lock:
                        self._frame = frame.copy()
                else:
                    time.sleep(0.001)
            except Exception as e:
                self._logger.warning(f"Picamera2 capture error: {e}")
                time.sleep(0.01)

    def get_frame(self) -> Optional[Any]:
        """Get the latest frame (thread-safe copy)."""
        with self._lock:
            return self._frame.copy() if self._frame is not None else None

    def get_actual_resolution(self) -> Tuple[int, int]:
        """Get the actual resolution being used."""
        return (self._actual_width, self._actual_height)

    def get_actual_fps(self) -> float:
        """Get the actual FPS."""
        return self._actual_fps

    def stop(self) -> None:
        """Stop capture thread and release camera."""
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        
        if self._cam is not None:
            try:
                if self._backend == "PICAMERA2":
                    self._cam.stop()
                    self._cam.close()
                else:
                    self._cam.release()
            except Exception as e:
                self._logger.warning(f"Error closing camera: {e}")
            self._cam = None
        
        with self._lock:
            self._frame = None
        self._logger.info("Camera stopped")

    def __enter__(self) -> "Camera":
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()


def list_cameras(max_index: int = 10) -> List[int]:
    """List available camera indices. Includes Picamera2 if available."""
    available = []
    # Try OpenCV cameras
    for i in range(max_index):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            ret, _ = cap.read()
            if ret:
                available.append(i)
            cap.release()
    
    # Try Picamera2
    try:
        from picamera2 import Picamera2
        available.append("picamera2 (CSI)")
    except ImportError:
        pass
    return available


def list_picamera2_modes() -> List[Dict]:
    """List available Picamera2 sensor modes (RPi only)."""
    try:
        from picamera2 import Picamera2
        cam = Picamera2()
        modes = cam.sensor_modes
        cam.close()
        return modes
    except ImportError:
        return []
    except Exception as e:
        logging.getLogger(__name__).error(f"Failed to list Picamera2 modes: {e}")
        return []